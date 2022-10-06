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

#include "ipc-class.hpp"
#include <iostream>

ipc::collection::collection(const std::string &name) : m_name(name) {}

ipc::collection::~collection() {}

std::string ipc::collection::get_name()
{
	return m_name;
}

bool ipc::collection::register_function(std::shared_ptr<ipc::function> func)
{
	std::string fnId = func->get_name();
	if (m_functions.count(fnId) > 0)
		return false;

	m_functions.insert(std::make_pair(fnId, func));
	return true;
}

std::shared_ptr<ipc::function> ipc::collection::get_function(const std::string &name)
{
	for (auto fct : m_functions) {
		if (fct.first.compare(name.c_str()) == 0)
			return fct.second;
	}
	return nullptr;
}