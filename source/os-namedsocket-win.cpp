// A custom IPC solution to bypass electron IPC.
// Copyright(C) 2017 Streamlabs
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

#include "os-namedsocket-win.hpp"
#include <iostream>

std::unique_ptr<OS::NamedSocket> OS::NamedSocket::Create() {
	return std::make_unique<OS::NamedSocketWindows>();
}

#pragma region NamedSocketWindows
#pragma region De-/Constructor
OS::NamedSocketWindows::NamedSocketWindows() {}

OS::NamedSocketWindows::~NamedSocketWindows() {
	Close();
}
#pragma endregion De-/Constructor

#pragma region Listen/Connect/Close
bool OS::NamedSocketWindows::_listen(std::string path, size_t backlog) {
	// Set Pipe Mode.
	m_openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
	m_pipeMode = PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE;

	// Validate and generate socket name
	if (path.length() > 255) // Path can't be larger than 255 characters, limit set by WinAPI.
		return false; // !TODO! Throw some kind of error to signal why it failed.
	for (char& v : path)
		if (v == '\\')
			v = '/';
	m_pipeName = "\\\\.\\pipe\\" + path;

	// Create sockets.
	try {
		for (size_t n = 0; n <= backlog; n++) {
			std::shared_ptr<NamedSocketConnectionWindows> ptr;
			if (n == 0) {
				ptr = std::make_shared<NamedSocketConnectionWindows>(this, m_pipeName,
					m_openMode | FILE_FLAG_FIRST_PIPE_INSTANCE, m_pipeMode);
			} else {
				ptr = std::make_shared<NamedSocketConnectionWindows>(this, m_pipeName,
					m_openMode, m_pipeMode);
			}
			m_connections.push_back(ptr);
		}
	} catch (...) {
		return false;
	}

	return true;
}

bool OS::NamedSocketWindows::_connect(std::string path) {
	// Validate and generate socket name
	if (path.length() > 255) // Path can't be larger than 255 characters, limit set by WinAPI.
		return false; // !TODO! Throw some kind of error to signal why it failed.
	for (char& v : path)
		if (v == '\\')
			v = '/';
	m_pipeName = "\\\\.\\pipe\\" + path;

	try {
		std::shared_ptr<NamedSocketConnectionWindows> ptr =
			std::make_shared<NamedSocketConnectionWindows>(this, m_pipeName);
		m_connections.push_back(ptr);
	} catch (...) {
		return false;
	}

	return true;
}

bool OS::NamedSocketWindows::_close() {
	m_connections.clear();
	return true;
}


#pragma endregion Listen/Connect/Close
#pragma endregion NamedSocketWindows

#pragma region Named Socket Connection Windows
OS::NamedSocketConnectionWindows::NamedSocketConnectionWindows(OS::NamedSocket* parent,
	std::string path, DWORD openFlags, DWORD pipeFlags) : m_parent(parent) {
	if (parent == nullptr) // No parent
		throw std::runtime_error("No parent");

	if (path.length() > 0xFF) // Maximum total path name.
		throw std::runtime_error("Path name too long.");

	// Convert pipe name to machine compatible format.
#ifdef UNICODE
	std::wstring pipeNameWS = path;
	LPCTSTR pipeName = pipeNameWS.c_str();
#else
	LPCTSTR pipeName = path.c_str();
#endif

	// Security Attributes
	m_isServer = true;
	SECURITY_ATTRIBUTES m_securityAttributes;
	memset(&m_securityAttributes, 0, sizeof(m_securityAttributes));

	// Create Pipe
	m_handle = CreateNamedPipe(pipeName,
		openFlags,
		pipeFlags,
		PIPE_UNLIMITED_INSTANCES,
		static_cast<DWORD>(parent->GetSendBufferSize()),
		static_cast<DWORD>(parent->GetReceiveBufferSize()),
		static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(parent->GetWaitTimeOut()).count()),
		&m_securityAttributes);
	if (m_handle == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		throw std::runtime_error("Unable to create socket.");
	}

	// Threading
	m_stopWorkers = false;
	m_managerThread = std::thread(ThreadMain, this);
}

