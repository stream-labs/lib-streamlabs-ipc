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

	typedef void(*ConnectHandler_t)(void* data, OS::NamedSocketConnection* socket);
	typedef void(*DisconnectHandler_t)(void* data, OS::NamedSocketConnection* socket);

	class NamedSocket {
		public:
		static std::unique_ptr<OS::NamedSocket> Create();

		// Only callable before Initialize()
		public:
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

		// The callback to call if a new client connected.
		virtual bool SetConnectHandler(ConnectHandler_t handler, void* data) = 0;
		virtual ConnectHandler_t GetConnectHandler() = 0;
		virtual void* GetConnectHandlerData() = 0;

		// The callback to call if an existing client disconnects (by any means).
		virtual bool SetDisconnectHandler(DisconnectHandler_t handler, void* data) = 0;
		virtual DisconnectHandler_t GetDisconnectHandler() = 0;
		virtual void* GetDisconnectHandlerData() = 0;

		// Socket Backlog
		/// Backlog defines how many sleeping and/or waiting connections to keep around.
		/// A low number means that clients have to wait while a high number might reduce performance.
		virtual bool SetBacklog(size_t count) = 0;
		virtual size_t GetBacklog() = 0;

		public:
		// Initialize the Named Socket.
		/// Behavior differs depending on mode:
		/// - Create acts like a Server and has control over the Socket.
		/// - Connect acts like a Client and just reads/writes to the Socket.
		///   (May fail if the Server has no more connections backlogged.)
		/// Create and Connect are not exclusive, use IsServer and IsClient to know what was chosen.
		virtual bool Initialize(std::string path, bool doCreate, bool doConnect) = 0;
		// Finalize the Named Socket.
		/// Different behavior depending on Initialized mode:
		/// - Create disconnects all clients and closes the socket.
		/// - Connect just disconnects from the socket and closes it.
		virtual bool Finalize() = 0;
		// Check if the Named Socket is run as a Server or Client.
		virtual bool IsServer() = 0;
		virtual bool IsClient() = 0;


		// Owner Functionality
		public:
		virtual bool WaitForConnection() = 0;
		virtual std::shared_ptr<OS::NamedSocketConnection> AcceptConnection() = 0;
		virtual bool Disconnect(std::shared_ptr<OS::NamedSocketConnection> connection) = 0;

		// Child & Owner Functionality
		public:


		// Child Functionality
		public:
		virtual std::shared_ptr<OS::NamedSocketConnection> GetConnection() = 0;

	};
}