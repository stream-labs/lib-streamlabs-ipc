// Code to test the raw functionality of a os::namedsocket

#include <iostream>
#include <chrono>
#include <varargs.h>
#include "os-namedsocket.hpp"
#include <inttypes.h>
#include <cstdarg>
#include <map>

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

class Timer {
	std::map<std::chrono::nanoseconds, size_t> timings;
	size_t calls = 0;

	protected:
	void track(std::chrono::nanoseconds dur) {
		auto el = timings.find(dur);
		if (el != timings.end()) {
			el->second++;
		} else {
			timings.insert(std::make_pair(dur, 1));
		}
		calls++;
	}

	public:
	Timer() {

	}

	~Timer() {

	}

	class Instance {
		Timer* parent;
		std::chrono::high_resolution_clock::time_point begin;

		public:
		inline __fastcall Instance(Timer* parent) : parent(parent) {
			begin = std::chrono::high_resolution_clock::now();
		}

		inline __fastcall ~Instance() {
			auto end = std::chrono::high_resolution_clock::now();
			if (parent) {
				auto dur = end - begin;
				parent->track(std::chrono::duration_cast<std::chrono::nanoseconds>(dur));
			}
		}

		void cancel() {
			parent = nullptr;
		}

		void reparent(Timer* new_parent) {
			parent = new_parent;
		}
	};

	std::unique_ptr<Instance> inline __fastcall track() {
		return std::make_unique<Instance>(this);
	}

	uint64_t count() {
		return calls;
	}

	std::chrono::nanoseconds total() {
		if (timings.size() == 0) {
			return std::chrono::nanoseconds(0);
		}

		std::chrono::nanoseconds val = std::chrono::nanoseconds(0);
		for (auto el : timings) {
			val += el.first * el.second;
		}
		return val;
	}

	double_t average() {
		if (timings.size() == 0) {
			return 0;
		}

		double_t val = 0;
		for (auto el : timings) {
			val += el.first.count() * el.second;
		}
		return (val / calls);
	}

	std::chrono::nanoseconds percentile(double_t pct, bool by_time = false) {
		if (timings.size() == 0) {
			return std::chrono::nanoseconds(0);
		}

		// Should we gather a percentile by time, or by calls?
		if (by_time) {
			// By time, so find the largest and smallest value.
			// This can be used for median, but not average.

			std::chrono::nanoseconds smallest, largest;
			smallest = timings.begin()->first;
			largest = timings.rbegin()->first;

			for (auto el : timings) {
				double_t el_pct = (double_t((el.first - smallest).count()) / double_t(largest.count()));

				if (is_equal(pct, el_pct, 0.0005)) {
					return el.first;
				}
			}

			return timings.rbegin()->first;
		} else {
			uint64_t curr_accu = 0;
			if (is_equal(pct, 0.0, 0.0005)) {
				return timings.begin()->first;
			}

			for (auto el : timings) {
				uint64_t last_accu = curr_accu;
				curr_accu += el.second;

				double_t last_pct = double_t(last_accu) / double_t(calls);
				double_t curr_pct = double_t(curr_accu) / double_t(calls);

				if ((last_pct < pct) && ((curr_pct > pct) || is_equal(pct, curr_pct, 0.0005))) {
					return el.first;
				}
			}

			return timings.rbegin()->first;
		}
	}
};

#define CONN "HelloWorldIPC"
#define CLIENTCOUNT 2

#define FORMAT_TITLE   "%-16s | %8s | %12s | %12s | %12s | %12s | %12s"
#define FORMAT_CONTENT "%-16s | %8llu | %12llu | %12llu | %12llu | %12llu | %12llu"

static int server(int argc, char* argv[]);
static int client(int argc, char* argv[]);

