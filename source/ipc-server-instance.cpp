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
#include "os-error.hpp"
#include <sstream>

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
	// Variables
	/// Reading
	os::error read_error = os::error::Ok;
	size_t read_length = 0;
	size_t read_full_length = 0;
	char read_buffer_temp = 0;
	std::vector<char> read_buffer;
	/// Processing
	::Authenticate proc_pb_auth;
	::AuthenticateReply proc_pb_auth_reply;
	::FunctionCall proc_pb_call;
	::FunctionResult proc_pb_result;
	std::vector<ipc::value> proc_args;
	std::vector<ipc::value> proc_rval;
	ipc::value proc_tempval;
	std::string proc_error;
	size_t proc_reply_size = 0;
	/// Writing
	os::error write_error = os::error::Ok;
	size_t write_length = 0;
	std::vector<char> write_buffer;
	std::queue<std::vector<char>> write_queue;

	// Prepare Buffers
	read_buffer.reserve(m_parent->m_socket->get_receive_buffer_size());
	write_buffer.reserve(m_parent->m_socket->get_send_buffer_size());

	// Loop
	while ((!m_stopWorkers) && m_socket->good()) {
		// Attempt to clear the output queue.
		if (write_queue.size() > 0) {
			while ((write_queue.size() > 0) || (write_length != write_buffer.size())) {
				auto& buf = write_queue.front();
				write_error = m_socket->write(buf.data(), buf.size(), write_length);
				if (write_error != os::error::Ok) {
					break;
				} else {
					write_queue.pop();
				}
			}
			/// Flush and give up time slice.
			if (m_isAuthenticated) {
				m_writeSignal->set();
			}
		#ifdef _WIN32
			Sleep(0);
		#endif
		}

		// Read Message
		if (!m_isAuthenticated) {
			if (m_socket->read_avail() == 0) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}
		} else {
			if (m_socket->read_avail() == 0) {
				switch (m_readSignal->wait(std::chrono::milliseconds(10))) {
					case os::error::Error:
					case os::error::Abandoned:
					case os::error::TimedOut:
					#ifdef _WIN32
						Sleep(0);
					#endif
						continue;
				}
			}
			m_readSignal->clear();
		}

		read_full_length = m_socket->read_avail();
		read_buffer.resize(read_full_length);
		read_error = m_socket->read(read_buffer.data(), read_full_length, read_length);
		if (read_error != os::error::Ok) {
			continue;
		}

		// Decode the new message.
		bool success = false;
		if (!m_isAuthenticated) {
			// Client is not authenticated.

			/// Authentication is required so the two sides have an event to actually wait on. This is necessary to 
			///  avoid a race condition that would cause us to lose data to a read that timed out just before it was 
			///  done.

			success = proc_pb_auth.ParsePartialFromArray(read_buffer.data(), read_full_length);
			if (!success) {
				continue;
			}

			std::string read_event_name = "Global\\" + proc_pb_auth.name() + "_r";
			std::string write_event_name = "Global\\" + proc_pb_auth.name() + "_w";
			m_readSignal = os::signal::create(read_event_name);
			m_writeSignal = os::signal::create(write_event_name);

			proc_pb_auth_reply.Clear();
			proc_pb_auth_reply.set_read_event(read_event_name);
			proc_pb_auth_reply.set_write_event(write_event_name);

			// Encode
			proc_reply_size = proc_pb_auth_reply.ByteSizeLong();
			if (proc_reply_size == 0) {
				continue;
			}

			write_buffer.resize(proc_reply_size);
			success = proc_pb_auth_reply.SerializePartialToArray(write_buffer.data(), proc_reply_size);
			if (!success) {
				continue;
			}

			m_isAuthenticated = true;
		} else {
			// Client is authenticated.

			// Parse
			success = proc_pb_call.ParsePartialFromArray(read_buffer.data(), read_full_length);
			if (!success) {
				continue;
			}

			// Decode Arguments
			proc_args.resize(proc_pb_call.arguments_size());
			for (size_t n = 0; n < proc_args.size(); n++) {
				DecodeProtobufToIPC(proc_pb_call.arguments((int)n), proc_tempval);
				proc_args[n] = std::move(proc_tempval);
			}

			// Execute
			proc_pb_result.Clear();
			proc_pb_result.set_timestamp(proc_pb_call.timestamp());
			proc_rval.resize(0);
			success = m_parent->client_call_function(m_clientId,
				proc_pb_call.classname(), proc_pb_call.functionname(),
				proc_args, proc_rval, proc_error);
			if (success) {
				for (size_t n = 0; n < proc_rval.size(); n++) {
					::Value* rv = proc_pb_result.add_value();
					EncodeIPCToProtobuf(proc_rval[n], rv);
				}
			} else {
				proc_pb_result.set_error(proc_error);
			}

			// Encode
			proc_reply_size = proc_pb_result.ByteSizeLong();
			if (proc_reply_size == 0) {
				continue;
			}

			write_buffer.resize(proc_reply_size);
			success = proc_pb_result.SerializePartialToArray(write_buffer.data(), proc_reply_size);
			if (!success) {
				continue;
			}
		}

		// Write new output.
		write_error = m_socket->write(write_buffer.data(), write_buffer.size(), write_length);
		if ((write_error != os::error::Ok) || (write_length != write_buffer.size())) {
			// Failed to write? Put it into the queue.
			write_queue.push(std::move(write_buffer));
			write_buffer.reserve(m_parent->m_socket->get_send_buffer_size());
		} else {
			/// Flush and give up current time slice to any signaled threads.
			if (m_isAuthenticated) {
				m_writeSignal->set();
			}
		#ifdef _WIN32
			Sleep(0);
		#endif
		}
	}
}
