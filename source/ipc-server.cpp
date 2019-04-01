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
#include "windows/semaphore.hpp"

using namespace std::placeholders;

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

						if (!m_rop || !m_rop->is_valid()) {
							m_watcher.buf.resize(sizeof(ipc_size_t));
							ec = socket->read(
							    m_watcher.buf.data(),
							    m_watcher.buf.size(),
							    m_rop,
							    std::bind(&server::read_callback_init, this, _1, _2));
							if (ec != os::error::Pending && ec != os::error::Success) {
								if (ec == os::error::Disconnected) {
									break;
								} else {
									throw std::exception("Unexpected error.");
								}
							}
						}
						ec = m_rop->wait(std::chrono::milliseconds(0));
						if (ec == os::error::Success) {
							continue;
						} else {
							ec = m_rop->wait(std::chrono::milliseconds(20));
							if (ec == os::error::TimedOut) {
								continue;
							} else if (ec == os::error::Disconnected) {
								break;
							} else if (ec == os::error::Error) {
								throw std::exception("Error");
							}
						}
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
	}
}

void ipc::server::read_callback_init(os::error ec, size_t size)
{
	os::error ec2 = os::error::Success;

	m_rop->invalidate();

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(m_watcher.buf);

		if (n_size != 0) {
			m_watcher.buf.resize(n_size);
			ec2 = m_sockets.front()->read(
			    m_watcher.buf.data(), m_watcher.buf.size(), m_rop, std::bind(&server::read_callback_msg, this, _1, _2));
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

void ipc::server::read_callback_msg(os::error ec, size_t size)
{
	std::pair<call_return_t, void*> cb;
	ipc::message::function_reply    fnc_reply_msg;

	m_rop->invalidate();

	try {
		fnc_reply_msg.deserialize(m_watcher.buf, 0);
	} catch (std::exception& e) {
		ipc::log("Deserialize failed with error %s.", e.what());
		throw e;
	}

	// Find the callback function.
	std::unique_lock<std::mutex> ulock(m_lock);
	auto                         cb2 = m_cb.find(fnc_reply_msg.uid.value_union.ui64);
	if (cb2 == m_cb.end()) {
		return;
	}
	cb = cb2->second;

	// Decode return values or errors.
	if (fnc_reply_msg.error.value_str.size() > 0) {
		fnc_reply_msg.values.resize(1);
		fnc_reply_msg.values.at(0).type      = ipc::type::Null;
		fnc_reply_msg.values.at(0).value_str = fnc_reply_msg.error.value_str;
	}

	// Call Callback
	cb.first(cb.second, fnc_reply_msg.values);

	// Remove cb entry
	/// ToDo: Figure out better way of registering functions, perhaps even a way to have "events" across a IPC connection.
	m_cb.erase(fnc_reply_msg.uid.value_union.ui64);
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
	} catch (std::exception & e) {
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

bool ipc::server::register_collection(std::shared_ptr<ipc::collection> cls) {
	if (m_classes.count(cls->get_name()) > 0)
		return false;

	m_classes.insert(std::make_pair(cls->get_name(), cls));
	return true;
}

bool ipc::server::client_call_function(int64_t cid, const std::string& cname, const std::string& fname, std::vector<ipc::value>& args, std::vector<ipc::value>& rval, std::string& errormsg) {
	if (m_classes.count(cname) == 0)
	{
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

bool ipc::server::cancel(int64_t const& id)
{
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

bool ipc::server::call(
    const std::string&      cname,
    const std::string&      fname,
    std::vector<ipc::value> args,
    call_return_t           fn,
    void*                   data,
    int64_t&                cbid)
{
	static std::mutex             mtx;
	static uint64_t               timestamp = 0;
	os::error                     ec;
	std::shared_ptr<os::async_op> write_op;
	ipc::message::function_call   fnc_call_msg;
	std::vector<char>             outbuf;

	if (!m_clients.begin()->first)
		return false;

	{
		std::unique_lock<std::mutex> ulock(mtx);
		timestamp++;
		fnc_call_msg.uid = ipc::value(timestamp);
	}

	// Set
	fnc_call_msg.class_name    = ipc::value(cname);
	fnc_call_msg.function_name = ipc::value(fname);
	fnc_call_msg.arguments     = std::move(args);

	// Serialize
	std::vector<char> buf(fnc_call_msg.size());
	try {
		fnc_call_msg.serialize(buf, 0);
	} catch (std::exception& e) {
		ipc::log("(write) %8llu: Failed to serialize, error %s.", fnc_call_msg.uid.value_union.ui64, e.what());
		throw e;
	}

	if (fn != nullptr) {
		std::unique_lock<std::mutex> ulock(m_lock);
		m_cb.insert(std::make_pair(fnc_call_msg.uid.value_union.ui64, std::make_pair(fn, data)));
		cbid = fnc_call_msg.uid.value_union.ui64;
	}

	ipc::make_sendable(outbuf, buf);
	auto cl = ++m_clients.begin();
	ec = (cl->first->write(outbuf.data(), outbuf.size(), write_op, nullptr));
	if (ec != os::error::Success && ec != os::error::Pending) {
		cancel(cbid);
		//write_op->cancel();
		return false;
	}

	ec = write_op->wait();
	if (ec != os::error::Success) {
		cancel(cbid);
		write_op->cancel();
		return false;
	}

	return true;
}

std::vector<ipc::value> ipc::server::call_synchronous_helper(
    const std::string&             cname,
    const std::string&             fname,
    const std::vector<ipc::value>& args)
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
	bool    success = call(cname, fname, std::move(args), cb, &cd, cbid);
	if (!success) {
		return {};
	}

	cd.sgn->wait();
	if (!cd.called) {
		cancel(cbid);
		return {};
	}
	return std::move(cd.values);
}