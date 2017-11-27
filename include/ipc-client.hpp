// A custom IPC solution to bypass electron IPC.
// Copyright(C) 2017 Streamlabs (General Workings Inc)
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

#pragma once
#include "ipc.hpp"
#include "os-namedsocket.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace IPC {
	typedef void(*CallReturn_t)(void* data, IPC::Value rval);

	class Client {
		public:
		Client(std::string socketPath);
		virtual ~Client();

		bool Authenticate();
		bool Call(std::string cname, std::string fname, std::vector<IPC::Value> args, CallReturn_t fn, void* data);
		
		private:
		std::unique_ptr<OS::NamedSocket> m_socket;
		bool m_authenticated = false;
		std::map<int64_t, std::pair<CallReturn_t, void*>> m_cb;

		private: // Threading
		bool m_stopWorkers = false;
		std::thread m_worker;
		std::mutex m_lock;
		static void WorkerThread(Client* ptr);
	};
}