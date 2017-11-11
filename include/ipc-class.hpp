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

#pragma once
#include "ipc.hpp"
#include "ipc-function.hpp"
#include "ipc-value.hpp"
#include <map>
#include <memory>

namespace IPC {
	class Class {
		public:
		Class(std::string name);
		~Class();

		std::string GetName();

		size_t CountFunctions();
		bool RegisterFunction(std::shared_ptr<Function> func);
		bool UnregisterFunction(std::shared_ptr<Function> func);
		std::shared_ptr<Function> GetFunction(size_t idx);
		std::shared_ptr<Function> GetFunction(std::string name);
		std::shared_ptr<Function> GetFunction(std::string name, std::vector<Type> params);
		Value CallFunction(std::string name, std::vector<Value> args, std::shared_ptr<Base> caller);

		private:
		std::string m_name;
		std::map<std::string, std::shared_ptr<Function>> m_functions;
	};
}