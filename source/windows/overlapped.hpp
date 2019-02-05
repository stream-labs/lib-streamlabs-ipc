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

#ifndef OS_WINDOWS_OVERLAPPED_HPP
#define OS_WINDOWS_OVERLAPPED_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "waitable.hpp"

namespace os {
	namespace windows {
		class overlapped {
			OVERLAPPED *      ov;
			std::vector<char> ov_buf;

			public:
			overlapped();
			~overlapped();

			OVERLAPPED *get_overlapped_pointer();

			static overlapped *get_pointer_from_overlapped(OVERLAPPED * ov);

			void signal();

			virtual void *get_waitable();
		};
	} // namespace windows
} // namespace os

#endif // OS_WINDOWS_OVERLAPPED_HPP
