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

#ifndef OS_ERROR_HPP
#define OS_ERROR_HPP

#include <cinttypes>
#include <iostream>
#include <string>

namespace os {
	enum class error {
		// Unknown
		Unknown = -1,

		// Everything went right
		Ok      = 0,
		Success = Ok,

		// Generic Error
		Error,

		// The buffer you passed is invalid.
		InvalidBuffer,

		// Buffer too small.
		BufferTooSmall,

		// Buffer too large.
		BufferTooLarge,

		// read() has more data available.
		MoreData,

		// Timed out
		TimedOut,

		// Disconnected
		Disconnected,

		// Too Much Data
		TooMuchData,

		// Connected (opposite of Disconnected, if you didn't know)
		Connected,

		// Pending IO or similar.
		Pending,

		// Buffer Overflow
		BufferOverflow,
	};
}

#endif // OS_ERROR_HPP
