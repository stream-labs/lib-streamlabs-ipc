#include "async_request.hpp"

void os::apple::async_request::set_sem(sem_t *sem) {
	this->sem             = sem;
	this->valid           = false;
	this->callback_called = false;
}

void os::apple::async_request::set_valid(bool valid) {
	this->valid           = valid;
	this->callback_called = false;
}

void *os::apple::async_request::get_waitable() {
	// return os::windows::overlapped::get_waitable();
	return NULL;
}

os::apple::async_request::~async_request() {
	if (os::apple::async_request::is_valid()) {
		os::apple::async_request::cancel();
	}
}

bool os::apple::async_request::is_valid() {
	return this->valid;
}

void os::apple::async_request::invalidate() {
	valid           = false;
	callback_called = true;
}

bool os::apple::async_request::is_complete() {
	if (!is_valid()) {
		return false;
	}
    return true;
	// return HasOverlappedIoCompleted(this->get_overlapped_pointer());
}

bool os::apple::async_request::cancel() {
	if (!is_valid()) {
		return false;
	}

	if (!is_complete()) {
		// return CancelIoEx(handle, this->get_overlapped_pointer());
	}
	return true;
}

void os::apple::async_request::call_callback() {
	// DWORD       bytes = 0;
	// OVERLAPPED *ov    = get_overlapped_pointer();
	// os::error   error = os::error::Success;

	// SetLastError(ERROR_SUCCESS);

	// if (getOverlappedResultEx) {
	// 	getOverlappedResultEx(handle, ov, &bytes, FALSE, TRUE);
	// } else {
	// 	GetOverlappedResult(handle, ov, &bytes, FALSE);
	// }

	// error = os::windows::utility::translate_error(GetLastError());

	// call_callback(error, (size_t)bytes);
}

void os::apple::async_request::call_callback(os::error ec, size_t length) {
	if (system.callback && !system.callback_called) {
		system.callback_called = true;
		auto runned_callback = system.callback;
		runned_callback(ec, length);
	}
	if (callback && !callback_called) {
		callback_called = true;
		auto runned_callback = callback;
		runned_callback(ec, length);
	}
}
