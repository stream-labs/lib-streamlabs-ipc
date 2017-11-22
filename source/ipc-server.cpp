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

#include "ipc-server.hpp"
#include "ipc.pb.h"

IPC::Server::Server() {
	m_socket = OS::NamedSocket::Create();
}

IPC::Server::~Server() {
	Finalize();
}

void IPC::Server::Initialize(std::string socketPath) {
	if (!m_socket->Listen(socketPath, 4))
		throw std::exception("Failed to initialize socket.");
	m_worker = std::thread(WorkerMain, this);
	m_isInitialized = true;
}

void IPC::Server::Finalize() {
	if (m_isInitialized) {
		m_stopWorker = true;
		if (m_worker.joinable())
			m_worker.join();
		m_clients.clear();
		m_socket->Close();
	}
}

void IPC::Server::SetConnectHandler(ServerConnectHandler_t handler, void* data) {
	m_handlerConnect = std::make_pair(handler, data);
}

void IPC::Server::SetDisconnectHandler(ServerDisconnectHandler_t handler, void* data) {
	m_handlerDisconnect = std::make_pair(handler, data);
}

void IPC::Server::SetMessageHandler(ServerMessageHandler_t handler, void* data) {
	m_handlerMessage = std::make_pair(handler, data);
}

bool IPC::Server::RegisterClass(IPC::Class cls) {
	if (m_classes.count(cls.GetName()) > 0)
		return false;

	m_classes.insert(std::make_pair(cls.GetName(), std::make_shared<IPC::Class>(cls)));
	return true;
}

std::vector<char> IPC::Server::HandleMessage(OS::ClientId_t clientId, std::vector<char> message) {
	if (m_handlerMessage.first)
		m_handlerMessage.first(m_handlerMessage.second, clientId, message);

	return std::vector<char>();

	//FunctionCall fcall;
	//if (fcall.ParseFromArray(message.data(), message.size())) {
	//	int64_t ts = fcall.timestamp();
	//	std::string className = fcall.classname();
	//	std::string functionName = fcall.functionname();
	//	if (m_classes.count(className) == 0)
	//		return;
	//	auto& icls = m_classes.at(className);
	//	std::vector<IPC::Type> params;
	//	//for (size_t n = 0; n < fcall.arguments_size(); n++) {
	//	//	IPC::Type tp = Type::Binary;
	//	//	auto& vl = fcall.arguments(n);
	//	//	switch (fcall.arguments(n)) {
	//	//		case ValueType::Null:
	//	//		case ValueType::Float:
	//	//	}
	//	//		params.push_back(tp);
	//	//}
	//}
}

void IPC::Server::WorkerMain(Server* ptr) {
	ptr->WorkerLocal();
}

void IPC::Server::WorkerLocal() {
	std::queue<OS::ClientId_t> dcQueue;
	while (m_stopWorker == false) {
		std::shared_ptr<OS::NamedSocketConnection> conn = m_socket->Accept().lock();
		if (conn) {
			bool allow = true;
			if (m_handlerConnect.first != nullptr)
				allow = m_handlerConnect.first(m_handlerConnect.second, conn->GetClientId());

			if (allow && conn->Connect()) {
				std::unique_lock<std::mutex> ulock(m_clientLock);
				std::shared_ptr<ServerInstance> instance = std::make_shared<ServerInstance>(this, conn);
				m_clients.insert(std::make_pair(conn->GetClientId(), instance));
			}
		}
		
		for (auto kv = m_clients.begin(); kv != m_clients.end(); kv++) {
			if (!kv->second->m_socket->IsConnected())
				dcQueue.push(kv->first);
		}
		while (dcQueue.size() > 0) {
			OS::ClientId_t id = dcQueue.front();
			if (m_handlerDisconnect.first != nullptr)
				m_handlerDisconnect.first(m_handlerDisconnect.second, id);
			m_clients.erase(id);
			dcQueue.pop();
		}
	}
}
