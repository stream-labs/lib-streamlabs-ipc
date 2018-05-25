// Copyright(C) 2018 Streamlabs (General Workings Inc)
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

#include "os-error.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace os {
	class signal {
		public:
		static std::shared_ptr<os::signal> create(bool initial_state = false, bool auto_reset = true);
		static std::shared_ptr<os::signal> create(std::string name, bool initial_state = false, bool auto_reset = true);

		static os::error wait_multiple(std::chrono::nanoseconds timeout, std::vector<std::shared_ptr<os::signal>> signals, size_t& signalled_index, bool wait_all = false);

		public:
		virtual ~signal();

		virtual os::error clear() = 0;
		virtual os::error set(bool state = true) = 0;
		virtual os::error pulse() = 0;
		virtual os::error wait(std::chrono::nanoseconds timeout) = 0;
	};
}
