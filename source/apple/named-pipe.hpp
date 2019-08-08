#ifndef OS_APPLE_NAMED_PIPE_HPP
#define OS_APPLE_NAMED_PIPE_HPP

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <memory>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>

#include "tags.hpp"
#include "error.hpp"
#include "semaphore.hpp"

namespace os {
    namespace apple {
        class named_pipe {
            bool created   = false;
            bool connected = true;
            int  file_descriptor = 0;
            public:
            named_pipe(os::create_only_t, const std::string name);
            named_pipe(os::open_only_t, const std::string name);
            ~named_pipe();

            uint32_t read(char *buffer, size_t buffer_length);

			uint32_t write(const char *buffer, size_t buffer_length);

            bool is_created();

			bool is_connected();

			void set_connected(bool is_connected);
        };
    }
}
#endif // OS_APPLE_NAMED_PIPE_HPP