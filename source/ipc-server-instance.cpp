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

call_return_t g_fn   = NULL;
void*         g_data = NULL;
int64_t       g_cbid = NULL;

ipc::server_instance::server_instance() {}

ipc::server_instance::server_instance(ipc::server* owner, std::shared_ptr<os::windows::named_pipe> conn): 
	m_stopWorkers(0),
	m_socket(conn),
	m_parent(owner),
	m_clientId(0) {
	m_worker = std::thread(std::bind(&server_instance::worker, this));
}

ipc::server_instance::~server_instance() {
	// Threading
	m_stopWorkers = true;
	if (m_worker.joinable())
		m_worker.join();
}

bool ipc::server_instance::is_alive() {
	if (!m_socket->is_connected())
		return false;

	if (m_stopWorkers)
		return false;

	return true;
}

void ipc::server_instance::worker() {
	os::error ec = os::error::Success;

	// Loop
	while ((!m_stopWorkers) && m_socket->is_connected()) {
		if (!m_rop || !m_rop->is_valid()) {
			size_t testSize = sizeof(ipc_size_t);
			m_rbuf.resize(sizeof(ipc_size_t));
			ec = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&server_instance::read_callback_init, this, _1, _2));
			if (ec != os::error::Pending && ec != os::error::Success) {
				if (ec == os::error::Disconnected) {
					break;
				} else {
					throw std::exception("Unexpected error.");
				}
			}
		}
		if (!m_wop || !m_wop->is_valid()) {
			if (m_write_queue.size() > 0) {
				std::vector<char>& fbuf = m_write_queue.front();
				ipc::make_sendable(m_wbuf, fbuf);
				ec = m_socket->write(m_wbuf.data(), m_wbuf.size(), m_wop, std::bind(&server_instance::write_callback, this, _1, _2));
				if (ec != os::error::Pending && ec != os::error::Success) {
					if (ec == os::error::Disconnected) {
						break;
					} else {
						throw std::exception("Unexpected error.");
					}
				}
				m_write_queue.pop();
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
		ipc_size_t n_size = read_size(m_rbuf);
		if (n_size != 0) {
			m_rbuf.resize(n_size);
			ec2 = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&server_instance::read_callback_msg, this, _1, _2));
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
		fnc_call_msg.deserialize(m_rbuf, 0);
	} catch (std::exception & e) {
		ipc::log("????????: Deserialization of Function Call message failed with error %s.", e.what());
		return;
	}

	// Execute
	proc_rval.resize(0);
	success = m_parent->client_call_function(m_clientId,
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
		if ((!m_wop || !m_wop->is_valid()) && (m_write_queue.size() == 0)) {
			ipc::make_sendable(m_wbuf, write_buffer);
			os::error ec2 = m_socket->write(m_wbuf.data(), m_wbuf.size(), m_wop, std::bind(&server_instance::write_callback, this, _1, _2));
			if (ec2 != os::error::Success && ec2 != os::error::Pending) {
				if (ec2 == os::error::Disconnected) {
					return;
				} else {
					throw std::exception("Unexpected Error");
				}
			}
		} else {
			m_write_queue.push(std::move(write_buffer));
		}
	} else {
		m_rop->invalidate();
	}
}

void ipc::server_instance::write_callback(os::error ec, size_t size) {
	m_wop->invalidate();
	m_rop->invalidate();
}

bool ipc::server_instance::cancel(int64_t const& id)
{
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

bool ipc::server_instance::call(
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

	if (!m_socket)
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
	ec = m_socket->write(outbuf.data(), outbuf.size(), write_op, nullptr);
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

std::vector<ipc::value> ipc::server_instance::call_synchronous_helper(
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