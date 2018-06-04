// Code to test the raw functionality of a os::namedsocket

#include <iostream>
#include <chrono>
#include <varargs.h>
#include "os-namedsocket.hpp"
#include "os-signal.hpp"
#include <inttypes.h>
#include <cstdarg>
#include <map>
#include <lib.h>

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

	return true;
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

template<typename T>
bool inline __fastcall is_equal(T a, T b, T edge) {
	return (abs(a - b) <= edge);
}

#define CONN "HelloWorldIPC"
#define CLIENTCOUNT 1

#define CLIENTSIGNALNAME_R "HelloWorldIPC_R"
#define CLIENTSIGNALNAME_W "HelloWorldIPC_W"

#define FORMAT_TITLE   "%-16s | %8s | %12s | %12s | %12s | %12s | %12s"
#define FORMAT_CONTENT "%-16s | %8llu | %12llu | %12llu | %12llu | %12llu | %12llu"

static int server(int argc, char* argv[]);
static int client(int argc, char* argv[]);

int main(int argc, char* argv[]) {
	shared::logger::to_stderr(false);
	shared::logger::to_stdout(true);
	shared::logger::to_debug(true);
	shared::logger::is_timestamp_relative_to_start(false);
	shared::logger::log("Logging Started.");

	if ((argc >= 2) || (strcmp(argv[0], "client") == 0)) {
		client(argc, argv);
	} else {
		server(argc, argv);
	}
}

int serverInstanceThread(std::shared_ptr<os::named_socket_connection> ptr) {
	std::vector<char> buf; buf.reserve(65535);
	size_t avail = 0;
	size_t index = 0;
	size_t readLength = 0;
	char storedChar = 0;

	shared::time::measure_timer tmrLoop;
	shared::time::measure_timer tmrReadAll, tmrReadSuccess;
	shared::time::measure_timer tmrWrite;
	shared::time::measure_timer tmrProcess;

	auto read_signal = os::signal::create(std::string(CLIENTSIGNALNAME_R));
	auto write_signal = os::signal::create(std::string(CLIENTSIGNALNAME_W));

	auto tmrLoopInst = tmrLoop.track();
	while (ptr->good()) {
		auto tmrProcessInst = tmrProcess.track();
		auto tmrReadInst = tmrReadAll.track();
		auto tmrReadSuccessInst = tmrReadSuccess.track();
		if (ptr->read_avail() == 0) {
			if (read_signal->wait(std::chrono::milliseconds(100)) != os::error::Ok) {
				tmrReadSuccessInst->cancel();
				tmrProcessInst->cancel();
				continue;
			}
		}
		size_t read_length = ptr->read_avail();
		buf.resize(read_length);
		os::error read_error = ptr->read(buf.data(), buf.size(), read_length);
		switch (read_error) {
			case os::error::Ok:
			{
				auto tmrWriteInst = tmrWrite.track();
				size_t write_length = 0;
				os::error write_error = ptr->write(buf.data(), buf.size(), write_length);
				if (write_length != buf.size()) {
					tmrWriteInst->cancel();
					tmrProcessInst->cancel();
					shared::logger::log("%llu: Send Buffer full.", ptr->get_client_id());
					continue;
				}
				write_signal->set();
				Sleep(0);
				break;
			}
			default:
				tmrReadSuccessInst->cancel();
				tmrProcessInst->cancel();
				break;
		}

		tmrProcessInst.reset();
	}
	tmrLoopInst.reset();

	shared::logger::log("%4llu Timings",
		ptr->get_client_id());
	shared::logger::log(FORMAT_TITLE,
		"Type", "Calls", "Total", "Average", "50th Pct", "99th Pct", "99.9th Pct");
	shared::logger::log("-----------------+----------+--------------+--------------+--------------+--------------+-------------");
	shared::logger::log(FORMAT_CONTENT,
		"Loop",
		tmrLoop.count(),
		tmrLoop.total().count(),
		uint64_t(tmrLoop.average()),
		tmrLoop.percentile(0.50).count(),
		tmrLoop.percentile(0.99).count(),
		tmrLoop.percentile(0.999).count());
	shared::logger::log(FORMAT_CONTENT,
		"Read (All)",
		tmrReadAll.count(),
		tmrReadAll.total().count(),
		uint64_t(tmrReadAll.average()),
		tmrReadAll.percentile(0.50).count(),
		tmrReadAll.percentile(0.99).count(),
		tmrReadAll.percentile(0.999).count());
	shared::logger::log(FORMAT_CONTENT,
		"Read (EC:OK)",
		tmrReadSuccess.count(),
		tmrReadSuccess.total().count(),
		uint64_t(tmrReadSuccess.average()),
		tmrReadSuccess.percentile(0.50).count(),
		tmrReadSuccess.percentile(0.99).count(),
		tmrReadSuccess.percentile(0.999).count());
	shared::logger::log(FORMAT_CONTENT,
		"Write",
		tmrWrite.count(),
		tmrWrite.total().count(),
		uint64_t(tmrWrite.average()),
		tmrWrite.percentile(0.50).count(),
		tmrWrite.percentile(0.99).count(),
		tmrWrite.percentile(0.999).count());
	shared::logger::log(FORMAT_CONTENT,
		"Process",
		tmrProcess.count(),
		tmrProcess.total().count(),
		uint64_t(tmrProcess.average()),
		tmrProcess.percentile(0.50).count(),
		tmrProcess.percentile(0.99).count(),
		tmrProcess.percentile(0.999).count());
	shared::logger::log("");

	return 0;
}

