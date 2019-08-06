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
#ifdef WIN32
#include "../source/windows/named-pipe.hpp"
#endif
namespace ipc {
	class server;

	class server_instance {
		friend class server;

		public:
		server_instance();
#ifdef WIN32
		server_instance(server* owner, std::shared_ptr<os::windows::named_pipe> conn);
#endif
        ~server_instance();
		
		bool is_alive();

		private: // Threading
		bool m_stopWorkers = false;
		std::thread m_worker;

		void worker();
#ifdef WIN32
		void read_callback_init(os::error ec, size_t size);
		void read_callback_msg(os::error ec, size_t size);
		void read_callback_msg_write(const std::vector<char>& write_buffer);
		void write_callback(os::error ec, size_t size);		
#endif
		protected:
#ifdef WIN32
		std::shared_ptr<os::windows::named_pipe> m_socket;
		std::shared_ptr<os::async_op> m_wop, m_rop;
#endif
		std::vector<char> m_wbuf, m_rbuf;
		std::queue<std::vector<char>> m_write_queue;

		private:
		server* m_parent = nullptr;
		int64_t m_clientId;
	};
}
