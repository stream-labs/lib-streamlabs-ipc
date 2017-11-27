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
				val->set_type(ValueType::Float);
				val->set_val_float(v.value.fp32);
				break;
			case Type::Double:
				val->set_type(ValueType::Double);
				val->set_val_double(v.value.fp64);
				break;
			case Type::Int32:
				val->set_type(ValueType::Int32);
				val->set_val_int32(v.value.i32);
				break;
			case Type::Int64:
				val->set_type(ValueType::Int64);
				val->set_val_int64(v.value.i64);
				break;
			case Type::UInt32:
				val->set_type(ValueType::UInt32);
				val->set_val_uint32(v.value.ui32);
				break;
			case Type::UInt64:
				val->set_type(ValueType::UInt64);
				val->set_val_uint64(v.value.ui64);
				break;
			case Type::String:
				val->set_type(ValueType::String);
				val->set_val_string(v.value_str);
				break;
			case Type::Binary:
			{
				val->set_type(ValueType::Binary);
				val->set_val_binary(v.value_bin.data(), v.value_bin.size());
				break;
			}
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
	std::vector<char> buf;
	while (!ptr->m_stopWorkers) {
		if (sock->Bad())
			break;

		if (sock->ReadAvail() == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}
		
		buf.resize(sock->ReadAvail());
		size_t bytes = sock->Read(buf);
		if (bytes == 0)
			continue;

		// Decode Result
		::FunctionResult fres;
		if (!fres.ParsePartialFromArray(buf.data(), buf.size()))
			continue;
		if (ptr->m_cb.count(fres.timestamp()) == 0)
			continue;

		std::pair<CallReturn_t, void*> cb;
		{
			std::unique_lock<std::mutex> ulock(ptr->m_lock);
			cb = ptr->m_cb.at(fres.timestamp());
		}

		/// Value
		IPC::Value val;
		if (fres.has_value()) {
			auto v = fres.value();
			switch (v.type()) {
				case ValueType::Float:
					val.type = Type::Float;
					val.value.fp32 = v.val_float();
					break;
				case ValueType::Double:
					val.type = Type::Double;
					val.value.fp64 = v.val_double();
					break;
				case ValueType::Int32:
					val.type = Type::Int32;
					val.value.i32 = v.val_int32();
					break;
				case ValueType::Int64:
					val.type = Type::Int64;
					val.value.i64 = v.val_int64();
					break;
				case ValueType::UInt32:
					val.type = Type::UInt32;
					val.value.ui32 = v.val_uint32();
					break;
				case ValueType::UInt64:
					val.type = Type::UInt64;
					val.value.ui64 = v.val_uint64();
					break;
				case ValueType::String:
					val.type = Type::String;
					val.value_str = v.val_string();
					break;
				case ValueType::Binary:
					val.type = Type::Binary;
					val.value_bin.resize(v.val_binary().size());
					memcpy(val.value_bin.data(), v.val_binary().data(), val.value_bin.size());
					break;
				case ValueType::Null:
				default:
					val.type = Type::Null;
					break;
			}
		} else if (fres.error().length() > 0) {
			val.type = Type::Null;
			val.value_str = fres.error();
		}

		/// Callback
		cb.first(cb.second, val);

		/// Remove cb entry
		{
			std::unique_lock<std::mutex> ulock(ptr->m_lock);
			ptr->m_cb.erase(fres.timestamp());
		}
	}
}
