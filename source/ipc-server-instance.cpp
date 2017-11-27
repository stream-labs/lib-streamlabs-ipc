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

IPC::ServerInstance::ServerInstance() {}

IPC::ServerInstance::ServerInstance(IPC::Server* owner, std::shared_ptr<OS::NamedSocketConnection> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = m_socket->GetClientId();

	m_readWorker.worker = std::thread(ReaderThread, this);
	m_executeWorker.worker = std::thread(ExecuteThread, this);
	m_writeWorker.worker = std::thread(WriterThread, this);
}

IPC::ServerInstance::~ServerInstance() {
	// Threading
	m_stopWorkers = true;
	m_readWorker.cv.notify_all();
	if (m_readWorker.worker.joinable())
		m_readWorker.worker.join();
	m_executeWorker.cv.notify_all();
	if (m_executeWorker.worker.joinable())
		m_executeWorker.worker.join();
	m_writeWorker.cv.notify_all();
	if (m_writeWorker.worker.joinable())
		m_writeWorker.worker.join();
}

void IPC::ServerInstance::QueueMessage(std::vector<char> data) {
	std::unique_lock<std::mutex> ulock(m_writeWorker.lock);
	m_writeWorker.queue.push(std::move(data));
	m_writeWorker.cv.notify_all();
}

bool IPC::ServerInstance::IsAlive() {
	if (m_socket->Bad())
		return false;

	if (m_stopWorkers)
		return false;

	return true;
}

void IPC::ServerInstance::ReaderThread(ServerInstance* ptr) {
	while (!ptr->m_stopWorkers) {
		if (ptr->m_socket->ReadAvail() == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		if (ptr->m_socket->Bad())
			break;

		std::vector<char> buf(ptr->m_socket->ReadAvail());
		size_t bytes = ptr->m_socket->Read(buf);
		if ((bytes != 0) && (bytes != std::numeric_limits<size_t>::max())) {
			{
				std::unique_lock<std::mutex> ulock(ptr->m_executeWorker.lock);
				ptr->m_executeWorker.queue.push(buf);
			}
			ptr->m_executeWorker.cv.notify_all();
		}
	}
}

void IPC::ServerInstance::ExecuteThread(ServerInstance* ptr) {
	std::vector<char> messageBuffer(4096);
	IPC::Value rval;
	std::string errMsg = "";

	std::unique_lock<std::mutex> ulock(ptr->m_executeWorker.lock);
	while (!ptr->m_stopWorkers) {
		ptr->m_executeWorker.cv.wait(ulock, [ptr]() {
			return (ptr->m_stopWorkers || (ptr->m_executeWorker.queue.size() > 0));
		});
		if (ptr->m_stopWorkers)
			continue;

		std::vector<char>& msg = ptr->m_executeWorker.queue.front();
		if (!ptr->m_isAuthenticated) {
			Authenticate msgAuthenticate;
			bool suc = msgAuthenticate.ParsePartialFromArray(msg.data(), msg.size());
			if (suc) {
				ptr->m_isAuthenticated = true;

				// Reply with classes?
			}
		} else {
			FunctionCall msgCall;
			if (msgCall.ParsePartialFromArray(msg.data(), msg.size())) {
				// Convert FCall into proper arguments
				std::vector<IPC::Value> args(msgCall.arguments_size());
				for (size_t n = 0; n < args.size(); n++) {
					IPC::Value val;
					auto& v = msgCall.arguments(n);
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
					args[n] = std::move(val);
				}

				FunctionResult res;
				res.set_timestamp(msgCall.timestamp());
				if (!ptr->m_parent->ClientCallFunction(ptr->m_clientId, msgCall.classname(), msgCall.functionname(), args, errMsg, rval)) {
					res.set_error(errMsg);
				}
				::Value* msgValue = res.mutable_value();
				switch (rval.type) {
					case Type::Float:
						msgValue->set_type(ValueType::Float);
						msgValue->set_val_float(rval.value.fp32);
						break;
					case Type::Double:
						msgValue->set_type(ValueType::Double);
						msgValue->set_val_double(rval.value.fp64);
						break;
					case Type::Int32:
						msgValue->set_type(ValueType::Int32);
						msgValue->set_val_int32(rval.value.i32);
						break;
					case Type::Int64:
						msgValue->set_type(ValueType::Int64);
						msgValue->set_val_int64(rval.value.i64);
						break;
					case Type::UInt32:
						msgValue->set_type(ValueType::UInt32);
						msgValue->set_val_uint32(rval.value.ui32);
						break;
					case Type::UInt64:
						msgValue->set_type(ValueType::UInt64);
						msgValue->set_val_uint64(rval.value.ui64);
						break;
					case Type::String:
						msgValue->set_type(ValueType::String);
						msgValue->set_val_string(rval.value_str);
						break;
					case Type::Binary:
					{
						msgValue->set_type(ValueType::Binary);
						msgValue->set_val_binary(rval.value_bin.data(), rval.value_bin.size());
						break;
					}
				}
				messageBuffer.resize(res.ByteSizeLong());
				if (res.SerializeToArray(messageBuffer.data(), messageBuffer.size() + 1)) {
					ptr->QueueMessage(messageBuffer);
				} else {
					// Error Signalling? This is technically a crash.
					abort();
				}
			}
		}
		ptr->m_executeWorker.queue.pop();
	}
}

void IPC::ServerInstance::WriterThread(ServerInstance* ptr) {
	std::unique_lock<std::mutex> ulock(ptr->m_writeWorker.lock);
	while (!ptr->m_stopWorkers && ptr->m_socket->Good()) {
		ptr->m_writeWorker.cv.wait(ulock, [ptr]() {
			return (ptr->m_stopWorkers || (ptr->m_writeWorker.queue.size() > 0));
		});
		
		if (ptr->m_socket->Bad() || ptr->m_stopWorkers)
			break;

		auto msg = std::move(ptr->m_writeWorker.queue.front());
		ptr->m_writeWorker.queue.pop();
		if (msg.size() > 0) {
			ulock.unlock();
			bool success = false;
			for (size_t attempt = 0; attempt < 5; attempt++) {
				size_t bytes = ptr->m_socket->Write(msg);
				if (bytes == msg.size()) {
					success = true;
					break;
				} else if (bytes == std::numeric_limits<size_t>::max()) {
					abort(); // Critical error, recovering requires reconnecting or restarting.
				}
			}
			ulock.lock();
			if (!success) {
				ptr->m_writeWorker.queue.push(std::move(msg));
			}
		}
	}
}

