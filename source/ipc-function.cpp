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

IPC::Function::Function(std::string name, std::vector<Type> params) {
	this->m_name = name;
	this->m_params = params;
}

IPC::Function::Function(std::string name) : Function(name, std::vector<Type>()) {}

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

}

void IPC::Function::Call(std::shared_ptr<IPC::Base> caller, std::vector<IPC::Value> args) {

}

IPC::Value IPC::Function::CallWithValue(std::shared_ptr<Base> caller, std::vector<IPC::Value> args) {
	return IPC::Value();
}

IPC::Value IPC::Function::OnCall(std::vector<IPC::Value> args, std::shared_ptr<IPC::Base> caller) {
	return IPC::Value();
}
//
//void IPC::Function::ClearCallHandlers() {
//	m_callHandlers.clear();
//}
//
//bool IPC::Function::AddCallHandler(CallHandler_t ptr, void* data) {
//	auto sp = std::make_pair(ptr, data);
//	if (m_callHandlers.count(sp))
//		return false;
//	m_callHandlers.insert(sp);
//	return true;
//}
//
//bool IPC::Function::RemoveCallHandler(CallHandler_t ptr, void* data) {
//	auto sp = std::make_pair(ptr, data);
//	if (m_callHandlers.count(sp) == 0)
//		return false;
//	m_callHandlers.erase(sp);
//	return true;
//}
//
//Value IPC::Function::Call(std::vector<Value> args, std::shared_ptr<Base> caller) {
//
//}
