/******************************************************************************
    Copyright (C) 2016-2019 by Streamlabs (General Workings Inc)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#include "utility.hpp"
#include "ipc.hpp"
#include <tlhelp32.h>

namespace
{
	DWORD get_parent_process_id()
	{
		DWORD        parent_process_id  = (DWORD)-1;
		const DWORD  current_process_id = GetCurrentProcessId();
		const HANDLE snapshot_handle    = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snapshot_handle == INVALID_HANDLE_VALUE) {
			return parent_process_id;
		}

		PROCESSENTRY32 process_entry;
		ZeroMemory(&process_entry, sizeof(process_entry));
		process_entry.dwSize = sizeof(process_entry);

		if (Process32First(snapshot_handle, &process_entry)) {
			do {
				if (process_entry.th32ProcessID == current_process_id) {
					parent_process_id = process_entry.th32ParentProcessID;
					break;
				}
			} while (Process32Next(snapshot_handle, &process_entry));
		}

		CloseHandle(snapshot_handle);
		return parent_process_id;
	}
} // namespace

os::error os::windows::utility::translate_error(DWORD error_code) {
	static DWORD prev_error_code = 0;
	if (prev_error_code != error_code) {
		ipc::log("IPC write to pipe failed with code %d", error_code);
		prev_error_code = error_code;
	}

	switch (error_code) {
	case ERROR_SUCCESS:
		return os::error::Success;
	case ERROR_IO_PENDING:
		// !FIXME! Should this have its own error code?
		return os::error::Pending;
	case ERROR_BROKEN_PIPE:
		return os::error::Disconnected;
	case ERROR_MORE_DATA:
		return os::error::MoreData;
	case ERROR_PIPE_CONNECTED:
		return os::error::Connected;
	case ERROR_TIMEOUT:
		return os::error::TimedOut;
	case ERROR_TOO_MANY_POSTS:
		// !FIXME! Should this have its own error code?
		return os::error::TooMuchData;
	case ERROR_NO_DATA:
		return os::error::Disconnected;		
	}

	return os::error::Error;
}

DWORD os::windows::utility::get_parent_process_exit_code()
{
	DWORD        exit_code             = (DWORD)-1;
	const DWORD  parent_process_id     = get_parent_process_id();
	const HANDLE parent_process_handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parent_process_id);
	if (parent_process_handle != INVALID_HANDLE_VALUE) {
		if (!GetExitCodeProcess(parent_process_handle, &exit_code)) {
			ipc::log("get_parent_process_exit_code failed with GetLastError=%d", GetLastError());
		}
	}

	CloseHandle(parent_process_handle);
	return exit_code;

}

