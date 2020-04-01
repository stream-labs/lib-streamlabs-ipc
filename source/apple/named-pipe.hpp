#ifndef OS_APPLE_NAMED_PIPE_HPP
#define OS_APPLE_NAMED_PIPE_HPP

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <string>
#include <memory>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <thread>
#include <aio.h>
#include <unistd.h>
#include "tags.hpp"
#include "error.hpp"
#include "semaphore.hpp"
#include "async_request.hpp"

enum SocketType: uint8_t {
    REQUEST,
    REPLY
};

namespace os {
    namespace apple {
        class named_pipe {
            private:
            bool created     = false;
            bool connected   = true;
            std::string name_req = "";
            std::string name_rep = "";
            int file_req;
            int file_rep;

			void handle_accept_callback(os::error code, size_t length);

            public:
            named_pipe(os::create_only_t, const std::string name);
            named_pipe(os::open_only_t, const std::string name);
            ~named_pipe();

            uint32_t read(char *buffer, size_t buffer_length, bool is_blocking, SocketType t);

			uint32_t write(const char *buffer, size_t buffer_length, SocketType t);

            bool is_created();

			bool is_connected();

			void set_connected(bool is_connected);

			public: // created only
			os::error accept(std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb);
        };
    }
}
#endif // OS_APPLE_NAMED_PIPE_HPP
