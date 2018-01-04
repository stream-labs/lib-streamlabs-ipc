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


IPC::Function::Function(std::string name, std::vector<IPC::Type> params, CallHandler_t ptr, void* data) {
	this->m_name = name;
	this->m_nameUnique = IPC::Base::MakeFunctionUniqueId(m_name, params);
	this->m_params = params;
	this->m_callHandler.first = ptr;
	this->m_callHandler.second = data;
}

IPC::Function::Function(std::string name, std::vector<IPC::Type> params, CallHandler_t ptr)
	: Function(name, params, ptr, nullptr) {}

IPC::Function::Function(std::string name, std::vector<IPC::Type> params, void* data)
	: Function(name, params, nullptr, data) {}

IPC::Function::Function(std::string name, std::vector<IPC::Type> params)
	: Function(name, params, nullptr, nullptr) {}

IPC::Function::Function(std::string name, CallHandler_t ptr, void* data)
	: Function(name, std::vector<IPC::Type>(), ptr, data) {}

IPC::Function::Function(std::string name, CallHandler_t ptr)
	: Function(name, std::vector<IPC::Type>(), ptr, nullptr) {}

IPC::Function::Function(std::string name, void* data)
	: Function(name, std::vector<IPC::Type>(), nullptr, data) {}

IPC::Function::Function(std::string name)
	: Function(name, std::vector<IPC::Type>(), nullptr, nullptr) {}

IPC::Function::~Function() {}

std::string IPC::Function::GetName() {
	return m_name;
}

std::string IPC::Function::GetUniqueName() {
	return m_nameUnique;
}

size_t IPC::Function::CountParameters() {
	return m_params.size();
}

IPC::Type IPC::Function::GetParameterType(size_t index) {
	if (index > m_params.size())
		throw std::out_of_range("index is out of range");
	return m_params.at(index);
}

void IPC::Function::SetCallHandler(CallHandler_t ptr, void* data) {
	m_callHandler.first = ptr;
	m_callHandler.second = data;
}

void IPC::Function::Call(const int64_t id, const std::vector<IPC::Value>& args, std::vector<IPC::Value>& rval) {
	if (m_callHandler.first) {
		return m_callHandler.first(m_callHandler.second, id, args, rval);
	}
}
