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

#ifdef _WIN32
#pragma push
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef RegisterClass
#pragma pop
#include <timeapi.h>
#endif

IPC::Value ipcShutdown(int64_t, void* shutdown, std::vector<IPC::Value> vals) {
	bool* ptrShutdown = (bool*)shutdown;
	*ptrShutdown = true;
	return IPC::Value();
}

IPC::Value ipcPing(int64_t id, void*, std::vector<IPC::Value> vals) {
	//std::cout << "Ping from " << id << std::endl;
	return vals.at(0);
}

IPC::Value ipcPingS(int64_t id, void*, std::vector<IPC::Value> vals) {
	//std::cout << "Ping from " << id << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	return vals.at(0);
}

bool OnConnect(void*, OS::ClientId_t id) {
	std::cout << "Connect from " << id << std::endl;
	return true;
}

void OnDisconnect(void*, OS::ClientId_t id) {
	std::cout << "Disconnect by " << id << std::endl;
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
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

#ifdef _WIN32
	timeEndPeriod(n);
#endif
	srv.Finalize();
}
