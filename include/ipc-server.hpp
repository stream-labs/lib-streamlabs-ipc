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
#include "ipc-class.hpp"
#include "ipc-server-instance.hpp"
#include <list>
#include <map>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <functional>
#include "ipc-socket.hpp"

namespace ipc {
	class server_instance;

	typedef bool(*server_connect_handler_t)(void*, int64_t);
	typedef void(*server_disconnect_handler_t)(void*, int64_t);
	typedef void(*server_message_handler_t)(void*, int64_t, const std::vector<char>&);
	typedef void(*server_pre_callback_t)(std::string, std::string, const std::vector<ipc::value>&, void*);
	typedef void(*server_post_callback_t)(std::string, std::string, const std::vector<ipc::value>&, void*);

	class server {
		bool m_isInitialized = false;

		// Functions		
		std::map<std::string, std::shared_ptr<ipc::collection>> m_classes;

		// Socket
		std::mutex m_sockets_mtx;
#ifdef WIN32
		std::list<std::shared_ptr<ipc::socket>> m_sockets;
#elif __APPLE__
		std::list<std::shared_ptr<ipc::socket>> m_sockets;
#endif
		std::string m_socketPath = "";

		// Client management.
		std::mutex m_clients_mtx;
#ifdef WIN32
		std::map<std::shared_ptr<ipc::socket>, std::shared_ptr<server_instance>> m_clients;
#elif __APPLE__
		std::map<std::shared_ptr<ipc::socket>, std::shared_ptr<server_instance>> m_clients;
#endif

		// Event Handlers
		std::pair<server_connect_handler_t, void*>    m_handlerConnect;
		std::pair<server_disconnect_handler_t, void*> m_handlerDisconnect;
		std::pair<server_message_handler_t, void*>    m_handlerMessage;
		std::pair<server_pre_callback_t, void*>       m_preCallback;
		std::pair<server_post_callback_t, void*>      m_postCallback;

		// Worker
		struct {
			std::thread worker;
			bool stop = false;
		} m_watcher;

		void watcher();

#ifdef WIN32
		void spawn_client(std::shared_ptr<ipc::socket> socket);
		void kill_client(std::shared_ptr<ipc::socket> socket);
#elif __APPLE__
		void spawn_client(std::shared_ptr<ipc::socket> socket);
		void kill_client(std::shared_ptr<ipc::socket> socket);
#endif

		public:
		server();
		~server();

#ifdef __APPLE__
		std::queue<void*> display_actions;
		std::mutex display_lock;
#endif

		public: // Status
		void initialize(std::string socketPath);
		void finalize();

		public: // Events
		void set_connect_handler(server_connect_handler_t handler, void* data);
		void set_disconnect_handler(server_disconnect_handler_t handler, void* data);
		void set_message_handler(server_message_handler_t handler, void* data);
		void set_pre_callback(server_pre_callback_t handler, void* data);
		void set_post_callback(server_post_callback_t handler, void* data);

		public: // Functionality
		bool register_collection(std::shared_ptr<ipc::collection> cls);

		public: // Client -> Server
		bool client_call_function(int64_t cid, const std::string &cname, const std::string &fname,
			std::vector<ipc::value>& args, std::vector<ipc::value>& rval, std::string& errormsg);

		friend class server_instance;
	};
}
