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
#include <boost/algorithm/string/replace.hpp>

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

#pragma region Options
bool OS::NamedSocketWindows::SetReceiveBufferSize(size_t size) {
	if (m_isInitialized)
		return false;
	if (size == 0)
		return false;
	m_bufferSizeRecv = size;
	return true;
}

size_t OS::NamedSocketWindows::GetReceiveBufferSize() {
	return m_bufferSizeRecv;
}

bool OS::NamedSocketWindows::SetSendBufferSize(size_t size) {
	if (m_isInitialized)
		return false;
	if (size == 0)
		return false;
	m_bufferSizeSend = size;
	return true;
}

size_t OS::NamedSocketWindows::GetSendBufferSize() {
	return m_bufferSizeSend;
}

bool OS::NamedSocketWindows::SetDefaultTimeOut(std::chrono::nanoseconds time) {
	if (m_isInitialized)
		return false;
	std::chrono::milliseconds trueTime = std::chrono::duration_cast<std::chrono::milliseconds>(time);
	if (trueTime.count() <= 0)
		return false;
	m_timeOutDefault = trueTime;
	return true;
}

std::chrono::nanoseconds OS::NamedSocketWindows::GetDefaultTimeOut() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		m_timeOutDefault);
}

bool OS::NamedSocketWindows::SetWaitTimeOut(std::chrono::nanoseconds time) {
	if (m_isInitialized)
		return false;
	std::chrono::milliseconds trueTime = std::chrono::duration_cast<std::chrono::milliseconds>(time);
	if (trueTime.count() < 0)
		return false;
	m_timeOutWait = trueTime;
	return true;
}

std::chrono::nanoseconds OS::NamedSocketWindows::GetWaitTimeOut() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(m_timeOutWait);
}

bool OS::NamedSocketWindows::SetReceiveTimeOut(std::chrono::nanoseconds time) {
	if (m_isInitialized)
		return false;
	std::chrono::milliseconds trueTime = std::chrono::duration_cast<std::chrono::milliseconds>(time);
	if (trueTime.count() <= 0)
		return false;
	m_timeOutRecv = trueTime;
	return true;
}

std::chrono::nanoseconds OS::NamedSocketWindows::GetReceiveTimeOut() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(m_timeOutRecv);
}

bool OS::NamedSocketWindows::SetSendTimeOut(std::chrono::nanoseconds time) {
	if (m_isInitialized)
		return false;
	std::chrono::milliseconds trueTime = std::chrono::duration_cast<std::chrono::milliseconds>(time);
	if (trueTime.count() <= 0)
		return false;
	m_timeOutSend = trueTime;
	return true;
}

std::chrono::nanoseconds OS::NamedSocketWindows::GetSendTimeOut() {
	return std::chrono::duration_cast<std::chrono::nanoseconds>(m_timeOutSend);
}
#pragma endregion Options

#pragma region Listen/Connect/Close
bool OS::NamedSocketWindows::Listen(std::string path, size_t backlog) {
	if (m_isInitialized)
		return false;

	// Validate Arguments
	if (!is_valid_path(path))
		return false;

	m_isServer = true;

	// Create Security descriptors.	
	m_securityAttributes.bInheritHandle = true;
	m_securityAttributes.lpSecurityDescriptor = nullptr;
	m_securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);

	// Create pipe.
	m_pipeName = make_valid_path(path);
#ifdef UNICODE
	std::wstring pipeNameWS = m_pipeName;
	LPCTSTR pipeName = pipeNameWS.c_str();
#else
	LPCTSTR pipeName = m_pipeName.c_str();
#endif
	m_handleMain = CreateNamedPipe(pipeName,
		PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE/* | FILE_FLAG_OVERLAPPED*/,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
		PIPE_UNLIMITED_INSTANCES,
		(DWORD)m_bufferSizeSend, (DWORD)m_bufferSizeRecv,
		(DWORD)m_timeOutDefault.count(), &m_securityAttributes);
	if (m_handleMain == INVALID_HANDLE_VALUE)
		return false;

	if (GetLastError() != ERROR_SUCCESS)
		return false;

	m_handlesSleeping.push_back(m_handleMain);

	// Create backlog handles.
	bool failedBacklog = false;
	for (size_t n = 1; n < m_connectionBacklog; n++) {
		HANDLE handle = create_pipe();
		if (!handle) {
			failedBacklog = true;
			break;
		}
		m_handlesSleeping.push_back(handle);
	}
	if (failedBacklog) {
		Close();
		return false;
	}

	m_isInitialized = true;
	return true;
}

