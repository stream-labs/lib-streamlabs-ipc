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

IPC::ServerInstance::ServerInstance() {}

IPC::ServerInstance::ServerInstance(IPC::Server* owner, std::shared_ptr<OS::NamedSocketConnection> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = m_socket->GetClientId();
	m_stopWorkers = false;
	m_worker = std::thread(WorkerThread, this);
}

IPC::ServerInstance::~ServerInstance() {
	// Threading
	m_stopWorkers = true;
	if (m_worker.joinable())
		m_worker.join();
}

bool IPC::ServerInstance::IsAlive() {
	if (m_socket->Bad())
		return false;

	if (m_stopWorkers)
		return false;

	return true;
}

void DecodeProtobufToIPC(const ::Value& v, IPC::Value& val) {
	switch (v.type()) {
		case ValueType::Float:
			val.type = IPC::Type::Float;
			val.value.fp32 = v.val_float();
			break;
		case ValueType::Double:
			val.type = IPC::Type::Double;
			val.value.fp64 = v.val_double();
			break;
		case ValueType::Int32:
			val.type = IPC::Type::Int32;
			val.value.i32 = v.val_int32();
			break;
		case ValueType::Int64:
			val.type = IPC::Type::Int64;
			val.value.i64 = v.val_int64();
			break;
		case ValueType::UInt32:
			val.type = IPC::Type::UInt32;
			val.value.ui32 = v.val_uint32();
			break;
		case ValueType::UInt64:
			val.type = IPC::Type::UInt64;
			val.value.ui64 = v.val_uint64();
			break;
		case ValueType::String:
			val.type = IPC::Type::String;
			val.value_str = v.val_string();
			break;
		case ValueType::Binary:
			val.type = IPC::Type::Binary;
			val.value_bin.resize(v.val_binary().size());
			memcpy(val.value_bin.data(), v.val_binary().data(), val.value_bin.size());
			break;
		case ValueType::Null:
		default:
			val.type = IPC::Type::Null;
			break;
	}
}
void EncodeIPCToProtobuf(const IPC::Value& v, ::Value* val) {
	switch (v.type) {
		case IPC::Type::Float:
			val->set_type(ValueType::Float);
			val->set_val_float(v.value.fp32);
			break;
		case IPC::Type::Double:
			val->set_type(ValueType::Double);
			val->set_val_double(v.value.fp64);
			break;
		case IPC::Type::Int32:
			val->set_type(ValueType::Int32);
			val->set_val_int32(v.value.i32);
			break;
		case IPC::Type::Int64:
			val->set_type(ValueType::Int64);
			val->set_val_int64(v.value.i64);
			break;
		case IPC::Type::UInt32:
			val->set_type(ValueType::UInt32);
			val->set_val_uint32(v.value.ui32);
			break;
		case IPC::Type::UInt64:
			val->set_type(ValueType::UInt64);
			val->set_val_uint64(v.value.ui64);
			break;
		case IPC::Type::String:
			val->set_type(ValueType::String);
			val->set_val_string(v.value_str);
			break;
		case IPC::Type::Binary:
			val->set_type(ValueType::Binary);
			val->set_val_binary(v.value_bin.data(), v.value_bin.size());
			break;
		case IPC::Type::Null:
		default:
			val->set_type(ValueType::Null);
			break;
	}
}

void IPC::ServerInstance::Worker() {
	std::vector<char> buf;
	Authenticate msgAuthenticate;
	FunctionCall msgCall;
	FunctionResult msgResult;
	std::vector<IPC::Value> args(64);
	IPC::Value val;
	std::string errMsg = "";

	size_t readAttempt = 0;

	while (!m_stopWorkers) {
		if (m_socket->ReadAvail() == 0) {
			readAttempt++;
			if (readAttempt > 100) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(0));
			}

			if (m_socket->Good())
				continue;

			break;
		}

		// Attempt to read a packet.
		buf.resize(m_socket->ReadAvail());
		size_t length = m_socket->Read(buf);
		if (length == 0)
			continue;

		readAttempt = 0;

		if (!m_isAuthenticated) {
			bool suc = msgAuthenticate.ParsePartialFromArray(buf.data(), length);
			if (suc)
				m_isAuthenticated = true;
		} else {
			if (!msgCall.ParsePartialFromArray(buf.data(), length))
				continue;

			// Decode Arguments
			args.resize(msgCall.arguments_size());
			for (size_t n = 0; n < args.size(); n++) {
				DecodeProtobufToIPC(msgCall.arguments(n), val);
				args[n] = std::move(val);
			}

			// Execute
			msgResult.Clear();
			msgResult.set_timestamp(msgCall.timestamp());
			if (!m_parent->ClientCallFunction(m_clientId, msgCall.classname(), msgCall.functionname(), args, errMsg, val)) {
				msgResult.set_error(errMsg);
			}

			// Return Value
			if (val.type != IPC::Type::Null) {
				::Value* rval = msgResult.mutable_value();
				EncodeIPCToProtobuf(val, rval);
			}

			// Encode
			buf.resize(msgResult.ByteSizeLong());
			if (!msgResult.SerializeToArray(buf.data(), buf.size()))
				continue;

			// Write
			if (buf.size() > 0) {
				for (size_t attempt = 0; attempt < 5; attempt++) {
					size_t bytes = m_socket->Write(buf);
					if (bytes == buf.size()) {
						break;
					}
				}
			}
		}
	}
}
