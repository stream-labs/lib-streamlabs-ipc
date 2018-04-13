#include "ipc-server.hpp"
#include "ipc-client.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#include <chrono>
#include <ctime>
#include <mutex>
#include <vector>
#include <cstdarg>

#pragma region Logging
std::chrono::high_resolution_clock hrc;
std::chrono::high_resolution_clock::time_point tp = std::chrono::high_resolution_clock::now();

inline std::string varlog(const char* format, va_list& args) {
	size_t length = _vscprintf(format, args);
	std::vector<char> buf = std::vector<char>(length + 1, '\0');
	size_t written = vsprintf_s(buf.data(), buf.size(), format, args);
	return std::string(buf.begin(), buf.begin() + length);
}

static void blog(const char* format, ...) {
	va_list args;
	va_start(args, format);
	std::string text = varlog(format, args);
	va_end(args);

	auto timeSinceStart = (std::chrono::high_resolution_clock::now() - tp);
	auto hours = std::chrono::duration_cast<std::chrono::hours>(timeSinceStart);
	timeSinceStart -= hours;
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(timeSinceStart);
	timeSinceStart -= minutes;
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeSinceStart);
	timeSinceStart -= seconds;
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceStart);
	timeSinceStart -= milliseconds;
	auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(timeSinceStart);
	timeSinceStart -= microseconds;
	auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceStart);

	std::vector<char> timebuf(65535, '\0');
	std::string timeformat = "%.2d:%.2d:%.2d.%.3d.%.3d.%.3d:  %*s\n";// "%*s";
	sprintf_s(
		timebuf.data(),
		timebuf.size(),
		timeformat.c_str(),
		hours.count(),
		minutes.count(),
		seconds.count(),
		milliseconds.count(),
		microseconds.count(),
		nanoseconds.count(),
		text.length(), text.c_str());
	std::cout << timebuf.data();
}
#pragma endregion Logging

#pragma region Windows
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <wchar.h>

