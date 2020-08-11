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

    fd_write = -1;
    fd_read_b = -1;
    fd_read_nb = -1;
    created = true;
}

os::apple::socket_osx::socket_osx(os::open_only_t, const std::string name)
{
    this->name_req = name + "-req";
    this->name_rep = name + "-rep";

    fd_write = -1;
    fd_read_b = -1;
    fd_read_nb = -1;
    connected = true;
}

os::apple::socket_osx::~socket_osx() {}

void os::apple::socket_osx::clean_file_descriptors() {
    if (fd_write > 0)
        close(fd_write);

    if (fd_read_b > 0)
        close(fd_read_b);

    if (fd_read_nb > 0)
        close(fd_read_nb);

    remove(name_req.c_str());
    remove(name_rep.c_str());
}

uint32_t os::apple::socket_osx::read(char *buffer, size_t buffer_length, bool is_blocking, SocketType t)
{
    os::error err = os::error::Error;
    int ret = 0;
    int sizeChunks = 8*1024; // 8KB
    int offset = 0;
    int file_descriptor = -1;
    std::string typePipe = t == REQUEST ? "server" : "client";

    if (is_blocking) {
        fd_read_b = open(t == REQUEST ? name_req.c_str() : name_rep.c_str(), O_RDWR);
        if (fd_read_b < 0) {
            goto end;
        }
    }
    else {
        fd_read_nb = open(t == REQUEST ? name_req.c_str() : name_rep.c_str(), O_RDWR | O_NONBLOCK);
        if (fd_read_nb < 0) {
            goto end;
        }
    }

    file_descriptor = is_blocking ? fd_read_b : fd_read_nb;

    while (ret <= 0) {
        ret = ::read(file_descriptor, buffer, buffer_length);
        if (ret > 0)
            offset += ret;

        while (ret != 0 && (ret == (sizeChunks - 8) || ret == sizeChunks || ret < 0)) {
            std::vector<char> new_chunks;
            new_chunks.resize(sizeChunks);
            ret = ::read(file_descriptor, new_chunks.data(), new_chunks.size());

            if (ret > 0) {
                ::memcpy(buffer + offset, new_chunks.data(), ret);
                offset += ret;
            }
        }
    }
    if (ret < 0) {
        goto end;
    }
    err = os::error::Success;

    if (!is_blocking) {
        if (fd_read_b > 0)
            close(fd_read_b);
        if (fd_read_nb > 0)
            close(fd_read_nb);
    }

end:
    return (uint32_t) err;
}

uint32_t os::apple::socket_osx::write(const char *buffer, size_t buffer_length, SocketType t)
{
    os::error err = os::error::Error;
    int ret = 0;
    int sizeChunks = 8*1024; // 8KB
    std::string typePipe = t == REQUEST ? "client" : "server";

    if (fd_write > 0)
        close(fd_write);

    fd_write = open(t == REQUEST ? name_req.c_str() : name_rep.c_str(), O_WRONLY | O_DSYNC);
    if (fd_write < 0) {
        goto end;
    }

    if (buffer_length <= sizeChunks) {
        ret = ::write(fd_write, buffer, buffer_length);
    } else {
        int size_wrote = 0;
        while (size_wrote < buffer_length) {
            if (fd_write < 0)
                fd_write = open(t == REQUEST ? name_req.c_str() : name_rep.c_str(), O_WRONLY | O_DSYNC);

            bool end = (buffer_length - size_wrote) <= sizeChunks;
            int size_to_write = end ? buffer_length - size_wrote : sizeChunks;
            ret = ::write(fd_write, buffer + size_wrote, size_to_write);
            if (ret < 0)
                break;
            size_wrote += size_to_write;
            close(fd_write);
            fd_write = -1;
        }
    }

    if (ret < 0) {
        goto end;
    }
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
