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

#ifndef OS_WAITABLE_HPP
#define OS_WAITABLE_HPP

#include <chrono>
#include <vector>
#include "error.hpp"

namespace os {
	class waitable {
		public:
		virtual void *get_waitable() = 0;

		inline os::error wait() {
			return os::waitable::wait(this);
		};
		inline os::error wait(std::chrono::nanoseconds timeout) {
			return os::waitable::wait(this, timeout);
		};

		static os::error wait(waitable *item);

		static os::error wait(waitable *item, std::chrono::nanoseconds timeout);

		static os::error wait_any(waitable **items, size_t items_count, size_t &signalled_index,
								  std::chrono::nanoseconds timeout);
	};
} // namespace os

#endif // OS_WAITABLE_HPP
