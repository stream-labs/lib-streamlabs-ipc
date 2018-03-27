// A custom IPC solution to bypass electron IPC.
// Copyright(C) 2017 Streamlabs (General Workings Inc)
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.

#include "ipc-function.hpp"
#include "ipc.hpp"


ipc::function::function(std::string name, std::vector<ipc::type> params, call_handler_t ptr, void* data) {
	this->m_name = name;
	this->m_nameUnique = ipc::base::make_unique_id(m_name, params);
	this->m_params = params;
	this->m_callHandler.first = ptr;
	this->m_callHandler.second = data;
}

ipc::function::function(std::string name, std::vector<ipc::type> params, call_handler_t ptr)
	: function(name, params, ptr, nullptr) {}

ipc::function::function(std::string name, std::vector<ipc::type> params, void* data)
	: function(name, params, nullptr, data) {}

ipc::function::function(std::string name, std::vector<ipc::type> params)
	: function(name, params, nullptr, nullptr) {}

ipc::function::function(std::string name, call_handler_t ptr, void* data)
	: function(name, std::vector<ipc::type>(), ptr, data) {}

ipc::function::function(std::string name, call_handler_t ptr)
	: function(name, std::vector<ipc::type>(), ptr, nullptr) {}

ipc::function::function(std::string name, void* data)
	: function(name, std::vector<ipc::type>(), nullptr, data) {}

ipc::function::function(std::string name)
	: function(name, std::vector<ipc::type>(), nullptr, nullptr) {}

ipc::function::~function() {}

std::string ipc::function::get_name() {
	return m_name;
}

std::string ipc::function::get_unique_name() {
	return m_nameUnique;
}

size_t ipc::function::count_parameters() {
	return m_params.size();
}

ipc::type ipc::function::get_parameter_type(size_t index) {
	if (index > m_params.size())
		throw std::out_of_range("index is out of range");
	return m_params.at(index);
}

void ipc::function::set_call_handler(call_handler_t ptr, void* data) {
	m_callHandler.first = ptr;
	m_callHandler.second = data;
}

void ipc::function::call(const int64_t id, const std::vector<ipc::value>& args, std::vector<ipc::value>& rval) {
	if (m_callHandler.first) {
		return m_callHandler.first(m_callHandler.second, id, args, rval);
	}
}