bool spawn(std::string program, std::string commandLine, std::string workingDirectory) {
	PROCESS_INFORMATION m_win32_processInformation;
	STARTUPINFOW m_win32_startupInfo;

	// Buffers
	std::vector<wchar_t> programBuf;
	std::vector<wchar_t> commandLineBuf;
	std::vector<wchar_t> workingDirectoryBuf;

	// Convert to WideChar
	DWORD wr;
	programBuf.resize(MultiByteToWideChar(CP_UTF8, 0,
		program.data(), (int)program.size(),
		nullptr, 0) + 1);
	wr = MultiByteToWideChar(CP_UTF8, 0,
		program.data(), (int)program.size(),
		programBuf.data(), (int)programBuf.size());
	if (wr == 0) {
		// Conversion failed.
		DWORD errorCode = GetLastError();
		return false;
	}

	commandLineBuf.resize(MultiByteToWideChar(CP_UTF8, 0,
		commandLine.data(), (int)commandLine.size(),
		nullptr, 0) + 1);
	wr = MultiByteToWideChar(CP_UTF8, 0,
		commandLine.data(), (int)commandLine.size(),
		commandLineBuf.data(), (int)commandLineBuf.size());
	if (wr == 0) {
		// Conversion failed.
		DWORD errorCode = GetLastError();
		return false;
	}

	if (workingDirectory.length() > 1) {
		workingDirectoryBuf.resize(MultiByteToWideChar(CP_UTF8, 0,
			workingDirectory.data(), (int)workingDirectory.size(),
			nullptr, 0) + 1);
		if (workingDirectoryBuf.size() > 0) {
			wr = MultiByteToWideChar(CP_UTF8, 0,
				workingDirectory.data(), (int)workingDirectory.size(),
				workingDirectoryBuf.data(), (int)workingDirectoryBuf.size());
			if (wr == 0) {
				// Conversion failed.
				DWORD errorCode = GetLastError();
				return false;
			}
		}
	}

	// Build information
	memset(&m_win32_startupInfo, 0, sizeof(m_win32_startupInfo));
	memset(&m_win32_processInformation, 0, sizeof(m_win32_processInformation));

	// Launch process
	size_t attempts = 0;
	while (!CreateProcessW(
		programBuf.data(),
		commandLineBuf.data(),
		nullptr,
		nullptr,
		false,
		CREATE_NEW_CONSOLE,
		nullptr,
		workingDirectory.length() > 0 ? workingDirectoryBuf.data() : nullptr,
		&m_win32_startupInfo,
		&m_win32_processInformation)) {
		if (attempts >= 5) {
			break;
		}
		attempts++;
		std::cerr << "Attempt " << attempts << ": Creating client failed." << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	if (attempts >= 5) {
		DWORD errorCode = GetLastError();
		return false;
	}
}

std::string get_working_directory() {
	std::vector<wchar_t> bufUTF16 = std::vector<wchar_t>(65535);
	std::vector<char> bufUTF8;

	_wgetcwd(bufUTF16.data(), bufUTF16.size());

	// Convert from Wide-char to UTF8
	DWORD bufferSize = WideCharToMultiByte(CP_UTF8, 0,
		bufUTF16.data(), bufUTF16.size(),
		nullptr, 0,
		NULL, NULL);
	bufUTF8.resize(bufferSize + 1);
	DWORD finalSize = WideCharToMultiByte(CP_UTF8, 0,
		bufUTF16.data(), bufUTF16.size(),
		bufUTF8.data(), bufUTF8.size(),
		NULL, NULL);
	if (finalSize == 0) {
		// Conversion failed.
		DWORD errorCode = GetLastError();
		return false;
	}

	return bufUTF8.data();
}

#endif
#pragma endregion Windows

#define CONN "HelloWorldIPC2"
#define CLIENTCOUNT 4ull

static int server(int argc, char* argv[]);
static int client(int argc, char* argv[]);

int main(int argc, char* argv[]) {
	if ((argc >= 2) || (strcmp(argv[0], "client") == 0)) {
		client(argc, argv);
	} else {
		server(argc, argv);
	}
}

static void function1(void* data, const int64_t id, const std::vector<ipc::value>& args, std::vector<ipc::value>& rval) {
	rval.resize(args.size());
	for (size_t idx = 0; idx < args.size(); idx++) {
		rval[idx] = args[idx];
	}
}

int server(int argc, char* argv[]) {
	blog("Starting server...");
	
	ipc::server socket;

	std::shared_ptr<ipc::collection> collection = std::make_shared<ipc::collection>("Default");
	collection->register_function(std::make_shared<ipc::function>("Function1", function1));
	socket.register_collection(collection);

	try {
		socket.initialize(CONN);
	} catch (...) {
		blog("Unable to start server.");
		std::cin.get();
		return -1;
	}

	blog("Spawning %llu clients...", CLIENTCOUNT);
	for (size_t idx = 0; idx < CLIENTCOUNT; idx++) {
		spawn(std::string(argv[0]), '"' + std::string(argv[0]) + '"' + " client", get_working_directory());
	}

	blog("Hit Enter to shut down server.");
	std::cin.get();
	
	blog("Shutting down server...");
	socket.finalize();

	return 0;
}

void client_call_handler(const void* data, const std::vector<ipc::value>& rval) {
	size_t* mydata = static_cast<size_t*>(const_cast<void*>(data));
	(*mydata)++;
}

int client(int argc, char* argv[]) {
	blog("Starting client...");

	std::shared_ptr<ipc::client> socket;

	try {
		socket = std::make_shared<ipc::client>(CONN);
	} catch (...) {
		blog("Unable to start client.");
		std::cin.get();
		return -1;
	}

	socket->authenticate();

	size_t inbox = 0, outbox = 0, total = 10000;
	blog("Attempting to make %llu calls...", total);

	auto tpstart = std::chrono::high_resolution_clock::now();
	while ((inbox < total) || (outbox < total)) {
		if (outbox < total) {
			if (!socket->call("Default", "Function1", {}, client_call_handler, &inbox)) {
				blog("Critical Failure: Could not call function.");
				break;
			}
			outbox++;
		}
		//blog("Called %llu times with %llu replies.", outbox, inbox);
	}

	while (inbox < outbox) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	auto tpend = std::chrono::high_resolution_clock::now();

	auto tpdurns = std::chrono::duration_cast<std::chrono::nanoseconds>(tpend - tpstart);
	auto tpdurms = std::chrono::duration_cast<std::chrono::milliseconds>(tpend - tpstart);

	blog("Sent %llu & Received %llu messages in %llu milliseconds.", outbox, inbox, tpdurms.count());
	blog("Average %llu ns per message.", tpdurns.count() / total);

	blog("Hit Enter to shut down client.");
	std::cin.get();

	blog("Shutting down client...");
	socket = nullptr;

	return 0;
}