int main(int argc, char* argv[]) {
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

	Timer tmrLoop;
	Timer tmrReadAll, tmrReadSuccess;
	Timer tmrWrite;
	Timer tmrProcess;

	auto tmrLoopInst = tmrLoop.track();
	while (ptr->good()) {
		auto tmrProcessInst = tmrProcess.track();
		auto tmrReadInst = tmrReadAll.track();
		auto tmrReadSuccessInst = tmrReadSuccess.track();
		os::error readError = ptr->read(&storedChar, 1, readLength);
		if (readError == os::error::Ok) {
			tmrReadInst.reset();
			tmrReadSuccessInst.reset();

			buf.resize(1);
			buf[0] = storedChar;
		} else if (readError == os::error::MoreData) {
			tmrReadInst.reset();

			avail = ptr->read_avail();
			buf.resize(avail + readLength);

			readError = ptr->read(buf.data() + 1, avail, readLength);
			if (readError == os::error::Ok) {
				tmrReadSuccessInst.reset();
				buf[0] = storedChar;
			} else {
				tmrReadSuccessInst->cancel();
				continue;
			}
		} else if (readError != os::error::Ok) {
			tmrReadSuccessInst->cancel();
			tmrReadInst.reset();
			tmrProcessInst->cancel();

			//blog("%s", os::to_string(readError));
			continue;
		}
		//blog("%llu: Message", ptr->get_client_id());

		auto tmrWriteInst = tmrWrite.track();
		size_t writeLength = ptr->write(buf);
		if (writeLength != buf.size()) {
			tmrWriteInst->cancel();
			tmrProcessInst->cancel();
			blog("%llu: Send Buffer full.", ptr->get_client_id());
			continue;
		}
		//blog("%llu: Message Reply", ptr->get_client_id());
		ptr->flush();
		Sleep(0);
		tmrProcessInst.reset();
	}
	tmrLoopInst.reset();

	blog("%4llu Timings",
		ptr->get_client_id());
	blog(FORMAT_TITLE,
		"Type", "Calls", "Total", "Average", "50th Pct", "99th Pct", "99.9th Pct");
	blog("-----------------+----------+--------------+--------------+--------------+--------------+-------------");
	blog(FORMAT_CONTENT,
		"Loop",
		tmrLoop.count(),
		tmrLoop.total().count(),
		uint64_t(tmrLoop.average()),
		tmrLoop.percentile(0.50).count(),
		tmrLoop.percentile(0.99).count(),
		tmrLoop.percentile(0.999).count());
	blog(FORMAT_CONTENT,
		"Read (All)",
		tmrReadAll.count(),
		tmrReadAll.total().count(),
		uint64_t(tmrReadAll.average()),
		tmrReadAll.percentile(0.50).count(),
		tmrReadAll.percentile(0.99).count(),
		tmrReadAll.percentile(0.999).count());
	blog(FORMAT_CONTENT,
		"Read (EC:OK)",
		tmrReadSuccess.count(),
		tmrReadSuccess.total().count(),
		uint64_t(tmrReadSuccess.average()),
		tmrReadSuccess.percentile(0.50).count(),
		tmrReadSuccess.percentile(0.99).count(),
		tmrReadSuccess.percentile(0.999).count());
	blog(FORMAT_CONTENT,
		"Write",
		tmrWrite.count(),
		tmrWrite.total().count(),
		uint64_t(tmrWrite.average()),
		tmrWrite.percentile(0.50).count(),
		tmrWrite.percentile(0.99).count(),
		tmrWrite.percentile(0.999).count());
	blog(FORMAT_CONTENT,
		"Process",
		tmrProcess.count(),
		tmrProcess.total().count(),
		uint64_t(tmrProcess.average()),
		tmrProcess.percentile(0.50).count(),
		tmrProcess.percentile(0.99).count(),
		tmrProcess.percentile(0.999).count());
	blog("");

	return 0;
}

