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
#include <chrono>
#include <memory>
#include <thread>
#include <string>
#include <vector>
#include <list>

namespace os {
	typedef int64_t ClientId_t;

	class named_socket_connection;
	class named_socket {
		public:
		static std::unique_ptr<os::named_socket> create();
		typedef void(*connect_handler_t)(std::shared_ptr<os::named_socket_connection> conn);
		typedef void(*disconnect_handler_t)(std::shared_ptr<os::named_socket_connection> conn);

		named_socket();
		virtual ~named_socket();

	#pragma region Options
		/// Adjust the incoming(receive) buffer size.
		bool set_receive_buffer_size(size_t size);
		size_t get_receive_buffer_size();

		/// Adjust the outgoing(send) buffer size.
		bool set_send_buffer_size(size_t size);
		size_t get_send_buffer_size();
		
		/// Adjust the timeout for waiting.
		bool set_wait_timeout(std::chrono::nanoseconds time);
		std::chrono::nanoseconds get_wait_timeout();

		/// Adjust the timeout for receiving data.
		bool set_receive_timeout(std::chrono::nanoseconds time);
		std::chrono::nanoseconds get_receive_timeout();

		/// Adjust the timeout for sending data.
		bool set_send_timeout(std::chrono::nanoseconds time);
		std::chrono::nanoseconds get_send_timeout();
	#pragma endregion Options

	#pragma region Listen/Connect/Close
		// Listen to a Named Socket.
		/// Listens on the specified path for connections of clients. These clients can be local or
		///  on the network depending on what platform this is run on.
		/// It will also attempt to keep a set amount of connections waiting for more clients, also
		///  known as the backlog. A larger backlog can negatively impact performance while a lower
		///  one means that less clients can connect simultaneously, resulting in delays.
		bool listen(std::string path, size_t backlog);

		// Connect to a Named Socket.
		/// Connects to an existing named socket (if possible), otherwise immediately returns false.
		bool connect(std::string path);

		// Close the Named Socket.
		/// Different behavior depending on Initialized mode:
		/// - Create disconnects all clients and closes the socket.
		/// - Connect just disconnects from the socket and closes it.
		bool close();
	#pragma endregion Listen/Connect/Close

	#pragma region Server & Client
		bool is_initialized();
		bool is_server();
		bool is_client();
	#pragma endregion Server & Client

	#pragma region Server Only
		std::weak_ptr<os::named_socket_connection> accept();
	#pragma endregion Server Only

	#pragma region Client Only
		std::shared_ptr<os::named_socket_connection> get_connection();
	#pragma endregion Client Only

		protected:
		virtual bool _listen(std::string path, size_t backlog) = 0;
		virtual bool _connect(std::string path) = 0;
		virtual bool _close() = 0;

		private:
		// Flags
		bool m_isInitialized;
		bool m_isListening;

		// Times for timing out.
		std::chrono::nanoseconds m_timeOutWait;
		std::chrono::nanoseconds m_timeOutReceive;
		std::chrono::nanoseconds m_timeOutSend;

		// Buffers
		size_t m_bufferReceiveSize;
		size_t m_bufferSendSize;

		// IO
		protected:
		std::list<std::shared_ptr<named_socket_connection>> m_connections;
	};

	class named_socket_connection {
		public:
		
		// Status
		virtual bool is_waiting() = 0;
		virtual bool is_connected() = 0;
		virtual bool connect() = 0;
		virtual bool disconnect() = 0;
		virtual bool eof() = 0;
		virtual bool good() = 0;
		virtual bool bad();

		// Reading
		virtual size_t read_avail() = 0;
		virtual size_t read(char* buf, size_t length) = 0;
		virtual size_t read(std::vector<char>& out) = 0;
		virtual std::vector<char> read() = 0;

		// Writing
		virtual size_t write(const char* buf, const size_t length) = 0;
		virtual size_t write(const std::vector<char>& buf) = 0;

		virtual os::error flush() = 0;

		// Info
		virtual ClientId_t get_client_id() = 0;
	};
}