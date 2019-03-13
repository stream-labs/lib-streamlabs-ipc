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

#include "async_op.hpp"

void os::async_op::set_callback(async_op_cb_t u_callback) {
	if (is_valid()) {
		if (!is_complete()) {
			throw std::runtime_error("Can't change callback for a valid but incomplete operation.");
		}
	}

	callback = u_callback;
}

void os::async_op::set_system_callback(async_op_cb_t u_callback) {
	if (is_valid()) {
		if (!is_complete()) {
			throw std::runtime_error("Can't change callback for a valid but incomplete operation.");
		}
	}

	system.callback = u_callback;
}
