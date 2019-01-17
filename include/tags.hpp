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
