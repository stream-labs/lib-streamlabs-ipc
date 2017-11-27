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
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace IPC {
	class Server;

	class ServerInstance {
		friend class Server;

		public:
		ServerInstance();
		ServerInstance(Server* owner, std::shared_ptr<OS::NamedSocketConnection> conn);
		~ServerInstance();
		
		void QueueMessage(std::vector<char> data);
		bool IsAlive();

		private: // Threading
		bool m_stopWorkers = false;
		struct {
			std::thread worker;
			std::mutex lock;
			std::condition_variable cv;
			std::queue<std::vector<char>> queue;
		} m_readWorker, m_executeWorker, m_writeWorker;

		static void ReaderThread(ServerInstance* ptr);
		static void ExecuteThread(ServerInstance* ptr);
		static void WriterThread(ServerInstance* ptr);
		
		protected:
		std::shared_ptr<OS::NamedSocketConnection> m_socket;

		private:
		Server* m_parent = nullptr;
		OS::ClientId_t m_clientId;
		bool m_isAuthenticated = false;
	};
}