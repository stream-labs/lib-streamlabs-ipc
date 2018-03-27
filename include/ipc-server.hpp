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
#include "ipc-class.hpp"
#include "ipc-server-instance.hpp"
#include "os-namedsocket.hpp"
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace ipc {
	class server_instance;

	typedef bool(*server_connect_handler_t)(void*, os::ClientId_t);
	typedef void(*server_disconnect_handler_t)(void*, os::ClientId_t);
	typedef void(*server_message_handler_t)(void*, os::ClientId_t, const std::vector<char>&);

	class server {
		friend class server_instance;

		public:
		server();
		~server();

		public: // Status
		void initialize(std::string socketPath);
		void finalize();

		public: // Events
		void set_connect_handler(server_connect_handler_t handler, void* data);
		void set_disconnect_handler(server_disconnect_handler_t handler, void* data);
		void set_message_handler(server_message_handler_t handler, void* data);

		public: // Functionality
		bool register_collection(ipc::collection cls);
		bool register_collection(std::shared_ptr<ipc::collection> cls);

		protected: // Client -> Server
		bool client_call_function(os::ClientId_t cid, std::string cname, std::string fname, 
			std::vector<ipc::value>& args, std::vector<ipc::value>& rval, std::string& errormsg);

		private: // Threading
		std::thread m_worker;
		bool m_stopWorker = false;

		static void worker_main(server* ptr);
		void worker_local();

		private:
		std::unique_ptr<os::named_socket> m_socket;
		bool m_isInitialized = false;

		// Client management.
		std::mutex m_clientLock;
		std::map<os::ClientId_t, std::shared_ptr<server_instance>> m_clients;
		std::map<std::string, std::shared_ptr<ipc::collection>> m_classes;

		// Event Handlers
		std::pair<server_connect_handler_t, void*> m_handlerConnect;
		std::pair<server_disconnect_handler_t, void*> m_handlerDisconnect;
		std::pair<server_message_handler_t, void*> m_handlerMessage;
	};
}
