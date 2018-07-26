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
#include "ipc-value.hpp"
#include <string>
#include <vector>
#include <functional>
#include <stdarg.h>

namespace ipc {
	typedef uint64_t ipc_size_t;
	typedef uint32_t ipc_size_real_t;
	typedef std::function<void(void* data, const char* fmt, va_list args)> log_callback_t;

	inline void make_sendable(std::vector<char>& out, std::vector<char> const& in) {
		out.resize(in.size() + sizeof(ipc_size_t));
		memcpy(out.data() + sizeof(ipc_size_t), in.data(), in.size());
		out[0] = 0;
		out[1] = 1;
		out[2] = 2;
		out[3] = 3;
		reinterpret_cast<ipc_size_real_t&>(out[sizeof(ipc_size_real_t)]) = ipc_size_real_t(in.size());
	}

	inline ipc_size_t read_size(std::vector<char> const& in) {
		return reinterpret_cast<const ipc_size_real_t&>(in[sizeof(ipc_size_real_t)]);
	}

	void set_log_callback(log_callback_t cb, void* data);

	void log(const char* fmt, ...);

	std::string vectortohex(const std::vector<char>&);

	class base {
		public:
		static std::string make_unique_id(std::string name, std::vector<type> parameters);
		
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

		struct authenticate {
			ipc::value name = ipc::value("");
			ipc::value password = ipc::value("");

			size_t size();
			size_t serialize(std::vector<char>& buf, size_t offset);
			size_t deserialize(std::vector<char>& buf, size_t offset);
		};

		struct authenticate_reply {
			ipc::value auth = ipc::value(false);

			size_t size();
			size_t serialize(std::vector<char>& buf, size_t offset);
			size_t deserialize(std::vector<char>& buf, size_t offset);
		};

	}
}