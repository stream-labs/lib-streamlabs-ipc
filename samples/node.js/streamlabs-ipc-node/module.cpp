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

#include "module.hpp"
#include "ipc-client.hpp"
#include <sstream>
#include <chrono>
#include <stdio.h>
#include <stdarg.h>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h> 

PROCESS_INFORMATION pi;
#endif

std::unique_ptr<IPC::Client> cl;

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

void JSInitialize(const v8::FunctionCallbackInfo<v8::Value>& args) {
	blog("JSInitialize");
	// This function only spawns a server process.
	if (args.Length() != 2) {
		args.GetIsolate()->ThrowException(v8::Exception::SyntaxError(
			v8::String::NewFromUtf8(args.GetIsolate(),
				"This function requires exactly 2 argument.")));
		return;
	}

	std::vector<char> cwdbuf(65535);
	_getcwd(cwdbuf.data(), cwdbuf.size());
	std::string cwdstr = cwdbuf.data();

	std::stringstream sstr;
	sstr << '"' << cwdstr << '\\' << *v8::String::Utf8Value(args[0]->ToString()) << '"' << " ";
	sstr << *v8::String::Utf8Value(args[1]->ToString());
	std::string dstr = sstr.str();
	const char* cstr = dstr.c_str();

#ifdef _WIN32

	STARTUPINFO sia; memset(&sia, 0, sizeof(STARTUPINFO));
	sia.cb = sizeof(STARTUPINFO);
	LPSTR str = const_cast<LPSTR>(cstr);
	memset(&pi, 0, sizeof(PROCESS_INFORMATION));
	if (!CreateProcessA(NULL, str, NULL, NULL, FALSE,
		CREATE_NEW_CONSOLE, NULL, NULL, &sia, &pi)) {
		DWORD ec = GetLastError();
		std::stringstream emsg;
		emsg << "Failed to spawn process with command '" << str << "'"
			<< " from root '" << cwdstr << "', error code " << ec << ".";
		std::string errmsg = emsg.str();

		args.GetIsolate()->ThrowException(v8::Exception::Error(
			v8::String::NewFromUtf8(args.GetIsolate(), errmsg.c_str())));
		args.GetReturnValue().Set(false);
		return;
	}
#endif

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	try {
		cl = std::make_unique<IPC::Client>(*v8::String::Utf8Value(args[1]->ToString()));
	} catch (std::exception e) {
		args.GetIsolate()->ThrowException(v8::Exception::Error(
			v8::String::NewFromUtf8(args.GetIsolate(),
				e.what())));
		args.GetReturnValue().Set(false);
	#ifdef _WIN32
		TerminateProcess(pi.hProcess, -1);
	#endif
		return;
	}
	if (!cl->Authenticate()) {
		args.GetIsolate()->ThrowException(v8::Exception::Error(
			v8::String::NewFromUtf8(args.GetIsolate(),
				"Failed to connect with IPC Client.")));
		args.GetReturnValue().Set(false);
		return;
	}

	args.GetReturnValue().Set(true);
	return;
}

void JSFinalize(const v8::FunctionCallbackInfo<v8::Value>& args) {
	blog("JSFinalize");
#ifdef _WIN32
	TerminateProcess(pi.hProcess, 0);
#endif
}

void JSConnect(const v8::FunctionCallbackInfo<v8::Value>& args) {
	blog("JSConnect");
	if (args.Length() != 1) {
		args.GetIsolate()->ThrowException(v8::Exception::SyntaxError(
			v8::String::NewFromUtf8(args.GetIsolate(),
				"This function requires exactly 1 argument.")));
		return;
	}

	try {
		cl = std::make_unique<IPC::Client>(*v8::String::Utf8Value(args[0]->ToString()));
	} catch (std::exception e) {
		args.GetIsolate()->ThrowException(v8::Exception::Error(
			v8::String::NewFromUtf8(args.GetIsolate(),
				e.what())));
		args.GetReturnValue().Set(false);
		return;
	}

	args.GetReturnValue().Set(true);
	return;
}

void JSAuthenticate(const v8::FunctionCallbackInfo<v8::Value>& args) {
	blog("JSAuthenticate");
	if (!cl->Authenticate()) {
		args.GetIsolate()->ThrowException(v8::Exception::Error(
			v8::String::NewFromUtf8(args.GetIsolate(),
				"Failed to connect with IPC Client.")));
		args.GetReturnValue().Set(false);
		return;
	}
	args.GetReturnValue().Set(true);
	return;
}

std::mutex PingMutex;
std::condition_variable PingCV;

void JSPingResponse(void* data, IPC::Value rval) {
	*((bool*)data) = true;
}

void JSPing(const v8::FunctionCallbackInfo<v8::Value>& args) {
	volatile bool pingResult = false;
	if (!cl->Call("Control", "Ping", { 0ull }, JSPingResponse, (bool*)(&pingResult))) {
		args.GetIsolate()->ThrowException(
			v8::Exception::Error(
				v8::String::NewFromUtf8(args.GetIsolate(),
					"Unable to send message."
			)));
		return;
	}
	while (pingResult == false) {

	}
	//args.GetReturnValue().Set(args[0]);
}

void JSPingS(const v8::FunctionCallbackInfo<v8::Value>& args) {
	bool pingResult = false;
	if (!cl->Call("Control", "PingS", { 0ull }, JSPingResponse, &pingResult)) {
		args.GetReturnValue().Set(false);
		return;
	}
	while (!pingResult) {
	}
}

void JSPong(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//blog("JSPong");
	args.GetReturnValue().Set(args[0]);
}

void JSPongS(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//blog("JSPongS");
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	args.GetReturnValue().Set(args[0]);
}

void Init(v8::Local<v8::Object> exports) {
	blog("Node Init");
	NODE_SET_METHOD(exports, "Initialize", JSInitialize);
	NODE_SET_METHOD(exports, "Finalize", JSFinalize);

	NODE_SET_METHOD(exports, "Connect", JSConnect);
	NODE_SET_METHOD(exports, "Authenticate", JSAuthenticate);

	NODE_SET_METHOD(exports, "Ping", JSPing);
	NODE_SET_METHOD(exports, "PingS", JSPingS);
	NODE_SET_METHOD(exports, "Pong", JSPong);
	NODE_SET_METHOD(exports, "PongS", JSPongS);
}

NODE_MODULE(MODULE_NAME, Init);
