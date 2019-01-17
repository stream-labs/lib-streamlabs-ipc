/* Copyright(C) 2018 Michael Fabian Dirks <info@xaymar.com>
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "async_request.hpp"
#include "utility.hpp"
#include <versionhelpers.h>

typedef void(WINAPI *AGRS)(HANDLE, LPOVERLAPPED, LPDWORD, DWORD, BOOL);

HINSTANCE kernel = LoadLibrary("kernel32.dll");
AGRS      getOverlappedResultEx = (AGRS)GetProcAddress(kernel, "GetOverlappedResultEx");

void os::windows::async_request::set_handle(HANDLE handle) {
	this->handle          = handle;
	this->valid           = false;
	this->callback_called = false;
}

void os::windows::async_request::set_valid(bool valid) {
	this->valid           = valid;
	this->callback_called = false;
}

void os::windows::async_request::completion_routine(DWORD dwErrorCode, DWORD dwBytesTransmitted, LPVOID ov) {
	os::windows::overlapped *ovp =
		os::windows::overlapped::get_pointer_from_overlapped(static_cast<LPOVERLAPPED>(ov));

	if (!ovp) {
		return;
	}
	ovp->signal();
}

void *os::windows::async_request::get_waitable() {
	return os::windows::overlapped::get_waitable();
}

os::windows::async_request::~async_request() {
	if (is_valid()) {
		cancel();
	}
}

bool os::windows::async_request::is_valid() {
	return this->valid;
}

void os::windows::async_request::invalidate() {
	valid           = false;
	callback_called = true;
}

bool os::windows::async_request::is_complete() {
	if (!is_valid()) {
		return false;
	}

	return HasOverlappedIoCompleted(this->get_overlapped_pointer());
}

bool os::windows::async_request::cancel() {
	if (!is_valid()) {
		return false;
	}

	if (!is_complete()) {
		return CancelIoEx(handle, this->get_overlapped_pointer());
	}
	return true;
}

void os::windows::async_request::call_callback() {
	DWORD       bytes = 0;
	OVERLAPPED *ov    = get_overlapped_pointer();
	os::error   error = os::error::Success;

	SetLastError(ERROR_SUCCESS);

	if (getOverlappedResultEx) {
		getOverlappedResultEx(handle, ov, &bytes, FALSE, TRUE);
	} else {
		GetOverlappedResult(handle, ov, &bytes, FALSE);
	}

	error = os::windows::utility::translate_error(GetLastError());

	call_callback(error, (size_t)bytes);
}

void os::windows::async_request::call_callback(os::error ec, size_t length) {
	if (system.callback && !system.callback_called) {
		system.callback_called = true;
		system.callback(ec, length);
	}
	if (callback && !callback_called) {
		callback_called = true;
		callback(ec, length);
	}
}
