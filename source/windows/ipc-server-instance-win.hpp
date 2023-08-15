#include "../include/ipc-server-instance.hpp"
#include "../include/error.hpp"
#include "ipc-socket-win.hpp"

#include "utility.hpp"

namespace ipc {
class server;

class server_instance_win : public server_instance {
private:
	bool m_stopWorkers = false;
	std::thread m_worker;
	std::shared_ptr<os::windows::socket_win> m_socket;
	std::shared_ptr<os::async_op> m_wop, m_rop;
	std::vector<char> m_wbuf, m_rbuf;
	std::queue<std::vector<char>> m_write_queue;
	server *m_parent = nullptr;
	int64_t m_clientId;

	std::thread m_watchdog_thread;
	std::mutex m_watchdog_mutex;
	void watchdog_callbacks(int call_timeout);
	std::chrono::steady_clock::time_point m_last_write_time;
	bool m_write_waiting = false;

public:
	server_instance_win(server *owner, std::shared_ptr<ipc::socket> socket, int call_timeout);
	~server_instance_win();

public:
	void worker();
	void read_callback_init(os::error ec, size_t size);
	void read_callback_msg(os::error ec, size_t size);
	void read_callback_msg_write(std::vector<char> &write_buffer);
	void write_callback(os::error ec, size_t size);
};
}