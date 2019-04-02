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


typedef void (*call_return_t)(const void* data, const std::vector<ipc::value>& rval);
extern call_return_t g_fn;
extern void*         g_data;
extern int64_t       g_cbid;

namespace ipc {
	class server;

	class server_instance {
		friend class server;

		// Functions
		std::map<std::string, std::shared_ptr<ipc::collection>> m_classes;

		public:
		server_instance();
		server_instance(server* owner, std::shared_ptr<os::windows::named_pipe> conn);
		~server_instance();

		std::mutex                                         m_lock;
		std::map<int64_t, std::pair<call_return_t, void*>> m_cb;

		bool call(
		    const std::string&      cname,
		    const std::string&      fname,
		    std::vector<ipc::value> args,
		    call_return_t           fn   = g_fn,
		    void*                   data = g_data,
		    int64_t&                cbid = g_cbid);
		bool cancel(int64_t const& id);
		std::vector<ipc::value> call_synchronous_helper(
		    const std::string&             cname,
		    const std::string&             fname,
		    const std::vector<ipc::value>& args);

		bool is_alive();

		private: // Threading
		bool m_stopWorkers = false;
		std::thread m_worker;

		void worker();
		void read_callback_init(os::error ec, size_t size);
		void read_callback_msg(os::error ec, size_t size);
		void write_callback(os::error ec, size_t size);		
		
		protected:
		std::shared_ptr<os::windows::named_pipe> m_socket;
		std::shared_ptr<os::async_op> m_wop, m_rop;
		std::vector<char> m_wbuf, m_rbuf;
		std::queue<std::vector<char>> m_write_queue;

		private:
		server* m_parent = nullptr;
		int64_t m_clientId;
	};
}