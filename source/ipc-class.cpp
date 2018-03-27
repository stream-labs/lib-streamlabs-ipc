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

#include "ipc-class.hpp"

ipc::collection::collection(std::string name) {
	this->m_name = name;
}

ipc::collection::~collection() {

}

std::string ipc::collection::get_name() {
	return m_name;
}

bool ipc::collection::register_function(std::shared_ptr<ipc::function> func) {
	std::string fnId = func->get_unique_name();
	if (m_functions.count(fnId) > 0)
		return false;

	m_functions.insert(std::make_pair(fnId, func));
	return true;
}

bool ipc::collection::unregister_function(std::shared_ptr<ipc::function> func) {
	std::string fnId = func->get_unique_name();
	return m_functions.erase(fnId) != 0;
}

bool ipc::collection::has_function(const std::string& name) {
	return get_function(name) != nullptr;
}

bool ipc::collection::has_function(const std::string& name, const std::vector<ipc::type>& params) {
	return get_function(name, params) != nullptr;
}

bool ipc::collection::has_function(const std::string& name, const std::vector<ipc::value>& params) {
	return get_function(name, params) != nullptr;
}

size_t ipc::collection::count_functions() {
	return m_functions.size();
}

std::shared_ptr<ipc::function> ipc::collection::get_function(const size_t& idx) {
	if (m_functions.size() <= idx)
		return nullptr;

	auto ptr = m_functions.begin();
	for (size_t n = 0; n < idx; n++, ptr++) {
	}
	return ptr->second;
}

std::shared_ptr<ipc::function> ipc::collection::get_function(const std::string& name, const std::vector<ipc::type>& params) {
	std::string fnId = ipc::base::make_unique_id(name, params);
	if (m_functions.count(fnId) == 0)
		return nullptr;
	return m_functions[fnId];
}

std::shared_ptr<ipc::function> ipc::collection::get_function(const std::string& name, const std::vector<ipc::value>& params) {
	std::vector<ipc::type> argts;
	argts.reserve(params.size());
	for (const auto& v : params) {
		argts.push_back(v.type);
	}
	return get_function(name, argts);
}

std::shared_ptr<ipc::function> ipc::collection::get_function(const std::string& name) {
	return get_function(name, std::vector<ipc::type>());
}
