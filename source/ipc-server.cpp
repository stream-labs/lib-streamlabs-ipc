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
	return RegisterClass(std::make_shared<IPC::Class>(cls));
}

bool IPC::Server::RegisterClass(std::shared_ptr<IPC::Class> cls) {
	if (m_classes.count(cls->GetName()) > 0)
		return false;

	m_classes.insert(std::make_pair(cls->GetName(), cls));
	return true;
}

bool IPC::Server::ClientCallFunction(OS::ClientId_t cid, std::string cname, std::string fname, std::vector<IPC::Value>& args, std::vector<IPC::Value>& rval, std::string& errormsg) {
	if (m_classes.count(cname) == 0) {
		errormsg = "Class '" + cname + "' is not registered.";
		return false;
	}
	auto cls = m_classes.at(cname);
	
	auto fnc = cls->GetFunction(fname, args);
	if (!fnc) {
		errormsg = "Function '" + fname + "' not found in class '" + cname + "'.";
		return false;
	}

	fnc->Call(cid, args, rval);

	return true;
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

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