OS::NamedSocketConnectionWindows::NamedSocketConnectionWindows(OS::NamedSocket* parent, std::string path)
	: m_parent(parent) {
	if (parent == nullptr) // No parent
		throw std::runtime_error("No parent");

	if (path.length() > 0xFF) // Maximum total path name.
		throw std::runtime_error("Path name too long.");

	// Convert pipe name to machine compatible format.
#ifdef UNICODE
	std::wstring pipeNameWS = path;
	LPCTSTR pipeName = pipeNameWS.c_str();
#else
	LPCTSTR pipeName = path.c_str();
#endif

	m_isServer = false;
	size_t attempts = 0;
	for (size_t attempt = 0; attempt < 5; attempt++) {
		m_handle = CreateFile(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
		if (m_handle != INVALID_HANDLE_VALUE)
			break;
		DWORD err = GetLastError();
		if (err != ERROR_PIPE_BUSY)
			throw std::runtime_error("Unable to create socket.");

		DWORD timeout = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
			parent->GetWaitTimeOut()).count();
		if (!WaitNamedPipe(pipeName, timeout)) {
			if (attempt < 4) {
				continue;
			} else {
				throw std::runtime_error("Unable to create socket.");
			}
		}
	}
	m_state = State::Connected;

	// Threading
	m_stopWorkers = false;
	m_managerThread = std::thread(ThreadMain, this);
}

OS::NamedSocketConnectionWindows::~NamedSocketConnectionWindows() {
	// Stop Threading
	m_stopWorkers = true;
	m_managerThread.join();

	CancelIo(m_handle);
	if (m_isServer)
		Disconnect();
}

bool OS::NamedSocketConnectionWindows::IsWaiting() {
	return m_state == State::Waiting;
}

bool OS::NamedSocketConnectionWindows::IsConnected() {
	return m_state == State::Connected;
}

bool OS::NamedSocketConnectionWindows::Connect() {
	if (!m_isServer)
		throw std::logic_error("Clients are automatically connected.");

	if (m_state != State::Waiting)
		return false;

	m_state = State::Connected;
	return true;
}

bool OS::NamedSocketConnectionWindows::Disconnect() {
	if (!m_isServer)
		throw std::logic_error("Clients are automatically disconnected.");

	if (m_state != State::Connected)
		return false;

	return !!DisconnectNamedPipe(m_handle);
}

bool OS::NamedSocketConnectionWindows::EoF() {
	return Bad() || IsWaiting() || (ReadAvail() == 0);
}

bool OS::NamedSocketConnectionWindows::Good() {
	ULONG pid;
	if (!GetNamedPipeClientProcessId(m_handle, &pid))
		return false;

	if (!PeekNamedPipe(m_handle, NULL, NULL, NULL, NULL, NULL)) {
		DWORD err = GetLastError();
		if (err == ERROR_BROKEN_PIPE)
			return false;
	}

	return true;
}

size_t OS::NamedSocketConnectionWindows::ReadAvail() {
	std::unique_lock<std::mutex> ulock(m_readLock);
	if (m_readQueue.size() == 0)
		return 0;
	return m_readQueue.front().size();
}

size_t OS::NamedSocketConnectionWindows::Read(char* buf, size_t length) {
	{
		std::unique_lock<std::mutex> ulock(m_readLock);
		if (m_readQueue.size() > 0) {
			auto m = std::move(m_readQueue.front());
			m_readQueue.pop();

			size_t trueread = min(length, m.size());
			memcpy(buf, m.data(), trueread);
			return trueread;
		}
	}
	return 0;
}

