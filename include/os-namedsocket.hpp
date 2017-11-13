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
#include <streambuf>
#include <memory>
#include <string>
#include <vector>

namespace OS {
	typedef int64_t ClientId_t;
	typedef void(*ConnectHandler_t)(void* data, OS::NamedSocketConnection* socket);
	typedef void(*DisconnectHandler_t)(void* data, OS::NamedSocketConnection* socket);

	class NamedSocketConnection;
	class NamedSocket {
		public:
		static std::unique_ptr<OS::NamedSocket> Create();

	#pragma region Options
		/// Adjust the incoming(receive) buffer size.
		virtual bool SetReceiveBufferSize(size_t size) = 0;
		virtual size_t GetReceiveBufferSize() = 0;

		/// Adjust the outgoing(send) buffer size.
		virtual bool SetSendBufferSize(size_t size) = 0;
		virtual size_t GetSendBufferSize() = 0;

		/// Adjust the default timeout for send/receive/wait.
		virtual bool SetDefaultTimeOut(std::chrono::nanoseconds time) = 0;
		virtual std::chrono::nanoseconds GetDefaultTimeOut() = 0;

		/// Adjust the timeout for waiting.
		virtual bool SetWaitTimeOut(std::chrono::nanoseconds time) = 0;
		virtual std::chrono::nanoseconds GetWaitTimeOut() = 0;

		/// Adjust the timeout for receiving data.
		virtual bool SetReceiveTimeOut(std::chrono::nanoseconds time) = 0;
		virtual std::chrono::nanoseconds GetReceiveTimeOut() = 0;

		/// Adjust the timeout for sending data.
		virtual bool SetSendTimeOut(std::chrono::nanoseconds time) = 0;
		virtual std::chrono::nanoseconds GetSendTimeOut() = 0;
	#pragma endregion Options

	#pragma region Listen/Connect/Close
		// Listen to a Named Socket.
		/// Listens on the specified path for connections of clients. These clients can be local or
		///  on the network depending on what platform this is run on.
		/// It will also attempt to keep a set amount of connections waiting for more clients, also
		///  known as the backlog. A larger backlog can negatively impact performance while a lower
		///  one means that less clients can connect simultaneously, resulting in delays.
		virtual bool Listen(std::string path, size_t backlog) = 0;

		// Connect to a Named Socket.
		/// Connects to an existing named socket (if possible), otherwise immediately returns false.
		virtual bool Connect(std::string path) = 0;

		// Finalize the Named Socket.
		/// Different behavior depending on Initialized mode:
		/// - Create disconnects all clients and closes the socket.
		/// - Connect just disconnects from the socket and closes it.
		virtual bool Close() = 0;
	#pragma endregion Listen/Connect/Close

	#pragma region Server Only
		virtual bool WaitForConnection() = 0;
		virtual std::shared_ptr<OS::NamedSocketConnection> AcceptConnection() = 0;
		virtual bool Disconnect(std::shared_ptr<OS::NamedSocketConnection> connection) = 0;
	#pragma endregion Server Only

	#pragma region Server & Client
		virtual bool IsServer() = 0;
		virtual bool IsClient() = 0;
	#pragma endregion Server & Client

	#pragma region Client Only
		virtual std::shared_ptr<OS::NamedSocketConnection> GetConnection() = 0;
	#pragma endregion Client Only
	};
	
	class NamedSocketConnection {
		public:
		virtual ClientId_t GetClientId() = 0;

		virtual bool Good() = 0;
		virtual bool Bad() = 0;

		virtual size_t Write(const char* buf, size_t length) = 0;
		virtual size_t Write(const std::vector<char>& buf) = 0;

		virtual size_t ReadAvail() = 0;
		virtual std::vector<char> Read() = 0;
		virtual size_t Read(char* buf, size_t length) = 0;
		virtual size_t Read(std::vector<char>& out) = 0;
	};
}