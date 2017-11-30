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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h> 

PROCESS_INFORMATION pi;
#endif

std::unique_ptr<IPC::Client> cl;

void JSInitialize(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
#ifdef _WIN32
	TerminateProcess(pi.hProcess, 0);
#endif
}

void JSConnect(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
bool pingResult = false;

void JSPingResponse(void* data, IPC::Value rval) {
	uint32_t* rv = (uint32_t*)data;
	*rv = rval.value.ui64;
	pingResult = true;
}

void JSPing(const v8::FunctionCallbackInfo<v8::Value>& args) {
	uint32_t r = 0;
	std::vector<IPC::Value> pars;
	pars.push_back(uint64_t(args[0]->Int32Value()));

	if (!cl->Call("Control", "Ping", pars, JSPingResponse, &r)) {
		args.GetReturnValue().Set(false);
		return;
	}
	std::unique_lock<std::mutex> ulock(PingMutex);
	PingCV.wait(ulock, [&r] {
		return r != 0;
	});

	args.GetReturnValue().Set(uint32_t(r));
}

void JSPingS(const v8::FunctionCallbackInfo<v8::Value>& args) {
	uint32_t r = 0;
	std::vector<IPC::Value> pars;
	pars.push_back(uint64_t(args[0]->Int32Value()));

	pingResult = false;
	if (!cl->Call("Control", "PingS", pars, JSPingResponse, &r)) {
		args.GetReturnValue().Set(false);
		return;
	}
	while (!pingResult) {}

	args.GetReturnValue().Set(uint32_t(r));
}

void JSPong(const v8::FunctionCallbackInfo<v8::Value>& args) {
	args.GetReturnValue().Set(args[0]);
}

void JSPongS(const v8::FunctionCallbackInfo<v8::Value>& args) {
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	args.GetReturnValue().Set(args[0]);
}

void Init(v8::Local<v8::Object> exports) {
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
