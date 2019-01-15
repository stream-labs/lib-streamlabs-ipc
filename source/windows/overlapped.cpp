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

#include "overlapped.hpp"

os::windows::overlapped::overlapped() {
	ov_buf.resize(sizeof(void *) + sizeof(OVERLAPPED));
	*reinterpret_cast<void **>(&ov_buf[0]) = this;
	ov                                     = reinterpret_cast<OVERLAPPED *>(&ov_buf[sizeof(void *)]);

	// Initialize OVERLAPPED
	memset(ov, 0, sizeof(OVERLAPPED));
	ov->hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
}

os::windows::overlapped::~overlapped() {
	if (ov_buf.size()) {
		CloseHandle(ov->hEvent);
	}
}

OVERLAPPED *os::windows::overlapped::get_overlapped_pointer() {
	return ov;
}

os::windows::overlapped *os::windows::overlapped::get_pointer_from_overlapped(OVERLAPPED *ov) {
	return *reinterpret_cast<os::windows::overlapped**>(reinterpret_cast<char *>(ov) - sizeof(void*));
}

void os::windows::overlapped::signal() {
	SetEvent(ov->hEvent);
}

void *os::windows::overlapped::get_waitable() {
	return (void *)ov->hEvent;
}
