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

void os::overlapped_manager::create_overlapped(std::shared_ptr<OVERLAPPED>& ov) {
	ov = std::make_shared<OVERLAPPED>();
	ov->Internal = ov->InternalHigh = ov->Offset = ov->OffsetHigh = 0;
	ov->Pointer = nullptr;
	ov->hEvent = CreateEventW(NULL, false, false, NULL);
}

void os::overlapped_manager::destroy_overlapped(std::shared_ptr<OVERLAPPED>& ov) {
	CloseHandle(ov->hEvent);
	ov = nullptr;
}

void os::overlapped_manager::append_create_overlapped() {
	std::shared_ptr<OVERLAPPED> ov = nullptr;
	create_overlapped(ov);
	freeOverlapped.push(ov);
}

bool os::overlapped_manager::create_security_attributes() {
	DWORD dwRes;

	// Create a well-known SID for the Everyone group.
	if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
		SECURITY_WORLD_RID,
		0, 0, 0, 0, 0, 0, 0,
		&pEveryoneSID)) {
		std::cerr << "AllocateAndInitializeSid Error" << GetLastError() << std::endl;
		return false;
	}

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow Everyone read access to the key.
	ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESS));
	ea[0].grfAccessPermissions = KEY_READ;
	ea[0].grfAccessMode = SET_ACCESS;
	ea[0].grfInheritance = NO_INHERITANCE;
	ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	ea[0].Trustee.ptstrName = (LPTSTR)pEveryoneSID;

	// Create a SID for the BUILTIN\Administrators group.
	if (!AllocateAndInitializeSid(&SIDAuthNT, 2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&pAdminSID)) {
		std::cerr << "AllocateAndInitializeSid Error" << GetLastError() << std::endl;
		return false;
	}

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow the Administrators group full access to
	// the key.
	ea[1].grfAccessPermissions = KEY_ALL_ACCESS;
	ea[1].grfAccessMode = SET_ACCESS;
	ea[1].grfInheritance = NO_INHERITANCE;
	ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
	ea[1].Trustee.ptstrName = (LPTSTR)pAdminSID;

	// Create a new ACL that contains the new ACEs.
	dwRes = SetEntriesInAcl(2, ea, NULL, &pACL);
	if (ERROR_SUCCESS != dwRes) {
		std::cerr << "SetEntriesInAcl Error" << GetLastError() << std::endl;
		return false;
	}

	// Initialize a security descriptor.  
	pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR,
		SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (NULL == pSD) {
		std::cerr << "LocalAlloc Error" << GetLastError() << std::endl;
		return false;
	}

	if (!InitializeSecurityDescriptor(pSD,
		SECURITY_DESCRIPTOR_REVISION)) {
		std::cerr << "InitializeSecurityDescriptor Error" << GetLastError() << std::endl;
		return false;
	}

	// Add the ACL to the security descriptor. 
	if (!SetSecurityDescriptorDacl(pSD,
		TRUE,     // bDaclPresent flag   
		pACL,
		FALSE))   // not a default DACL 
	{
		std::cerr << "SetSecurityDescriptorDacl Error" << GetLastError() << std::endl;
		return false;
	}

	// Initialize a security attributes structure.
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = pSD;
	sa.bInheritHandle = FALSE;
	return true;
}

void os::overlapped_manager::destroy_security_attributes() {
	if (pEveryoneSID)
		FreeSid(pEveryoneSID);
	if (pAdminSID)
		FreeSid(pAdminSID);
	if (pACL)
		LocalFree(pACL);
	if (pSD)
		LocalFree(pSD);
}

os::overlapped_manager::overlapped_manager() {
	if (!create_security_attributes()) {
		destroy_security_attributes();
		throw std::runtime_error("can't create security attributes");
	}

	// Prepare for 8 queued writes.
	for (size_t idx = 0; idx < 8; idx++) {
		append_create_overlapped();
	}
}

