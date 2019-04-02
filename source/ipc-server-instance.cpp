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

#include "ipc-server-instance.hpp"
#include <sstream>
#include <functional>

#ifdef _WIN32
#include <windows.h>
using namespace std::placeholders;
#endif

ipc::server_instance::server_instance() {}

ipc::server_instance::server_instance(ipc::server* owner, std::shared_ptr<os::windows::named_pipe> conn): 
	m_parent(owner), m_clientId(0)
{
	m_watcher.stop = false;
	m_socket = std::make_unique<os::windows::named_pipe>(*conn);
	m_watcher.worker = std::thread(std::bind(&server_instance::worker, this));
}

ipc::server_instance::~server_instance() {
	// Threading
	m_watcher.stop = true;
	if (m_watcher.worker.joinable())
		m_watcher.worker.join();
}

bool ipc::server_instance::is_alive() {
	if (!m_socket->is_connected())
		return false;

	if (m_watcher.stop)
		return false;

	return true;
}

void ipc::server_instance::worker() {
	os::error ec = os::error::Success;

	// Loop
	while ((!m_watcher.stop) && m_socket->is_connected()) {
		if (!m_rop || !m_rop->is_valid()) {
			size_t testSize = sizeof(ipc_size_t);
			m_watcher.rbuf.resize(sizeof(ipc_size_t));
			ec = m_socket->read(
			    m_watcher.rbuf.data(),
			    m_watcher.rbuf.size(),
			    m_rop,
			    std::bind(&server_instance::read_callback_init, this, _1, _2));
			if (ec != os::error::Pending && ec != os::error::Success) {
				if (ec == os::error::Disconnected) {
					break;
				} else {
					throw std::exception("Unexpected error.");
				}
			}
		}
		if (!m_wop || !m_wop->is_valid()) {
			if (m_watcher.write_queue.size() > 0) {
				std::vector<char>& fbuf = m_watcher.write_queue.front();
				ipc::make_sendable(m_watcher.wbuf, fbuf);
				ec = m_socket->write(
				    m_watcher.wbuf.data(),
				    m_watcher.wbuf.size(),
				    m_wop,
				    std::bind(&server_instance::write_callback, this, _1, _2));
				if (ec != os::error::Pending && ec != os::error::Success) {
					if (ec == os::error::Disconnected) {
						break;
					} else {
						throw std::exception("Unexpected error.");
					}
				}
				m_watcher.write_queue.pop();
			}
		}

		os::waitable * waits[] = { m_rop.get(), m_wop.get() };
		size_t                      wait_index = -1;
		for (size_t idx = 0; idx < 2; idx++) {
			if (waits[idx] != nullptr) {
				if (waits[idx]->wait(std::chrono::milliseconds(0)) == os::error::Success) {
					wait_index = idx;
					break;
				}
			}
		}
		if (wait_index == -1) {
			os::error code = os::waitable::wait_any(waits, 2, wait_index, std::chrono::milliseconds(20));
			if (code == os::error::TimedOut) {
				continue;
			} else if (code == os::error::Disconnected) {
				break;
			} else if (code == os::error::Error) {
				throw std::exception("Error");
			}
		}
	}
}

void ipc::server_instance::read_callback_init(os::error ec, size_t size) {
	os::error ec2 = os::error::Success;

	m_rop->invalidate();

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(m_watcher.rbuf);
		if (n_size != 0) {
			m_watcher.rbuf.resize(n_size);
			ec2 = m_socket->read(
			    m_watcher.rbuf.data(),
			    m_watcher.rbuf.size(),
			    m_rop,
			    std::bind(&server_instance::read_callback_msg, this, _1, _2));
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

void ipc::server_instance::read_callback_msg(os::error ec, size_t size) {
	/// Processing
	std::vector<ipc::value> proc_rval;
	ipc::value proc_tempval;
	std::string proc_error;
	std::vector<char> write_buffer;

	ipc::message::function_call fnc_call_msg;
	ipc::message::function_reply fnc_reply_msg;

	if (ec != os::error::Success) {
		return;
	}

	bool success = false;

	try {
		fnc_call_msg.deserialize(m_watcher.rbuf, 0);
	} catch (std::exception & e) {
		ipc::log("????????: Deserialization of Function Call message failed with error %s.", e.what());
		return;
	}

	// Execute
	proc_rval.resize(0);
	success = call_function(m_clientId,
		fnc_call_msg.class_name.value_str, fnc_call_msg.function_name.value_str,
		fnc_call_msg.arguments, proc_rval, proc_error);

	// Set
	fnc_reply_msg.uid = fnc_call_msg.uid;
	std::swap(proc_rval, fnc_reply_msg.values); // Fast "copy" of parameters.
	if (!success) {
		fnc_reply_msg.error = ipc::value(proc_error);
	}

	// Serialize
	write_buffer.resize(fnc_reply_msg.size());
	try {
		fnc_reply_msg.serialize(write_buffer, 0);
	} catch (std::exception & e) {
		ipc::log("%8llu: Serialization of Function Reply message failed with error %s.",
			fnc_call_msg.uid.value_union.ui64, e.what());
		return;
	}

	if (write_buffer.size() != 0) {
		if ((!m_wop || !m_wop->is_valid()) && (m_watcher.write_queue.size() == 0)) {
			ipc::make_sendable(m_watcher.wbuf, write_buffer);
			os::error ec2 = m_socket->write(
			    m_watcher.wbuf.data(),
			    m_watcher.wbuf.size(),
			    m_wop,
			    std::bind(&server_instance::write_callback, this, _1, _2));
			if (ec2 != os::error::Success && ec2 != os::error::Pending) {
				if (ec2 == os::error::Disconnected) {
					return;
				} else {
					throw std::exception("Unexpected Error");
				}
			}
		} else {
			m_watcher.write_queue.push(std::move(write_buffer));
		}
	} else {
		m_rop->invalidate();
	}
}

bool ipc::server_instance::call_function(
	int64_t                  cid,
	const std::string&       cname,
	const std::string&       fname,
	std::vector<ipc::value>& args,
	std::vector<ipc::value>& rval,
	std::string&             errormsg)
{
	return m_parent->client_call_function(
	    m_clientId, cname, fname, args, rval, errormsg);
}