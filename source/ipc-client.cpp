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

#include "ipc-client.hpp"
#include "ipc.pb.h"

IPC::Client::Client(std::string socketPath) {
	m_socket = OS::NamedSocket::Create();
	if (!m_socket->Connect(socketPath)) {
		throw std::exception("Failed to initialize socket.");
	}

	m_stopWorkers = false;
	m_worker = std::thread(WorkerThread, this);
}

IPC::Client::~Client() {
	m_stopWorkers = true;
	if (m_worker.joinable())
		m_worker.join();
	m_socket->Close();
}

bool IPC::Client::Authenticate() {
	auto sock = m_socket->GetConnection();
	if (sock->Bad())
		return false;

	::Authenticate msg;
	msg.set_password("Hello World"); // Eventually will be used.

	std::vector<char> buf(msg.ByteSizeLong());
	if (!msg.SerializePartialToArray(buf.data(), buf.size()))
		return false;

	for (size_t attempt = 0; attempt < 5; attempt++) {
		if (sock->Bad())
			return false;
		
		size_t written = sock->Write(buf);
		if (written == buf.size()) {
			m_authenticated = true;
			return true;
		}
	}

	return false;
}

bool IPC::Client::Call(std::string cname, std::string fname, std::vector<IPC::Value> args, CallReturn_t fn, void* data) {
	auto sock = m_socket->GetConnection();
	if (sock->Bad())
		return false;

	::FunctionCall msg;
	msg.set_timestamp(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	msg.set_classname(cname);
	msg.set_functionname(fname);
	auto b = msg.mutable_arguments();
	for (IPC::Value& v : args) {
		::Value* val = b->Add();
		switch (v.type) {
			case Type::Float:
				val->set_val_float(v.value.fp32);
				break;
			case Type::Double:
				val->set_val_double(v.value.fp64);
				break;
			case Type::Int32:
				val->set_val_int32(v.value.i32);
				break;
			case Type::Int64:
				val->set_val_int64(v.value.i64);
				break;
			case Type::UInt32:
				val->set_val_uint32(v.value.ui32);
				break;
			case Type::UInt64:
				val->set_val_uint64(v.value.ui64);
				break;
			case Type::String:
				val->set_val_string(v.value_str);
				break;
			case Type::Binary:
				val->set_val_binary(v.value_bin.data(), v.value_bin.size());
				break;
		}
	}

	std::vector<char> buf(msg.ByteSizeLong());
	if (!msg.SerializePartialToArray(buf.data(), buf.size()))
		return false;

	if (fn != nullptr) {
		std::unique_lock<std::mutex> ulock(m_lock);
		m_cb.insert(std::make_pair(msg.timestamp(), std::make_pair(fn, data)));
	}

	for (size_t attempt = 0; attempt < 5; attempt++) {
		if (sock->Bad())
			break;

		size_t written = sock->Write(buf);
		if (written == buf.size())
			return true;
	}
	
	if (fn != nullptr) {
		std::unique_lock<std::mutex> ulock(m_lock);
		m_cb.erase(msg.timestamp());
	}
	return false;
}

void IPC::Client::WorkerThread(Client* ptr) {
	auto sock = ptr->m_socket->GetConnection();

	// Message
	size_t messageSize = ptr->m_socket->GetReceiveBufferSize() > ptr->m_socket->GetSendBufferSize() ? ptr->m_socket->GetReceiveBufferSize() : ptr->m_socket->GetSendBufferSize();
	std::vector<char> messageBuffer(messageSize);

	// Parsing
	::FunctionResult fres;
	std::vector<IPC::Value> rval;

	// Loop
	while (!ptr->m_stopWorkers) {
		// Attempt to read a message (respects timeout values).
		if ((messageSize = sock->Read(messageBuffer)) > 0) {
			// Decode Result
			fres.Clear();
			if (!fres.ParsePartialFromArray(messageBuffer.data(), messageSize))
				continue;

			// Check if there is a callback to call.
			if (ptr->m_cb.count(fres.timestamp()) == 0)
				continue;
			std::pair<CallReturn_t, void*> cb;
			{
				std::unique_lock<std::mutex> ulock(ptr->m_lock);
				cb = ptr->m_cb.at(fres.timestamp());
			}

			/// Value
			if (fres.error().length() > 0) {
				rval.resize(1);
				rval.at(0).type = IPC::Type::Null;
				rval.at(0).value_str = fres.error();
			} else if (fres.value_size() > 0) {
				rval.resize(fres.value_size());
				for (size_t n = 0; n < rval.size(); n++) {
					auto& v = fres.value(n);
					switch (v.value_case()) {
						case ::Value::ValueCase::kValBool:
							rval.at(n).type = IPC::Type::Int32;
							rval.at(n).value.i32 = v.val_bool() ? 1 : 0;
							break;
						case ::Value::ValueCase::kValFloat:
							rval.at(n).type = IPC::Type::Float;
							rval.at(n).value.fp32 = v.val_float();
							break;
						case ::Value::ValueCase::kValDouble:
							rval.at(n).type = IPC::Type::Double;
							rval.at(n).value.fp64 = v.val_double();
							break;
						case ::Value::ValueCase::kValInt32:
							rval.at(n).type = IPC::Type::Int32;
							rval.at(n).value.i32 = v.val_int32();
							break;
						case ::Value::ValueCase::kValInt64:
							rval.at(n).type = IPC::Type::Int64;
							rval.at(n).value.i64 = v.val_int64();
							break;
						case ::Value::ValueCase::kValUint32:
							rval.at(n).type = IPC::Type::UInt32;
							rval.at(n).value.ui32 = v.val_uint32();
							break;
						case ::Value::ValueCase::kValUint64:
							rval.at(n).type = IPC::Type::UInt64;
							rval.at(n).value.ui64 = v.val_uint64();
							break;
						case ::Value::ValueCase::kValString:
							rval.at(n).type = IPC::Type::String;
							rval.at(n).value_str = v.val_string();
							break;
						case ::Value::ValueCase::kValBinary:
							rval.at(n).type = IPC::Type::Binary;
							memcpy(rval.at(n).value_bin.data(), v.val_binary().data(), v.val_binary().size());
							break;
					}
				}
			}

			/// Callback
			cb.first(cb.second, rval);

			/// Remove cb entry
			{ // ToDo: Figure out better way of registering functions, perhaps even a way to have "events" across a IPC connection.
				std::unique_lock<std::mutex> ulock(ptr->m_lock);
				ptr->m_cb.erase(fres.timestamp());
			}
		}
	}
}
