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

namespace OS {
	typedef int64_t ClientId_t;

	class NamedSocketConnection;
	class NamedSocket {
		public:
		static std::unique_ptr<OS::NamedSocket> Create();
		typedef void(*ConnectHandler_t)(std::shared_ptr<OS::NamedSocketConnection> conn);
		typedef void(*DisconnectHandler_t)(std::shared_ptr<OS::NamedSocketConnection> conn);

		NamedSocket();
		virtual ~NamedSocket();

	#pragma region Options
		/// Adjust the incoming(receive) buffer size.
		bool SetReceiveBufferSize(size_t size);
		size_t GetReceiveBufferSize();

		/// Adjust the outgoing(send) buffer size.
		bool SetSendBufferSize(size_t size);
		size_t GetSendBufferSize();
		
		/// Adjust the timeout for waiting.
		bool SetWaitTimeOut(std::chrono::nanoseconds time);
		std::chrono::nanoseconds GetWaitTimeOut();

		/// Adjust the timeout for receiving data.
		bool SetReceiveTimeOut(std::chrono::nanoseconds time);
		std::chrono::nanoseconds GetReceiveTimeOut();

		/// Adjust the timeout for sending data.
		bool SetSendTimeOut(std::chrono::nanoseconds time);
		std::chrono::nanoseconds GetSendTimeOut();
	#pragma endregion Options

	#pragma region Listen/Connect/Close
		// Listen to a Named Socket.
		/// Listens on the specified path for connections of clients. These clients can be local or
		///  on the network depending on what platform this is run on.
		/// It will also attempt to keep a set amount of connections waiting for more clients, also
		///  known as the backlog. A larger backlog can negatively impact performance while a lower
		///  one means that less clients can connect simultaneously, resulting in delays.
		bool Listen(std::string path, size_t backlog);

		// Connect to a Named Socket.
		/// Connects to an existing named socket (if possible), otherwise immediately returns false.
		bool Connect(std::string path);

		// Close the Named Socket.
		/// Different behavior depending on Initialized mode:
		/// - Create disconnects all clients and closes the socket.
		/// - Connect just disconnects from the socket and closes it.
		bool Close();
	#pragma endregion Listen/Connect/Close

	#pragma region Server & Client
		bool IsInitialized();
		bool IsServer();
		bool IsClient();
	#pragma endregion Server & Client

	#pragma region Server Only
		std::weak_ptr<OS::NamedSocketConnection> Accept();
	#pragma endregion Server Only

	#pragma region Client Only
		std::shared_ptr<OS::NamedSocketConnection> GetConnection();
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
		std::list<std::shared_ptr<NamedSocketConnection>> m_connections;
	};

	class NamedSocketConnection {
		public:
		
		// Status
		virtual bool IsWaiting() = 0;
		virtual bool IsConnected() = 0;
		virtual bool Connect() = 0;
		virtual bool Disconnect() = 0;
		virtual bool EoF() = 0;
		virtual bool Good() = 0;
		virtual bool Bad();

		// Reading
		virtual size_t ReadAvail() = 0;
		virtual size_t Read(char* buf, size_t length) = 0;
		virtual size_t Read(std::vector<char>& out) = 0;
		virtual std::vector<char> Read() = 0;

		// Writing
		virtual size_t Write(const char* buf, const size_t length) = 0;
		virtual size_t Write(const std::vector<char>& buf) = 0;

		// Waiting
		virtual void Wait() = 0;

		// Info
		virtual ClientId_t GetClientId() = 0;
	};
}