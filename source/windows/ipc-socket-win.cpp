#include "ipc-socket-win.hpp"
#include "utility.hpp"

#include <codecvt>
#include <locale>
#include <string>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define DEFAULT_BUFFER_SIZE 16 * 1024 * 1024
#define DEFAULT_WAIT_TIME 100

#define MAX_PATH_MINUS_PREFIX (MAX_PATH - 9)

inline std::wstring make_wide_string(std::string text) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	return converter.from_bytes(text);
}

inline void validate_create_param(std::string name, size_t max_instances) {
	if (name.length() == 0) {
		throw std::invalid_argument("'name' can't be empty.");
	} else if (name.length() >= MAX_PATH_MINUS_PREFIX) {
		throw std::invalid_argument("'name' can't be longer than " TOSTRING(MAX_PATH_MINUS_PREFIX) " characters.");
	} else if (max_instances == 0) {
		throw std::invalid_argument("'max_instances' can't be zero.");
	} else if (max_instances > PIPE_UNLIMITED_INSTANCES) {
		throw std::invalid_argument("'max_instances' can't be greater than " TOSTRING(PIPE_UNLIMITED_INSTANCES));
	}
}

inline void validate_open_param(std::string name) {
	if (name.length() == 0) {
		throw std::invalid_argument("'name' can't be empty.");
	} else if (name.length() >= MAX_PATH_MINUS_PREFIX) {
		throw std::invalid_argument("'name' can't be longer than " TOSTRING(MAX_PATH_MINUS_PREFIX) " characters.");
	}
}

inline std::string make_windows_compatible(std::string &name) {
	std::string out = name;
	for (char &v : out) {
		if (v == '\\') {
			v = '/';
		}
	}
	return {"\\\\.\\pipe\\" + out};
}

inline void create_logic(HANDLE &handle, std::wstring name, size_t max_instances, os::windows::pipe_type type,
						 os::windows::pipe_read_mode mode, bool is_unique, SECURITY_ATTRIBUTES &attr) {
	DWORD pipe_open_mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED;
	DWORD pipe_type      = 0;
	DWORD pipe_read_mode = 0;

	if (is_unique) {
		pipe_open_mode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
	}

	switch (type) {
	case os::windows::pipe_type::Message:
		pipe_type = PIPE_TYPE_MESSAGE;
		break;
	default:
		pipe_type = PIPE_TYPE_BYTE;
		break;
	}

	switch (mode) {
	case os::windows::pipe_read_mode::Message:
		pipe_read_mode = PIPE_READMODE_MESSAGE;
		break;
	default:
		pipe_read_mode = PIPE_READMODE_BYTE;
		break;
	}

	SetLastError(ERROR_SUCCESS);
	handle = CreateNamedPipeW(name.c_str(), pipe_open_mode, pipe_type | pipe_read_mode | PIPE_WAIT,
							  DWORD(max_instances), DEFAULT_BUFFER_SIZE, DEFAULT_BUFFER_SIZE, DEFAULT_WAIT_TIME, &attr);
	if (handle == INVALID_HANDLE_VALUE) {
		std::vector<char> msg(2048);
		sprintf_s(msg.data(), msg.size(), "Creating Named Pipe failed with error code %lX.\0", GetLastError());
		throw std::runtime_error(msg.data());
	}
}

inline void open_logic(HANDLE &handle, std::wstring name, os::windows::pipe_read_mode mode) {
	SetLastError(ERROR_SUCCESS);
	handle = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
						 FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
	if (!handle || (GetLastError() != ERROR_SUCCESS)) {
		std::vector<char> msg(2048);
		sprintf_s(msg.data(), msg.size(), "Opening Named Pipe failed with error code %lX.\0", GetLastError());
		throw std::runtime_error(msg.data());
	}

	DWORD pipe_read_mode = PIPE_WAIT;
	switch (mode) {
	case os::windows::pipe_read_mode::Message:
		pipe_read_mode |= PIPE_READMODE_MESSAGE;
		break;
	default:
		pipe_read_mode |= PIPE_READMODE_BYTE;
		break;
	}

	SetLastError(ERROR_SUCCESS);
	if (!SetNamedPipeHandleState(handle, &pipe_read_mode, NULL, NULL)) {
		std::vector<char> msg(2048);
		sprintf_s(msg.data(), msg.size(), "Changing Named Pipe read mode failed with error code %lX.\0",
				  GetLastError());
		throw std::runtime_error(msg.data());
	}
}

std::unique_ptr<os::windows::socket_win> os::windows::socket_win::create(os::create_only_t, const std::string &name) {
    return std::make_unique<os::windows::socket_win>(os::create_only, name);
}

std::unique_ptr<os::windows::socket_win> os::windows::socket_win::create(os::open_only_t, const std::string &name) {
    return std::make_unique<os::windows::socket_win>(os::open_only, name);
}

os::windows::socket_win::socket_win() {
	handle  = INVALID_HANDLE_VALUE;
	created = false;
	security_attributes.nLength              = sizeof(SECURITY_ATTRIBUTES);
	security_attributes.lpSecurityDescriptor = nullptr;
	security_attributes.bInheritHandle       = true;
	set_connected(false);
}

os::windows::socket_win::socket_win(os::create_only_t,
    const std::string & name,
    size_t max_instances,
    pipe_type type,
    pipe_read_mode mode,
    bool is_unique) : socket_win() {
	validate_create_param(name, max_instances);

	std::wstring wide_name = make_wide_string(make_windows_compatible(name + '\0'));
	create_logic(handle, wide_name, max_instances, type, mode, is_unique, security_attributes);
	created = true;
}