os::overlapped_manager::~overlapped_manager() {
	std::shared_ptr<OVERLAPPED> ov = nullptr;
	while (freeOverlapped.size() > 0) {
		ov = freeOverlapped.front();
		freeOverlapped.pop();
		destroy_overlapped(ov);
	}

	while (usedOverlapped.size() > 0) {
		ov = usedOverlapped.front();
		usedOverlapped.pop_front();
		destroy_overlapped(ov);
	}

	destroy_security_attributes();
}

std::shared_ptr<OVERLAPPED> os::overlapped_manager::alloc() {
	std::unique_lock<std::mutex> ulock(mtx);
	if (freeOverlapped.size() == 0) {
		append_create_overlapped();
	}

	std::shared_ptr<OVERLAPPED> ov = freeOverlapped.front();
	freeOverlapped.pop();
	return ov;
}

void os::overlapped_manager::free(std::shared_ptr<OVERLAPPED> ov) {
	std::unique_lock<std::mutex> ulock(mtx);
	for (auto itr = usedOverlapped.begin(); itr != usedOverlapped.end(); itr++) {
		if (*itr == ov) {
			usedOverlapped.erase(itr);
		}
	}
	ResetEvent(ov->hEvent);
	freeOverlapped.push(ov);
}

std::unique_ptr<os::named_socket> os::named_socket::create() {
	return std::make_unique<os::named_socket_win>();
}

#pragma region NamedSocketWindows
#pragma region De-/Constructor
os::named_socket_win::named_socket_win() {}

os::named_socket_win::~named_socket_win() {
	close();
}
#pragma endregion De-/Constructor

#pragma region Listen/Connect/Close
bool os::named_socket_win::_listen(std::string path, size_t backlog) {
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
			std::shared_ptr<named_socket_connection_win> ptr;
			if (n == 0) {
				ptr = std::make_shared<named_socket_connection_win>(this, m_pipeName,
					m_openMode | FILE_FLAG_FIRST_PIPE_INSTANCE, m_pipeMode);
			} else {
				ptr = std::make_shared<named_socket_connection_win>(this, m_pipeName,
					m_openMode, m_pipeMode);
			}
			m_connections.push_back(ptr);
		}
	} catch (...) {
		return false;
	}

	return true;
}

bool os::named_socket_win::_connect(std::string path) {
	// Validate and generate socket name
	if (path.length() > 255) // Path can't be larger than 255 characters, limit set by WinAPI.
		return false; // !TODO! Throw some kind of error to signal why it failed.
	for (char& v : path)
		if (v == '\\')
			v = '/';
	m_pipeName = "\\\\.\\pipe\\" + path;

	try {
		std::shared_ptr<named_socket_connection_win> ptr =
			std::make_shared<named_socket_connection_win>(this, m_pipeName);
		m_connections.push_back(ptr);
	} catch (...) {
		return false;
	}

	return true;
}

bool os::named_socket_win::_close() {
	m_connections.clear();
	return true;
}


#pragma endregion Listen/Connect/Close
#pragma endregion NamedSocketWindows

#pragma region Named Socket Connection Windows
os::named_socket_connection_win::named_socket_connection_win(os::named_socket* parent,
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
		static_cast<DWORD>(parent->get_send_buffer_size()),
		static_cast<DWORD>(parent->get_receive_buffer_size()),
		static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(parent->get_wait_timeout()).count()),
		&m_securityAttributes);
	if (m_handle == INVALID_HANDLE_VALUE) {
		DWORD err = GetLastError();
		throw std::runtime_error("Unable to create socket.");
	}

	// Threading
	m_stopWorkers = false;
	m_managerThread = std::thread(thread_main, this);
}

