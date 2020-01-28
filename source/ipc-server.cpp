/******************************************************************************
    Copyright (C) 2016-2019 by Streamlabs (General Workings Inc)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#include "ipc-server.hpp"
#include <chrono>
#include "../include/error.hpp"
#include "../include/tags.hpp"

void ipc::server::watcher() {
	os::error ec;
    std::cout << "server - start watcher" << std::endl;
	struct pending_accept {
		server* parent;
		std::shared_ptr<os::async_op> op;
#ifdef WIN32
		std::shared_ptr<os::windows::named_pipe> socket;
#elif __APPLE__
		std::shared_ptr<os::apple::named_pipe> socket;
#endif
		std::chrono::high_resolution_clock::time_point start;

		void accept_client_cb(os::error ec, size_t length) {
			if (ec == os::error::Connected) {
                std::cout << "Spawning a new client" << std::endl;
				// A client has connected, so spawn a new client.
				parent->spawn_client(socket);
			}
#ifdef WIN32
			op->invalidate();
#endif
		}
	};

#ifdef WIN32
	std::map<std::shared_ptr<os::windows::named_pipe>, pending_accept> pa_map;
#elif __APPLE__
	std::map<std::shared_ptr<os::apple::named_pipe>, pending_accept> pa_map;
#endif

	while (!m_watcher.stop) {
		// Verify the state of sockets.
		{
			// std::cout << "Checking sockets" << std::endl;
			std::unique_lock<std::mutex> ul(m_sockets_mtx);
			for (auto socket : m_sockets) {
				auto pending = pa_map.find(socket);
				auto client = m_clients.find(socket);

				if (client != m_clients.end()) {
					if (!socket->is_connected()) {
						// Client died.
						client = m_clients.end();
						kill_client(socket);
					} else {
						auto delta = std::chrono::high_resolution_clock::now() - socket->last_process;
						if (!socket->server_execute_state &&
							std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() > 100) {
							exit(1);
						}
					}
				} else if (pending == pa_map.end()) {
					pending_accept pa;
					pa.parent = this;
					pa.start = std::chrono::high_resolution_clock::now();
					pa.socket = socket;
#ifdef WIN32
					ec = socket->accept(pa.op, std::bind(&pending_accept::accept_client_cb, &pa, std::placeholders::_1, std::placeholders::_2));
					if (ec == os::error::Success) {
						// There was no client waiting to connect, but there might be one in the future.
						pa_map.insert_or_assign(socket, pa);
					}
#elif __APPLE__
					ec = socket->accept(pa.op, std::bind(&pending_accept::accept_client_cb, &pa, std::placeholders::_1, std::placeholders::_2));
					if (ec == os::error::Success) {
						// There was no client waiting to connect, but there might be one in the future.
						pa_map.insert_or_assign(socket, pa);
					}
#endif
				}
			}
		}
		// Wait for sockets to connect.
		std::vector<os::waitable*> waits;
#ifdef WIN32
		std::vector<std::shared_ptr<os::windows::named_pipe>> idx_to_socket;
#elif __APPLE__
		std::vector<std::shared_ptr<os::apple::named_pipe>> idx_to_socket;
#endif
		for (auto kv : pa_map) {
			waits.push_back(kv.second.op.get());
			idx_to_socket.push_back(kv.first);
		}

		if (waits.size() == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}
	}
}

#ifdef WIN32
void ipc::server::spawn_client(std::shared_ptr<os::windows::named_pipe> socket) {
	std::unique_lock<std::mutex> ul(m_clients_mtx);
	std::shared_ptr<ipc::server_instance> client = std::make_shared<ipc::server_instance>(this, socket);
	if (m_handlerConnect.first) {
		m_handlerConnect.first(m_handlerConnect.second, 0);
	}
	m_clients.insert_or_assign(socket, client);
}

void ipc::server::kill_client(std::shared_ptr<os::windows::named_pipe> socket) {
	if (m_handlerDisconnect.first) {
		m_handlerDisconnect.first(m_handlerDisconnect.second, 0);
	}
	m_clients.erase(socket);
}
#endif

#ifdef __APPLE__
void ipc::server::spawn_client(std::shared_ptr<os::apple::named_pipe> socket) {
    std::cout << "Server - spawn_client" << std::endl;
	std::unique_lock<std::mutex> ul(m_clients_mtx);

	std::shared_ptr<ipc::server_instance> client = std::make_shared<ipc::server_instance>(this, socket);
	if (m_handlerConnect.first) {
		m_handlerConnect.first(m_handlerConnect.second, 0);
	}
	m_clients.insert_or_assign(socket, client);
}

void ipc::server::kill_client(std::shared_ptr<os::apple::named_pipe> socket) {
	if (m_handlerDisconnect.first) {
		m_handlerDisconnect.first(m_handlerDisconnect.second, 0);
	}
	m_clients.erase(socket);
}
#endif

ipc::server::server() {
	// Start Watcher
	m_watcher.stop = false;
	m_watcher.worker = std::thread(std::bind(&ipc::server::watcher, this));
}

ipc::server::~server() {
	finalize();

	m_watcher.stop = true;
	if (m_watcher.worker.joinable()) {
		m_watcher.worker.join();
	}
}

void ipc::server::initialize(std::string socketPath) {
	// Start a few sockets.

	try {
#ifdef WIN32
		std::unique_lock<std::mutex> ul(m_sockets_mtx);
		m_sockets.insert(m_sockets.end(),
			std::make_shared<os::windows::named_pipe>(os::create_only, socketPath, 255,
				os::windows::pipe_type::Byte, os::windows::pipe_read_mode::Byte, true));
		for (size_t idx = 1; idx < backlog; idx++) {
			m_sockets.insert(m_sockets.end(),
				std::make_shared<os::windows::named_pipe>(os::create_only, socketPath, 255,
					os::windows::pipe_type::Byte, os::windows::pipe_read_mode::Byte, false));
		}
#elif __APPLE__
		std::unique_lock<std::mutex> ul(m_sockets_mtx);
        std::cout << "Server create socket" << std::endl;
		m_sockets.insert(m_sockets.end(),
			std::make_shared<os::apple::named_pipe>(os::create_only, socketPath));
#endif
	} catch (std::exception e) {
		throw e;
	}

	m_isInitialized = true;
	m_socketPath = std::move(socketPath);
}

void ipc::server::finalize() {
	if (!m_isInitialized) {
		return;
	}

	// Lock sockets mutex so that watcher pauses.
	std::unique_lock<std::mutex> ul(m_sockets_mtx);
// 
	{ // Kill/Disconnect any clients
		std::unique_lock<std::mutex> ul(m_clients_mtx);
		while (m_clients.size() > 0) {
			kill_client(m_clients.begin()->first);
		}
	}

	// Kill any remaining sockets
	m_sockets.clear();
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

void ipc::server::set_pre_callback(server_pre_callback_t handler, void* data) {
	m_preCallback = std::make_pair(handler, data);
}

void ipc::server::set_post_callback(server_post_callback_t handler, void* data) {
	m_postCallback = std::make_pair(handler, data);
}

bool ipc::server::register_collection(std::shared_ptr<ipc::collection> cls) {
	if (m_classes.count(cls->get_name()) > 0)
		return false;

	m_classes.insert(std::make_pair(cls->get_name(), cls));
	return true;
}

bool ipc::server::client_call_function(int64_t cid, const std::string & cname, const std::string &fname, std::vector<ipc::value>& args, std::vector<ipc::value>& rval, std::string& errormsg) {
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

	if (m_preCallback.first) {
		m_preCallback.first(cname, fname, args, m_preCallback.second);
	}

	fnc->call(cid, args, rval);

	if (m_postCallback.first) {
		m_postCallback.first(cname, fname, rval, m_postCallback.second);
	}

	return true;
}