int server(int argc, char* argv[]) {
	shared::logger::log("Starting server...");

	std::unique_ptr<os::named_socket> socket = os::named_socket::create();
	socket->set_receive_timeout(std::chrono::nanoseconds(1000000ull));
	socket->set_send_timeout(std::chrono::nanoseconds(1000000ull));
	socket->set_send_buffer_size(128 * 1024 * 1024);
	socket->set_receive_buffer_size(128 * 1024 * 1024);
	if (!socket->listen(CONN, 8)) {
		shared::logger::log("Failed to start server.");
		std::cin.get();
		return -1;
	}

	shared::logger::log("Spawning %lld clients.", (int64_t)CLIENTCOUNT);
	for (size_t idx = 0; idx < CLIENTCOUNT; idx++) {
		spawn(std::string(argv[0]), '"' + std::string(argv[0]) + '"' + " client", get_working_directory());
	}

	shared::logger::log("Waiting for data...");
	bool doShutdown = false;
	std::list<std::thread> instances;
	while (!doShutdown) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
	shared::logger::log("Starting client...");

	std::shared_ptr<os::named_socket> socket = os::named_socket::create();
	socket->set_receive_timeout(std::chrono::nanoseconds(1000000ull));
	socket->set_send_timeout(std::chrono::nanoseconds(1000000ull));
	socket->set_send_buffer_size(128 * 1024 * 1024);
	socket->set_receive_buffer_size(128 * 1024 * 1024);
	if (!socket->connect(CONN)) {
		shared::logger::log("Failed starting client.");
		return -1;
	}

	uint64_t inbox = 0;
	uint64_t outbox = 0;
	uint64_t total = 10000;
	auto tpstart = std::chrono::high_resolution_clock::now();
	std::vector<char> buf;
	size_t readLength = 0;
	char storedChar = 0;
	bool stop = false;

	shared::time::measure_timer tmrLoop;
	shared::time::measure_timer tmrWrite;
	shared::time::measure_timer tmrReadAll, tmrReadSuccess;
	shared::time::measure_timer tmrProcess;

	std::vector<char> databuf;
	const size_t maxsize = 1024;
	databuf.resize(maxsize);
	for (size_t n = 0; n < databuf.size(); n++) {
		databuf[n] = char((1 << n) * (n / 4) * (n * n) % 256);
	}

	auto write_signal = os::signal::create(std::string(CLIENTSIGNALNAME_R));
	auto read_signal = os::signal::create(std::string(CLIENTSIGNALNAME_W));

	auto tmrLoopInst = tmrLoop.track();
	while (socket->get_connection()->good()) {
		if (outbox == total) {
			if (inbox == total) {
				break;
			}
		}

		if (outbox < total) {
			auto tmrWriteInst = tmrWrite.track();
			size_t temp;
			if (socket->get_connection()->write(databuf.data(), 10 + (outbox % 20), temp) == os::error::Ok) {
				tmrWriteInst.reset();
				outbox++;
				write_signal->set();
				Sleep(0);
			} else {
				shared::logger::log("Write failed.");
				tmrWriteInst->cancel();
				continue;
			}
		}

		if (inbox < total) {
			auto tra = tmrReadAll.track();
			auto trs = tmrReadSuccess.track();
			if (socket->get_connection()->read_avail() == 0) {
				if (read_signal->wait(std::chrono::milliseconds(1)) != os::error::Ok) {
					trs->cancel();
					continue;
				}
			}

			size_t read_length = socket->get_connection()->read_avail();
			buf.resize(read_length);
			os::error readError = socket->get_connection()->read(buf.data(), buf.size(), read_length);
			switch (readError) {
				case os::error::Ok:
					trs.reset();
					inbox++;
					break;
				default:
					trs->cancel();
					break;
			}
		}
	}
	tmrLoopInst.reset();
	
	socket->close();
	socket = nullptr;

	shared::logger::log(FORMAT_TITLE,
		"Type", "Calls", "Total", "Average", "50th Pct", "99th Pct", "99.9th Pct");
	shared::logger::log("-----------------+----------+--------------+--------------+--------------+--------------+-------------");
	shared::logger::log(FORMAT_CONTENT,
		"Loop",
		tmrLoop.count(),
		tmrLoop.total().count(),
		uint64_t(tmrLoop.average()),
		tmrLoop.percentile(0.50).count(),
		tmrLoop.percentile(0.99).count(),
		tmrLoop.percentile(0.999).count());
	shared::logger::log(FORMAT_CONTENT,
		"Write",
		tmrWrite.count(),
		tmrWrite.total().count(),
		uint64_t(tmrWrite.average()),
		tmrWrite.percentile(0.50).count(),
		tmrWrite.percentile(0.99).count(),
		tmrWrite.percentile(0.999).count());
	shared::logger::log(FORMAT_CONTENT,
		"Read (All)",
		tmrReadAll.count(),
		tmrReadAll.total().count(),
		uint64_t(tmrReadAll.average()),
		tmrReadAll.percentile(0.50).count(),
		tmrReadAll.percentile(0.99).count(),
		tmrReadAll.percentile(0.999).count());
	shared::logger::log(FORMAT_CONTENT,
		"Read (EC:OK)",
		tmrReadSuccess.count(),
		tmrReadSuccess.total().count(),
		uint64_t(tmrReadSuccess.average()),
		tmrReadSuccess.percentile(0.50).count(),
		tmrReadSuccess.percentile(0.99).count(),
		tmrReadSuccess.percentile(0.999).count());
	shared::logger::log(FORMAT_CONTENT,
		"Process",
		tmrProcess.count(),
		tmrProcess.total().count(),
		uint64_t(tmrProcess.average()),
		tmrProcess.percentile(0.50).count(),
		tmrProcess.percentile(0.99).count(),
		tmrProcess.percentile(0.999).count());
	shared::logger::log("");

	std::cin.get();
	return 0;
}
