// A custom IPC solution to bypass electron IPC.
// Copyright(C) 2017 Streamlabs (General Workings Inc)
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

#include "ipc-server.hpp"
#include "ipc-class.hpp"
#include "ipc-function.hpp"
#include <iostream>
#include <chrono>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef RegisterClass
#include <timeapi.h>
#endif


std::chrono::high_resolution_clock g_hrc;
std::chrono::high_resolution_clock::time_point g_loadTime = std::chrono::high_resolution_clock::now();

static std::vector<char> varlog(std::string format, va_list args) {
	std::vector<char> buffer(65535, '\0');
	buffer.resize(vsprintf_s(buffer.data(), buffer.size(), format.c_str(), args));
	return buffer;
}

static void blog(std::string format, ...) {
	auto duration = (std::chrono::high_resolution_clock::now() - g_loadTime);

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


IPC::Value ipcShutdown(int64_t, void* shutdown, std::vector<IPC::Value> vals) {
	blog("Shutdown");
	bool* ptrShutdown = (bool*)shutdown;
	*ptrShutdown = true;
	return IPC::Value();
}

IPC::Value ipcPing(int64_t id, void*, std::vector<IPC::Value> vals) {
	//blog("Ping");
	//std::cout << "Ping from " << id << std::endl;
	return IPC::Value(0ull);//vals.at(0);
}

IPC::Value ipcPingS(int64_t id, void*, std::vector<IPC::Value> vals) {
	//blog("PingS");
	//std::cout << "Ping from " << id << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return IPC::Value(0ull);
}

bool OnConnect(void*, OS::ClientId_t id) {
	blog("Connect from %lld", id);
	return true;
}

void OnDisconnect(void*, OS::ClientId_t id) {
	blog("Disconnect by %lld", id);
}

int main(int argc, char** argv) {
	bool shutdown = false;

#ifdef _WIN32
	unsigned int n = 0;
	for (n = 0; timeBeginPeriod(n) == TIMERR_NOCANDO; n++) {}
#endif

	IPC::Server srv;
	IPC::Class cls("Control");
	IPC::Function fncShutdown("Shutdown", ipcShutdown, &shutdown);
	IPC::Function fncPing("Ping", std::vector<IPC::Type>({ IPC::Type::UInt64 }), ipcPing, nullptr);
	cls.RegisterFunction(std::make_shared<IPC::Function>(fncPing));
	IPC::Function fncPingS("PingS", std::vector<IPC::Type>({ IPC::Type::UInt64 }), ipcPingS, nullptr);
	cls.RegisterFunction(std::make_shared<IPC::Function>(fncPingS));
	srv.RegisterClass(cls);
	srv.SetConnectHandler(OnConnect, nullptr);
	srv.SetDisconnectHandler(OnDisconnect, nullptr);
	srv.Initialize(argv[1]);
	std::cout << argv[1] << std::endl;

	while (!shutdown) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

#ifdef _WIN32
	timeEndPeriod(n);
#endif
	srv.Finalize();
}