os::named_socket_connection_win::named_socket_connection_win(os::named_socket* parent, std::string path)
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
		m_handle = CreateFile(pipeName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
		if (m_handle != INVALID_HANDLE_VALUE)
			break;
		DWORD err = GetLastError();
		if (err != ERROR_PIPE_BUSY)
			throw std::runtime_error("Unable to create socket.");

		DWORD timeout = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
			parent->get_wait_timeout()).count();
		if (!WaitNamedPipe(pipeName, timeout)) {
			if (attempt < 4) {
				continue;
			} else {
				throw std::runtime_error("Unable to create socket.");
			}
		}
	}
	m_state = state::Connected;

	DWORD flags = PIPE_READMODE_MESSAGE | PIPE_WAIT;
	if (!SetNamedPipeHandleState(m_handle, &flags, NULL, NULL)) {
		// well, what do we do now?
	}

	// Threading
	m_stopWorkers = false;
	m_managerThread = std::thread(thread_main, this);
}

os::named_socket_connection_win::~named_socket_connection_win() {
	// Stop Threading
	m_stopWorkers = true;
	m_managerThread.join();

	CancelIo(m_handle);
	if (m_isServer)
		disconnect();
	else
		CloseHandle(m_handle);
}

bool os::named_socket_connection_win::is_waiting() {
	return m_state == state::Waiting;
}

bool os::named_socket_connection_win::is_connected() {
	return m_state == state::Connected;
}

bool os::named_socket_connection_win::connect() {
	if (!m_isServer)
		throw std::logic_error("Clients are automatically connected.");

	if (m_state != state::Waiting)
		return false;

	m_state = state::Connected;
	return true;
}

bool os::named_socket_connection_win::disconnect() {
	if (!m_isServer)
		throw std::logic_error("Clients are automatically disconnected.");

	if (m_state != state::Connected)
		return false;

	return !!DisconnectNamedPipe(m_handle);
}

bool os::named_socket_connection_win::eof() {
	return bad() || is_waiting() || (read_avail() == 0);
}

bool os::named_socket_connection_win::good() {
	if (m_state != state::Connected) {
		return false;
	}

	return true;
}

size_t os::named_socket_connection_win::avail() {
	DWORD totalBytes = 0;
	PeekNamedPipe(m_handle, NULL, NULL, NULL, &totalBytes, NULL);
	return totalBytes;
}

size_t os::named_socket_connection_win::read_avail() {
	DWORD availBytes = 0;
	DWORD totalBytes = 0;
	PeekNamedPipe(m_handle, NULL, NULL, NULL, &totalBytes, &availBytes);
	return availBytes;
}

size_t os::named_socket_connection_win::read(char* buf, size_t length) {
	DWORD bytesRead = 0;
	DWORD errorCode = 0;

	if (length > m_parent->get_receive_buffer_size())
		return -1;

	std::shared_ptr<OVERLAPPED> ov = dynamic_cast<named_socket_win*>(m_parent)->ovm.alloc();

	// Attempt to read from the handle.
	SetLastError(ERROR_SUCCESS);
	ReadFile(m_handle, buf, (DWORD)length, &bytesRead, ov.get());

	// Test for actual return code.
	errorCode = GetLastError();
	if (errorCode == ERROR_SUCCESS) {
		// ERROR_SUCCESS should mean that we immediately read everything.
		if (!GetOverlappedResult(m_handle, ov.get(), &bytesRead, false)) {
			// In case it didn't, just wait as normal.
			goto resume_wait;
		}
		// In case it did, continue to success.
		goto read_success;
	} else if (errorCode == ERROR_BROKEN_PIPE) {
		// Disconnected.
		m_state = state::Disconnected;
		goto read_fail;
	} else if (errorCode != ERROR_IO_PENDING) {
		// Any other code than ERROR_IO_PENDING means that there's nothing to read from.
		goto read_fail;
	}

resume_wait:
	// Now wait until we actually have a result available.
	DWORD waitTime = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
		m_parent->get_receive_timeout()).count();
	errorCode = WaitForSingleObjectEx(m_handle, waitTime, true);
	if (errorCode == WAIT_TIMEOUT) {
		if (!HasOverlappedIoCompleted(ov.get())) {
			// If we timed out and it still hasn't completed, consider the request failed.
			goto read_fail;
		} else {
			if (!GetOverlappedResult(m_handle, ov.get(), &bytesRead, false)) {
				// Overlapped IO completed, but we can't get any result back.
				errorCode = GetLastError();
				goto read_fail;
			}
		}
	} else if (errorCode == WAIT_ABANDONED) {
		goto read_fail;
	} else if (errorCode == WAIT_FAILED) {
		goto read_fail;
	}

