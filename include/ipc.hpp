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
#include "ipc-value.hpp"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <stdarg.h>

#ifdef _DEBUG
#define TRACE_IPC_ENABLED
#endif 


#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#define DWORD unsigned long
#endif

namespace ipc {
	typedef uint64_t ipc_size_t;
	typedef uint32_t ipc_size_real_t;
	typedef std::function<void(void* data, const char* fmt, va_list args)> log_callback_t;

	struct ProcessInfo
	{
		uint64_t handle;
		uint64_t id;
		DWORD    exit_code;

		enum ExitCode
		{
			STILL_RUNNING = 259,
			VERSION_MISMATCH = 252,
			OTHER_ERROR = 253,
			MISSING_DEPENDENCY = 254,
			NORMAL_EXIT = 0
		};

		typedef std::map<ProcessInfo::ExitCode, std::string> ProcessDescriptionMap;

		private:
			static ProcessDescriptionMap descriptions;
			static ProcessDescriptionMap initDescriptions();

		public:
			ProcessInfo() : handle(0), id(0), exit_code(0) {}
			ProcessInfo(uint64_t h, uint64_t i) : handle(h), id(i), exit_code(0) {}
			static std::string getDescription(DWORD key);
	};

	inline void make_sendable(std::vector<char> &in) {
		reinterpret_cast<ipc_size_real_t&>(in[sizeof(ipc_size_real_t)]) = ipc_size_real_t(in.size() - sizeof(ipc_size_t));
	}

	inline ipc_size_t read_size(std::vector<char> const& in) {
		return reinterpret_cast<const ipc_size_real_t&>(in[sizeof(ipc_size_real_t)]);
	}

	void log(const char* fmt, ...);
	void register_log_callback(ipc::log_callback_t callback, void* data);

	std::string vectortohex(const std::vector<char>&);

	class base {
		public:
		static std::string make_unique_id(const std::string & name, const std::vector<type>& parameters);
		
	};

	namespace message {
		struct function_call {
			ipc::value uid = ipc::value(0ull);
			ipc::value class_name = ipc::value("");
			ipc::value function_name = ipc::value("");
			std::vector<ipc::value> arguments;

			size_t size();
			size_t serialize(std::vector<char>& buf, size_t offset);
			size_t deserialize(std::vector<char>& buf, size_t offset);
		};

		struct function_reply {
			ipc::value uid = ipc::value(0ull);
			std::vector<ipc::value> values;
			ipc::value error = ipc::value("");

			size_t size();
			size_t serialize(std::vector<char>& buf, size_t offset);
			size_t deserialize(std::vector<char>& buf, size_t offset);
		};
	}
}