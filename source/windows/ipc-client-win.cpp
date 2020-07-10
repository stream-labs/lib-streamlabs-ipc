#include "ipc-client-win.hpp"
#include "semaphore.hpp"

call_return_t g_fn   = NULL;
void*         g_data = NULL;
int64_t       g_cbid = NULL;

using namespace std::placeholders;

std::shared_ptr<ipc::client> ipc::client::create(std::string socketPath) {
	return std::make_unique<ipc::client_win>(socketPath);
}

ipc::client_win::client_win(std::string socketPath) {
	m_socket = os::windows::socket_win::create(os::open_only, socketPath);

	m_watcher.stop   = false;
	m_watcher.worker = std::thread(std::bind(&ipc::client_win::worker, this));
}

ipc::client_win::~client_win() {
	m_watcher.stop = true;
	if (m_watcher.worker.joinable()) {
		m_watcher.worker.join();
	}
	m_socket = nullptr;
}

bool ipc::client_win::call(
    const std::string& cname,
    const std::string& fname,
    std::vector<ipc::value> args,
    call_return_t fn,
    void* data,
    int64_t& cbid)
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

std::vector<ipc::value> ipc::client_win::call_synchronous_helper(
    const std::string& cname,
    const std::string& fname,
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

	auto cb = [](void* data, const std::vector<ipc::value>& rval) {
		CallData& cd = *static_cast<CallData*>(data);

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

	static std::chrono::nanoseconds freez_timeout = std::chrono::seconds(3); 
	bool freez_flagged = false;
	while ( cd.sgn->wait(freez_timeout) == os::error::TimedOut ) {
		if (freez_flagged)
			continue;
		freez_flagged = true;

		if (freez_cb)
			freez_cb(true, app_state_path);
	}
	if (freez_flagged) {
		if (freez_cb)
			freez_cb(false, app_state_path);
	}
	if (!cd.called) {
		cancel(cbid);
		return {};
	}
	return std::move(cd.values);
}

void ipc::client::set_freez_callback(call_on_freez_t cb, std::string app_state)
{
	freez_cb = cb;
	app_state_path = app_state;
}

void ipc::client_win::worker() {
	os::error ec = os::error::Success;
	std::vector<ipc::value> proc_rval;

	while (m_socket->is_connected() && !m_watcher.stop) {
		if (!m_rop || !m_rop->is_valid()) {
			m_watcher.buf.resize(sizeof(ipc_size_t));
			ec = m_socket->read(m_watcher.buf.data(), m_watcher.buf.size(), m_rop, std::bind(&ipc::client_win::read_callback_init, this, _1, _2));
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

	// Call any remaining callbacks.
	proc_rval.resize(1);
	proc_rval[0].type = ipc::type::Null;
	proc_rval[0].value_str = "Lost IPC Connection";

	{
		std::unique_lock<std::mutex> ulock(m_lock);
		for (auto& cb : m_cb) {
			cb.second.first(cb.second.second, proc_rval);
		}

		m_cb.clear();
	}

	if (!m_socket->is_connected()) {
		exit(1);
	}
}

void ipc::client_win::read_callback_init(os::error ec, size_t size) {
	os::error ec2 = os::error::Success;

	m_rop->invalidate();

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(m_watcher.buf);
		if (n_size != 0) {
			m_watcher.buf.resize(n_size);
			ec2 = m_socket->read(m_watcher.buf.data(), m_watcher.buf.size(), m_rop, std::bind(&ipc::client_win::read_callback_msg, this, _1, _2));
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

void ipc::client_win::read_callback_msg(os::error ec, size_t size) {
	std::pair<call_return_t, void*> cb;
	ipc::message::function_reply fnc_reply_msg;

	m_rop->invalidate();

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

bool ipc::client_win::cancel(int64_t const& id) {
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}