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

#include <codecvt>
#include <locale>
#include <string>
#include "semaphore.hpp"

os::windows::semaphore::semaphore(int32_t initial_count /*= 0*/, int32_t maximum_count /*= UINT32_MAX*/) {
	if (initial_count > maximum_count) {
		throw std::invalid_argument("initial_count can't be larger than maximum_count");
	} else if (maximum_count == 0) {
		throw std::invalid_argument("maximum_count can't be 0");
	}

	SetLastError(ERROR_SUCCESS);
	handle = CreateSemaphoreW(NULL, initial_count, maximum_count, NULL);
	if (!handle || (GetLastError() != ERROR_SUCCESS)) {
		std::vector<char> msg(2048);
		sprintf_s(msg.data(), msg.size(), "Semaphore creation failed with error code %lX.\0", GetLastError());
		throw std::runtime_error(msg.data());
	}
}

os::windows::semaphore::~semaphore() {
	if (handle) {
		CloseHandle(handle);
	}
}

os::error os::windows::semaphore::signal(uint32_t count /*= 1*/) {
	SetLastError(ERROR_SUCCESS);
	DWORD result = ReleaseSemaphore(handle, count, NULL);
	if (!result) {
		DWORD err = GetLastError();
		if (err == ERROR_TOO_MANY_POSTS) {
			return os::error::TooMuchData;
		}
		return os::error::Error;
	}
	return os::error::Success;
}

void *os::windows::semaphore::get_waitable() {
	return (void *)handle;
}
