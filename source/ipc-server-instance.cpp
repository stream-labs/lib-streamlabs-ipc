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
#include <sstream>
#include <functional>

using namespace std::placeholders;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

ipc::server_instance::server_instance() {}

ipc::server_instance::server_instance(ipc::server* owner, std::shared_ptr<os::windows::named_pipe> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = 0;

	m_stopWorkers = false;
	m_worker = std::thread(std::bind(&server_instance::worker, this));
}

ipc::server_instance::~server_instance() {
	// Threading
	m_stopWorkers = true;
	if (m_worker.joinable())
		m_worker.join();
}

bool ipc::server_instance::is_alive() {
	if (!m_socket->is_connected())
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
	os::error ec = os::error::Success;

	// Prepare Buffers
	m_rbuf.reserve(65535);
	m_wbuf.reserve(65535);

	// Loop
	while ((!m_stopWorkers) && m_socket->is_connected()) {
		if (!m_rop || !m_rop->is_valid()) {
			m_rbuf.resize(sizeof(ipc_size_t));
			ec = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&server_instance::read_callback_init, this, _1, _2));
			if (ec != os::error::Pending && ec != os::error::Success) {
				if (ec == os::error::Disconnected) {
					break;
				} else {
					throw std::exception("Unexpected error.");
				}
			}
		}
		if (!m_wop || !m_wop->is_valid()) {
			if (m_write_queue.size() > 0) {
				std::vector<char>& fbuf = m_write_queue.front();
				ipc::make_sendable(m_wbuf, fbuf);
				ec = m_socket->write(m_wbuf.data(), m_wbuf.size(), m_wop, std::bind(&server_instance::write_callback, this, _1, _2));
				if (ec != os::error::Pending && ec != os::error::Success) {
					if (ec == os::error::Disconnected) {
						break;
					} else {
						throw std::exception("Unexpected error.");
					}
				}
				m_write_queue.pop();
			}
		}

		os::waitable * waits[] = { m_rop.get(), m_wop.get() };
		size_t                      wait_index = -1;
		for (size_t idx = 0; idx < 2; idx++) {
			if (waits[idx] != nullptr) {
				if (waits[idx]->wait(std::chrono::milliseconds(0)) == os::error::Success) {
					wait_index = idx;
					break;
				}
			}
		}
		if (wait_index == -1) {
			os::error code = os::waitable::wait_any(waits, 2, wait_index, std::chrono::milliseconds(20));
			if (code == os::error::TimedOut) {
				continue;
			} else if (code == os::error::Disconnected) {
				break;
			} else if (code == os::error::Error) {
				throw std::exception("Error");
			}
		}
	}
}

void ipc::server_instance::read_callback_init(os::error ec, size_t size) {
	os::error ec2 = os::error::Success;

	m_rop->invalidate();

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(m_rbuf);
		if (n_size != 0) {
			m_rbuf.resize(n_size);
			ec2 = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&server_instance::read_callback_msg, this, _1, _2));
			if (ec2 != os::error::Pending && ec2 != os::error::Success) {
				if (ec2 == os::error::Disconnected) {
					return;
				} else {
					throw std::exception("Unexpected error.");
				}
			}
		}
	}
}

void ipc::server_instance::read_callback_msg(os::error ec, size_t size) {
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
	std::vector<char> write_buffer;

	m_rop->invalidate();

	if (ec != os::error::Success) {
		return;
	}

	bool success = false;
	if (!m_isAuthenticated) {
		// Client is not authenticated.
		/// Authentication is required so that both sides know that the other is ready.

		success = proc_pb_auth.ParsePartialFromArray(m_rbuf.data(), int(m_rbuf.size()));
		if (!success) {
			std::cerr << "Failed to parse Authenticate message." << std::endl;
			return;
		}

		proc_pb_auth_reply.Clear();
		proc_pb_auth_reply.set_password(proc_pb_auth.password());

		// Encode
		proc_reply_size = proc_pb_auth_reply.ByteSizeLong();
		if (proc_reply_size == 0) {
			std::cerr << "Failed to encode AuthenticateReply message." << std::endl;
			return;
		}

		write_buffer.resize(proc_reply_size);
		success = proc_pb_auth_reply.SerializePartialToArray(write_buffer.data(), int(proc_reply_size));
		if (!success) {
			std::cerr << "Failed to serialize AuthenticateReply message." << std::endl;
			return;
		}

		m_isAuthenticated = true;
	} else {
		// Client is authenticated.

		// Parse
		success = proc_pb_call.ParsePartialFromArray(m_rbuf.data(), int(m_rbuf.size()));
		if (!success) {
			std::cerr << "Failed to parse FunctionCall message." << std::endl;
			return;
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
			std::cerr << "Failed to encode FunctionResult message." << std::endl;
			return;
		}

		write_buffer.resize(proc_reply_size);
		success = proc_pb_result.SerializePartialToArray(write_buffer.data(), int(proc_reply_size));
		if (!success) {
			std::cerr << "Failed to serialize FunctionResult message." << std::endl;
			return;
		}
	}

	if (write_buffer.size() != 0) {
		if ((!m_wop || !m_wop->is_valid()) && (m_write_queue.size() == 0)) {
			ipc::make_sendable(m_wbuf, write_buffer);
			os::error ec2 = m_socket->write(m_wbuf.data(), m_wbuf.size(), m_wop, std::bind(&server_instance::write_callback, this, _1, _2));
			if (ec2 != os::error::Success && ec2 != os::error::Pending) {
				if (ec2 == os::error::Disconnected) {
					return;
				} else {
					throw std::exception("Unexpected Error");
				}
			}
		} else {
			m_write_queue.push(std::move(write_buffer));
		}
	}
}

void ipc::server_instance::write_callback(os::error ec, size_t size) {
	m_wop->invalidate();
	// Do we need to do anything here? Not really.

	// Uncomment this to give up the rest of the time slice to the next thread.
	// Not recommended since we do this anyway with the next wait.	
	/*
	#ifdef _WIN32
		Sleep(0);
	#endif
	*/
}
