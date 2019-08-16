#include "named-pipe.hpp"

os::apple::named_pipe::named_pipe(os::create_only_t, const std::string name)
{
    int ret = 0;

    ret = remove(name.c_str());
    std::cout << "Creating pipe" << std::endl;
    ret = mkfifo(name.c_str(), S_IRUSR | S_IWUSR);
    std::cout << "Opening pipe" << std::endl;
    file_descriptor = open(name.c_str(), O_RDONLY);

    if (ret < 0)
        std::cout << "Could not create named pipe. " << strerror(errno) << std::endl;
    
    this->name = name;
    created = true;
    
//    close(file_descriptor);
    std::cout << "pipe created" << std::endl;
}

os::apple::named_pipe::named_pipe(os::open_only_t, const std::string name)
{
    std::cout << "Client openning pipe" << std::endl;
    file_descriptor = open(name.c_str(), O_WRONLY);
    
    if (file_descriptor < 0) {
        std::cout << "Could not open pipe" << std::endl;
        throw "Couldn't open pipe.";
    }

    this->name = name;
    connected = true;

//    close(file_descriptor);
    std::cout << "pipe opened" << std::endl;
}

os::apple::named_pipe::~named_pipe() {
    remove(name.c_str());
}

void read_cb (int sig) {
    std::cout << "read cb called!" << std::endl;
}

void write_cb (int sig) {
    std::cout << "write cb called!" << std::endl;
}

uint32_t os::apple::named_pipe::read(char *buffer, size_t buffer_length, std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb)
{
//    std::cout << "Reading - start: " << buffer_length << std::endl;
    ssize_t ret = 0;

    // Set callback
    std::shared_ptr<os::apple::async_request> ar = std::static_pointer_cast<os::apple::async_request>(op);
    if(!ar) {
        ar = std::make_shared<os::apple::async_request>();
        op = std::static_pointer_cast<os::async_op>(ar);
    }
    ar->set_callback(cb);
    ar->set_sem(NULL);

//    file_descriptor = open(name.c_str(), O_RDONLY);// | O_NDELAY);
    if (file_descriptor < 0) {
        std::cout << "Could not open " << strerror(errno) << std::endl;
//        close(file_descriptor);
        return (uint32_t) os::error::Pending;
    }

    struct aiocb *aio = NULL;
    aio = (struct aiocb*)malloc(sizeof(struct aiocb));
    memset(aio, 0x0, sizeof(struct aiocb));
    
    aio->aio_fildes = file_descriptor;
    aio->aio_buf = buffer;
    aio->aio_nbytes = buffer_length;
    aio->aio_reqprio = 0;
    
    const int signalNumber = SIGIO;
    aio->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    aio->aio_sigevent.sigev_signo = signalNumber;
    aio->aio_sigevent.sigev_value.sival_ptr = &aio;

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = read_cb;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(signalNumber, &sa, NULL) == -1) {
        std::cout << "Sigaction failed" << std::endl;
    }
    
    int err = aio_read(aio);
    
    if (err != 0) {
        std::cout << "Invalid read " << strerror(errno) << std::endl;
        ar->set_valid(true);
//        close(file_descriptor);
        return (uint32_t) os::error::Pending;
    }

    while (aio_error (aio) == EINPROGRESS);
    
    ar->call_callback(os::error::Success, buffer_length);
    ar->cancel();

    return (uint32_t) os::error::Success;
}

uint32_t os::apple::named_pipe::write(const char *buffer, size_t buffer_length)
{
    std::cout << "write - start" << std::endl;
    if (file_descriptor < 0) {
        std::cout << "Could not open " << strerror(errno) << std::endl;
        close(file_descriptor);
        return (uint32_t) os::error::Pending;
    }
    
    struct aiocb *aio = NULL;
    aio = (struct aiocb*)malloc(sizeof(struct aiocb));
    memset(aio, 0x0, sizeof(struct aiocb));
    
    aio->aio_fildes = file_descriptor;
    aio->aio_buf = (void *) buffer;
    aio->aio_nbytes = buffer_length;
    aio->aio_reqprio = 0;
    
    const int signalNumber = SIGIO;
    aio->aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    aio->aio_sigevent.sigev_signo = signalNumber;
    aio->aio_sigevent.sigev_value.sival_ptr = &aio;
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = write_cb;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(signalNumber, &sa, NULL) == -1) {
        std::cout << "Sigaction failed" << std::endl;
    }
    
    int err = aio_write(aio);
    
    if (err) {
        std::cout << "Invalid write " << strerror(errno) << std::endl;
//        ar->set_valid(true);
        close(file_descriptor);
        return (uint32_t) os::error::Error;
    }
    
    err = aio_error(aio);
    
    while (aio_error (aio) == EINPROGRESS);
    
//    close(file_descriptor);
    std::cout << "write - end" << std::endl;
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
