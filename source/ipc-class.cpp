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

IPC::Class::Class(std::string name) {
	this->m_name = name;
}

IPC::Class::~Class() {

}

std::string IPC::Class::GetName() {
	return m_name;
}

bool IPC::Class::RegisterFunction(std::shared_ptr<IPC::Function> func) {
	std::string fnId = func->GetUniqueName();
	if (m_functions.count(fnId) > 0)
		return false;

	m_functions.insert(std::make_pair(fnId, func));
	return true;
}

bool IPC::Class::UnregisterFunction(std::shared_ptr<IPC::Function> func) {
	std::string fnId = func->GetUniqueName();
	return m_functions.erase(fnId) != 0;
}

bool IPC::Class::HasFunction(const std::string& name) {
	return GetFunction(name) != nullptr;
}

bool IPC::Class::HasFunction(const std::string& name, const std::vector<IPC::Type>& params) {
	return GetFunction(name, params) != nullptr;
}

bool IPC::Class::HasFunction(const std::string& name, const std::vector<IPC::Value>& params) {
	return GetFunction(name, params) != nullptr;
}

size_t IPC::Class::CountFunctions() {
	return m_functions.size();
}

std::shared_ptr<IPC::Function> IPC::Class::GetFunction(const size_t& idx) {
	if (m_functions.size() <= idx)
		return nullptr;

	auto ptr = m_functions.begin();
	for (size_t n = 0; n < idx; n++, ptr++) {
	}
	return ptr->second;
}

std::shared_ptr<IPC::Function> IPC::Class::GetFunction(const std::string& name, const std::vector<IPC::Type>& params) {
	std::string fnId = IPC::Base::MakeFunctionUniqueId(name, params);
	if (m_functions.count(fnId) == 0)
		return nullptr;
	return m_functions[fnId];
}

std::shared_ptr<IPC::Function> IPC::Class::GetFunction(const std::string& name, const std::vector<IPC::Value>& params) {
	std::vector<IPC::Type> argts;
	argts.reserve(params.size());
	for (const auto& v : params) {
		argts.push_back(v.type);
	}
	return GetFunction(name, argts);
}

std::shared_ptr<IPC::Function> IPC::Class::GetFunction(const std::string& name) {
	return GetFunction(name, std::vector<IPC::Type>());
}
