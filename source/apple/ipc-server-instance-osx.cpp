#include "ipc-server-instance-osx.hpp"

std::shared_ptr<ipc::server_instance> ipc::server_instance::create(server* owner, std::shared_ptr<ipc::socket> socket)
{
	return std::make_unique<ipc::server_instance_osx>(owner, socket);
}

ipc::server_instance_osx::server_instance_osx(ipc::server* owner, std::shared_ptr<ipc::socket> conn) {
	m_parent = owner;
	m_socket = std::dynamic_pointer_cast<os::apple::socket_osx>(conn);
	m_clientId = 0;

	m_stopWorkers = false;

	sem_unlink(reader_sem_name.c_str());
	remove(reader_sem_name.c_str());
	m_reader_sem = sem_open(reader_sem_name.c_str(), O_CREAT | O_EXCL, 0644, 1);

	sem_unlink(writer_sem_name.c_str());
	remove(writer_sem_name.c_str());
	m_writer_sem = sem_open(writer_sem_name.c_str(), O_CREAT | O_EXCL, 0644, 0);

	m_worker_requests = std::thread(std::bind(&ipc::server_instance_osx::worker_req, this));
	m_worker_replies = std::thread(std::bind(&ipc::server_instance_osx::worker_rep, this));
}

ipc::server_instance_osx::~server_instance_osx() {
	// Threading
	m_stopWorkers = true;

	// Unblock current sync read by send dummy data
	std::vector<char> buffer;
	buffer.push_back('1');
	std::vector<char> outbuffer;
	ipc::make_sendable(outbuffer, buffer);
	m_socket->write(outbuffer.data(), outbuffer.size(), REQUEST);

	if (m_worker_replies.joinable())
		m_worker_replies.join();

	if (m_worker_requests.joinable())
		m_worker_requests.join();

	sem_close(m_writer_sem);
	m_socket->clean_file_descriptors();
}

bool ipc::server_instance_osx::is_alive() {
	if (!m_socket->is_connected())
		return false;

	if (m_stopWorkers)
		return false;

	return true;
}

void ipc::server_instance_osx::worker_req() {
	// Loop
	while ((!m_stopWorkers) && m_socket->is_connected()) {
		sem_wait(m_reader_sem);
        m_rbuf.resize(sizeof(ipc_size_t));
		os::error ec = (os::error) m_socket->read(m_rbuf.data(),
						m_rbuf.size(), true, REQUEST);
		read_callback_init(ec, m_rbuf.size());
	}
}

void ipc::server_instance_osx::worker_rep() {
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
		std::vector<char> write_buffer;

		msg_mtx.lock();
		fnc_call_msg = msgs.front();
		msgs.pop();
		msg_mtx.unlock();

		proc_rval.resize(0);
		success = m_parent->client_call_function(m_clientId,
			fnc_call_msg.class_name.value_str, fnc_call_msg.function_name.value_str,
			fnc_call_msg.arguments, proc_rval, proc_error);

		// Set
		fnc_reply_msg.uid = fnc_call_msg.uid;
		std::swap(proc_rval, fnc_reply_msg.values);
		if (!success) {
			fnc_reply_msg.error = ipc::value(proc_error);
		}

		// Serialize
		write_buffer.resize(fnc_reply_msg.size());
		try {
			fnc_reply_msg.serialize(write_buffer, 0);
		} catch (std::exception & e) {
			ipc::log("%8llu: Serialization of Function Reply message failed with error %s.",
				fnc_reply_msg.uid.value_union.ui64, e.what());
			return;
		}
		read_callback_msg_write(write_buffer);
		sem_post(m_reader_sem);
	}
}

void ipc::server_instance_osx::read_callback_init(os::error ec, size_t size) {
	os::error ec2 = os::error::Success;

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(m_rbuf);
		if (n_size > 1) {
			m_rbuf.resize(n_size);
			ec2 = (os::error) m_socket->read(m_rbuf.data(),
				m_rbuf.size(), false, REQUEST);
			read_callback_msg(ec, m_rbuf.size());
		} else {
			sem_post(m_writer_sem);
		}
	}
}

void ipc::server_instance_osx::read_callback_msg(os::error ec, size_t size) {
	ipc::message::function_call fnc_call_msg;

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

void ipc::server_instance_osx::read_callback_msg_write(const std::vector<char>& write_buffer)
{
	if (write_buffer.size() != 0) {
		if ((!m_wop || !m_wop->is_valid()) && (m_write_queue.size() == 0)) {
			ipc::make_sendable(m_wbuf, write_buffer);
			os::error ec2 = (os::error)m_socket->write(m_wbuf.data(), m_wbuf.size(), REPLY);
		} else {
			m_write_queue.push(std::move(write_buffer));
		}
	} else {
		m_rop->invalidate();
	}
}

void ipc::server_instance_osx::write_callback(os::error ec, size_t size) {
	m_wop->invalidate();
	m_rop->invalidate();
}