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

#include "ipc-client-instance.hpp"
#include <sstream>
#include <functional>

#ifdef _WIN32
#include <windows.h>
using namespace std::placeholders;
#endif

ipc::client_instance::client_instance() {}

ipc::client_instance::client_instance(ipc::client* owner, std::shared_ptr<os::windows::named_pipe> conn)
{
	m_socket = conn;
	m_parent = owner;
	startWorker();
}

ipc::client_instance::~client_instance() {}

bool ipc::client_instance::call_function(
    int64_t                  cid,
    const std::string&       cname,
    const std::string&       fname,
    std::vector<ipc::value>& args,
    std::vector<ipc::value>& rval,
    std::string&             errormsg)
{
	return m_parent->server_call_function(cid, cname, fname, args, rval, errormsg);
}