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
	Authenticate msgAuthenticate;
	FunctionCall msgCall;
	FunctionResult msgResult;
	std::vector<ipc::value> args(64);
	std::vector<ipc::value> rval;
	ipc::value val;
	std::string errMsg = "";
	
	// Message
	size_t messageSize = this->m_parent->m_socket->get_receive_buffer_size() > this->m_parent->m_socket->get_send_buffer_size() ? this->m_parent->m_socket->get_receive_buffer_size() : this->m_parent->m_socket->get_send_buffer_size();
	std::vector<char> messageBuffer(messageSize);

	// Loop
	while (!m_stopWorkers) {
		// Attempt to read a message (respects timeout values).
		messageSize = m_socket->read(messageBuffer);
		if (messageSize > 0) {
			if (!m_isAuthenticated) {
				bool suc = msgAuthenticate.ParsePartialFromArray(messageBuffer.data(), (int)messageSize);
				if (suc) {
					m_isAuthenticated = true;
				}
			} else {
				if (!msgCall.ParsePartialFromArray(messageBuffer.data(), (int)messageSize))
					continue;

				// Decode Arguments
				args.resize(msgCall.arguments_size());
				for (size_t n = 0; n < args.size(); n++) {
					DecodeProtobufToIPC(msgCall.arguments((int)n), val);
					args[n] = std::move(val);
				}

				// Execute
				msgResult.Clear();
				msgResult.set_timestamp(msgCall.timestamp());
				rval.clear();
				if (!m_parent->client_call_function(m_clientId, msgCall.classname(), msgCall.functionname(), args, rval, errMsg)) {
					msgResult.set_error(errMsg);
				} else {
					for (size_t n = 0; n < rval.size(); n++) {
						::Value* rv = msgResult.add_value();
						EncodeIPCToProtobuf(val, rv);
					}
				}

				// Encode
				messageSize = msgResult.ByteSizeLong();
				if (!msgResult.SerializePartialToArray(messageBuffer.data(), (int)messageSize))
					continue;

				// Write
				if (messageSize > 0) {
					for (size_t attempt = 0; attempt < 5; attempt++) {
						size_t bytes = m_socket->write(messageBuffer.data(), messageSize);
						if (bytes == messageSize) {
							break;
						}
					}
				}
			}
		}
	}
}
