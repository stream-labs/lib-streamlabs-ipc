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

#include "os-namedsocket.hpp"
#include <mutex>
#include <thread>
extern "C" { // clang++ compatible
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
}

namespace OS {
	class NamedSocketWindows : public NamedSocket {
		public:
		NamedSocketWindows();
		virtual ~NamedSocketWindows();

	#pragma region Options
		virtual bool SetReceiveBufferSize(size_t size) override;
		virtual size_t GetReceiveBufferSize() override;

		virtual bool SetSendBufferSize(size_t size) override;
		virtual size_t GetSendBufferSize() override;

		virtual bool SetDefaultTimeOut(std::chrono::nanoseconds time) override;
		virtual std::chrono::nanoseconds GetDefaultTimeOut() override;

		virtual bool SetWaitTimeOut(std::chrono::nanoseconds time) override;
		virtual std::chrono::nanoseconds GetWaitTimeOut() override;

		virtual bool SetReceiveTimeOut(std::chrono::nanoseconds time) override;
		virtual std::chrono::nanoseconds GetReceiveTimeOut() override;

		virtual bool SetSendTimeOut(std::chrono::nanoseconds time) override;
		virtual std::chrono::nanoseconds GetSendTimeOut() override;
	#pragma endregion Options

	#pragma region Listen/Connect/Close
		virtual bool Listen(std::string path, size_t backlog) override;
		virtual bool Connect(std::string path) override;
		virtual bool Close() override;
	#pragma endregion Listen/Connect/Close

	#pragma region Server Only
		virtual bool Wait() override;
		virtual std::shared_ptr<OS::NamedSocketConnection> Accept() override;
		virtual bool Disconnect(std::shared_ptr<OS::NamedSocketConnection> connection) override;
	#pragma endregion Server Only

	#pragma region Server & Client
		virtual bool IsServer() override;
		virtual bool IsClient() override;
	#pragma endregion Server & Client

	#pragma region Client Only
		virtual std::shared_ptr<OS::NamedSocketConnection> GetConnection() override;
	#pragma endregion Client Only

		protected:
	#pragma region Utility Functions
		bool is_valid_path(std::string path);
		LPCTSTR make_valid_path(std::string path);
		HANDLE create_pipe();
	#pragma endregion Utility Functions

		private:
		static void _ConnectionDestructorHandler(void* data, OS::NamedSocketConnection* ptr);

		// Private Memory
		private:
		bool m_isInitialized = false;
		bool m_isServer = false;
		std::string m_pipeName = "";
		size_t m_bufferSizeSend = 65535;
		size_t m_bufferSizeRecv = 65535;
		std::chrono::milliseconds m_timeOutDefault = std::chrono::milliseconds(50);
		std::chrono::milliseconds m_timeOutWait = std::chrono::milliseconds(50);
		std::chrono::milliseconds m_timeOutSend = std::chrono::milliseconds(50);
		std::chrono::milliseconds m_timeOutRecv = std::chrono::milliseconds(50);

		/// Simulate TCP networking.
		size_t m_connectionBacklog = 4;
		HANDLE m_handleMain = 0;
		SECURITY_ATTRIBUTES m_securityAttributes;
		std::mutex
			m_handlesSleepingMtx,
			m_handlesAwakeMtx,
			m_handlesWorkingMtx;
		std::vector<HANDLE>
			m_handlesSleeping,	// Sleeping, can be waited on.
			m_handlesAwake,	// Have a connection pending.
			m_handlesWorking;	// Connected to a client.
		std::shared_ptr<OS::NamedSocketConnection> m_clientConnection;
	};

	class NamedSocketConnectionWindows : public NamedSocketConnection {
		public:
		NamedSocketConnectionWindows(OS::NamedSocket* handler, HANDLE socket);
		virtual ~NamedSocketConnectionWindows();

		virtual ClientId_t GetClientId() override;

		virtual bool Good() override;
		virtual bool Bad() override;

		virtual size_t Write(const char* buf, size_t length) override;
		virtual size_t Write(const std::vector<char>& buf) override;

		virtual size_t ReadAvail() override;
		virtual size_t Read(char* buf, size_t length) override;
		virtual size_t Read(std::vector<char>& out) override;
		virtual std::vector<char> Read() override;

		protected:
		typedef void(*DestructorHandler_t)(void* data, OS::NamedSocketConnection* socket);
		void SetDestructorCallback(DestructorHandler_t func, void* data);

		protected:
		HANDLE m_handle;

		private:
		DestructorHandler_t m_cbDestructor;
		void* m_cbDestructorData;

		friend class NamedSocketWindows;
	};
}