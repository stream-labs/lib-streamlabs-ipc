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
#include "ipc-server.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "../source/windows/named-pipe.hpp"
#include "../source/windows/semaphore.hpp"
#include "ipc-communication.hpp"

namespace ipc {
	class server;

	class server_instance: public ipc_communication {
		friend class server;

		public:
		server_instance();
		server_instance(server* owner, std::shared_ptr<os::windows::named_pipe> conn);
		virtual ~server_instance();

		bool call_function(
		    const std::string&       cname,
		    const std::string&       fname,
		    std::vector<ipc::value>& args,
		    std::vector<ipc::value>& rval,
		    std::string&             errormsg);

		bool host = false;

		private:
		server* m_parent = nullptr;
		int64_t m_clientId;
	};
}