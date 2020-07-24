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

	buffer.resize(65000);
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
	std::cout << "call" << cname.c_str() << "::" << fname.c_str() << std::endl;
	static std::mutex mtx;
	static uint64_t timestamp = 0;
	os::error ec;
	std::cout << "call - 0" << std::endl;
	std::shared_ptr<os::async_op> write_op;
	ipc::message::function_call fnc_call_msg;
	std::vector<char> outbuf;
	std::vector<char> local_buffer;
	local_buffer.resize(65000);

	if (!m_socket)
		return false;

	{
	std::cout << "call - 1" << std::endl;
		std::unique_lock<std::mutex> ulock(mtx);
		timestamp++;
		fnc_call_msg.uid = ipc::value(timestamp);
	std::cout << "call - 2" << std::endl;
	}

	// Set	
	std::cout << "call - 3" << std::endl;
	fnc_call_msg.class_name = ipc::value(cname);
	std::cout << "call - 4" << std::endl;
	fnc_call_msg.function_name = ipc::value(fname);
	std::cout << "call - 5" << std::endl;
	fnc_call_msg.arguments = std::move(args);
	std::cout << "call - 6" << std::endl;

	// Serialize
	std::vector<char> buf(fnc_call_msg.size());
	std::cout << "call - 7" << std::endl;
	try {
		fnc_call_msg.serialize(buf, 0);
	std::cout << "call - 8" << std::endl;
	} catch (std::exception& e) {
	std::cout << "call - 9" << std::endl;
		ipc::log("(write) %8llu: Failed to serialize, error %s.", fnc_call_msg.uid.value_union.ui64, e.what());
		throw e;
	}

	if (fn != nullptr) {
	std::cout << "call - 10" << std::endl;
		std::unique_lock<std::mutex> ulock(m_lock);
	std::cout << "call - 11" << std::endl;
		m_cb.insert(std::make_pair(fnc_call_msg.uid.value_union.ui64, std::make_pair(fn, data)));
	std::cout << "call - 12" << std::endl;
		cbid = fnc_call_msg.uid.value_union.ui64;
	}

	sem_wait(m_writer_sem);
	std::cout << "call - 13" << std::endl;

	std::cout << "ipc-client write " << buf.size() << std::endl;
    ec = (os::error) m_socket->write(buf.data(), buf.size(), REQUEST);
	m_socket->read(local_buffer.data(),
				local_buffer.size(), true, REPLY);
	std::cout << "ipc-client read " << local_buffer.size() << std::endl;
	read_callback_msg(ec, 65000);
	std::cout << "call - 14" << std::endl;
	sem_post(m_writer_sem);

	std::cout << "ipc-client call - end " << cname.c_str() << "::" << fname.c_str() << std::endl;
	return true;
}

std::vector<ipc::value> ipc::client_osx::call_synchronous_helper(
    const std::string& cname,
    const std::string& fname,
    const std::vector<ipc::value>& args)
{
	std::cout << "call_synchronous_helper: " << cname.c_str() << "::" << fname.c_str() << std::endl;
	std::cout << "call_synchronous_helper - 0" << std::endl;
	struct CallData {
		sem_t *sem;
		bool called = false;
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		std::vector<ipc::value> values;
	} cd;

	std::cout << "call_synchronous_helper - 1" << std::endl;
	auto cb = [](void* data, const std::vector<ipc::value>& rval) {
		std::cout << "call_synchronous_helper - 2" << std::endl;
		CallData& cd = *static_cast<CallData*>(data);
		std::cout << "call_synchronous_helper - 3" << std::endl;
		cd.values.reserve(rval.size());
		std::cout << "call_synchronous_helper - 4" << std::endl;
		std::copy(rval.begin(), rval.end(), std::back_inserter(cd.values));
		std::cout << "call_synchronous_helper - 5" << std::endl;
		cd.called = true;
		sem_post(cd.sem);
		std::cout << "call_synchronous_helper - 6" << std::endl;
	};

	int uniqueId = cname.size() + fname.size() + rand();
	std::string sem_name = "sem-cb" + std::to_string(uniqueId);
	std::string path = "/tmp/" + sem_name;
	sem_unlink(path.c_str());
	std::cout << "call_synchronous_helper - 7" << std::endl;
	remove(path.c_str());
	std::cout << "call_synchronous_helper - 8" << std::endl;
	cd.sem = sem_open(path.c_str(), O_CREAT | O_EXCL, 0644, 0);
	std::cout << "call_synchronous_helper - 9" << std::endl;
	if (cd.sem == SEM_FAILED) {
		return {};
	}

	int64_t cbid = 0;
	bool success = call(cname, fname, std::move(args), cb, &cd, cbid);
	if (!success) {
		return {};
	}
	std::cout << "call_synchronous_helper - 10" << std::endl;
	sem_wait(cd.sem);
	std::cout << "call_synchronous_helper - 11" << std::endl;
	sem_close(cd.sem);
	std::cout << "call_synchronous_helper - 12" << std::endl;
	sem_unlink(path.c_str());
	std::cout << "call_synchronous_helper - 12" << std::endl;
	remove(path.c_str());
	std::cout << "call_synchronous_helper - 13" << std::endl;

	if (!cd.called) {
		cancel(cbid);
		return {};
	}
	std::cout << "call_synchronous_helper - 14" << std::endl;
	return std::move(cd.values);
}

void ipc::client_osx::read_callback_msg(os::error ec, size_t size) {
	std::cout << "read_callback_msg - 0" << std::endl;
	std::pair<call_return_t, void*> cb;
	ipc::message::function_reply fnc_reply_msg;

	std::cout << "read_callback_msg - 1" << std::endl;
	try {
	std::cout << "read_callback_msg - 2" << std::endl;
		fnc_reply_msg.deserialize(buffer, 0);
	std::cout << "read_callback_msg - 3" << std::endl;
	} catch (std::exception& e) {
	std::cout << "read_callback_msg - 4" << std::endl;
		ipc::log("Deserialize failed with error %s.", e.what());
		throw e;
	}

	std::cout << "read_callback_msg - 5" << std::endl;
	// Find the callback function.
	std::unique_lock<std::mutex> ulock(m_lock);
	auto cb2 = m_cb.find(fnc_reply_msg.uid.value_union.ui64);
	if (cb2 == m_cb.end()) {
		return;
	}
	std::cout << "read_callback_msg - 6" << std::endl;
	cb = cb2->second;
	// Decode return values or errors.
	if (fnc_reply_msg.error.value_str.size() > 0) {
		fnc_reply_msg.values.resize(1);
		fnc_reply_msg.values.at(0).type = ipc::type::Null;
		fnc_reply_msg.values.at(0).value_str = fnc_reply_msg.error.value_str;
	}
	std::cout << "read_callback_msg - 7" << std::endl;

	// Call Callback
	cb.first(cb.second, fnc_reply_msg.values);

	std::cout << "read_callback_msg - 8" << std::endl;
	// Remove cb entry
	m_cb.erase(fnc_reply_msg.uid.value_union.ui64);
	std::cout << "read_callback_msg - 9" << std::endl;
}

bool ipc::client_osx::cancel(int64_t const& id) {
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

void ipc::client::set_freez_callback(call_on_freez_t cb, std::string app_state)
{

}