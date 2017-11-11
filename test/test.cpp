#include "ipc-server.hpp"
#include "ipc-client.hpp"
#include <iostream>
#include <thread>
#include <sstream>
#include <chrono>
#include <ctime>
#include <mutex>

#ifdef _WIN32
#include <windows.h>

#endif

bool killswitch = false;
std::string sockpath = "";

std::chrono::high_resolution_clock hrc;

std::chrono::high_resolution_clock::time_point tp = std::chrono::high_resolution_clock::now();

static std::vector<char> varlog(std::string format, va_list args) {
	std::vector<char> buffer(65535, '\0');
	buffer.resize(vsprintf_s(buffer.data(), buffer.size(), format.c_str(), args));
	return buffer;
}

static void blog(std::string format, ...) {
	auto duration = (std::chrono::high_resolution_clock::now() - tp);

	auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
	duration -= hours;
	auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
	duration -= minutes;
	auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
	duration -= seconds;
	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
	duration -= milliseconds;
	auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
	duration -= microseconds;
	auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

	std::chrono::high_resolution_clock::now();

	va_list argptr;
	va_start(argptr, format);
	std::vector<char> formatted = varlog(format, argptr);
	va_end(argptr);

	std::vector<char> buf(65535, '\0');
	std::string timeformat = "%.2d:%.2d:%.2d.%.3d.%.3d.%.3d:  %*s\n";// "%*s";
	size_t formattedsize = formatted.size();
	sprintf_s(
		buf.data(),
		buf.size(),
		timeformat.c_str(),
		hours.count(),
		minutes.count(),
		seconds.count(),
		milliseconds.count(),
		microseconds.count(),
		nanoseconds.count(),
		formattedsize, formatted.data());
	std::cout << buf.data();
}

std::mutex lck;
size_t msgcount = 0;
uint64_t msgtime = 0;
std::chrono::high_resolution_clock::time_point msglast = std::chrono::high_resolution_clock::time_point(std::chrono::nanoseconds(0));

bool serverOnConnect(void* data, OS::ClientId_t id) {
	msglast = std::chrono::high_resolution_clock::now();
	blog(std::string("Server: Connect from %lld."), id);
	return true;
}

void serverOnDisconnect(void* data, OS::ClientId_t id) {
	blog(std::string("Server: Disconnect by %lld."), id);
}

void serverOnMessage(void* data, OS::ClientId_t id, const std::vector<char>& msg) {
	//blog(std::string("Server: Incoming Message by %lld: %*s"), id, msg.size(), msg.data());
	std::unique_lock<std::mutex> ulock(lck);
	msgcount++;
	msgtime += (std::chrono::high_resolution_clock::now() - msglast).count();
	msglast = std::chrono::high_resolution_clock::now();
}

int serverThread() {
	IPC::Server server;
	server.SetMessageHandler(serverOnMessage, nullptr);
	server.SetConnectHandler(serverOnConnect, nullptr);
	server.SetDisconnectHandler(serverOnDisconnect, nullptr);
	blog("Server: Starting...");
	server.Initialize(sockpath);
	blog("Server: Started.");
	while (!killswitch) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	blog("Server: %lld Messages", msgcount);
	blog("Server: %lld ns Time", msgtime);
	blog("Server: %lld ns Average Time", uint64_t(msgtime / double_t(msgcount)));
	blog("Server: Shutting down...");
	server.Finalize();
	blog("Server: Shut down.");
	return 0;
}

int clientThread() {
	blog("Client: Starting...");
	IPC::Client client = { sockpath };
	blog("Client: Started.");
	std::vector<char> data(5);
	data[0] = 'P'; data[1] = 'i'; data[2] = 'n'; data[3] = 'g'; data[4] = '\0';
	auto bg = std::chrono::high_resolution_clock::now();

	const size_t maxmsg = 100000;
	for (size_t idx = 0; idx < maxmsg; idx++) {
		client.RawWrite(data);
	}
	size_t ns = (std::chrono::high_resolution_clock::now() - bg).count();
	blog("Client: Sent %lld in %lld ns, average %lld ns.", maxmsg, ns, uint64_t(ns / double_t(maxmsg)));
	blog("Client: Shutting down...");
	return 0;
}

int main(int argc, char** argv) {
	for (int idx = 0; idx < argc; idx++) {
		blog("[%d] %s", idx, argv[idx]);
	}

	if (argc == 1 || argc != 3) {
		std::cout << "Usage: " << argv[0] << " <server|client> <socketPath>" << std::endl;
		std::cin.get();
		return -1;
	}

	bool isServer = strcmp(argv[1], "server") == 0;
	sockpath = argv[2];
	std::thread worker;
	if (isServer) {
		worker = std::thread(serverThread);

	#ifdef _WIN32
		std::stringstream args;
		args << '"' << argv[0] << '"' << " ";
		args << "client"
			<< " "
			<< argv[2];
		std::string farg = args.str();
		std::vector<char> buf = std::vector<char>(farg.data(), farg.data() + farg.length() + 1);

		blog("Starting Client...");

		STARTUPINFO si; memset(&si, 0, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);

		PROCESS_INFORMATION pi; memset(&pi, 0, sizeof(PROCESS_INFORMATION));
		if (!CreateProcessA(
			NULL,
			buf.data(),
			NULL,
			NULL,
			false,
			0,
			NULL,
			NULL,
			&si,
			&pi)) {
			blog("Starting Client failed.");
		}
	#endif
		std::cin.get();
		killswitch = true;
	} else {
		worker = std::thread(clientThread);
	}
	worker.join();

	return 0;
}
