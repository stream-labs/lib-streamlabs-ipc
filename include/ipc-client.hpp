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
#include "ipc.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "../source/windows/named-pipe.hpp"

namespace ipc {
	typedef void(*call_return_t)(const void* data, const std::vector<ipc::value>& rval);

	class client {
		std::unique_ptr<os::windows::named_pipe> m_socket;
		std::shared_ptr<os::async_op> m_rop;

		bool m_authenticated = false;
		std::mutex m_lock;
		std::map<int64_t, std::pair<call_return_t, void*>> m_cb;

		// Threading
		struct {
			std::thread worker;
			bool stop = false;
			std::vector<char> buf;
		} m_watcher;
		
		void worker();
		void read_callback_init(os::error ec, size_t size);
		void read_callback_msg(os::error ec, size_t size);

		public:
		client(std::string socketPath);
		virtual ~client();

		bool call(const std::string& cname, const std::string& fname, std::vector<ipc::value> args, call_return_t fn, void* data, int64_t& cbid);

		bool cancel(int64_t const& id);

		std::vector<ipc::value> call_synchronous_helper(std::string cname, std::string fname, std::vector<ipc::value> args);
	};
}