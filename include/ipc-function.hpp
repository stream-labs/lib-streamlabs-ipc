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
	typedef void(*CallHandler_t)(void* data, const int64_t id, const std::vector<IPC::Value>& args, std::vector<IPC::Value>& rval);

	class Function {
		public:
		Function(std::string name, std::vector<IPC::Type> params, CallHandler_t ptr, void* data);
		Function(std::string name, std::vector<IPC::Type> params, CallHandler_t ptr);
		Function(std::string name, std::vector<IPC::Type> params, void* data);
		Function(std::string name, std::vector<IPC::Type> params);
		Function(std::string name, CallHandler_t ptr, void* data);
		Function(std::string name, CallHandler_t ptr);
		Function(std::string name, void* data);
		Function(std::string name);
		virtual ~Function();

		/** Get the proper name for this function.
		 * 
		 * Retrieves the actual name of the function used during creation,
		 * useful for overloading functions.
		 * 
		 * @return A std::string containing the name of the function.
		 */
		std::string GetName();

		/** Get the unique name for this function used to identify it.
		 * 
		 * This produces a similar result to what native compilers do to allow
		 * overloading of the same name but with different functions.
		 * 
		 * @return A std::string containing the unique name of the function.
		 */
		std::string GetUniqueName();

		/** Amount of Parameters for this function.
		 * 
		 * Returns the number of parameters that this function has for use with GetParameterType().
		 * 
		 * @return Amount of parameters as size_t.
		 */
		size_t CountParameters();

		/** Retrieve parameter by index.
		 * 
		 * 
		 * 
		 * @return IPC::Type that this parameter has.
		 */
		IPC::Type GetParameterType(size_t index);

		/** Assign Call Handler
		 * 
		 */
		void SetCallHandler(CallHandler_t ptr, void* data);

		/** Call this function
		*
		*/
		void Call(const int64_t id, const std::vector<IPC::Value>& args, std::vector<IPC::Value>& rval);
		
		private:
		std::string m_name, m_nameUnique;
		std::vector<IPC::Type> m_params;

		std::pair<CallHandler_t, void*> m_callHandler;
	};
}