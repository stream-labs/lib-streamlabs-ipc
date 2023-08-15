
#include "ipc-server-instance-win.hpp"

#include <memory>

using namespace std::placeholders;

std::shared_ptr<ipc::server_instance> ipc::server_instance::create(server *owner, std::shared_ptr<ipc::socket> socket, int call_timeout)
{
	return std::make_unique<ipc::server_instance_win>(owner, socket, call_timeout);
}

ipc::server_instance_win::server_instance_win(server *owner, std::shared_ptr<ipc::socket> socket, int call_timeout)
{
	m_stopWorkers = false;
	m_parent = owner;
	m_clientId = 0;
	m_socket = std::dynamic_pointer_cast<os::windows::socket_win>(socket);
	m_worker = std::thread(std::bind(&ipc::server_instance_win::worker, this));

	if (call_timeout)
		m_watchdog_thread = std::thread(std::bind(&server_instance_win::watchdog_callbacks, this, call_timeout));
}

ipc::server_instance_win::~server_instance_win()
{
	// Threading
	m_stopWorkers = true;
	if (m_worker.joinable())
		m_worker.join();
	if (m_watchdog_thread.joinable())
		m_watchdog_thread.join();
}

void ipc::server_instance_win::watchdog_callbacks(int call_timeout)
{
	while (!m_stopWorkers) {
		std::unique_lock<std::mutex> lock(m_watchdog_mutex);
		if (m_write_waiting) {
			if (std::chrono::steady_clock::now() - m_last_write_time > std::chrono::seconds(call_timeout)) {
				throw std::exception("No write in 30 seconds");
			}
		}
		lock.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void ipc::server_instance_win::worker()
{
	os::error ec = os::error::Success;

	// Loop
	while ((!m_stopWorkers) && m_socket->is_connected()) {
		if (!m_rop || !m_rop->is_valid()) {
			size_t testSize = sizeof(ipc_size_t);
			m_rbuf.resize(sizeof(ipc_size_t));
			ec = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&ipc::server_instance_win::read_callback_init, this, _1, _2));
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
				std::vector<char> &fbuf = m_write_queue.front();
				ipc::make_sendable(fbuf);
				ec = m_socket->write(fbuf.data(), fbuf.size(), m_wop, std::bind(&ipc::server_instance_win::write_callback, this, _1, _2));
				if (ec != os::error::Pending && ec != os::error::Success) {
					if (ec == os::error::Disconnected) {
						break;
					} else {
						const DWORD parent_proc_exit_code = os::windows::utility::get_parent_process_exit_code();
						ipc::log("Write buffer operation failed with error %d %p, pp_exit_code=%d", static_cast<int>(ec), &fbuf,
							 parent_proc_exit_code);
						throw std::exception("Write buffer operation failed");
					}
				}

				std::unique_lock<std::mutex> lock(m_watchdog_mutex);
				m_write_waiting = false;
				lock.unlock();
				m_write_queue.pop();
			}
		}

		os::waitable *waits[] = {m_rop.get(), m_wop.get()};
		size_t wait_index = -1;
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

void ipc::server_instance_win::read_callback_init(os::error ec, size_t size)
{
	os::error ec2 = os::error::Success;

	m_rop->invalidate();

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(m_rbuf);
		if (n_size != 0) {
			m_rbuf.resize(n_size);
			ec2 = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&ipc::server_instance_win::read_callback_msg, this, _1, _2));
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

void ipc::server_instance_win::read_callback_msg(os::error ec, size_t size)
{
	/// Processing
	std::vector<ipc::value> proc_rval;
	ipc::value proc_tempval;
	std::string proc_error;
	std::vector<char> write_buffer;

	ipc::message::function_call fnc_call_msg;
	ipc::message::function_reply fnc_reply_msg;

	std::unique_lock<std::mutex> lock(m_watchdog_mutex);
	m_write_waiting = true;
	m_last_write_time = std::chrono::steady_clock::now();
	lock.unlock();

	if (ec != os::error::Success) {
		throw std::exception("Unexpected error.");
		return;
	}

	bool success = false;

	try {
		fnc_call_msg.deserialize(m_rbuf, 0);
	} catch (std::exception &e) {
		ipc::log("????????: Deserialization of Function Call message failed with error %s.", e.what());
		throw std::exception("Deserialization of Function Call message failed.");
		return;
	}

	// Execute
	proc_rval.resize(0);
	success = m_parent->client_call_function(m_clientId, fnc_call_msg.class_name.value_str, fnc_call_msg.function_name.value_str, fnc_call_msg.arguments,
						 proc_rval, proc_error);

	// Set
	fnc_reply_msg.uid = fnc_call_msg.uid;
	std::swap(proc_rval, fnc_reply_msg.values); // Fast "copy" of parameters.
	if (!success) {
		fnc_reply_msg.error = ipc::value(proc_error);
	}

	// Serialize
	write_buffer.resize(fnc_reply_msg.size() + sizeof(ipc_size_t));
	try {
		fnc_reply_msg.serialize(write_buffer, sizeof(ipc_size_t));
	} catch (std::exception &e) {
		ipc::log("%8llu: Serialization of Function Reply message failed with error %s.", fnc_reply_msg.uid.value_union.ui64, e.what());
		throw std::exception("Serialization of Function Reply message failed.");
		return;
	}

	read_callback_msg_write(write_buffer);
}

void ipc::server_instance_win::read_callback_msg_write(std::vector<char> &write_buffer)
{
	if (write_buffer.size() != 0) {
		m_write_queue.push(std::move(write_buffer));
	} else {
		m_rop->invalidate();
	}
}

void ipc::server_instance_win::write_callback(os::error ec, size_t size)
{
	m_wop->invalidate();
	m_rop->invalidate();
}