#include "ipc-client-osx.hpp"

call_return_t g_fn   = NULL;
void*         g_data = NULL;
int64_t       g_cbid = NULL;

std::shared_ptr<ipc::client> ipc::client::create(std::string socketPath) {
	return std::make_unique<ipc::client_osx>(socketPath);
}

ipc::client_osx::client_osx(std::string socketPath) {
	m_socket = os::apple::socket_osx::create(os::open_only, socketPath);

	sem_unlink(writer_sem_name.c_str());
	remove(writer_sem_name.c_str());
	m_writer_sem = sem_open(writer_sem_name.c_str(), O_CREAT | O_EXCL, 0644, 1);

	buffer.resize(130000);
}

ipc::client_osx::~client_osx() {
	sem_post(m_writer_sem);
	sem_close(m_writer_sem);
	m_socket = nullptr;
}

bool ipc::client_osx::call(
    const std::string& cname,
    const std::string& fname,
    std::vector<ipc::value> args,
    call_return_t fn,
    void* data,
    int64_t& cbid)
{
	static std::mutex mtx;
	static uint64_t timestamp = 0;
	os::error ec = os::error::Error;

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

	ipc::make_sendable(outbuf, buf);

	sem_wait(m_writer_sem);
	while (ec == os::error::Error) {
		ec = (os::error) m_socket->write(outbuf.data(), outbuf.size(), REQUEST);
		if (ec == os::error::Error)
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	buffer.resize(sizeof(ipc_size_t));
	ec = (os::error) m_socket->read(buffer.data(),
				buffer.size(), true, REPLY);
	read_callback_init(ec, buffer.size());
	return true;
}

std::vector<ipc::value> ipc::client_osx::call_synchronous_helper(
    const std::string& cname,
    const std::string& fname,
    const std::vector<ipc::value>& args)
{
	struct CallData {
		sem_t *sem;
		bool called = false;
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		std::vector<ipc::value> values;
	} cd;

	auto cb = [](void* data, const std::vector<ipc::value>& rval) {
		CallData& cd = *static_cast<CallData*>(data);
		cd.values.reserve(rval.size());
		std::copy(rval.begin(), rval.end(), std::back_inserter(cd.values));
		cd.called = true;
		sem_post(cd.sem);
	};

	int uniqueId = cname.size() + fname.size() + rand();
	std::string sem_name = "sem-cb" + std::to_string(uniqueId);
	std::string path = "/tmp/" + sem_name;
	sem_unlink(path.c_str());
	remove(path.c_str());
	cd.sem = sem_open(path.c_str(), O_CREAT | O_EXCL, 0644, 0);
	if (cd.sem == SEM_FAILED) {
		return {};
	}

	int64_t cbid = 0;
	bool success = call(cname, fname, std::move(args), cb, &cd, cbid);
	if (!success) {
		return {};
	}
	sem_wait(cd.sem);
	sem_close(cd.sem);
	sem_unlink(path.c_str());
	remove(path.c_str());

	if (!cd.called) {
		cancel(cbid);
		return {};
	}
	sem_post(m_writer_sem);
	return std::move(cd.values);
}

void ipc::client_osx::read_callback_init(os::error ec, size_t size) {
	os::error ec2 = os::error::Success;

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(buffer);
		if (n_size != 0) {
			buffer.resize(n_size);
			ec2 = (os::error) 	m_socket->read(buffer.data(),
				buffer.size(), false, REPLY);
			read_callback_msg(ec, buffer.size());
		}
	}
}

void ipc::client_osx::read_callback_msg(os::error ec, size_t size) {
	std::pair<call_return_t, void*> cb;
	ipc::message::function_reply fnc_reply_msg;

	try {
		fnc_reply_msg.deserialize(buffer, 0);
	} catch (std::exception& e) {
		ipc::log("Deserialize failed with error %s.", e.what());
		throw e;
	}

	// Find the callback function.
	std::unique_lock<std::mutex> ulock(m_lock);
	auto cb2 = m_cb.find(fnc_reply_msg.uid.value_union.ui64);
	if (cb2 == m_cb.end()) {
		sem_post(m_writer_sem);
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

bool ipc::client_osx::cancel(int64_t const& id) {
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

void ipc::client::set_freez_callback(call_on_freez_t cb, std::string app_state)
{

}