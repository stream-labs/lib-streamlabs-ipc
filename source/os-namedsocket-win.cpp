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
	DWORD availBytes = 0;
	PeekNamedPipe(m_handle, NULL, NULL, NULL, NULL, &availBytes);
	return availBytes;
}

size_t OS::NamedSocketConnectionWindows::Read(char* buf, size_t length) {
	if (length > m_parent->GetReceiveBufferSize())
		return -1;
	
	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));
	ov.hEvent = CreateEvent(NULL, true, false, NULL);

	DWORD bytesRead = 0;
	ReadFile(m_handle, buf, length, &bytesRead, &ov);
	DWORD res = GetLastError();
	if (res == ERROR_SUCCESS) {
		if (!GetOverlappedResult(m_handle, &ov, &bytesRead, false)) {
			goto read_fail;
		}
		goto read_success;
	} else if (res != ERROR_IO_PENDING) {
		std::cout << "IO not pending!" << std::endl;
		goto read_fail;
	}

	DWORD waitTime = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
		m_parent->GetReceiveTimeOut()).count();
	res = WaitForSingleObjectEx(m_handle, waitTime, false);
	if (res == WAIT_TIMEOUT) {
		if (!HasOverlappedIoCompleted(&ov)) {
			goto read_fail;
		} else {
			if (!GetOverlappedResult(m_handle, &ov, &bytesRead, false)) {
				goto read_fail;
			}
		}
	} else if (res == WAIT_ABANDONED) {
		goto read_fail;
	} else if (res == WAIT_FAILED) {
		goto read_fail;
	}

read_success:
	CloseHandle(ov.hEvent);
	return bytesRead;

read_fail:
	CancelIoEx(m_handle, &ov);
	CloseHandle(ov.hEvent);
	return 0;
}

size_t OS::NamedSocketConnectionWindows::Read(std::vector<char>& out) {
	size_t readLength = out.size();
	if (readLength > m_parent->GetReceiveBufferSize())
		readLength = m_parent->GetReceiveBufferSize();
	return Read(out.data(), readLength);
}

std::vector<char> OS::NamedSocketConnectionWindows::Read() {
	size_t bytes = ReadAvail();
	if (bytes == 0)
		return std::vector<char>();

	std::vector<char> buf(bytes);
	size_t read = Read(buf);
	if (read == 0ull || read == std::numeric_limits<size_t>::max())
		return std::vector<char>();
	
	buf.resize(read);
	return std::move(buf);
}

size_t OS::NamedSocketConnectionWindows::Write(const char* buf, size_t length) {
	if (length >= m_parent->GetSendBufferSize())
		return -1;

	OVERLAPPED ov;
	memset(&ov, 0, sizeof(ov));
	ov.hEvent = CreateEvent(NULL, true, false, NULL);

	DWORD bytesWritten = 0;
	WriteFile(m_handle, buf, length, &bytesWritten, &ov);
	DWORD res = GetLastError();
	if (res == ERROR_SUCCESS) {
		goto write_success;
	} else if (res != ERROR_IO_PENDING) {
		goto write_fail;
	}
	
	DWORD waitTime = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
		m_parent->GetSendTimeOut()).count();
	res = WaitForSingleObjectEx(m_handle, waitTime, false);
	if (res == WAIT_TIMEOUT) {
		if (!HasOverlappedIoCompleted(&ov)) {
			goto write_fail;
		} else {
			if (!GetOverlappedResult(m_handle, &ov, &bytesWritten, false)) {
				goto write_fail;
			}
		}
	} else if (res == WAIT_ABANDONED) {
		goto write_fail;
	} else if (res == WAIT_FAILED) {
		goto write_fail;
	}

write_success:
	CloseHandle(ov.hEvent);
	return bytesWritten;

write_fail:
	CancelIoEx(m_handle, &ov);
	CloseHandle(ov.hEvent);
	return 0;
}

size_t OS::NamedSocketConnectionWindows::Write(const std::vector<char>& buf) {
	return Write(buf.data(), buf.size());
}

OS::ClientId_t OS::NamedSocketConnectionWindows::GetClientId() {
	return static_cast<OS::ClientId_t>(reinterpret_cast<intptr_t>(m_handle));
}

void OS::NamedSocketConnectionWindows::ThreadMain(void* ptr) {
	reinterpret_cast<NamedSocketConnectionWindows*>(ptr)->ThreadLocal();
}

void OS::NamedSocketConnectionWindows::ThreadLocal() {
	OVERLAPPED ovWrite;
	createOverlapped(ovWrite);
	
	bool pendingIO = false;
	
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
						BOOL success = GetOverlappedResult(m_handle, &ovWrite, &bytes, FALSE);
						ResetEvent(ovWrite.hEvent);
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
			pendingIO = false;
		} else if (m_state == State::Connected) {
			if (m_isServer) {
				if (!Good()) {
					Disconnect();
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
