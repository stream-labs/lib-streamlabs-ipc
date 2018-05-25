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

#include "os-signal.hpp"

#ifdef _WIN32
#include "os-signal-win.hpp"
#endif

std::shared_ptr<os::signal> os::signal::create(bool initial_state /*= false*/, bool auto_reset /*= true*/) {
#ifdef _WIN32
	return std::dynamic_pointer_cast<os::signal>(std::make_shared<os::signal_win>(initial_state, auto_reset));
#endif
}

std::shared_ptr<os::signal> os::signal::create(std::string name, bool initial_state /*= false*/, bool auto_reset /*= true*/) {
#ifdef _WIN32
	return std::dynamic_pointer_cast<os::signal>(std::make_shared<os::signal_win>(name, initial_state, auto_reset));
#endif
}

os::signal::~signal() {}
