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

bool killswitch = false;
std::string sockpath = "";
const char* longmessage = "Hey so this is a really long message okay? It about matches the size of an average call.\0";

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

IPC::Value callstuff(int64_t id, void* data, std::vector<IPC::Value> v) {
	auto& cd = clientInfo.at(id);

	auto tp = std::chrono::high_resolution_clock::now();
	cd.messageCount++;
	cd.messageTotalTime += (tp - cd.lastMessageTime).count();
	cd.lastMessageTime = tp;

	cd.replyCount++;
	IPC::Value val;
	val.type = IPC::Type::UInt64;
	val.value.ui64 = v.at(0).value.ui64;
	auto tp2 = std::chrono::high_resolution_clock::now();
	cd.replyTotalTime += (tp2 - tp).count();

	return val;
}

int serverThread() {
	IPC::Server server;
	IPC::Class cls("Hello");
	std::vector<IPC::Type> args; args.push_back(IPC::Type::UInt64);
	IPC::Function func("Ping", args);
	func.SetCallHandler(callstuff, nullptr);
	cls.RegisterFunction(std::make_shared<IPC::Function>(func));
	server.RegisterClass(cls);
	 
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

struct ClientOnly {
	uint64_t counter = 0;
	uint64_t timedelta = 0;
};

void incCtr(void* data, IPC::Value rval) {
	ClientOnly* co = (ClientOnly*)data;
	co->counter++;
}

int clientThread() {
	blog("Client: Starting...");
	IPC::Client client = { sockpath };
	blog("Client: Started.");

	std::vector<char> data(longmessage, longmessage+strlen(longmessage));
	auto bg = std::chrono::high_resolution_clock::now();
	while (!client.Authenticate()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		blog("Client: Failed to authenticate, retrying... (Ctrl-C to quit)");
	}

	const size_t maxmsg = 10000;
	size_t idx = 0;
	size_t failidx = 0;
	ClientOnly co;
	std::vector<IPC::Value> args;
	args.push_back(IPC::Value(0ull));
	while (idx < maxmsg) {
		uint64_t counter_last = co.counter;
		args.at(0).value.ui64 = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		if (client.Call("Hello", "Ping", args, incCtr, &co)) {
			while (counter_last == co.counter) {}
			co.timedelta = (std::chrono::high_resolution_clock::now().time_since_epoch().count() - args.at(0).value.ui64);
			idx++;
		} else {
			failidx++;
			if (failidx > 1000)
				break;
		}
	}
	size_t ns = (std::chrono::high_resolution_clock::now() - bg).count();

	while (co.counter < idx) {
		blog("Client: Waiting for replies... (%lld out of %lld)", co.counter, idx);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	blog("Client: Sent %lld messages (%lld errors) in %lld ns, average %lld ns.",
		maxmsg, failidx, ns, uint64_t(ns / double_t(maxmsg)));
	blog("Client: Received %lld responses in %lld ns, average %lld ns.",
		co.counter, co.timedelta, uint64_t(co.timedelta / double_t(co.counter)));
	
	blog("Client: Shutting down...");
	std::cin.get();
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
		killswitch = true;
	} else {
		worker = std::thread(clientThread);
	}
	if (worker.joinable())
		worker.join();

	return 0;
}
