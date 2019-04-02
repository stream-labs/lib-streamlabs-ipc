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

#include "ipc-communication.hpp"

call_return_t g_fn   = NULL;
void*         g_data = NULL;
int64_t       g_cbid = NULL;

bool ipc::ipc_communication::cancel(int64_t const& id)
{
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

bool ipc::ipc_communication::call(
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

std::vector<ipc::value> ipc::ipc_communication::call_synchronous_helper(
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

void ipc::ipc_communication::write_callback(os::error ec, size_t size)
{
	m_wop->invalidate();
	m_rop->invalidate();
}

void ipc::ipc_communication::handle_fnc_call()
{
	std::vector<ipc::value>      proc_rval;
	std::string                  proc_error;
	ipc::message::function_call  fnc_call_msg;
	ipc::message::function_reply fnc_reply_msg;
	std::vector<char>            write_buffer;
	bool                         success = false;

	try {
		fnc_call_msg.deserialize(m_watcher.rbuf, 0);
	} catch (std::exception e) {
		ipc::log("????????: Deserialization of Function Call message failed with error %s.", e.what());
		return;
	}

	// Execute
	proc_rval.resize(0);
	try {
		success = call_function(
		    -1, // Server
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
}

void ipc::ipc_communication::handle_fnc_reply()
{
	std::pair<call_return_t, void*> cb;
	ipc::message::function_reply    fnc_reply_msg;

	m_rop->invalidate();

	try {
		fnc_reply_msg.deserialize(m_watcher.rbuf, 0);
	} catch (std::exception e) {
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