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

#ifndef OS_TAGS_HPP
#define OS_TAGS_HPP

namespace os {
	struct create_only_t {};

	static const create_only_t create_only = create_only_t();

	struct create_or_open_t {};

	static const create_or_open_t create_or_open = create_or_open_t();

	struct open_only_t {};

	static const open_only_t open_only = open_only_t();

} // namespace os

#endif // OS_TAGS_HPP
