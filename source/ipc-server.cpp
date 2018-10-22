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
#include <chrono>
#include "source/os/error.hpp"
#include "source/os/tags.hpp"
#include "ipc-server-instance.hpp"
#include "source/os/windows/semaphore.hpp"

static const size_t buffer_size = 128 * 1024 * 1024;

void ipc::server::watcher() {
	os::error ec;

	struct pending_accept {
		server* parent;
		std::shared_ptr<os::async_op> op;
		std::shared_ptr<os::windows::named_pipe> socket;
		std::chrono::high_resolution_clock::time_point start;

		void accept_client_cb(os::error ec, size_t length) {
			if (ec == os::error::Connected) {
				// A client has connected, so spawn a new client.
				parent->spawn_client(socket);
			}
			op->invalidate();
		}
	};

	std::map<std::shared_ptr<os::windows::named_pipe>, pending_accept> pa_map;

	while (!m_watcher.stop) {
		// Verify the state of sockets.
		{
			std::unique_lock<std::mutex> ul(m_sockets_mtx);
			for (auto socket : m_sockets) {
				auto pending = pa_map.find(socket);
				auto client = m_clients.find(socket);

				if (client != m_clients.end()) {
					if (!socket->is_connected()) {
						// Client died.
						client = m_clients.end();
						kill_client(socket);
					}
				} else if (pending == pa_map.end()) {
					pending_accept pa;
					pa.parent = this;
					pa.start = std::chrono::high_resolution_clock::now();
					pa.socket = socket;
					ec = socket->accept(pa.op, std::bind(&pending_accept::accept_client_cb, &pa, std::placeholders::_1, std::placeholders::_2));
					if (ec == os::error::Success) {
						// There was no client waiting to connect, but there might be one in the future.
						pa_map.insert_or_assign(socket, pa);
					}
				}
			}
		}

		// Wait for sockets to connect.
		std::vector<os::waitable*> waits;
		std::vector<std::shared_ptr<os::windows::named_pipe>> idx_to_socket;
		for (auto kv : pa_map) {
			waits.push_back(kv.second.op.get());
			idx_to_socket.push_back(kv.first);
		}

		if (waits.size() == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			continue;
		}

		size_t idx = -1;
		ec = os::waitable::wait_any(waits, idx, std::chrono::milliseconds(20));
		if (ec == os::error::TimedOut) {
			continue;
		} else if (ec == os::error::Connected) {
			pending_accept pa;
			auto kv = pa_map.find(idx_to_socket[idx]);
			if (kv != pa_map.end()) {
				pa_map.erase(idx_to_socket[idx]);
			}
		} else {
			// Unknown error.
		}
	}
}

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
		std::unique_lock<std::mutex> ul(m_sockets_mtx);
		m_sockets.insert(m_sockets.end(),
			std::make_shared<os::windows::named_pipe>(os::create_only, socketPath, 255,
				os::windows::pipe_type::Byte, os::windows::pipe_read_mode::Byte, true));
		for (size_t idx = 1; idx < backlog; idx++) {
			m_sockets.insert(m_sockets.end(),
				std::make_shared<os::windows::named_pipe>(os::create_only, socketPath, 255,
					os::windows::pipe_type::Byte, os::windows::pipe_read_mode::Byte, false));
		}
	} catch (std::exception e) {
		throw e;
	}

	m_isInitialized = true;
	m_socketPath = socketPath;
}

void ipc::server::finalize() {
	if (!m_isInitialized) {
		return;
	}

	// Lock sockets mutex so that watcher pauses.
	std::unique_lock<std::mutex> ul(m_sockets_mtx);

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

bool ipc::server::register_collection(ipc::collection cls) {
	return register_collection(std::make_shared<ipc::collection>(cls));
}

bool ipc::server::register_collection(std::shared_ptr<ipc::collection> cls) {
	if (m_classes.count(cls->get_name()) > 0)
		return false;

	m_classes.insert(std::make_pair(cls->get_name(), cls));
	return true;
}

std::vector<ipc::value> ipc::server::call_synchronous_helper(
    std::shared_ptr<os::windows::named_pipe>	client,
    std::string									cname,
    std::string									fname,
    std::vector<ipc::value>						args,
    std::chrono::nanoseconds					timeout)
{
	auto& clientInfo = m_clients.find(client);
	if (clientInfo == m_clients.end()) {
		return {};
	}

	// Set up call reference data.
	struct CallData
	{
		std::shared_ptr<os::windows::semaphore>        sgn    = std::make_shared<os::windows::semaphore>();
		bool                                           called = false;
		std::chrono::high_resolution_clock::time_point start  = std::chrono::high_resolution_clock::now();

		std::vector<ipc::value> values;
	} cd;

	auto cb = [](const void* data, const std::vector<ipc::value>& rval) {
		CallData& cd = const_cast<CallData&>(*static_cast<const CallData*>(data));

		// This copies the data off of the reply thread to the main thread.
		cd.values.reserve(rval.size());
		std::copy(rval.begin(), rval.end(), std::back_inserter(cd.values));

		cd.called = true;
		cd.sgn->signal();
	};

	int64_t cbid    = 0;
	bool    success = clientInfo->second->call(cname, fname, std::move(args), cb, &cd, cbid);
	if (!success) {
		return {};
	}

	cd.sgn->wait(timeout);
	if (!cd.called) {
		clientInfo->second->cancel(cbid);
		return {};
	}

	return std::move(cd.values);
}

void ipc::server::call_synchronous_broadcast_helper(
    std::string                       cname,
    std::string                       fname,
    std::vector<ipc::value>           args,
    server_client_broadcast_handler_t clientCallback,
    std::chrono::nanoseconds          timeout)
{
	// For each client
	for (auto& client : m_clients)
	{
		// Set up call reference data.
		struct CallData
		{
			std::shared_ptr<os::windows::semaphore>        sgn    = std::make_shared<os::windows::semaphore>();
			bool                                           called = false;
			std::chrono::high_resolution_clock::time_point start  = std::chrono::high_resolution_clock::now();

			std::vector<ipc::value> values;
		} cd;

		auto cb = [](const void* data, const std::vector<ipc::value>& rval) {
			CallData& cd = const_cast<CallData&>(*static_cast<const CallData*>(data));

			// This copies the data off of the reply thread to the main thread.
			cd.values.reserve(rval.size());
			std::copy(rval.begin(), rval.end(), std::back_inserter(cd.values));

			cd.called = true;
			cd.sgn->signal();
		};
		
		int64_t cbid    = 0;
		bool    success = client.second->call(cname, fname, std::move(args), cb, &cd, cbid);
		if (!success) {
			if (clientCallback) {
				clientCallback(client.first, {});
			}
			continue;
		}

		cd.sgn->wait(timeout);
		if (!cd.called) {
			client.second->cancel(cbid);
			if (clientCallback) {
				clientCallback(client.first, {});
			}
			continue;
		}

		if (clientCallback) {
			clientCallback(client.first, cd.values);
		}
	}
}

bool ipc::server::client_call_function(int64_t cid, std::string cname, std::string fname, std::vector<ipc::value>& args, std::vector<ipc::value>& rval, std::string& errormsg) {
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
