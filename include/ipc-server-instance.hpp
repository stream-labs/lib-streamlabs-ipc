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

		private:
		static void WorkerMain(ServerInstance* ptr, bool writer);
		void WorkerLocal(bool writer);
		bool ReaderTask();
		bool WriterTask();

		bool IsAlive();

		protected:
		std::shared_ptr<OS::NamedSocketConnection> m_socket;

		private:
		Server* m_parent = nullptr;
		OS::ClientId_t m_clientId;

		struct WorkerStruct {
			std::thread worker;
			bool shutdown = false;
		} m_readWorker, m_writeWorker;
		std::queue<std::vector<char>> m_writeQueue;
		std::mutex m_writeLock;
		std::condition_variable m_writeCV;
	};
}