#include "../include/ipc-client.hpp"
#include "../include/error.hpp"
#include "ipc-socket-win.hpp"

#include <atomic>
#include <mutex>
#include <map>

namespace ipc {
class client_win : public ipc::client {
public:
	client_win(const std::string &socketPath, call_on_disconnect_t disconnectionCallback);
	client_win(std::string socketPath);
	~client_win();

public:
	void start();
	void stop() override;

	virtual bool call(const std::string &cname, const std::string &fname, std::vector<ipc::value> args, call_return_t fn = g_fn, void *data = g_data,
			  int64_t &cbid = g_cbid) override;

	virtual std::vector<ipc::value> call_synchronous_helper(const std::string &cname, const std::string &fname, const std::vector<ipc::value> &args,
								bool expect_reply = true) override;

private:
	std::string m_socketPath;
	call_on_disconnect_t m_disconnectionCallback;
	std::unique_ptr<os::windows::socket_win> m_socket;
	std::shared_ptr<os::async_op> m_rop;

	bool m_authenticated = false;
	std::mutex m_lock;
	std::map<int64_t, std::pair<call_return_t, void *>> m_cb;

	// Threading
	struct {
		std::thread worker;
		std::atomic_bool stop = true;
		std::vector<char> buf;
	} m_watcher;

	void worker();
	void read_callback_init(os::error ec, size_t size);
	void read_callback_msg(os::error ec, size_t size);
	bool cancel(int64_t const &id);
};
}
