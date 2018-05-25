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
#include "ipc-server.hpp"
#include "os-namedsocket.hpp"
#include "os-signal.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace ipc {
	class server;

	class server_instance {
		friend class server;

		public:
		server_instance();
		server_instance(server* owner, std::shared_ptr<os::named_socket_connection> conn);
		~server_instance();
		
		bool is_alive();

		private: // Threading
		bool m_stopWorkers = false;
		std::thread m_worker;

		void worker();
		static void worker_main(server_instance* ptr) {
			ptr->worker();
		};
		
		protected:
		std::shared_ptr<os::named_socket_connection> m_socket;

		private:
		server* m_parent = nullptr;
		os::ClientId_t m_clientId;
		bool m_isAuthenticated = false;
		std::shared_ptr<os::signal> m_readSignal;
		std::shared_ptr<os::signal> m_writeSignal;
	};
}