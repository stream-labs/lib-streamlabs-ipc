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
#include "ipc-function.hpp"
#include "ipc-value.hpp"
#include <map>
#include <memory>

namespace ipc {
	class collection {
		public:
		collection(const std::string & name);
		virtual ~collection();

		std::string get_name();
		bool register_function(std::shared_ptr<function> func);
		std::shared_ptr<function> get_function(const std::string& name, const std::vector<ipc::type>& params);
		std::shared_ptr<function> get_function(const std::string& name, const std::vector<ipc::value>& params);

		private:
		std::string m_name;
		std::map<std::string, std::shared_ptr<function>> m_functions;
	};
}