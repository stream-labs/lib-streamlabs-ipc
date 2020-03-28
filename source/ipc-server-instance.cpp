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
#ifdef WIN32
ipc::server_instance::server_instance(ipc::server* owner, std::shared_ptr<os::windows::named_pipe> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = 0;

	m_stopWorkers = false;
	m_worker = std::thread(std::bind(&server_instance::worker, this));
}
#elif __APPLE__
ipc::server_instance::server_instance(ipc::server* owner, std::shared_ptr<os::apple::named_pipe> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = 0;

	m_stopWorkers = false;

	sem_unlink(writer_sem_name.c_str());
	remove(writer_sem_name.c_str());
	m_writer_sem = sem_open(writer_sem_name.c_str(), O_CREAT | O_EXCL, 0644, 0);

	m_worker_requests = std::thread(std::bind(&server_instance::worker_req, this));
	m_worker_replies = std::thread(std::bind(&server_instance::worker_rep, this));
}

#endif
ipc::server_instance::~server_instance() {
	ipc::log("destroy start");
	// Threading
	m_stopWorkers = true;

	// Unblock current sync read by send dummy data
	std::vector<char> buffer;
	buffer.push_back('1');
	m_socket->write(buffer.data(), buffer.size(), REQUEST);

	if (m_worker_replies.joinable())
		m_worker_replies.join();

	if (m_worker_requests.joinable())
		m_worker_requests.join();

	sem_close(m_writer_sem);
	ipc::log("destroy end");
}

bool ipc::server_instance::is_alive() {
	if (!m_socket->is_connected())
		return false;

	if (m_stopWorkers)
		return false;

	return true;
}

void ipc::server_instance::worker_req() {
	// Loop
	while ((!m_stopWorkers) && m_socket->is_connected()) {
        m_rbuf.resize(65000);
		(os::error) m_socket->read(m_rbuf.data(),
									m_rbuf.size(),
									m_rop,
									std::bind(&server_instance::read_callback_msg,
												this,
												std::placeholders::_1,
												std::placeholders::_2), true, REQUEST);
	}
}

void ipc::server_instance::worker_rep() {
	// Loop
	while ((!m_stopWorkers) && m_socket->is_connected()) {
		sem_wait(m_writer_sem);

		if (m_stopWorkers)
			return;

		std::vector<ipc::value> proc_rval;
		std::string proc_error;
		ipc::message::function_call fnc_call_msg;
		ipc::message::function_reply fnc_reply_msg;
		bool success = false;

		msg_mtx.lock();
		fnc_call_msg = msgs.front();
		msgs.pop();
		msg_mtx.unlock();
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
		m_wbuf.resize(fnc_reply_msg.size());
		try {
			fnc_reply_msg.serialize(m_wbuf, 0);
		} catch (std::exception & e) {
			ipc::log("%8llu: Serialization of Function Reply message failed with error %s.",
				fnc_reply_msg.uid.value_union.ui64, e.what());
			return;
		}
		read_callback_msg_write(m_wbuf);
	}
}

void ipc::server_instance::read_callback_init(os::error ec, size_t size) {
// 	os::error ec2 = os::error::Success;

// 	m_rop->invalidate();

// 	if (ec == os::error::Success || ec == os::error::MoreData) {
// 		ipc_size_t n_size = read_size(m_rbuf);
//         // std::cout << "Size IPC message: " << n_size << std::endl;
// #ifdef _DEBUG
// 		std::string hex_msg = ipc::vectortohex(m_rbuf);
// 		ipc::log("????????: %.*s => %llu", hex_msg.size(), hex_msg.data(), n_size);
// #endif
// 		if (n_size != 0) {
// 			m_rbuf.resize(n_size);
// #ifdef WIN32
// 			ec2 = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&server_instance::read_callback_msg, this, _1, _2));
// #elif __APPLE__
// 			ec2 = (os::error)m_socket->read(m_rbuf.data(),
// 			                                m_rbuf.size(),
// 			                                m_rop,
// 			                                std::bind(&server_instance::read_callback_msg,
// 			                                          this,
// 			                                          std::placeholders::_1,
// 			                                          std::placeholders::_2), false, REQUEST);
// #endif
// 			if (ec2 != os::error::Pending && ec2 != os::error::Success) {
// 				if (ec2 == os::error::Disconnected) {
// 					return;
// 				} else {
// 					throw std::exception((const std::exception&)"Unexpected error.");
// 				}
// 			}
// 		}
// 	}
}

void ipc::server_instance::read_callback_msg(os::error ec, size_t size) {
	ipc::message::function_call fnc_call_msg;

	if (ec != os::error::Success) {
		return;
	}

	try {
		fnc_call_msg.deserialize(m_rbuf, 0);
	} catch (std::exception & e) {
		ipc::log("????????: Deserialization of Function Call message failed with error %s.", e.what());
		return;
	}
	m_rbuf.clear();

	msg_mtx.lock();
	msgs.push(fnc_call_msg);
	msg_mtx.unlock();

	sem_post(m_writer_sem);
}

void ipc::server_instance::read_callback_msg_write(const std::vector<char>& write_buffer)
{
	// std::cout << "read_callback_msg_write" << std::endl;
	if (write_buffer.size() != 0) {
		if ((!m_wop || !m_wop->is_valid()) && (m_write_queue.size() == 0)) {
#ifdef WIN32
			os::error ec2 = m_socket->write(m_wbuf.data(), m_wbuf.size(), m_wop, std::bind(&server_instance::write_callback, this, _1, _2));
#elif __APPLE__
			os::error ec2 = (os::error)m_socket->write(write_buffer.data(), write_buffer.size(), REPLY);
#endif
		} else {
			m_write_queue.push(std::move(write_buffer));
		}
	} else {
#ifdef _DEBUG
		ipc::log("????????: No Output, continuing as if nothing happened.");
#endif
		m_rop->invalidate();
	}
}

void ipc::server_instance::write_callback(os::error ec, size_t size) {
	m_wop->invalidate();
	m_rop->invalidate();
}
