#include "../include/ipc-client.hpp"
#include "../include/error.hpp"
#include "ipc-socket-osx.hpp"
#include "async_request.hpp"

#include <mutex>
#include <map>
#include <semaphore.h>
#include <vector>
#include <thread>

namespace ipc {
	class client_osx : public ipc::client {
        public:
            client_osx(std::string socketPath);
		    ~client_osx();

        public:
            virtual bool call(
                const std::string&      cname,
                const std::string&      fname,
                std::vector<ipc::value> args,
                call_return_t           fn   = g_fn,
                void*                   data = g_data,
                int64_t&                cbid = g_cbid
            ) override;

            virtual std::vector<ipc::value> call_synchronous_helper(
                const std::string & cname,
                const std::string &fname,
                const std::vector<ipc::value> & args
            ) override;

        private:
        std::unique_ptr<os::apple::socket_osx> m_socket;
        std::string writer_sem_name = "semaphore-client-writer";
		sem_t *m_writer_sem;
		std::mutex m_lock;
		std::map<int64_t, std::pair<call_return_t, void*>> m_cb;

        std::vector<char> buffer;

        void read_callback_init(os::error ec, size_t size);
        void read_callback_msg(os::error ec, size_t size);
        bool cancel(int64_t const& id);
    };
}