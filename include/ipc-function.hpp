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
#include "ipc-value.hpp"
#include <memory>

namespace IPC {
	typedef Value(*CallHandler_t)(int64_t id, void* data, std::vector<Value>);

	class Function {
		public:
		Function(std::string name);
		Function(std::string name, std::vector<Type> params);
		Function(std::string name, CallHandler_t ptr, void* data);
		Function(std::string name, std::vector<Type> params, CallHandler_t ptr, void* data);
		virtual ~Function();

		/** Retrieve the original name of this function.
		 * 
		 */
		std::string GetName();

		/** Get the unique name for this function used to identify it.
		 * 
		 * This produces a similar result to what native compilers do to allow
		 * overloading of the same name but with different functions.
		 */
		std::string GetUniqueName();

		/** Count the number of parameters.
		 * 
		 */
		size_t CountParameters();

		/** Retrieve parameter by index.
		 *
		 */
		Type GetParameterType(size_t index);

		/** Call handling
		 * 
		 */
		void SetCallHandler(CallHandler_t ptr, void* data);
		
		Value Call(int64_t id, std::vector<Value> args);
		
		private:
		std::string m_name;
		std::vector<Type> m_params;

		std::pair<CallHandler_t, void*> m_callHandler;
	};
}