size_t OS::NamedSocketConnectionWindows::Read(std::vector<char>& out) {
	{
		std::unique_lock<std::mutex> ulock(m_readLock);
		if (m_readQueue.size() > 0) {
			out = std::move(m_readQueue.front());
			m_readQueue.pop();
			return out.size();
		}
	}
	return 0;
}

std::vector<char> OS::NamedSocketConnectionWindows::Read() {
	{
		std::unique_lock<std::mutex> ulock(m_readLock);
		if (m_readQueue.size() > 0) {
			auto m = std::move(m_readQueue.front());
			m_readQueue.pop();
			return std::move(m);
		}
	}
	return std::vector<char>();
}

size_t OS::NamedSocketConnectionWindows::Write(const char* buf, size_t length) {
	if (length >= m_parent->GetSendBufferSize())
		return 0;

	{
		std::unique_lock<std::mutex> ulock(m_writeLock);
		m_writeQueue.push(std::vector<char>(buf, buf + length));
	}

	return length;
}

void OS::NamedSocketConnectionWindows::Wait() {
	while (m_state == State::Connected) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

size_t OS::NamedSocketConnectionWindows::Write(const std::vector<char>& buf) {
	if (buf.size() >= m_parent->GetSendBufferSize())
		return 0;

	{
		std::unique_lock<std::mutex> ulock(m_writeLock);
		m_writeQueue.push(buf);
	}

	return buf.size();
}

OS::ClientId_t OS::NamedSocketConnectionWindows::GetClientId() {
	return static_cast<OS::ClientId_t>(reinterpret_cast<intptr_t>(m_handle));
}

void OS::NamedSocketConnectionWindows::ThreadMain(void* ptr) {
	reinterpret_cast<NamedSocketConnectionWindows*>(ptr)->ThreadLocal();
}

void OS::NamedSocketConnectionWindows::ThreadLocal() {
	OVERLAPPED ovWrite, ovRead;
	createOverlapped(ovWrite);
	createOverlapped(ovRead);
	
	bool pendingIO = false;
	bool pendingWrite = false;
	bool pendingRead = false;

	auto writeIOtime = std::chrono::high_resolution_clock::now();
	auto readIOtime = std::chrono::high_resolution_clock::now();

	std::vector<char> msg(m_parent->GetReceiveBufferSize());
	DWORD actuallyRead;

	while (!m_stopWorkers) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (m_state == State::Sleeping) {
			if (!pendingIO) {
				if (!ConnectNamedPipe(m_handle, &ovWrite)) {
					DWORD err = GetLastError();
					switch (err) {
						case ERROR_IO_PENDING:
							pendingIO = true;
							break;
						case ERROR_PIPE_CONNECTED:
							pendingIO = false;
							m_state = State::Waiting;
							break;
						default:
							pendingIO = false;
							break;
					}
				}
			} else {
				DWORD timeout = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
					m_parent->GetWaitTimeOut()).count();
				DWORD res = WaitForSingleObjectEx(ovWrite.hEvent, timeout, true);
				switch (res) {
					case WAIT_OBJECT_0:
					{
						DWORD bytes;
						ResetEvent(ovWrite.hEvent);
						BOOL success = GetOverlappedResult(m_handle, &ovWrite, &bytes, FALSE);
						if (success) {
							m_state = State::Waiting;
						} else {
							// Error?
						}
						pendingIO = false;
					}
						break;
					case WAIT_TIMEOUT:
						break;
					default:
						pendingIO = false;
						break;
				}
			}
		} else if (m_state == State::Waiting) {
			if (!PeekNamedPipe(m_handle, NULL, NULL, NULL, NULL, NULL)) {
				DWORD err = GetLastError();
				err = err;
			}
			readIOtime = writeIOtime = std::chrono::high_resolution_clock::now();
			pendingIO = pendingRead = pendingWrite = false;
		} else if (m_state == State::Connected) {
			if (m_isServer) {
				if (!Good()) {
					Disconnect();
					m_state = State::Sleeping;
				}
			}

			// Read
			if (!pendingRead) {
				DWORD availBytes = 0;
				if (PeekNamedPipe(m_handle, NULL, NULL, NULL, &availBytes, NULL)) {
					if (availBytes > 0) {
						ReadFile(m_handle, msg.data(), availBytes, &actuallyRead, &ovRead);
						DWORD err = GetLastError();
						switch (err) {
							case ERROR_IO_PENDING:
								pendingRead = true;
								readIOtime = std::chrono::high_resolution_clock::now();
								break;
							case ERROR_PIPE_NOT_CONNECTED:
								if (m_isServer)
									Disconnect();
								m_state = State::Sleeping;
								break;
							default:
								err = err;
								break;
						}
					}
				}
			} else {
				if (HasOverlappedIoCompleted(&ovRead)) {
					DWORD bytes;
					BOOL success = GetOverlappedResult(m_handle, &ovRead, &bytes, FALSE);
					if (success) {
						ResetEvent(ovRead.hEvent);
						pendingRead = false;
						std::cout << "read  " << (std::chrono::high_resolution_clock::now() - readIOtime).count() << " ns" << std::endl;
						readIOtime = std::chrono::high_resolution_clock::now();
						std::unique_lock<std::mutex> ulock(m_readLock);
						m_readQueue.push(msg);
					} else {
						DWORD err = GetLastError();
						pendingRead = false;
						// Error?
					}
				}
			}

			// Write/Read Tasks
			if (!pendingWrite) {
				std::unique_lock<std::mutex> ulock(m_writeLock);
				if (m_writeQueue.size() > 0) {
					auto& m = m_writeQueue.front();
					DWORD bytes;
					WriteFile(m_handle, m.data(), m.size(), &bytes, &ovWrite);
					DWORD err = GetLastError();
					switch (err) {
						case ERROR_IO_PENDING:
							pendingWrite = true;
							writeIOtime = std::chrono::high_resolution_clock::now();
							break;
						case ERROR_PIPE_NOT_CONNECTED:
							if (m_isServer)
								Disconnect();
							m_state = State::Sleeping;
							break;
					}
				}
			} else {
				if (HasOverlappedIoCompleted(&ovWrite)) {
					DWORD bytes;
					BOOL success = GetOverlappedResult(m_handle, &ovWrite, &bytes, FALSE);
					if (success) {
						ResetEvent(ovWrite.hEvent);
						pendingWrite = false;
						std::cout << "write " << (std::chrono::high_resolution_clock::now() - writeIOtime).count() << " ns" << std::endl;
						writeIOtime = std::chrono::high_resolution_clock::now();
						std::unique_lock<std::mutex> ulock(m_writeLock);
						m_writeQueue.pop();
					} else {
						DWORD err = GetLastError();
						pendingWrite = false;
						// Error?
					}
				}
			}

			if (m_isServer) {
				auto wtime = std::chrono::high_resolution_clock::now() - writeIOtime;
				auto rtime = std::chrono::high_resolution_clock::now() - readIOtime;
				if ((wtime > m_parent->GetSendTimeOut()) && (rtime > m_parent->GetReceiveTimeOut())) {
					CancelIo(m_handle);
					DisconnectNamedPipe(m_handle);
					pendingRead = pendingWrite = pendingIO = false;
					m_state = State::Sleeping;
				}
			}
		}
	}

	destroyOverlapped(ovWrite);
}

void OS::NamedSocketConnectionWindows::createOverlapped(OVERLAPPED& ov) {
	memset(&ov, 0, sizeof(OVERLAPPED));
	ov.hEvent = CreateEvent(NULL, true, false, NULL);
}

void OS::NamedSocketConnectionWindows::destroyOverlapped(OVERLAPPED& ov) {
	CloseHandle(ov.hEvent);
	memset(&ov, 0, sizeof(OVERLAPPED));
}

#pragma endregion Named Socket Connection Windows
