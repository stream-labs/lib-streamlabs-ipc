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

#include "ipc-server-instance.hpp"
#include <sstream>
#include <functional>

using namespace std::placeholders;

#ifdef _WIN32
#include <windows.h>
#endif

ipc::server_instance::server_instance() {}

ipc::server_instance::server_instance(ipc::server* owner, std::shared_ptr<os::windows::named_pipe> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = 0;

	m_stopWorkers = false;
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

	// Prepare Buffers
	m_rbuf.reserve(65535);
	m_wbuf.reserve(65535);

	// Loop
	while ((!m_stopWorkers) && m_socket->is_connected()) {
		if (!m_rop || !m_rop->is_valid()) {
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
#ifdef _DEBUG
				ipc::log("????????: Sending %llu bytes...", m_wbuf.size());
				std::string hex_msg = ipc::vectortohex(m_wbuf);
				ipc::log("????????: %.*s.", hex_msg.size(), hex_msg.data());
#endif
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
#ifdef _DEBUG
		std::string hex_msg = ipc::vectortohex(m_rbuf);
		ipc::log("????????: %.*s => %llu", hex_msg.size(), hex_msg.data(), n_size);
#endif
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
	ipc::message::authenticate auth_msg;
	ipc::message::authenticate_reply auth_reply_msg;

#ifdef _DEBUG
	ipc::log("????????: read_callback_msg %lld %llu", (int64_t)ec, size);
	std::string hex_msg = ipc::vectortohex(m_rbuf);
	ipc::log("????????: %.*s.", hex_msg.size(), hex_msg.data());
#endif

	if (ec != os::error::Success) {
		return;
	}

	bool success = false;
	if (!m_isAuthenticated) {
		// Client is not authenticated.
		/// Authentication is required so that both sides know that the other is ready.

#ifdef _DEBUG
		ipc::log("????????: Unauthenticated Client, attempting deserialize of Authenticate message.");
#endif

		try {
			auth_msg.deserialize(m_rbuf, 0);
		} catch (std::exception e) {
			ipc::log("????????: Deserialization of Authenticate message failed with error %s.", e.what());
			return;
		}

#ifdef _DEBUG
		ipc::log("????????: Authenticated Client with name '%.*s' and password '%.*s'.",
			auth_msg.name.value_str.size(), auth_msg.name.value_str.c_str(),
			auth_msg.password.value_str.size(), auth_msg.password.value_str.c_str());
#endif

		// Set
		auth_reply_msg.auth = ipc::value(true);

		// Serialize
		write_buffer.resize(auth_reply_msg.size());
		try {
			auth_reply_msg.serialize(write_buffer, 0);
		} catch (std::exception e) {
			ipc::log("????????: Serialization of Authenticate Reply message failed with error %s.", e.what());
			return;
		}

#ifdef _DEBUG
		ipc::log("????????: Authenticate Reply serialized, size %lld.", write_buffer.size());
#endif

		m_isAuthenticated = true;
	} else {
		// Client is authenticated.

		bool is_fnc_call = ipc::message::is_function_call(m_rbuf, 0);
		if (is_fnc_call) {
			handle_fnc_call(write_buffer);
		} else {
			handle_fnc_reply();
		}
	}

	if (write_buffer.size() != 0) {
		if ((!m_wop || !m_wop->is_valid()) && (m_write_queue.size() == 0)) {
			ipc::make_sendable(m_wbuf, write_buffer);
#ifdef _DEBUG
			ipc::log("????????: Sending %llu bytes...", m_wbuf.size());
			std::string hex_msg = ipc::vectortohex(m_wbuf);
			ipc::log("????????: %.*s.", hex_msg.size(), hex_msg.data());
#endif
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
#ifdef _DEBUG
		ipc::log("????????: No Output, continuing as if nothing happened.");
#endif
		m_rop->invalidate();
	}
}

void ipc::server_instance::handle_fnc_call(std::vector<char>& writeBuffer) {

	std::vector<ipc::value>		 proc_rval;
	std::string                  proc_error;
	ipc::message::function_call  fnc_call_msg;
	ipc::message::function_reply fnc_reply_msg;
	bool                         success = false;

#ifdef _DEBUG
	ipc::log("????????: Authenticated Client, attempting deserialize of Function Call message.");
#endif

	try {
		fnc_call_msg.deserialize(m_rbuf, 0);
	} catch (std::exception e) {
		ipc::log("????????: Deserialization of Function Call message failed with error %s.", e.what());
		return;
	}

#ifdef _DEBUG
	ipc::log(
	    "%8llu: Function Call deserialized, class '%.*s' and function '%.*s', %llu arguments.",
	    fnc_call_msg.uid.value_union.ui64,
	    (uint64_t)fnc_call_msg.class_name.value_str.size(),
	    fnc_call_msg.class_name.value_str.c_str(),
	    (uint64_t)fnc_call_msg.function_name.value_str.size(),
	    fnc_call_msg.function_name.value_str.c_str(),
	    (uint64_t)fnc_call_msg.arguments.size());
	for (size_t idx = 0; idx < fnc_call_msg.arguments.size(); idx++) {
		ipc::value& arg = fnc_call_msg.arguments[idx];
		switch (arg.type) {
		case type::Int32:
			ipc::log("\t%llu: %ld", idx, arg.value_union.i32);
			break;
		case type::Int64:
			ipc::log("\t%llu: %lld", idx, arg.value_union.i64);
			break;
		case type::UInt32:
			ipc::log("\t%llu: %lu", idx, arg.value_union.ui32);
			break;
		case type::UInt64:
			ipc::log("\t%llu: %llu", idx, arg.value_union.ui64);
			break;
		case type::Float:
			ipc::log("\t%llu: %f", idx, (double)arg.value_union.fp32);
			break;
		case type::Double:
			ipc::log("\t%llu: %f", idx, arg.value_union.fp64);
			break;
		case type::String:
			ipc::log("\t%llu: %.*s", idx, arg.value_str.size(), arg.value_str.c_str());
			break;
		case type::Binary:
			ipc::log("\t%llu: (Binary)", idx);
			break;
		}
	}
#endif

	// Execute
	proc_rval.resize(0);
	try {
		success = m_parent->client_call_function(
		    m_clientId,
		    fnc_call_msg.class_name.value_str,
		    fnc_call_msg.function_name.value_str,
		    fnc_call_msg.arguments,
		    proc_rval,
		    proc_error);
	} catch (std::exception e) {
		ipc::log(
		    "%8llu: Unexpected exception during client call, error %s.", fnc_call_msg.uid.value_union.ui64, e.what());
		throw e;
	}

	// Set
	fnc_reply_msg.uid = fnc_call_msg.uid;
	std::swap(proc_rval, fnc_reply_msg.values); // Fast "copy" of parameters.
	if (!success) {
		fnc_reply_msg.error = ipc::value(proc_error);
	}

#ifdef _DEBUG
	ipc::log(
	    "%8llu: FunctionCall Execute Complete, %llu return values",
	    fnc_call_msg.uid.value_union.ui64,
	    (uint64_t)fnc_reply_msg.values.size());
	for (size_t idx = 0; idx < fnc_reply_msg.values.size(); idx++) {
		ipc::value& arg = fnc_reply_msg.values[idx];
		switch (arg.type) {
		case type::Int32:
			ipc::log("\t%llu: %ld", idx, arg.value_union.i32);
			break;
		case type::Int64:
			ipc::log("\t%llu: %lld", idx, arg.value_union.i64);
			break;
		case type::UInt32:
			ipc::log("\t%llu: %lu", idx, arg.value_union.ui32);
			break;
		case type::UInt64:
			ipc::log("\t%llu: %llu", idx, arg.value_union.ui64);
			break;
		case type::Float:
			ipc::log("\t%llu: %f", idx, (double)arg.value_union.fp32);
			break;
		case type::Double:
			ipc::log("\t%llu: %f", idx, arg.value_union.fp64);
			break;
		case type::String:
			ipc::log("\t%llu: %.*s", idx, arg.value_str.size(), arg.value_str.c_str());
			break;
		case type::Binary:
			ipc::log("\t%llu: (Binary)", idx);
			break;
		}
	}
#endif

	// Serialize
	writeBuffer.resize(fnc_reply_msg.size());
	try {
		fnc_reply_msg.serialize(writeBuffer, 0);
	} catch (std::exception e) {
		ipc::log(
		    "%8llu: Serialization of Function Reply message failed with error %s.",
		    fnc_call_msg.uid.value_union.ui64,
		    e.what());
		return;
	}

#ifdef _DEBUG
	ipc::log(
	    "%8llu: Function Reply serialized, %llu bytes.",
	    fnc_call_msg.uid.value_union.ui64,
	    (uint64_t)writeBuffer.size());
	std::string hex_msg = ipc::vectortohex(writeBuffer);
	ipc::log("%8llu: %.*s.", fnc_call_msg.uid.value_union.ui64, hex_msg.size(), hex_msg.data());
#endif
}

void ipc::server_instance::handle_fnc_reply() {
	std::pair<call_return_t, void*> cb;
	ipc::message::function_reply    fnc_reply_msg;

	m_rop->invalidate();

	try {
		fnc_reply_msg.deserialize(m_rbuf, 0);
	} catch (std::exception e) {
		ipc::log("Deserialize failed with error %s.", e.what());
		throw e;
	}

#ifdef _DEBUG
	ipc::log(
	    "(read) %8llu: Deserialized with %lld return values.",
	    fnc_reply_msg.uid.value_union.ui64,
	    fnc_reply_msg.values.size());
	for (size_t idx = 0; idx < fnc_reply_msg.values.size(); idx++) {
		ipc::value& arg = fnc_reply_msg.values[idx];
		switch (arg.type) {
		case type::Int32:
			ipc::log("(read) \t%llu: %ld", idx, arg.value_union.i32);
			break;
		case type::Int64:
			ipc::log("(read) \t%llu: %lld", idx, arg.value_union.i64);
			break;
		case type::UInt32:
			ipc::log("(read) \t%llu: %lu", idx, arg.value_union.ui32);
			break;
		case type::UInt64:
			ipc::log("(read) \t%llu: %llu", idx, arg.value_union.ui64);
			break;
		case type::Float:
			ipc::log("(read) \t%llu: %f", idx, (double)arg.value_union.fp32);
			break;
		case type::Double:
			ipc::log("(read) \t%llu: %f", idx, arg.value_union.fp64);
			break;
		case type::String:
			ipc::log("(read) \t%llu: %.*s", idx, arg.value_str.size(), arg.value_str.c_str());
			break;
		case type::Binary:
			ipc::log("(read) \t%llu: (Binary)", idx);
			break;
		}
	}
	ipc::log("(read) \terror: %.*s", fnc_reply_msg.error.value_str.size(), fnc_reply_msg.error.value_str.c_str());
#endif

	// Find the callback function.
	std::unique_lock<std::mutex> ulock(m_lock);
	auto                         cb2 = m_cb.find(fnc_reply_msg.uid.value_union.ui64);
	if (cb2 == m_cb.end()) {
#ifdef _DEBUG
		ipc::log("(read) %8llu: No callback, returning.", fnc_reply_msg.uid.value_union.ui64);
#endif
		return;
	}
	cb = cb2->second;

	// Decode return values or errors.
	if (fnc_reply_msg.error.value_str.size() > 0) {
		fnc_reply_msg.values.resize(1);
		fnc_reply_msg.values.at(0).type      = ipc::type::Null;
		fnc_reply_msg.values.at(0).value_str = fnc_reply_msg.error.value_str;
	}

#ifdef _DEBUG
	ipc::log("(read) %8llu: Calling callback Function Reply...", fnc_reply_msg.uid.value_union.ui64);
#endif

	// Call Callback
	cb.first(cb.second, fnc_reply_msg.values);

	// Remove cb entry
	/// ToDo: Figure out better way of registering functions, perhaps even a way to have "events" across a IPC connection.
	m_cb.erase(fnc_reply_msg.uid.value_union.ui64);

#ifdef _DEBUG
	ipc::log("(read) %8llu: Done.", fnc_reply_msg.uid.value_union.ui64);
#endif
}

void ipc::server_instance::write_callback(os::error ec, size_t size) {
	m_wop->invalidate();
	m_rop->invalidate();
	// Do we need to do anything here? Not really.

	// Uncomment this to give up the rest of the time slice to the next thread.
	// Not recommended since we do this anyway with the next wait.	
	/*
	#ifdef _WIN32
		Sleep(0);
	#endif
	*/
}

bool ipc::server_instance::cancel(int64_t const& id)
{
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

bool ipc::server_instance::call(
    std::string             cname,
    std::string             fname,
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

#ifdef _DEBUG
	ipc::log(
	    "(write) %8llu: Serializing Function Call for class '%.*s' with function '%.*s' and %llu arguments.",
	    fnc_call_msg.uid.value_union.ui64,
	    (uint64_t)fnc_call_msg.class_name.value_str.size(),
	    fnc_call_msg.class_name.value_str.c_str(),
	    (uint64_t)fnc_call_msg.function_name.value_str.size(),
	    fnc_call_msg.function_name.value_str.c_str(),
	    (uint64_t)fnc_call_msg.arguments.size());
	for (size_t idx = 0; idx < fnc_call_msg.arguments.size(); idx++) {
		ipc::value& arg = fnc_call_msg.arguments[idx];
		switch (arg.type) {
		case type::Int32:
			ipc::log("(write) \t%llu: %ld", idx, arg.value_union.i32);
			break;
		case type::Int64:
			ipc::log("(write) \t%llu: %lld", idx, arg.value_union.i64);
			break;
		case type::UInt32:
			ipc::log("(write) \t%llu: %lu", idx, arg.value_union.ui32);
			break;
		case type::UInt64:
			ipc::log("(write) \t%llu: %llu", idx, arg.value_union.ui64);
			break;
		case type::Float:
			ipc::log("(write) \t%llu: %f", idx, (double)arg.value_union.fp32);
			break;
		case type::Double:
			ipc::log("(write) \t%llu: %f", idx, arg.value_union.fp64);
			break;
		case type::String:
			ipc::log("(write) \t%llu: %.*s", idx, arg.value_str.size(), arg.value_str.c_str());
			break;
		case type::Binary:
			ipc::log("(write) \t%llu: (Binary)", idx);
			break;
		}
	}
#endif

	// Serialize
	std::vector<char> buf(fnc_call_msg.size());
	try {
		fnc_call_msg.serialize(buf, 0);
	} catch (std::exception e) {
		ipc::log("(write) %8llu: Failed to serialize, error %s.", fnc_call_msg.uid.value_union.ui64, e.what());
		throw e;
	}

	if (fn != nullptr) {
		std::unique_lock<std::mutex> ulock(m_lock);
		m_cb.insert(std::make_pair(fnc_call_msg.uid.value_union.ui64, std::make_pair(fn, data)));
		cbid = fnc_call_msg.uid.value_union.ui64;
	}

#ifdef _DEBUG
	ipc::log("(write) %8llu: Sending %llu bytes...", fnc_call_msg.uid.value_union.ui64, buf.size());
	std::string hex_msg = ipc::vectortohex(buf);
	ipc::log("(write) %8llu: %.*s", fnc_call_msg.uid.value_union.ui64, hex_msg.size(), hex_msg.data());
#endif

	ipc::make_sendable(outbuf, buf);
#ifdef _DEBUG
	hex_msg = ipc::vectortohex(outbuf);
	ipc::log("(write) %8llu: %.*s", fnc_call_msg.uid.value_union.ui64, hex_msg.size(), hex_msg.data());
#endif
	ec = m_socket->write(outbuf.data(), outbuf.size(), write_op, nullptr);
	if (ec != os::error::Success && ec != os::error::Pending) {
		cancel(cbid);
		write_op->cancel();
		return false;
	}

	ec = write_op->wait(std::chrono::milliseconds(15000));
	if (ec != os::error::Success) {
		cancel(cbid);
		write_op->cancel();
		return false;
	}

#ifdef _DEBUG
	ipc::log("(write) %8llu: Sent.", fnc_call_msg.uid.value_union.ui64);
#endif

	return true;
}