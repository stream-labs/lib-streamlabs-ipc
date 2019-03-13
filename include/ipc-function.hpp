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

#pragma once
#include "ipc.hpp"
#include "ipc-value.hpp"
#include <memory>

namespace ipc {
	typedef void(*call_handler_t)(void* data, const int64_t id, const std::vector<ipc::value>& args, std::vector<ipc::value>& rval);

	class function {
		public:
		function(const std::string & name, const std::vector<ipc::type>& params, call_handler_t ptr, void* data);
		function(const std::string & name, const std::vector<ipc::type>& params, call_handler_t ptr);
		function(const std::string & name, const std::vector<ipc::type>& params, void* data);
		function(const std::string & name, const std::vector<ipc::type>& params);
		function(const std::string & name, call_handler_t ptr, void* data);
		function(const std::string & name, call_handler_t ptr);
		function(const std::string & name, void* data);
		function(const std::string & name);
		virtual ~function();

		/** Get the unique name for this function used to identify it.
		 * 
		 * This produces a similar result to what native compilers do to allow
		 * overloading of the same name but with different functions.
		 * 
		 * @return A std::string containing the unique name of the function.
		 */
		std::string get_unique_name();

		/** Call this function
		*
		*/
		void call(const int64_t id, const std::vector<ipc::value>& args, std::vector<ipc::value>& rval);
		
		private:
		std::string m_name, m_nameUnique;
		std::vector<ipc::type> m_params;

		std::pair<call_handler_t, void*> m_callHandler;
	};
}