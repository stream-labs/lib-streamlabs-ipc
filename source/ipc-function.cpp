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


IPC::Function::Function(std::string name, std::vector<Type> params, CallHandler_t ptr, void* data) {
	this->m_name = name;
	this->m_params = params;
	this->m_callHandler.first = ptr;
	this->m_callHandler.second = data;
}

IPC::Function::Function(std::string name, CallHandler_t ptr, void* data) : Function(name, std::vector<Type>(), ptr, data) {}

IPC::Function::Function(std::string name, std::vector<Type> params) : Function(name, params, nullptr, nullptr) {}

IPC::Function::Function(std::string name) : Function(name, std::vector<Type>(), nullptr, nullptr) {}

IPC::Function::~Function() {}

std::string IPC::Function::GetName() {
	return m_name;
}

std::string IPC::Function::GetUniqueName() {
	return IPC::Base::MakeFunctionUniqueId(m_name, m_params);
}

size_t IPC::Function::CountParameters() {
	return m_params.size();
}

IPC::Type IPC::Function::GetParameterType(size_t index) {
	return m_params.at(index);
}

void IPC::Function::SetCallHandler(CallHandler_t ptr, void* data) {
	m_callHandler.first = ptr;
	m_callHandler.second = data;
}

IPC::Value IPC::Function::Call(int64_t id, std::vector<IPC::Value> args) {
	if (m_callHandler.first)
		return m_callHandler.first(id, m_callHandler.second, args);
	return IPC::Value();
}
