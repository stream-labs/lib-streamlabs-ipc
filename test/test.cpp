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

using namespace std::chrono;
struct ClientData {
	high_resolution_clock::time_point connectTime;
	high_resolution_clock::time_point lastMessageTime;
	uint64_t messageCount, messageTotalTime;
	uint64_t replyCount, replyTotalTime;
};

std::map<OS::ClientId_t, ClientData> clientInfo;

bool serverOnConnect(void* data, OS::ClientId_t id) {
	ClientData cd;
	cd.connectTime = std::chrono::high_resolution_clock::now();
	cd.lastMessageTime = cd.connectTime;
	cd.messageCount = cd.messageTotalTime = 0;
	cd.replyCount = cd.replyTotalTime = 1;
	clientInfo.insert(std::make_pair(id, cd));
	blog(std::string("Server: Connect from %lld."), id);
	return true;
}

void serverOnDisconnect(void* data, OS::ClientId_t id) {
	ClientData& cd = clientInfo.at(id);
	blog("Server: Disconnect by %lld\n"
		"- Messages: %lld, Time: %lld ns, Average: %lld ns\n"
		"- Replies: %lld, Time: %lld ns, Average: %lld ns", id,
		cd.messageCount, cd.messageTotalTime, uint64_t(double_t(cd.messageTotalTime) / double_t(cd.messageCount)),
		cd.replyCount, cd.replyTotalTime, uint64_t(double_t(cd.replyTotalTime) / double_t(cd.replyCount))
	);
}

void serverOnMessage(void* data, OS::ClientId_t id, const std::vector<char>& msg) {
	ClientData& cd = clientInfo.at(id);
	uint64_t delta = duration_cast<nanoseconds>(high_resolution_clock::now() - cd.lastMessageTime).count();
	cd.lastMessageTime = high_resolution_clock::now();
	cd.messageCount++;
	cd.messageTotalTime += delta;

	// Reply?
	if ((cd.messageCount % 1000) == 0) {
		blog("Server: Messages by %lld so far: %lld, Time: %lld ns, Average: %lld ns",
			id, cd.messageCount, cd.messageTotalTime, uint64_t(double_t(cd.messageTotalTime) / double_t(cd.messageCount)));
		//cd.messageCount = cd.messageTotalTime = 0;
	}
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
	client.RawWrite(data);
	std::this_thread::sleep_for(std::chrono::milliseconds(1));

	const size_t maxmsg = 100000;
	size_t idx = 0;
	while (idx < maxmsg) {
		if (client.RawWrite(data) != 0)
			idx++;
		if ((idx % 1000) == 0)
			blog("Send! %lld", idx);
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

		std::cin.get();
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
			CREATE_NEW_CONSOLE,
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
		std::cin.get();
	}
	if (worker.joinable())
		worker.join();

	return 0;
}