int server(int argc, char* argv[]) {
	blog("Starting server...");

	std::unique_ptr<os::named_socket> socket = os::named_socket::create();
	socket->set_receive_timeout(std::chrono::nanoseconds(1000000ull));
	socket->set_send_timeout(std::chrono::nanoseconds(1000000ull));
	socket->set_send_buffer_size(128 * 1024 * 1024);
	socket->set_receive_buffer_size(128 * 1024 * 1024);
	if (!socket->listen(CONN, 8)) {
		blog("Failed to start server.");
		std::cin.get();
		return -1;
	}

	blog("Spawning %lld clients.", (int64_t)CLIENTCOUNT);
	for (size_t idx = 0; idx < CLIENTCOUNT; idx++) {
		spawn(std::string(argv[0]), '"' + std::string(argv[0]) + '"' + " client", get_working_directory());
	}

	blog("Waiting for data...");
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

void clientThread(std::shared_ptr<os::named_socket> socket, std::vector<char>* databuf, uint64_t* inbox, uint64_t* outbox, bool* stop) {
	std::vector<char> buf;
	size_t readLength = 0;
	char storedChar = 0;
	while (socket->get_connection()->good() && !*stop) {
		os::error readError = socket->get_connection()->read(&storedChar, 1, readLength);
		if (readError == os::error::Ok) {
			buf.resize(1);
			buf[0] = storedChar;
		} else if (readError == os::error::MoreData) {
			size_t avail = socket->get_connection()->read_avail();
			buf.resize(avail + 1);
			if (socket->get_connection()->read(buf.data() + 1, buf.size(), avail) != os::error::Ok) {
				if (buf.size() != 10 + (*inbox % 20)) {
					std::cout << "Size changes failure, should " << (10 + (*inbox % 20)) << " have " << buf.size() << std::endl;
				}
				std::cout << "Catastrophic failure" << std::endl;
				break;
			}
			*inbox = *inbox + 1;
			buf[0] = storedChar;
		} else {
			continue;
		}
	}
	return;
}

int client(int argc, char* argv[]) {
	blog("Starting client...");

	std::shared_ptr<os::named_socket> socket = os::named_socket::create();
	socket->set_receive_timeout(std::chrono::nanoseconds(1000000ull));
	socket->set_send_timeout(std::chrono::nanoseconds(1000000ull));
	socket->set_send_buffer_size(128 * 1024 * 1024);
	socket->set_receive_buffer_size(128 * 1024 * 1024);
	if (!socket->connect(CONN)) {
		blog("Failed starting client.");
		return -1;
	}

	uint64_t inbox = 0;
	uint64_t outbox = 0;
	uint64_t total = 100000;
	auto tpstart = std::chrono::high_resolution_clock::now();
	std::vector<char> buf;
	size_t readLength = 0;
	char storedChar = 0;
	bool stop = false;

	Timer tmrLoop;
	Timer tmrWrite;
	Timer tmrReadAll, tmrReadSuccess;
	Timer tmrProcess;

	std::vector<char> databuf;
	const size_t maxsize = 1 * 128 * 1024;
	databuf.resize(maxsize);
	for (size_t n = 0; n < databuf.size(); n++) {
		databuf[n] = (1 << n) * (n / 4) * (n * n);
	}

	std::thread worker = std::thread(clientThread, socket, &databuf, &inbox, &outbox, &stop);

	auto tmrLoopInst = tmrLoop.track();
	while (socket->get_connection()->good()) {
		if (outbox < total) {
			auto tmrProcessInst = tmrProcess.track();
			auto tmrWriteInst = tmrWrite.track();
			size_t temp;
			if (socket->get_connection()->write(databuf.data(), 10 + (outbox % 20), temp) == os::error::Ok) {
				tmrWriteInst.reset();
				outbox++;
				socket->get_connection()->flush();
				Sleep(0);
			} else {
				blog("Write failed.");
				tmrWriteInst->cancel();
				tmrWriteInst.reset();
				tmrProcessInst->cancel();
				tmrProcessInst.reset();
				continue;
			}

			tmrProcessInst.reset();
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		//blog("%llu %llu", outbox, inbox);
		if (outbox == total) {
			if (inbox == total) {
				break;
			}
		}
	}
	tmrLoopInst.reset();

	stop = true;
	if (worker.joinable())
		worker.join();

	socket->close();
	socket = nullptr;

	blog(FORMAT_TITLE,
		"Type", "Calls", "Total", "Average", "50th Pct", "99th Pct", "99.9th Pct");
	blog("-----------------+----------+--------------+--------------+--------------+--------------+-------------");
	blog(FORMAT_CONTENT,
		"Loop",
		tmrLoop.count(),
		tmrLoop.total().count(),
		uint64_t(tmrLoop.average()),
		tmrLoop.percentile(0.50).count(),
		tmrLoop.percentile(0.99).count(),
		tmrLoop.percentile(0.999).count());
	blog(FORMAT_CONTENT,
		"Write",
		tmrWrite.count(),
		tmrWrite.total().count(),
		uint64_t(tmrWrite.average()),
		tmrWrite.percentile(0.50).count(),
		tmrWrite.percentile(0.99).count(),
		tmrWrite.percentile(0.999).count());
	blog(FORMAT_CONTENT,
		"Read (All)",
		tmrReadAll.count(),
		tmrReadAll.total().count(),
		uint64_t(tmrReadAll.average()),
		tmrReadAll.percentile(0.50).count(),
		tmrReadAll.percentile(0.99).count(),
		tmrReadAll.percentile(0.999).count());
	blog(FORMAT_CONTENT,
		"Read (EC:OK)",
		tmrReadSuccess.count(),
		tmrReadSuccess.total().count(),
		uint64_t(tmrReadSuccess.average()),
		tmrReadSuccess.percentile(0.50).count(),
		tmrReadSuccess.percentile(0.99).count(),
		tmrReadSuccess.percentile(0.999).count());
	blog(FORMAT_CONTENT,
		"Process",
		tmrProcess.count(),
		tmrProcess.total().count(),
		uint64_t(tmrProcess.average()),
		tmrProcess.percentile(0.50).count(),
		tmrProcess.percentile(0.99).count(),
		tmrProcess.percentile(0.999).count());
	blog("");

	std::cin.get();
	return 0;
}
