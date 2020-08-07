#include "../include/ipc-server-instance.hpp"
#include "../include/error.hpp"
#include "ipc-socket-osx.hpp"

namespace ipc {
	class server;

	class server_instance_osx : public server_instance {
        public:
		server_instance_osx(server* owner, std::shared_ptr<ipc::socket> socket);
        ~server_instance_osx();

        private:
		bool m_stopWorkers = false;
		std::thread m_worker_requests;
		std::thread m_worker_replies;
		std::string reader_sem_name = "semaphore-server-reader";
		std::string writer_sem_name = "semaphore-server-writer";
		sem_t *m_writer_sem;
		sem_t *m_reader_sem;
		std::shared_ptr<os::apple::socket_osx> m_socket;
		std::shared_ptr<os::async_op> m_wop, m_rop;
		std::vector<char> m_wbuf, m_rbuf;
		std::queue<std::vector<char>> m_write_queue;

		std::mutex msg_mtx;
		std::queue<ipc::message::function_call> msgs;

		private:
		server* m_parent = nullptr;
		int64_t m_clientId;

		bool is_alive();
		void worker_req();
		void worker_rep();
		void read_callback_init(os::error ec, size_t size);
		void read_callback_msg(os::error ec, size_t size);
		void read_callback_msg_write(const std::vector<char>& write_buffer);
		void write_callback(os::error ec, size_t size);		
    };
}