// A custom IPC solution to bypass electron IPC.
// Copyright(C) 2017 Streamlabs (General Workings Inc)
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.

#include "ipc-server-instance.hpp"
#include "ipc.pb.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

ipc::server_instance::server_instance() {}

ipc::server_instance::server_instance(ipc::server* owner, std::shared_ptr<os::named_socket_connection> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = m_socket->get_client_id();
	m_stopWorkers = false;
	m_worker = std::thread(worker_main, this);
}

ipc::server_instance::~server_instance() {
	// Threading
	m_stopWorkers = true;
	if (m_worker.joinable())
		m_worker.join();
}

bool ipc::server_instance::is_alive() {
	if (m_socket->bad())
		return false;

	if (m_stopWorkers)
		return false;

	return true;
}

void DecodeProtobufToIPC(const ::Value& v, ipc::value& val) {
	switch (v.value_case()) {
		case Value::ValueCase::kValBool:
			val.type = ipc::type::Int32; // ToDo: Add Bool type
			val.value_union.i32 = v.val_bool() ? 1 : 0;
			break;
		case Value::ValueCase::kValFloat:
			val.type = ipc::type::Float;
			val.value_union.fp32 = v.val_float();
			break;
		case Value::ValueCase::kValDouble:
			val.type = ipc::type::Double;
			val.value_union.fp64 = v.val_double();
			break;
		case Value::ValueCase::kValInt32:
			val.type = ipc::type::Int32;
			val.value_union.i32 = v.val_int32();
			break;
		case Value::ValueCase::kValInt64:
			val.type = ipc::type::Int64;
			val.value_union.i64 = v.val_int64();
			break;
		case Value::ValueCase::kValUint32:
			val.type = ipc::type::UInt32;
			val.value_union.ui32 = v.val_uint32();
			break;
		case Value::ValueCase::kValUint64:
			val.type = ipc::type::UInt64;
			val.value_union.ui64 = v.val_uint64();
			break;
		case Value::ValueCase::kValString:
			val.type = ipc::type::String;
			val.value_str = v.val_string();
			break;
		case Value::ValueCase::kValBinary:
			val.type = ipc::type::Binary;
			val.value_bin.resize(v.val_binary().size());
			memcpy(val.value_bin.data(), v.val_binary().data(), val.value_bin.size());
			break;
		default:
			val.type = ipc::type::Null;
			break;
	}
}
void EncodeIPCToProtobuf(const ipc::value& v, ::Value* val) {
	switch (v.type) {
		case ipc::type::Float:
			val->set_val_float(v.value_union.fp32);
			break;
		case ipc::type::Double:
			val->set_val_double(v.value_union.fp64);
			break;
		case ipc::type::Int32:
			val->set_val_int32(v.value_union.i32);
			break;
		case ipc::type::Int64:
			val->set_val_int64(v.value_union.i64);
			break;
		case ipc::type::UInt32:
			val->set_val_uint32(v.value_union.ui32);
			break;
		case ipc::type::UInt64:
			val->set_val_uint64(v.value_union.ui64);
			break;
		case ipc::type::String:
			val->set_val_string(v.value_str);
			break;
		case ipc::type::Binary:
			val->set_val_binary(v.value_bin.data(), v.value_bin.size());
			break;
		case ipc::type::Null:
		default:
			break;
	}
}

void ipc::server_instance::worker() {
	std::vector<char> input_buffer;
	std::vector<char> output_buffer;
	std::queue<std::vector<char>> output_queue;
	std::vector<ipc::value> function_arguments;
	std::vector<ipc::value> function_returnvalues;
	::Authenticate authenticate_pb;
	::FunctionCall functioncall_pb;
	::FunctionResult functionresult_pb;
	ipc::value value_ipc;
	std::string error_ipc;

	input_buffer.reserve(m_parent->m_socket->get_receive_buffer_size());
	output_buffer.reserve(m_parent->m_socket->get_send_buffer_size());

	while ((!m_stopWorkers) && m_socket->good()) {
		size_t input_size = 0, stream_input_size = 0;
		size_t output_size = 0, stream_output_size = 0;

		// Attempt to clear the output queue.
		if (output_queue.size() > 0) {
			while (output_queue.size() > 0) {
				stream_output_size = m_socket->write(output_queue.front());
				if (stream_output_size != output_queue.front().size()) {
					break;
				} else {
					output_queue.pop();
				}
			}
		}

		// Attempt to read new message.
		if ((input_size = m_socket->read_avail()) == 0) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
			continue;
		}

		input_buffer.resize(input_size);
		stream_input_size = m_socket->read(input_buffer.data(), input_size);
		if (stream_input_size != input_size) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
			continue;
		}
		
		// Process read message.
		bool success = false;
		if (!m_isAuthenticated) {
			// Client is not authenticated.
			success = authenticate_pb.ParsePartialFromArray(input_buffer.data(), stream_input_size);
			if (success) {
				m_isAuthenticated = true;
				continue;
			}
		} else {
			// Client is authenticated.

			// Parse
			success = functioncall_pb.ParsePartialFromArray(input_buffer.data(), stream_input_size);
			if (!success) {
				//sprintf_s("Skipped parsing invalid packet from %llu.", m_clientId);
				continue;
			}

			// Decode Arguments
			function_arguments.resize(functioncall_pb.arguments_size());
			for (size_t n = 0; n < function_arguments.size(); n++) {
				DecodeProtobufToIPC(functioncall_pb.arguments((int)n), value_ipc);
				function_arguments[n] = std::move(value_ipc);
			}

			// Execute
			functionresult_pb.Clear();
			functionresult_pb.set_timestamp(functioncall_pb.timestamp());
			function_returnvalues.resize(0);
			success = m_parent->client_call_function(m_clientId,
				functioncall_pb.classname(), functioncall_pb.functionname(),
				function_arguments, function_returnvalues, error_ipc);
			if (success) {
				for (size_t n = 0; n < function_returnvalues.size(); n++) {
					::Value* rv = functionresult_pb.add_value();
					EncodeIPCToProtobuf(function_returnvalues[n], rv);
				}
			} else {
				functionresult_pb.set_error(error_ipc);
			}

			// Encode
			output_size = functionresult_pb.ByteSizeLong();
			if (output_size == 0) {
				continue;
			}

			output_buffer.resize(output_size);
			success = functionresult_pb.SerializePartialToArray(output_buffer.data(), output_size);
			if (!success) {
				continue;
			}
		}

		// Write new output.
		stream_output_size = m_socket->write(output_buffer.data(), output_size);
		if (stream_output_size != output_size) {
			// Failed to write? Put it into the queue.
			output_queue.push(std::move(output_buffer));
			output_buffer.reserve(m_parent->m_socket->get_send_buffer_size());
		}
	}
}
