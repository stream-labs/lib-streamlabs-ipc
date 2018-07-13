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

namespace ipc {
	typedef uint32_t ipc_size_t;

	inline void make_sendable(std::vector<char>& out, std::vector<char> const& in) {
		out.resize(in.size() + sizeof(ipc_size_t));
		memcpy(out.data() + sizeof(ipc_size_t), in.data(), in.size());
		reinterpret_cast<ipc_size_t&>(out[0]) = ipc_size_t(in.size());
	}

	inline ipc_size_t read_size(std::vector<char> const& in) {
		return reinterpret_cast<const ipc_size_t&>(in[0]);
	}

	class base {
		public:
		static std::string make_unique_id(std::string name, std::vector<type> parameters);
		
	};
}