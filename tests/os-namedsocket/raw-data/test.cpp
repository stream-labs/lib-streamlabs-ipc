// Code to test the raw functionality of a os::namedsocket

#include <iostream>
#include <chrono>
#include <varargs.h>
#include "os-namedsocket.hpp"
#include <inttypes.h>
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

#define CONN "HelloWorldIPC"
#define CLIENTCOUNT 8

static int server(int argc, char* argv[]);
static int client(int argc, char* argv[]);

int main(int argc, char* argv[]) {
	if ((argc == 2) || (strcmp(argv[0], "client") == 0)) {
		client(argc, argv);
	} else {
		server(argc, argv);
	}
}

int serverInstanceThread(std::shared_ptr<os::named_socket_connection> ptr) {
	while (ptr->good()) {
		size_t msg = ptr->read_avail();
		if (msg > 0) {
			ptr->write(ptr->read());
			//blog("Reflected message with side %" PRIu64 ".", msg);
		}
		if (ptr->read_avail() == 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	return 0;
}

int server(int argc, char* argv[]) {
	blog("Starting server...");

	std::unique_ptr<os::named_socket> socket = os::named_socket::create();
	if (!socket->listen(CONN, 8)) {
		blog("Failed to start server.");
		std::cin.get();
		return -1;
	}

	blog("Spawning %lld clients.", (int64_t)CLIENTCOUNT);
	for (size_t idx = 0; idx < CLIENTCOUNT; idx++) {
		spawn(argv[0], std::string(argv[0]) + "client", get_working_directory());
	}

	blog("Waiting for data...");
	bool doShutdown = false;
	std::list<std::thread> instances;
	while (!doShutdown) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		try {
			auto conn = socket->accept().lock();
			if (!conn) {
				continue;
			}

			conn->connect();
			instances.push_back(std::thread(serverInstanceThread, conn));
		} catch (...) {
			doShutdown = true;
		}
	}
	socket->close();

	for (auto& thr : instances) {
		if (thr.joinable())
			thr.join();
	}

	return 0;
}

int client(int argc, char* argv[]) {
	blog("Starting client...");

	std::unique_ptr<os::named_socket> socket = os::named_socket::create();
	if (!socket->connect(CONN)) {
		blog("Failed starting client.");
		std::cin.get();
		return -1;
	}

	uint64_t inbox = 0;
	uint64_t outbox = 0;
	uint64_t total = 10000;
	while (socket->get_connection()->good()) {
		//std::cout << inbox << ", " << outbox << ", " << total << "." << std::endl;
		if (outbox < total) {
			if (socket->get_connection()->write("Hello World\0", 11) == 11) {
				outbox++;
				if (outbox % 100 == 0) {
					blog("Sent %lld messages so far.", outbox, 0, 0, 0, 0, 0);
				}
			}
		}

		if (inbox < total) {
			size_t msg = socket->get_connection()->read_avail();
			while (msg > 0) {
				auto buf = socket->get_connection()->read();
				inbox++;
				if (inbox % 100 == 0) {
					blog("Received %lld messages so far.", inbox, 0, 0, 0, 0, 0);
				}
				msg = socket->get_connection()->read_avail();
			}
		}

		if (outbox == total) {
			if (inbox == total) {
				break;
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	socket->close();

	return 0;
}
