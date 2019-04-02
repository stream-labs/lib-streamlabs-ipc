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

#include "ipc-client.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <iterator>
#include "../include/error.hpp"
#include "../include/tags.hpp"
#include "windows/semaphore.hpp"

#ifdef _WIN32
#include <windows.h>
#include <Objbase.h>
#endif

using namespace std::placeholders;

ipc::client::client(std::string socketPath) : ipc_class_manager() {
	m_socket = std::make_unique<os::windows::named_pipe>(os::open_only, socketPath, os::windows::pipe_read_mode::Byte);

	m_watcher.stop   = false;
	m_watcher.worker = std::thread(std::bind(&client::worker, this));
}

ipc::client::~client() {
	m_watcher.stop = true;
	if (m_watcher.worker.joinable()) {
		m_watcher.worker.join();
	}
	m_socket = nullptr;
}

bool ipc::client::call_function(
    const std::string&       cname,
    const std::string&       fname,
    std::vector<ipc::value>& args,
    std::vector<ipc::value>& rval,
    std::string&             errormsg)
{
	if (m_classes.count(cname) == 0) {
		errormsg = "Class '" + cname + "' is not registered.";
		return false;
	}
	auto cls = m_classes.at(cname);

	auto fnc = cls->get_function(fname, args);
	if (!fnc) {
		errormsg = "Function '" + fname + "' not found in class '" + cname + "'.";
		return false;
	}

	fnc->call(-1, args, rval);

	return true;
}