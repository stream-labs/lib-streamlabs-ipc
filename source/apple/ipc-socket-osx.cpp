#include "ipc-socket-osx.hpp"

#include <errno.h>

std::unique_ptr<os::apple::socket_osx> os::apple::socket_osx::create(os::create_only_t, const std::string &name) {
    return std::make_unique<os::apple::socket_osx>(os::create_only, name);
}

std::unique_ptr<os::apple::socket_osx> os::apple::socket_osx::create(os::open_only_t, const std::string &name) {
    return std::make_unique<os::apple::socket_osx>(os::open_only, name);
}

os::apple::socket_osx::socket_osx(os::create_only_t, const std::string name)
{
    this->name_req = name + "-req";
    this->name_rep = name + "-rep";

    remove(name_req.c_str());
    if (mkfifo(name_req.c_str(), S_IRUSR | S_IWUSR) < 0)
        throw std::exception(
                (const std::exception&)"Could not create request pipe");

    remove(name_rep.c_str());
    if (mkfifo(name_rep.c_str(), S_IRUSR | S_IWUSR) < 0)
        throw std::exception(
                (const std::exception&)"Could not create reply pipe");

    file_req = open(name_req.c_str(), O_RDONLY | O_NONBLOCK);
    if (file_req < 0)
        throw std::exception(
            (const std::exception&)"Could not open reader request pipe");

    file_rep = open(name_rep.c_str(), O_WRONLY | O_DSYNC);
    if (file_rep < 0)
        throw std::exception(
            (const std::exception&)"Could not open write reply pipe");  

    created = true;
}

os::apple::socket_osx::socket_osx(os::open_only_t, const std::string name)
{
    this->name_req = name + "-req";
    this->name_rep = name + "-rep";

    file_rep = open(name_rep.c_str(), O_RDONLY | O_NONBLOCK);
    if (file_rep < 0)
        throw std::exception(
            (const std::exception&)"Could not open reader reply pipe");

    file_req = open(name_req.c_str(), O_WRONLY | O_DSYNC);
    if (file_req < 0)
        throw std::exception(
            (const std::exception&)"Could not open write request pipe");

    connected = true;
}

os::apple::socket_osx::~socket_osx() {
    close(file_req);
    remove(name_req.c_str());
    close(file_rep);
    remove(name_rep.c_str());
}

uint32_t os::apple::socket_osx::read(char *buffer, size_t buffer_length, bool is_blocking, SocketType t)
{
    os::error err = os::error::Error;
    int ret = 0;
    int sizeChunks = 8*1024; // 8KB
    int offset = 0;
    int file_descriptor = t == REQUEST ? file_req : file_rep;
    std::string typePipe = t == REQUEST ? "request" : "reply";

    std::cout << "read " << typePipe.c_str() << std::endl;
    if (file_descriptor < 0)
        goto end;

    while (ret <= 0) {
        ret = ::read(file_descriptor, buffer, buffer_length);
        std::cout << "Size read: " << ret << std::endl;
        while (ret == sizeChunks) {
            std::cout << "chunk data - 0" << std::endl;
            offset += sizeChunks;
            std::vector<char> new_chunks;
            std::cout << "chunk data - 1" << std::endl;
            new_chunks.resize(sizeChunks);
            std::cout << "chunk data - 2" << std::endl;
            ret = ::read(file_descriptor, new_chunks.data(), new_chunks.size());
            std::cout << "chunk data - 3" << std::endl;
            std::cout << "ret " << ret << std::endl;
            std::cout << "new_chunks.size() " << new_chunks.size() << std::endl;
            int errnum;
            if (ret > 0)
                ::memcpy(&buffer[offset], new_chunks.data(), ret);
            else {
                errnum = errno;
                std::cout << "Error: " << strerror(errnum) << std::endl;
                ret = offset;
            }
            std::cout << "chunk data - end" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    err = os::error::Success;

    std::cout << "read " << typePipe.c_str() << " - end" << std::endl;
end:
    return (uint32_t) err;
}

uint32_t os::apple::socket_osx::write(const char *buffer, size_t buffer_length, SocketType t)
{
    os::error err = os::error::Error;
    int ret = 0;
    int file_descriptor = t == REQUEST ? file_req : file_rep;
    std::string typePipe = t == REQUEST ? "request" : "reply";

    if (file_descriptor < 0)
        goto end;

    ret = ::write(file_descriptor, buffer, buffer_length);
    if (ret < 0)
        goto end;

    err = os::error::Success;
end:
    return (uint32_t) err;
}

bool os::apple::socket_osx::is_created() {
	return created;
}

bool os::apple::socket_osx::is_connected() {
    return connected;
}

void os::apple::socket_osx::set_connected(bool is_connected) {
    connected = is_connected;
}

void os::apple::socket_osx::handle_accept_callback(os::error code, size_t length) {
	if (code == os::error::Connected || code == os::error::Success) {
		set_connected(true);
	} else {
		set_connected(false);
	}
}

os::error os::apple::socket_osx::accept(std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb) {
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
		std::bind(&os::apple::socket_osx::handle_accept_callback, this, std::placeholders::_1, std::placeholders::_2));
    ar->set_sem(NULL);
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
