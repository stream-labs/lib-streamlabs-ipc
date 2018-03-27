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

namespace ipc {
	class collection {
		public:
		collection(std::string name);
		virtual ~collection();

		/** 
		 * 
		 */
		std::string get_name();

		/**
		*
		*/
		size_t count_functions();

		/**
		*
		*/
		bool register_function(std::shared_ptr<function> func);

		/**
		*
		*/
		bool unregister_function(std::shared_ptr<function> func);

		/**
		*
		*/
		bool has_function(const std::string& name);

		/**
		*
		*/
		bool has_function(const std::string& name, const std::vector<ipc::type>& params);

		/**
		*
		*/
		bool has_function(const std::string& name, const std::vector<ipc::value>& params);

		/**
		*
		*/
		std::shared_ptr<function> get_function(const size_t& idx);

		/**
		*
		*/
		std::shared_ptr<function> get_function(const std::string& name);

		/**
		*
		*/
		std::shared_ptr<function> get_function(const std::string& name, const std::vector<ipc::type>& params);

		/**
		*
		*/
		std::shared_ptr<function> get_function(const std::string& name, const std::vector<ipc::value>& params);

		private:
		std::string m_name;
		std::map<std::string, std::shared_ptr<function>> m_functions;
	};
}