bool OS::NamedSocketWindows::Connect(std::string path) {
	if (m_isInitialized)
		return false;

	// Validate Arguments
	if (!is_valid_path(path))
		return false;

	m_isServer = false;

	// Connect to pipe.
	std::string fullPath = make_valid_path(path);
#ifdef UNICODE
	std::wstring pipeNameWS = fullPath;
	LPCTSTR pipeName = pipeNameWS.c_str();
#else
	LPCTSTR pipeName = fullPath.c_str();
#endif

	size_t attempts = 0;
	for (size_t attempt = 0; attempt < 5; attempt++) {
		m_handleMain = CreateFile(pipeName,
			GENERIC_READ | GENERIC_WRITE,
			0, NULL,
			OPEN_EXISTING,
			0, NULL);

		if (m_handleMain != INVALID_HANDLE_VALUE)
			break;

		if (GetLastError() != ERROR_PIPE_BUSY)
			return false;

		if (!WaitNamedPipe(pipeName, (DWORD)m_timeOutWait.count())) {
			if (attempt < 4) {
				continue;
			} else {
				return false;
			}
		}
	}
	m_clientConnection = std::make_shared<OS::NamedSocketConnectionWindows>(this, m_handleMain);

	m_isInitialized = true;
	return true;
}

bool OS::NamedSocketWindows::Close() {
	if (!m_isInitialized)
		return false;

	if (m_isServer) {
		{
			std::unique_lock<std::mutex> ulock(m_handlesWorkingMtx);
			for (HANDLE hndl : m_handlesWorking) {
				DisconnectNamedPipe(hndl);
				CloseHandle(hndl);
			}
			m_handlesWorking.clear();
		}
		{
			std::unique_lock<std::mutex> ulock(m_handlesAwakeMtx);
			for (HANDLE hndl : m_handlesAwake) {
				DisconnectNamedPipe(hndl);
				CloseHandle(hndl);
			}
			m_handlesAwake.clear();
		}
		{
			std::unique_lock<std::mutex> ulock(m_handlesSleepingMtx);
			for (HANDLE hndl : m_handlesSleeping) {
				CloseHandle(hndl);
			}
			m_handlesSleeping.clear();
		}
	} else {
		CloseHandle(m_handleMain);
	}

	m_isInitialized = false;
	return !m_isInitialized;
}
#pragma endregion Listen/Connect/Close

