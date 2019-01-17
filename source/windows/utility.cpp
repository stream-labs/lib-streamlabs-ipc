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

#include "utility.hpp"

os::error os::windows::utility::translate_error(DWORD error_code) {
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
	}

	return os::error::Error;
}
