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
#include <chrono>

static const size_t buffer_size = 128 * 1024 * 1024;

ipc::server::server() {
	m_socket = os::named_socket::create();
	m_socket->set_send_timeout(std::chrono::nanoseconds(1000000ull));
	m_socket->set_receive_timeout(std::chrono::nanoseconds(1000000ull));
	m_socket->set_receive_buffer_size(buffer_size);
	m_socket->set_send_buffer_size(buffer_size);
}

ipc::server::~server() {
	finalize();
}

void ipc::server::initialize(std::string socketPath) {
	if (!m_socket->listen(socketPath, 4))
		throw std::exception("Failed to initialize socket.");
	m_worker = std::thread(worker_main, this);
	m_isInitialized = true;
	m_socketPath = socketPath;
}

void ipc::server::finalize() {
	if (m_isInitialized) {
		m_stopWorker = true;
		if (m_worker.joinable())
			m_worker.join();
		m_clients.clear();
		m_socket->close();
	}
}

void ipc::server::set_connect_handler(server_connect_handler_t handler, void* data) {
	m_handlerConnect = std::make_pair(handler, data);
}

void ipc::server::set_disconnect_handler(server_disconnect_handler_t handler, void* data) {
	m_handlerDisconnect = std::make_pair(handler, data);
}

void ipc::server::set_message_handler(server_message_handler_t handler, void* data) {
	m_handlerMessage = std::make_pair(handler, data);
}

bool ipc::server::register_collection(ipc::collection cls) {
	return register_collection(std::make_shared<ipc::collection>(cls));
}

bool ipc::server::register_collection(std::shared_ptr<ipc::collection> cls) {
	if (m_classes.count(cls->get_name()) > 0)
		return false;

	m_classes.insert(std::make_pair(cls->get_name(), cls));
	return true;
}

bool ipc::server::client_call_function(os::ClientId_t cid, std::string cname, std::string fname, std::vector<ipc::value>& args, std::vector<ipc::value>& rval, std::string& errormsg) {
	if (m_classes.count(cname) == 0) {
		errormsg = "Class '" + cname + "' is not registered.";
		return false;
	}
	auto cls = m_classes.at(cname);

	auto fnc = cls->get_function(fname, args);
	if (!fnc) {
		errormsg = "Function '" + fname + "' not found in class '" + cname + "'.";
		return false;
	}

	fnc->call(cid, args, rval);

	return true;
}

void ipc::server::worker_main(server* ptr) {
	ptr->worker_local();
}

void ipc::server::worker_local() {
	std::queue<os::ClientId_t> dcQueue;
	while (m_stopWorker == false) {
		std::shared_ptr<os::named_socket_connection> conn = m_socket->accept().lock();
		if (conn) {
			bool allow = true;
			if (m_handlerConnect.first != nullptr)
				allow = m_handlerConnect.first(m_handlerConnect.second, conn->get_client_id());

			if (allow && conn->connect()) {
				std::unique_lock<std::mutex> ulock(m_clientLock);
				std::shared_ptr<server_instance> instance = std::make_shared<server_instance>(this, conn);
				m_clients.insert(std::make_pair(conn->get_client_id(), instance));
			}
		}

		for (auto kv = m_clients.begin(); kv != m_clients.end(); kv++) {
			if (!kv->second->m_socket->is_connected())
				dcQueue.push(kv->first);
		}
		while (dcQueue.size() > 0) {
			os::ClientId_t id = dcQueue.front();
			if (m_handlerDisconnect.first != nullptr)
				m_handlerDisconnect.first(m_handlerDisconnect.second, id);
			m_clients.erase(id);
			dcQueue.pop();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
