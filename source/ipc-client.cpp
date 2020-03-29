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

#include "ipc-client.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <iterator>
#include "../include/error.hpp"
#include "../include/tags.hpp"
#ifdef WIN32
#include "windows/semaphore.hpp"
#include <windows.h>
#include <Objbase.h>
#endif
#include <memory>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

using namespace std::placeholders;

call_return_t g_fn   = NULL;
void*         g_data = NULL;
int64_t       g_cbid = NULL;

// void ipc::client::worker() {
// 	while (m_socket->is_connected() && !m_watcher.stop) {
// 		if (!m_rop || !m_rop->is_valid()) {
// 			if (m_watcher.stop)
// 				break;
// 			m_watcher.buf.resize(65000);
// 			m_socket->read(m_watcher.buf.data(),
// 							m_watcher.buf.size(),
// 							m_rop, std::bind(&client::read_callback_msg,
// 											this,
// 											std::placeholders::_1,
// 											std::placeholders::_2), true, REPLY);
// 		}
// 	}
// }

void ipc::client::read_callback_init(os::error ec, size_t size) {
}

void ipc::client::read_callback_msg(os::error ec, size_t size) {
	// std::cout << "ipc::client::read - end" << std::endl;
	std::pair<call_return_t, void*> cb;
	ipc::message::function_reply fnc_reply_msg;

	// m_rop->invalidate();

	try {
		fnc_reply_msg.deserialize(m_watcher.buf, 0);
	} catch (std::exception& e) {
		ipc::log("Deserialize failed with error %s.", e.what());
		throw e;
	}

	// Find the callback function.
	std::unique_lock<std::mutex> ulock(m_lock);
	auto cb2 = m_cb.find(fnc_reply_msg.uid.value_union.ui64);
	if (cb2 == m_cb.end()) {
		return;
	}
	cb = cb2->second;
	// Decode return values or errors.
	if (fnc_reply_msg.error.value_str.size() > 0) {
		fnc_reply_msg.values.resize(1);
		fnc_reply_msg.values.at(0).type = ipc::type::Null;
		fnc_reply_msg.values.at(0).value_str = fnc_reply_msg.error.value_str;
	}

	// Call Callback
	cb.first(cb.second, fnc_reply_msg.values);

	// Remove cb entry
	m_cb.erase(fnc_reply_msg.uid.value_union.ui64);
}

ipc::client::client(std::string socketPath) {
#ifdef WIN32
	m_socket = std::make_unique<os::windows::named_pipe>(os::open_only, socketPath, os::windows::pipe_read_mode::Byte);
#elif __APPLE__
	m_socket = std::make_unique<os::apple::named_pipe>(os::open_only, socketPath);
#endif
	m_watcher.stop   = false;

	sem_unlink(writer_sem_name.c_str());
	remove(writer_sem_name.c_str());
	m_writer_sem = sem_open(writer_sem_name.c_str(), O_CREAT | O_EXCL, 0644, 1);

	m_watcher.buf.resize(65000);
}

ipc::client::~client() {
	m_watcher.stop = true;
	sem_post(m_writer_sem);
	sem_close(m_writer_sem);
	m_socket = nullptr;
}


bool ipc::client::cancel(int64_t const& id) {
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

bool ipc::client::call(const std::string& cname, const std::string& fname, std::vector<ipc::value> args, call_return_t fn, void* data, int64_t& cbid) {
	// std::cout << "ipc-client::call::" << cname.c_str() << "::" << fname.c_str() << std::endl;
	static std::mutex mtx;
	static uint64_t timestamp = 0;
	os::error ec;

	std::shared_ptr<os::async_op> write_op;
	ipc::message::function_call fnc_call_msg;
	std::vector<char> outbuf;

	if (!m_socket)
		return false;

	{
		std::unique_lock<std::mutex> ulock(mtx);
		timestamp++;
		fnc_call_msg.uid = ipc::value(timestamp);
	}

	// Set	
	fnc_call_msg.class_name = ipc::value(cname);
	fnc_call_msg.function_name = ipc::value(fname);
	fnc_call_msg.arguments = std::move(args);

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

#ifdef WIN32
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
#elif __APPLE__
	sem_wait(m_writer_sem);
	if (m_watcher.stop)
		return true;
    ec = (os::error) m_socket->write(buf.data(), buf.size(), REQUEST);
	m_socket->read(m_watcher.buf.data(),
				m_watcher.buf.size(), true, REPLY);
	// std::cout << "buffer size " << m_watcher.buf.size() << std::endl;
	read_callback_msg(ec, 65000);
	sem_post(m_writer_sem);
#endif
	return true;
}

int count = 0;

std::vector<ipc::value> ipc::client::call_synchronous_helper(const std::string & cname, const std::string & fname, const std::vector<ipc::value>& args) {
	// Set up call reference data.
	struct CallData {
#ifdef WIN32
		std::shared_ptr<os::windows::semaphore> sgn = std::make_shared<os::windows::semaphore>();
#elif __APPLE__
		sem_t *sem;
#endif
		bool called = false;
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		std::vector<ipc::value> values;
	} cd;

	auto cb = [](void* data, const std::vector<ipc::value>& rval) {
		CallData& cd = *static_cast<CallData*>(data);
		// This copies the data off of the reply thread to the main thread.
		cd.values.reserve(rval.size());
		std::copy(rval.begin(), rval.end(), std::back_inserter(cd.values));
		// std::cout << "Response from the server " << rval[1].value_str.c_str() << std::endl;
		// std::cout << "Count " << count++ << std::endl;
		cd.called = true;
#ifdef WIN32
		cd.sgn->signal();
#elif __APPLE__
		sem_post(cd.sem);
#endif  
	};

#ifdef __APPLE__
	int uniqueId = cname.size() + fname.size() + rand();
	std::string sem_name = "sem-cb" + std::to_string(uniqueId);
	std::string path = "/tmp/" + sem_name;
	sem_unlink(path.c_str());
	remove(path.c_str());
	cd.sem = sem_open(path.c_str(), O_CREAT | O_EXCL, 0644, 0);
	if (cd.sem == SEM_FAILED) {
		return {};
	}
#endif

	int64_t cbid = 0;
	bool success = call(cname, fname, std::move(args), cb, &cd, cbid);
	if (!success) {
		return {};
	}

#ifdef WIN32
	cd.sgn->wait();
#elif __APPLE__
	sem_wait(cd.sem);
	sem_close(cd.sem);
	sem_unlink(path.c_str());
	remove(path.c_str());
#endif

	if (!cd.called) {
		cancel(cbid);
		return {};
	}
	return std::move(cd.values);
}