read_success:
	dynamic_cast<named_socket_win*>(m_parent)->ovm.free(ov);
	return bytesRead;

read_fail:
	CancelIoEx(m_handle, ov.get());
	dynamic_cast<named_socket_win*>(m_parent)->ovm.free(ov);
	return 0;
}

size_t os::named_socket_connection_win::read(std::vector<char>& out) {
	size_t readLength = out.size();
	return read(out.data(), readLength);
}

std::vector<char> os::named_socket_connection_win::read() {
	size_t bytes = read_avail();
	if (bytes == 0)
		return std::vector<char>();

	std::vector<char> buf(bytes);
	size_t cread = read(buf);
	if (cread == 0ull || cread == std::numeric_limits<size_t>::max())
		return std::vector<char>();

	buf.resize(cread);
	return std::move(buf);
}

os::error os::named_socket_connection_win::read(char* buffer, size_t length, size_t& read_length) {
	if (m_state != state::Connected) {
		read_length = 0;
		return os::error::Disconnected;
	}

	DWORD bytesRead = 0;
	DWORD errorCode = 0;
	os::error returnCode = os::error::Ok;
	std::shared_ptr<OVERLAPPED> ov = dynamic_cast<named_socket_win*>(m_parent)->ovm.alloc();

	// Attempt to read from the handle.
	SetLastError(ERROR_SUCCESS);
	ReadFile(m_handle, buffer, (DWORD)length, &bytesRead, ov.get());

test_error:
	// Test for actual return code.
	errorCode = GetLastError();
	if (errorCode == ERROR_SUCCESS) {
		// ERROR_SUCCESS should mean that we immediately read everything.
		if (!GetOverlappedResult(m_handle, ov.get(), &bytesRead, false)) {
			// In case it didn't, just wait as normal.
			goto resume_wait;
		}

		// In case it did, continue to success.
		returnCode = os::error::Ok;
		read_length = bytesRead;
		goto read_success;
	} else if (errorCode == ERROR_MORE_DATA) {
		// ERROR_MORE_DATA means that there is additional data to be read.
		GetOverlappedResult(m_handle, ov.get(), &bytesRead, false);

		returnCode = os::error::MoreData;
		read_length = bytesRead;
		goto read_success;
	} else if (errorCode == ERROR_BROKEN_PIPE) {
		// Disconnected.
		m_state = state::Disconnected;
		returnCode = os::error::Disconnected;
		goto read_fail;
	} else if (errorCode != ERROR_IO_PENDING) {
		// Any other code than ERROR_IO_PENDING means that there's nothing to read from.
		returnCode = os::error::Error;
		goto read_fail;
	}

resume_wait:
	// Now wait until we actually have a result available.
	DWORD waitTime = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
		m_parent->get_receive_timeout()).count();
	errorCode = WaitForSingleObjectEx(m_handle, waitTime, true);
	if (errorCode == WAIT_TIMEOUT) {
		if (!HasOverlappedIoCompleted(ov.get())) {
			// If we timed out and it still hasn't completed, consider the request failed.
			returnCode = os::error::TimedOut;
			goto read_fail;
		} else {
			if (!GetOverlappedResult(m_handle, ov.get(), &bytesRead, false)) {
				// Overlapped IO completed, but we can't get any result back.
				errorCode = GetLastError();
				goto test_error;
			}
		}
	} else if (errorCode == WAIT_ABANDONED) {
		returnCode = os::error::Disconnected;
		goto read_fail;
	} else if (errorCode == WAIT_FAILED) {
		errorCode = GetLastError();
		returnCode = os::error::Error;
		goto read_fail;
	}
	read_length = bytesRead;
	returnCode = os::error::Ok;

