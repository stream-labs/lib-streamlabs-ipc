#include "named-pipe.hpp"

os::apple::named_pipe::named_pipe(os::create_only_t, const std::string name)
{
    int ret = 0;

    ret = remove(name.c_str());

    ret = mkfifo(name.c_str(), 0600);

    if (ret < 0)
        throw "Could not create named pipe.";

    file_descriptor = open(name.c_str(), O_RDWR);

    if (file_descriptor < 0)
        throw "Could not open named pipe.";

    connected = true;
}

os::apple::named_pipe::named_pipe(os::open_only_t, const std::string name)
{
    file_descriptor = open(name.c_str(), O_RDWR);

    if (file_descriptor < 0)
        throw "Could not open named pipe.";

    connected = true;
}

os::apple::named_pipe::~named_pipe() {

}

uint32_t os::apple::named_pipe::read(char *buffer, size_t buffer_length)
{
    ssize_t ret = 0;
    ret = ::read(file_descriptor, buffer, buffer_length);

    if (ret < 0)
        return 1;

    return 0;
}

uint32_t os::apple::named_pipe::write(const char *buffer, size_t buffer_length)
{
    ssize_t ret = 0;
    ret = ::write(file_descriptor, buffer, buffer_length);

    if (ret < 0)
        return 1;

    return 0;
}

bool os::apple::named_pipe::is_created() {
	return created;
}

bool os::apple::named_pipe::is_connected() {
    return connected;
}

void os::apple::named_pipe::set_connected(bool is_connected) {
    connected = is_connected;
}