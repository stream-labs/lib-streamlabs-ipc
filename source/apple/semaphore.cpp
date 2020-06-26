#include "semaphore.hpp"

os::apple::semaphore::semaphore(int32_t initial_count /*= 0*/, int32_t maximum_count /*= UINT32_MAX*/) {
	if (initial_count > maximum_count) {
		throw std::invalid_argument("initial_count can't be larger than maximum_count");
	} else if (maximum_count == 0) {
		throw std::invalid_argument("maximum_count can't be 0");
	}

	sem = sem_open("semaphore", O_CREAT | O_EXCL);
}

os::apple::semaphore::~semaphore() {
    // if (sem_close(sem) < 0)
    //     throw "Could remove the semaphore.";
}

os::error os::apple::semaphore::signal(uint32_t count /*= 1*/) {
	if (sem_post(sem) < 0)
        return os::error::Error;

	return os::error::Success;
}

void *os::apple::semaphore::get_waitable() {
    return (void *)sem;
}