read_success:
	dynamic_cast<named_socket_win*>(m_parent)->ovm.free(ov);
	return returnCode;

read_fail:
	CancelIoEx(m_handle, ov.get());
	dynamic_cast<named_socket_win*>(m_parent)->ovm.free(ov);
	return returnCode;
}

size_t os::named_socket_connection_win::write(const char* buf, size_t length) {
	DWORD bytesWritten = 0;

	std::shared_ptr<OVERLAPPED> ov = dynamic_cast<named_socket_win*>(m_parent)->ovm.alloc();

	SetLastError(ERROR_SUCCESS);
	WriteFile(m_handle, buf, (DWORD)length, &bytesWritten, ov.get());

	DWORD res = GetLastError();
	if (res == ERROR_SUCCESS) {
		if (HasOverlappedIoCompleted(ov.get())) {
			goto write_success;
		}
	} else if (res == ERROR_BROKEN_PIPE) {
		// Disconnected.
		m_state = state::Disconnected;
		goto write_fail;
	} else if (res != ERROR_IO_PENDING) {
		goto write_fail;
	}

	DWORD waitTime = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
		m_parent->get_send_timeout()).count();
	res = WaitForSingleObject(m_handle, waitTime);
	if (res == WAIT_TIMEOUT || res == WAIT_IO_COMPLETION) {
		if (!HasOverlappedIoCompleted(ov.get())) {
			goto write_fail;
		} else {
			if (!GetOverlappedResult(m_handle, ov.get(), &bytesWritten, false)) {
				goto write_fail;
			}
		}
	} else if (res == WAIT_ABANDONED) {
		goto write_fail;
	} else if (res == WAIT_FAILED) {
		goto write_fail;
	}

write_success:
	dynamic_cast<named_socket_win*>(m_parent)->ovm.free(ov);
	return bytesWritten;

write_fail:
	CancelIoEx(m_handle, ov.get());
	dynamic_cast<named_socket_win*>(m_parent)->ovm.free(ov);
	return 0;
}

size_t os::named_socket_connection_win::write(const std::vector<char>& buf) {
	return write(buf.data(), buf.size());
}

os::error os::named_socket_connection_win::write(char const* buffer, size_t const length, size_t& write_length) {
	if (m_state != state::Connected) {
		write_length = 0;
		return os::error::Disconnected;
	}

	DWORD bytesWritten = 0;
	DWORD errorCode = ERROR_SUCCESS;
	os::error returnCode = os::error::Ok;
	std::shared_ptr<OVERLAPPED> ov = dynamic_cast<named_socket_win*>(m_parent)->ovm.alloc();

	SetLastError(ERROR_SUCCESS);
	WriteFile(m_handle, buffer, (DWORD)length, NULL, ov.get());

write_test_error:
	// Test for actual return code.
	errorCode = GetLastError();
	switch (errorCode) {
		case ERROR_SUCCESS:
			// ERROR_SUCCESS should mean that we immediately read everything.
			if (!GetOverlappedResult(m_handle, ov.get(), &bytesWritten, false)) {
				// In case it didn't, just wait as normal.
				goto write_resume_wait;
			}

			// In case it did, continue to success.
			returnCode = os::error::Ok;
			write_length = bytesWritten;
			goto write_success;
			break;
		case ERROR_IO_PENDING:
			goto write_resume_wait;
			break;
		default:
			returnCode = os::error::Error;
			goto write_fail;
	}

write_resume_wait:
	// Now wait until we actually have a result available.
	DWORD waitTime = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
		m_parent->get_send_timeout()).count();
	errorCode = WaitForSingleObject(m_handle, waitTime);
	if (errorCode == WAIT_TIMEOUT || errorCode == WAIT_IO_COMPLETION) {
		if (!HasOverlappedIoCompleted(ov.get())) {
			// If we timed out and it still hasn't completed, consider the request failed.
			returnCode = os::error::TimedOut;
			goto write_fail;
		} else {
			if (!GetOverlappedResult(m_handle, ov.get(), &bytesWritten, false)) {
				// Overlapped IO completed, but we can't get any result back.
				errorCode = GetLastError();
				goto write_test_error;
			}
		}
	} else if (errorCode == WAIT_ABANDONED) {
		returnCode = os::error::Disconnected;
		goto write_fail;
	} else if (errorCode == WAIT_FAILED) {
		errorCode = GetLastError();
		returnCode = os::error::Error;
		goto write_fail;
	}
	write_length = bytesWritten;
	returnCode = os::error::Ok;

