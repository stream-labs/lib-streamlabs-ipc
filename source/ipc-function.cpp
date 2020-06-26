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

#include "ipc-function.hpp"
#include "ipc.hpp"
#include <iostream>

ipc::function::function(const std::string & name, const std::vector<ipc::type>& params, call_handler_t ptr, void* data) {
	this->m_name = name;
	this->m_nameUnique = ipc::base::make_unique_id(m_name, params);
	this->m_params = params;
	this->m_callHandler.first = ptr;
	this->m_callHandler.second = data;
}

ipc::function::function(const std::string & name, const std::vector<ipc::type>& params, call_handler_t ptr)
	: function(name, params, ptr, nullptr) {}

ipc::function::function(const std::string & name, const std::vector<ipc::type>& params, void* data)
	: function(name, params, nullptr, data) {}

ipc::function::function(const std::string & name, const std::vector<ipc::type>& params)
	: function(name, params, nullptr, nullptr) {}

ipc::function::function(const std::string & name, call_handler_t ptr, void* data)
	: function(name, std::vector<ipc::type>(), ptr, data) {}

ipc::function::function(const std::string & name, call_handler_t ptr)
	: function(name, std::vector<ipc::type>(), ptr, nullptr) {}

ipc::function::function(const std::string & name, void* data)
	: function(name, std::vector<ipc::type>(), nullptr, data) {}

ipc::function::function(const std::string & name)
	: function(name, std::vector<ipc::type>(), nullptr, nullptr) {}

ipc::function::~function() {}

std::string ipc::function::get_unique_name() {
	return m_nameUnique;
}

void ipc::function::call(const int64_t id, const std::vector<ipc::value>& args, std::vector<ipc::value>& rval) {
	if (m_callHandler.first) {
		return m_callHandler.first(m_callHandler.second, id, args, rval);
	}
}
