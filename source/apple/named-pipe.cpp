#include "named-pipe.hpp"

os::apple::named_pipe::named_pipe(os::create_only_t, const std::string name)
{
    int ret = 0;

    ret = remove(name.c_str());

    ret = mkfifo(name.c_str(), S_IRUSR|S_IWUSR);

    if (ret < 0)
        std::cout << "Could not create named pipe. " << strerror(errno) << std::endl;
    else
    {
        std::cout << "Created successfully" << std::endl;
    }
    
    this->name = name;
    created = true;
}

os::apple::named_pipe::named_pipe(os::open_only_t, const std::string name)
{
    this->name = name;
    connected = true;
}

os::apple::named_pipe::~named_pipe() {
    remove(name.c_str());
}

uint32_t os::apple::named_pipe::read(char *buffer, size_t buffer_length)
{
    ssize_t ret = 0;
    file_descriptor = open(name.c_str(), O_RDONLY);//| O_NONBLOCK);

    if (file_descriptor < 0) {
        std::cout << "Could not open " << strerror(errno) << std::endl;
        return (uint32_t) os::error::Pending;
    }
    ret = ::read(file_descriptor, buffer, buffer_length);

    if (ret < 0) {
        std::cout << "Invalid read " << strerror(errno) << std::endl;
        return (uint32_t) os::error::Pending;
    } else {
        std::cout << "Successful read " << buffer << std::endl;
    }
    close(file_descriptor);
    return (uint32_t) os::error::Success;
}

uint32_t os::apple::named_pipe::write(const char *buffer, size_t buffer_length)
{
    ssize_t ret = 0;

    file_descriptor = open(name.c_str(), O_WRONLY);

    if (file_descriptor < 0) {
        std::cout << "Could not open " << strerror(errno) << std::endl;
        return (uint32_t) os::error::Pending;
    }
    ret = ::write(file_descriptor, buffer, buffer_length);

    if (ret < 0) {
        std::cout << "Invalid write " << strerror(errno) << std::endl;
        return (uint32_t) os::error::Error;
    }

    close(file_descriptor);

    return (uint32_t) os::error::Success;
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

void os::apple::named_pipe::handle_accept_callback(os::error code, size_t length) {
	if (code == os::error::Connected || code == os::error::Success) {
		set_connected(true);
	} else {
		set_connected(false);
	}
}

os::error os::apple::named_pipe::accept(std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb) {
    if (!is_created()) {
		return os::error::Error;
	}

	std::shared_ptr<os::apple::async_request> ar = std::static_pointer_cast<os::apple::async_request>(op);

    if (!ar) {
		ar = std::make_shared<os::apple::async_request>();
		op = std::static_pointer_cast<os::async_op>(ar);
	}

	ar->set_callback(cb);
	ar->set_system_callback(
		std::bind(&named_pipe::handle_accept_callback, this, std::placeholders::_1, std::placeholders::_2));
	// ar->set_sem(sem);

    connected = true;

    os::error ec = os::error::Connected;
	if (ec != os::error::Pending && ec != os::error::Connected) {
		ar->call_callback(ec, 0);
		ar->cancel();
		
	} else {
		ar->set_valid(true);
		if (ec == os::error::Connected) {
			ar->call_callback(ec, 0);
		}
	}

	return ec;
}