write_success:
	dynamic_cast<named_socket_win*>(m_parent)->ovm.free(ov);
	return returnCode;

write_fail:
	CancelIoEx(m_handle, ov.get());
	dynamic_cast<named_socket_win*>(m_parent)->ovm.free(ov);
	return returnCode;
}

os::error os::named_socket_connection_win::flush() {
	if (!good()) {
		return os::error::Disconnected;
	}
	SetLastError(ERROR_SUCCESS);
	FlushFileBuffers(m_handle);
	switch (GetLastError()) {
		case ERROR_INVALID_HANDLE:
			return os::error::Error;
		case ERROR_SUCCESS:
			return os::error::Ok;
		default:
			return os::error::Error;
	}
}

os::ClientId_t os::named_socket_connection_win::get_client_id() {
	return static_cast<os::ClientId_t>(reinterpret_cast<intptr_t>(m_handle));
}

void os::named_socket_connection_win::thread_main(void* ptr) {
	reinterpret_cast<named_socket_connection_win*>(ptr)->threadlocal();
}

void os::named_socket_connection_win::threadlocal() {
	OVERLAPPED ovWrite;
	create_overlapped(ovWrite);

	bool pendingIO = false;

	while (!m_stopWorkers) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

		if (m_state == state::Sleeping) {
			if (!pendingIO) {
				if (!ConnectNamedPipe(m_handle, &ovWrite)) {
					DWORD err = GetLastError();
					switch (err) {
						case ERROR_IO_PENDING:
							pendingIO = true;
							break;
						case ERROR_PIPE_CONNECTED:
							pendingIO = false;
							m_state = state::Waiting;
							break;
						default:
							pendingIO = false;
							break;
					}
				}
			} else {
				DWORD timeout = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(
					m_parent->get_wait_timeout()).count();
				DWORD res = WaitForSingleObjectEx(ovWrite.hEvent, timeout, true);
				switch (res) {
					case WAIT_OBJECT_0:
					{
						DWORD bytes;
						BOOL success = GetOverlappedResult(m_handle, &ovWrite, &bytes, FALSE);
						ResetEvent(ovWrite.hEvent);
						if (success) {
							m_state = state::Waiting;
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
		} else if (m_state == state::Waiting) {
			if (!PeekNamedPipe(m_handle, NULL, NULL, NULL, NULL, NULL)) {
				DWORD err = GetLastError();
				err = err;
			}
			pendingIO = false;
		} else if (m_state == state::Connected) {
			if (m_isServer) {
				if (!good()) {
					m_state = state::Disconnected;
				}
			}
		} else if (m_state == state::Disconnected) {
			if (m_isServer) {
				disconnect();
				m_state = state::Sleeping;
			}
		}
	}

	destroy_overlapped(ovWrite);
}

void os::named_socket_connection_win::create_overlapped(OVERLAPPED& ov) {
	memset(&ov, 0, sizeof(OVERLAPPED));
	ov.hEvent = CreateEvent(NULL, true, false, NULL);
}

void os::named_socket_connection_win::destroy_overlapped(OVERLAPPED& ov) {
	CloseHandle(ov.hEvent);
	memset(&ov, 0, sizeof(OVERLAPPED));
}

#pragma endregion Named Socket Connection Windows
