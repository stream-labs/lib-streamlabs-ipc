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

namespace os {
    namespace apple {
        class named_pipe {
            bool created     = false;
            bool connected   = true;
            int file_descriptor = -1;
            std::string name = "";

			void handle_accept_callback(os::error code, size_t length);

            public:
            named_pipe(os::create_only_t, const std::string name);
            named_pipe(os::open_only_t, const std::string name);
            ~named_pipe();

            uint32_t read(char *buffer, size_t buffer_length, std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb);

			uint32_t write(const char *buffer, size_t buffer_length);

            bool is_created();

			bool is_connected();

			void set_connected(bool is_connected);

			public: // created only
			os::error accept(std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb);
        };
    }
}
#endif // OS_APPLE_NAMED_PIPE_HPP