os::windows::socket_win::socket_win(os::open_only_t,
    const std::string &name,
    pipe_read_mode mode) : socket_win() {
	validate_open_param(name);

	std::wstring wide_name = make_wide_string(make_windows_compatible(name + '\0'));
	open_logic(handle, wide_name, mode);
	set_connected(true);
}

os::windows::socket_win::~socket_win() {
	if (handle) {
		DisconnectNamedPipe(handle);
		CloseHandle(handle);
	}
}

void os::windows::socket_win::handle_accept_callback(os::error code, size_t length) {
	if (code == os::error::Connected || code == os::error::Success) {
		set_connected(true);
	} else {
		set_connected(false);
	}
}

bool os::windows::socket_win::is_created() {
    return created;
}

bool os::windows::socket_win::is_connected() {
	ULONG processId, sessionId;

	if (created) {
		if (!GetNamedPipeClientProcessId(handle, &processId)) {
			return false;
		}
		if (!GetNamedPipeClientSessionId(handle, &sessionId)) {
			return false;
		}
	} else {
		if (!GetNamedPipeServerProcessId(handle, &processId)) {
			return false;
		}
		if (!GetNamedPipeServerSessionId(handle, &sessionId)) {
			return false;
		}
	}

	if ((processId != remoteId.processId) || (processId == 0)) {
		remoteId.processId = 0;
		remoteId.sessionId = 0;
		return false;
	}

	if ((sessionId != remoteId.sessionId) || (sessionId == 0)) {
		remoteId.processId = 0;
		remoteId.sessionId = 0;
		return false;
	}

	if (!PeekNamedPipe(handle, NULL, NULL, NULL, NULL, NULL)) {
		DWORD err = GetLastError();
		return false;
	}

	remoteId = {sessionId, processId};
	return true;
}

void os::windows::socket_win::set_connected(bool is_connected) {
	if (is_connected) {
		ULONG processId, sessionId;

		if (created) {
			if (!GetNamedPipeClientProcessId(handle, &processId)) {
				return;
			}
			if (!GetNamedPipeClientSessionId(handle, &sessionId)) {
				return;
			}
		} else {
			if (!GetNamedPipeServerProcessId(handle, &processId)) {
				return;
			}
			if (!GetNamedPipeServerSessionId(handle, &sessionId)) {
				return;
			}
		}

		remoteId = {sessionId, processId};
	} else {
		remoteId = {0, 0};
	}
}

os::error os::windows::socket_win::accept(std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb) {
	if (!is_created()) {
		return os::error::Error;
	}

	std::shared_ptr<os::windows::async_request> ar = std::static_pointer_cast<os::windows::async_request>(op);
	if (!ar) {
		ar = std::make_shared<os::windows::async_request>();
		op = std::static_pointer_cast<os::async_op>(ar);
	}
	ar->set_callback(cb);
	ar->set_system_callback(
		std::bind(&os::windows::socket_win::handle_accept_callback, this, std::placeholders::_1, std::placeholders::_2));
	ar->set_handle(handle);

	SetLastError(ERROR_SUCCESS);
	BOOL suc = ConnectNamedPipe(handle, ar->get_overlapped_pointer());
	os::error ec = os::windows::utility::translate_error(GetLastError());

	if (ec != os::error::Pending && ec != os::error::Connected) {
		ar->call_callback(ec, 0);
		ar->cancel();
		
	} else {
		ar->set_valid(true);
		if (ec == os::error::Connected) {
			ar->call_callback(ec, 0);
		}
	}
	return ec;
}

os::error os::windows::socket_win::read(char *buffer, size_t buffer_length, std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb) {
	if (!is_connected()) {
		return os::error::Disconnected;
	}

	std::shared_ptr<os::windows::async_request> ar = std::static_pointer_cast<os::windows::async_request>(op);
	if (!ar) {
		ar = std::make_shared<os::windows::async_request>();
		op = std::static_pointer_cast<os::async_op>(ar);
	}
	ar->set_callback(cb);
	ar->set_handle(handle);

	SetLastError(ERROR_SUCCESS);
	BOOL suc = ReadFileEx(
		handle, buffer, DWORD(buffer_length),
		ar->get_overlapped_pointer(),
		(LPOVERLAPPED_COMPLETION_ROUTINE)&os::windows::async_request::completion_routine
	);

	os::error ec = utility::translate_error(GetLastError());

	if (suc == 0) {
		ar->call_callback(ec, buffer_length);
		ar->cancel();
	} else {
		ar->set_valid(true);
	}
	return ec;
}

os::error os::windows::socket_win::write(const char *buffer, size_t buffer_length, std::shared_ptr<os::async_op> &op,
				os::async_op_cb_t cb) {
	if (!is_connected()) {
		return os::error::Disconnected;
	}

	std::shared_ptr<os::windows::async_request> ar = std::static_pointer_cast<os::windows::async_request>(op);
	if (!ar) {
		ar = std::make_shared<os::windows::async_request>();
		op = std::static_pointer_cast<os::async_op>(ar);
	}
	ar->set_callback(cb);
	ar->set_handle(handle);

	SetLastError(ERROR_SUCCESS);
	BOOL  suc   = WriteFileEx(
		handle, buffer,
		DWORD(buffer_length),
		ar->get_overlapped_pointer(),
		(LPOVERLAPPED_COMPLETION_ROUTINE)&os::windows::async_request::completion_routine
	);

	os::error ec = utility::translate_error(GetLastError());

	if (suc == 0) {
		ar->call_callback(ec, buffer_length);
		ar->cancel();
	} else {
		ar->set_valid(true);
	}

	return ec;
}