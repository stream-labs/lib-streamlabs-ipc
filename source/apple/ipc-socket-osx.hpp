#ifndef IPC_SOCKET_IPC_H
#define IPC_SOCKET_IPC_H

#include "../include/ipc-socket.hpp"
#include "async_request.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <thread>

enum SocketType: uint8_t {
    REQUEST,
    REPLY
};

namespace os {
	namespace apple {
        class socket_osx : public ipc::socket {
			public:
			static std::unique_ptr<os::apple::socket_osx> create(os::create_only_t, const std::string& name);
			static std::unique_ptr<os::apple::socket_osx> create(os::open_only_t, const std::string& name);

            socket_osx(os::create_only_t, const std::string name);
            socket_osx(os::open_only_t, const std::string name);
            ~socket_osx();

            uint32_t read(char *buffer, size_t buffer_length, bool is_blocking, SocketType t);
			uint32_t write(const char *buffer, size_t buffer_length, SocketType t);

			virtual void handle_accept_callback(os::error code, size_t length) override;
			virtual bool is_created() override;
			virtual bool is_connected() override;
			virtual void set_connected(bool is_connected) override;
			virtual os::error accept(std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb) override;

            void clean_file_descriptors();

            private:
            bool created     = false;
            bool connected   = true;
            std::string name_req = "";
            std::string name_rep = "";
            int file_req;
            int file_rep;
            int fd_write;
            int fd_read_b;
            int fd_read_nb;
		};
	}
}

#endif