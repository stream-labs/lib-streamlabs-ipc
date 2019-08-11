#include "async_op.hpp"
#include "waitable.hpp"
#include <semaphore.h>

os::error os::waitable::wait(waitable *item, std::chrono::nanoseconds timeout) {
	sem_t *sem           = (sem_t*)item->get_waitable();
	int64_t ms_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();

wait_retry:
	auto start = std::chrono::high_resolution_clock::now();
	if (ms_timeout < 0) {
		ms_timeout = 0;
	}

    int result = sem_trywait(sem);
	if (result == 0) {
		os::async_op *aop = dynamic_cast<os::async_op *>(item);
		if (aop) {
			aop->call_callback();
		}
		return os::error::Success;
	} else {
		ms_timeout -=
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start)
				.count();
		goto wait_retry;
	}
	return os::error::Error;
}

os::error os::waitable::wait(waitable *item) {
    sem_t *sem = (sem_t*)item->get_waitable();

    if (sem_wait(sem) < 0)
        return os::error::Error;

    return os::error::Success;
}

os::error os::waitable::wait_any(waitable **items, size_t items_count, size_t &signalled_index,
								 std::chrono::nanoseconds timeout) {
// 	if (items == nullptr) {
// 		throw std::invalid_argument("'items' can't be nullptr.");
// 	} else if (items_count > MAXIMUM_WAIT_OBJECTS) {
// 		throw std::invalid_argument("Too many items to wait for.");
// 	}

// 	// Need to create a sequential array of HANDLEs here.
// 	size_t              valid_handles = 0;
// 	std::vector<HANDLE> handles(items_count);
// 	std::vector<size_t> idxToTrueIdx(items_count);
// 	for (size_t idx = 0, eidx = items_count; idx < eidx; idx++) {
// 		waitable *obj = items[idx];
// 		if (obj) {
// 			handles[valid_handles] = (HANDLE)obj->get_waitable();
// 			idxToTrueIdx[idx]      = valid_handles;
// 			valid_handles++;
// 		}
// 	}

// 	int64_t ms_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();

// wait_any_retry:
// 	auto start = std::chrono::high_resolution_clock::now();
// 	if (ms_timeout < 0) {
// 		ms_timeout = 0;
// 	}

// 	DWORD result = WaitForMultipleObjectsEx(DWORD(valid_handles), handles.data(), FALSE, DWORD(ms_timeout), TRUE);
// 	if ((result >= WAIT_OBJECT_0) && result < (WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS)) {
// 		signalled_index = idxToTrueIdx[result - WAIT_OBJECT_0];

// 		os::async_op *aop = dynamic_cast<os::async_op *>(items[signalled_index]);
// 		if (aop) {
// 			aop->call_callback();
// 		}

// 		return os::error::Success;
// 	} else if (result == WAIT_TIMEOUT) {
// 		signalled_index = -1;
// 		return os::error::TimedOut;
// 	} else if ((result >= WAIT_ABANDONED_0) && result < (WAIT_ABANDONED_0 + MAXIMUM_WAIT_OBJECTS)) {
// 		signalled_index = idxToTrueIdx[result - WAIT_ABANDONED_0];
// 		return os::error::Disconnected; // Disconnected Semaphore from original Owner
// 	} else if (result == WAIT_IO_COMPLETION) {
// 		ms_timeout -=
// 			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start)
// 				.count();
// 		goto wait_any_retry;
// 	}
	return os::error::Error;
}