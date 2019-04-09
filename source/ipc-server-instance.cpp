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

#include "ipc-server-instance.hpp"
#include <sstream>
#include <functional>

#ifdef _WIN32
#include <windows.h>
using namespace std::placeholders;
#endif

ipc::server_instance::server_instance() {}

ipc::server_instance::server_instance(ipc::server* owner, std::shared_ptr<os::windows::named_pipe> conn): 
	m_parent(owner), m_clientId(0)
{
	m_watcher.stop = false;
	m_socket = std::make_unique<os::windows::named_pipe>(*conn);
	m_watcher.worker = std::thread(std::bind(&server_instance::worker, this));
}

ipc::server_instance::~server_instance() {
}

bool ipc::server_instance::call_function(
	const std::string&       cname,
	const std::string&       fname,
	std::vector<ipc::value>& args,
	std::vector<ipc::value>& rval,
	std::string&             errormsg)
{
	return m_parent->client_call_function(
	    m_clientId, cname, fname, args, rval, errormsg);
}
