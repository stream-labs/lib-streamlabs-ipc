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
#include <list>
extern "C" { // clang++ compatible
#define NOMINMAX
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#include <windows.h>
#include <AclAPI.h>
#include <AccCtrl.h>
}

namespace os {
	class overlapped_manager {
		std::queue<std::shared_ptr<OVERLAPPED>> freeOverlapped;
		std::list<std::shared_ptr<OVERLAPPED>> usedOverlapped;
		std::mutex mtx;
		SECURITY_ATTRIBUTES sa;
		PSECURITY_DESCRIPTOR pSD = NULL;
		PSID pEveryoneSID = NULL, pAdminSID = NULL;
		PACL pACL = NULL;
		EXPLICIT_ACCESS ea[2];
		SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
		SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;

		void create_overlapped(std::shared_ptr<OVERLAPPED>& ov);
		void destroy_overlapped(std::shared_ptr<OVERLAPPED>& ov);
		void append_create_overlapped();

		bool create_security_attributes();
		void destroy_security_attributes();

		public:
		overlapped_manager();
		~overlapped_manager();

		std::shared_ptr<OVERLAPPED> alloc();
		void free(std::shared_ptr<OVERLAPPED> ov);
	};

	class named_socket_win : public named_socket {
		public:
		named_socket_win();
		virtual ~named_socket_win();

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

		protected:
		overlapped_manager ovm;

		friend class named_socket_connection_win;
	};

	class named_socket_connection_win : public named_socket_connection {
		public:
		named_socket_connection_win(os::named_socket* parent, std::string path, DWORD openFlags, DWORD pipeFlags);
		named_socket_connection_win(os::named_socket* parent, std::string path);
		virtual ~named_socket_connection_win();
		
		// Status
		virtual bool is_waiting() override;
		virtual bool is_connected() override;
		virtual bool connect() override;
		virtual bool disconnect() override;
		virtual bool eof() override;
		virtual bool good() override;

		// Reading
		virtual size_t read_avail() override;
		virtual size_t read(char* buf, size_t length) override;
		virtual size_t read(std::vector<char>& out) override;
		virtual std::vector<char> read() override;

		// Writing
		virtual size_t write(const char* buf, const size_t length) override;
		virtual size_t write(const std::vector<char>& buf) override;

		// Info
		virtual ClientId_t get_client_id() override;

		private:
		static void thread_main(void* ptr);
		void threadlocal();
		static void create_overlapped(OVERLAPPED& ov);
		static void destroy_overlapped(OVERLAPPED& ov);

		private:
		os::named_socket* m_parent;
		HANDLE m_handle;

		bool m_stopWorkers = false;
		std::thread m_managerThread;

		std::mutex m_writeLock, m_readLock;
		std::queue<std::vector<char>>
			m_writeQueue, m_readQueue;

		// Status
		bool m_isServer = false;
		enum class state {
			Sleeping,
			Waiting,
			Connected
		};
		state m_state = state::Sleeping;
	};
}