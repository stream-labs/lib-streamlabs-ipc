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
#include <semaphore.h>

#ifdef WIN32
#include "../source/windows/named-pipe.hpp"
#elif __APPLE__
#include "../source/apple/named-pipe.hpp"
#endif

typedef void (*call_return_t)(void* data, const std::vector<ipc::value>& rval);
extern call_return_t g_fn;
extern void*         g_data;
extern int64_t       g_cbid;

namespace ipc {

	class client {
#ifdef WIN32
		std::unique_ptr<os::windows::named_pipe> m_socket;
#elif __APPLE__
		std::unique_ptr<os::apple::named_pipe> m_socket;
#endif
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

		std::string reader_sem_name = "semaphore-client-reader";
		std::string writer_sem_name = "semaphore-client-writer";
		sem_t *m_reader_sem;
		sem_t *m_writer_sem;
		
		void worker();
		void read_callback_init(os::error ec, size_t size);
		void read_callback_msg(os::error ec, size_t size);

		public:
		client(std::string socketPath);
		virtual ~client();

		bool call(
		    const std::string&      cname,
		    const std::string&      fname,
		    std::vector<ipc::value> args,
		    call_return_t           fn   = g_fn,
		    void*                   data = g_data,
		    int64_t&                cbid = g_cbid);

		bool cancel(int64_t const& id);

		std::vector<ipc::value> call_synchronous_helper(const std::string & cname, const std::string &fname, const std::vector<ipc::value> & args);
	};
}
