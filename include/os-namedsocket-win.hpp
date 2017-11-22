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
#include <map>
#include <mutex>
#include <thread>
#include <queue>
extern "C" { // clang++ compatible
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#include <windows.h>
}

namespace OS {
	class NamedSocketWindows : public NamedSocket {
		public:
		NamedSocketWindows();
		virtual ~NamedSocketWindows();

		protected:
	#pragma region Listen/Connect/Close
		virtual bool _listen(std::string path, size_t backlog) override;
		virtual bool _connect(std::string path) override;
		virtual bool _close() override;
	#pragma endregion Listen/Connect/Close

		// Private Memory
		private:
		std::string m_pipeName;
		DWORD m_openMode;
		DWORD m_pipeMode;
	};

	class NamedSocketConnectionWindows : public NamedSocketConnection {
		public:
		NamedSocketConnectionWindows(OS::NamedSocket* parent, std::string path, DWORD openFlags, DWORD pipeFlags);
		NamedSocketConnectionWindows(OS::NamedSocket* parent, std::string path);
		virtual ~NamedSocketConnectionWindows();
		
		// Status
		virtual bool IsWaiting() override;
		virtual bool IsConnected() override;
		virtual bool Connect() override;
		virtual bool Disconnect() override;
		virtual bool EoF() override;
		virtual bool Good() override;

		// Reading
		virtual size_t ReadAvail() override;
		virtual size_t Read(char* buf, size_t length) override;
		virtual size_t Read(std::vector<char>& out) override;
		virtual std::vector<char> Read() override;

		// Writing
		virtual size_t Write(const char* buf, const size_t length) override;
		virtual size_t Write(const std::vector<char>& buf) override;

		// Info
		virtual ClientId_t GetClientId() override;

		private:
		static void ThreadMain(void* ptr);
		void ThreadLocal();
		static void createOverlapped(OVERLAPPED& ov);
		static void destroyOverlapped(OVERLAPPED& ov);

		private:
		OS::NamedSocket* m_parent;
		HANDLE m_handle;

		bool m_stopWorkers = false;
		std::thread m_managerThread;

		std::mutex m_writeLock, m_readLock;
		std::queue<std::vector<char>>
			m_writeQueue, m_readQueue;

		// Status
		bool m_isServer = false;
		enum class State {
			Sleeping,
			Waiting,
			Connected
		};
		State m_state = State::Sleeping;
	};
}