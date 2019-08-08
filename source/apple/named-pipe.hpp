#include "async_request.hpp"
#include "tags.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <memory>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

namespace os {
    namespace apple {
        class named_pipe {
            public:
            named_pipe(os::create_only_t, const std::string name);
            named_pipe(os::open_only_t, const std::string name);
        };
    }
}