#pragma region Server Only
bool OS::NamedSocketWindows::Wait() {
	if (!m_isServer)
		return false;

	// Clear waiting handles first.
	{
		std::unique_lock<std::mutex> slock(m_handlesAwakeMtx);
		if (m_handlesAwake.size() > 0)
			return true;
	}

	std::unique_lock<std::mutex> slock(m_handlesSleepingMtx);
	if (m_handlesSleeping.size() > MAXIMUM_WAIT_OBJECTS) {
		// Workaround to extend the maximum wait-able objects limit.
		// This keeps it all in a single thread so we have less Kernel & Scheduler overhead.
		auto t_start = std::chrono::high_resolution_clock::now();
		auto t_end = std::chrono::high_resolution_clock::now();

		size_t objects = m_handlesSleeping.size() / MAXIMUM_WAIT_OBJECTS;
		size_t objectsRemaining = m_handlesSleeping.size() - (MAXIMUM_WAIT_OBJECTS * objects);

		while (std::chrono::duration_cast<std::chrono::milliseconds>(t_start - t_end) < m_timeOutWait) {
			for (size_t idx = 0; idx < m_handlesSleeping.size(); idx += MAXIMUM_WAIT_OBJECTS) {
				DWORD result = WaitForMultipleObjectsEx((DWORD)(m_handlesSleeping.size() - idx),
					m_handlesSleeping.data() + idx,
					FALSE,
					0,
					TRUE);
				if ((result >= WAIT_OBJECT_0) && (result < WAIT_OBJECT_0 + MAXIMUM_WAIT_OBJECTS)) {
					size_t nidx = result - WAIT_OBJECT_0 + idx;
					HANDLE hndl = m_handlesSleeping.at(nidx);
					m_handlesSleeping.erase(m_handlesSleeping.begin() + nidx);
					std::unique_lock<std::mutex> slock(m_handlesAwakeMtx);
					m_handlesAwake.push_back(hndl);
					return true;
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			t_end = std::chrono::high_resolution_clock::now();
		}
	} else {
		DWORD result = WaitForMultipleObjectsEx((DWORD)m_handlesSleeping.size(),
			m_handlesSleeping.data(),
			FALSE,
			(DWORD)m_timeOutWait.count(),
			TRUE);
		if ((result >= WAIT_OBJECT_0) && (result < WAIT_OBJECT_0 + m_handlesSleeping.size())) {
			size_t idx = result - WAIT_OBJECT_0;
			HANDLE hndl = m_handlesSleeping.at(idx);
			m_handlesSleeping.erase(m_handlesSleeping.begin() + idx);
			std::unique_lock<std::mutex> slock(m_handlesAwakeMtx);
			m_handlesAwake.push_back(hndl);
			return true;
		}
	}

	return false;
}

std::shared_ptr<OS::NamedSocketConnection> OS::NamedSocketWindows::Accept() {
	if (!m_isServer)
		return nullptr;

	std::unique_lock<std::mutex> slock(m_handlesAwakeMtx);
	if (m_handlesAwake.size() == 0)
		return nullptr;

	HANDLE hndl = m_handlesAwake.back();
	OVERLAPPED ov; memset(&ov, 0, sizeof(ov));
	BOOL result = ConnectNamedPipe(hndl, NULL);
	if (result == 0) {
		DWORD err = GetLastError();
		m_handlesAwake.pop_back();
		std::unique_lock<std::mutex> slock(m_handlesSleepingMtx);
		m_handlesSleeping.push_back(hndl);
		return nullptr;
	}

	std::shared_ptr<OS::NamedSocketConnectionWindows> client =
		std::make_shared<OS::NamedSocketConnectionWindows>(this, hndl);
	client->SetDestructorCallback(_ConnectionDestructorHandler, this);
	m_handlesAwake.pop_back();
	{
		std::unique_lock<std::mutex> slock(m_handlesWorkingMtx);
		m_handlesWorking.push_back(hndl);
	}

	hndl = create_pipe();
	if (hndl) {
		std::unique_lock<std::mutex> slock(m_handlesSleepingMtx);
		m_handlesSleeping.push_back(hndl);
	} else {
		// !TODO! Replace with proper warning.
	}

	return client;
}

bool OS::NamedSocketWindows::Disconnect(std::shared_ptr<OS::NamedSocketConnection> socket) {
	std::shared_ptr<OS::NamedSocketConnectionWindows> sock =
		std::dynamic_pointer_cast<OS::NamedSocketConnectionWindows>(socket);
	return DisconnectNamedPipe(sock->m_handle) != 0;
}

#pragma endregion Server Only

#pragma region Server & Client
bool OS::NamedSocketWindows::IsServer() {
	return m_isServer;
}

bool OS::NamedSocketWindows::IsClient() {
	return !m_isServer;
}
#pragma endregion Server & Client

#pragma region Client Only
std::shared_ptr<OS::NamedSocketConnection> OS::NamedSocketWindows::GetConnection() {
	return m_clientConnection;
}
#pragma endregion Client Only

#pragma region Utility Functions
bool OS::NamedSocketWindows::is_valid_path(std::string path) {
	// Path can't be larger than 255 characters, limit set by WinAPI.
	if (path.length() > 255)
		return false; // !TODO! Throw some kind of error to signal why it failed.

	return true;
}

LPCTSTR OS::NamedSocketWindows::make_valid_path(std::string path) {
	m_pipeName = "\\\\.\\pipe\\";

	// Convert path to proper Win32 Named Pipe name.
	/// Backslash is not allowed.
	boost::replace_all(path, "\\", "/");

	// Generate proper name.
	m_pipeName += path;
	return m_pipeName.data();
}

HANDLE OS::NamedSocketWindows::create_pipe() {
#ifdef UNICODE
	std::wstring pipeNameWS = m_pipeName;
	LPCTSTR pipeName = pipeNameWS.c_str();
#else
	LPCTSTR pipeName = m_pipeName.c_str();
#endif
	HANDLE handle = CreateNamedPipe(pipeName,
		PIPE_ACCESS_DUPLEX/* | FILE_FLAG_OVERLAPPED*/,
		PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
		PIPE_UNLIMITED_INSTANCES,
		(DWORD)m_bufferSizeSend, (DWORD)m_bufferSizeRecv,
		(DWORD)m_timeOutDefault.count(), &m_securityAttributes);
	if (!m_handleMain) {
		// !TODO! Use GetLastError to determine actual error.
		return nullptr;
	}

	return handle;
}
#pragma endregion Utility Functions

#pragma endregion NamedSocketWindows

void OS::NamedSocketWindows::_ConnectionDestructorHandler(void* data, OS::NamedSocketConnection* ptr) {
	OS::NamedSocketWindows* nsw = static_cast<OS::NamedSocketWindows*>(data);
	OS::NamedSocketConnectionWindows* sock = dynamic_cast<OS::NamedSocketConnectionWindows*>(ptr);
	{
		std::unique_lock<std::mutex> ulock(nsw->m_handlesWorkingMtx);
		for (size_t idx = 0; idx < nsw->m_handlesWorking.size(); idx++) {
			if (nsw->m_handlesWorking.at(idx) == sock->m_handle) {
				nsw->m_handlesWorking.erase(nsw->m_handlesWorking.begin() + idx);
			}
		}
	}
	{
		std::unique_lock<std::mutex> nlock(nsw->m_handlesSleepingMtx);
		nsw->m_handlesSleeping.push_back(sock->m_handle);
	}
}

OS::NamedSocketConnectionWindows::NamedSocketConnectionWindows(OS::NamedSocket* handler, HANDLE socket) {
	m_handle = socket;
	m_cbDestructor = nullptr;
	m_cbDestructorData = nullptr;
	OS::NamedSocketWindows* winhandler = dynamic_cast<OS::NamedSocketWindows*>(handler);
}

OS::NamedSocketConnectionWindows::~NamedSocketConnectionWindows() {
	if (m_cbDestructor)
		m_cbDestructor(m_cbDestructorData, this);
}

OS::ClientId_t OS::NamedSocketConnectionWindows::GetClientId() {
	ULONG pid;
	if (GetNamedPipeClientProcessId(m_handle, &pid)) {
		size_t uuid = reinterpret_cast<size_t>(m_handle);
		return uuid;
	} else {
		return 0;
	}
}

bool OS::NamedSocketConnectionWindows::Good() {
	ULONG pid;
	if (GetNamedPipeClientProcessId(m_handle, &pid)) {
		return pid != 0;
	}
	return false;
}

bool OS::NamedSocketConnectionWindows::Bad() {
	return !Good();
}

size_t OS::NamedSocketConnectionWindows::Write(const char* buf, size_t length) {
	DWORD msgLen = 0;
	DWORD success = WriteFile(m_handle, buf, (DWORD)length, &msgLen, NULL);
	return msgLen;
}

size_t OS::NamedSocketConnectionWindows::Write(const std::vector<char>& buf) {
	return Write(buf.data(), buf.size());
}

size_t OS::NamedSocketConnectionWindows::ReadAvail() {
	DWORD availBytes = 0;
	PeekNamedPipe(m_handle, NULL, NULL, NULL, &availBytes, NULL);
	return availBytes;
}

size_t OS::NamedSocketConnectionWindows::Read(char* buf, size_t length) {
	DWORD msgLen = 0;
	ReadFile(m_handle, buf, (DWORD)length, &msgLen, NULL);
	return msgLen;
}

size_t OS::NamedSocketConnectionWindows::Read(std::vector<char>& out) {
	return Read(out.data(), out.size());
}

std::vector<char> OS::NamedSocketConnectionWindows::Read() {
	DWORD msgLen = 0;
	PeekNamedPipe(m_handle, NULL, NULL, NULL, NULL, &msgLen);
	std::vector<char> buf(msgLen);
	Read(buf);
	return buf;
}

void OS::NamedSocketConnectionWindows::SetDestructorCallback(DestructorHandler_t func, void* data) {
	m_cbDestructor = func;
	m_cbDestructorData = data;
}
