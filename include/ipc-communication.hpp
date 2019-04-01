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
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "../source/windows/named-pipe.hpp"
#include "ipc-class.hpp"

namespace ipc {
	class communication {
		public:
		communication();
		~communication();
		
		bool is_alive();

		protected: // Threading
		bool m_stopWorkers = false;
		std::thread m_worker;
		void        startWorker();

		public:
		void worker();
		void read_callback_init(os::error ec, size_t size);
		void read_callback_msg(os::error ec, size_t size);
		void write_callback(os::error ec, size_t size);	
		
		public:
		std::shared_ptr<os::windows::named_pipe> m_socket;
		std::shared_ptr<os::async_op> m_wop, m_rop;
		std::vector<char> m_wbuf, m_rbuf;
		std::queue<std::vector<char>> m_write_queue;

		public:
		virtual bool call_function(int64_t                  cid,
		                           const std::string&       cname,
		                           const std::string&       fname,
		                           std::vector<ipc::value>& args,
		                           std::vector<ipc::value>& rval,
		                           std::string&             errormsg) = 0;
		private:
		int64_t                                                 m_clientId;
	};
}