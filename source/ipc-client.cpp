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

#include "ipc-client.hpp"
#include "ipc.pb.h"
#include "os-error.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Objbase.h>
#endif

static const size_t buffer_size = 128 * 1024 * 1024;

ipc::client::client(std::string socketPath) {
	m_socket = os::named_socket::create();
	m_socket->set_receive_timeout(std::chrono::nanoseconds(1000000ull));
	m_socket->set_send_timeout(std::chrono::nanoseconds(1000000ull));
	m_socket->set_wait_timeout(std::chrono::nanoseconds(10000000ull));
	m_socket->set_receive_buffer_size(buffer_size);
	m_socket->set_send_buffer_size(buffer_size);
	if (!m_socket->connect(socketPath)) {
		throw std::exception("Failed to initialize socket.");
	}

	m_stopWorkers = false;
	// Worker is created on authenatication.
}

ipc::client::~client() {
	m_stopWorkers = true;
	if (m_worker.joinable())
		m_worker.join();
	m_socket->close();
}

bool ipc::client::authenticate() {
	if (m_authenticated)
		return true;

	auto sock = m_socket->get_connection();
	if (sock->bad())
		return false;

	::Authenticate msg;
	msg.set_password("Hello World"); // Eventually will be used.
	std::string signalname = "";
#ifdef _WIN32
	GUID guid;
	CoCreateGuid(&guid);
	std::stringstream sstr;
	sstr << std::hex << std::setw(8) << std::setfill('0') << guid.Data1 <<
		"-" << std::hex << std::setw(8) << std::setfill('0') << guid.Data2 <<
		"-" << std::hex << std::setw(8) << std::setfill('0') << guid.Data3 <<
		"-" << std::hex << std::setw(8) << std::setfill('0') << *reinterpret_cast<unsigned long*>(guid.Data4);
	signalname = sstr.str();
#endif
	msg.set_name(signalname);

	std::vector<char> buf(msg.ByteSizeLong());
	if (!msg.SerializePartialToArray(buf.data(), (int)buf.size()))
		return false;

	bool success = false;
	auto tp_begin = std::chrono::high_resolution_clock::now();
	while ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tp_begin).count() < 100)
		&& sock->good()) {

		size_t temp = 0;
		if (sock->write(buf.data(), buf.size(), temp) == os::error::Ok) {
			sock->flush();
		#ifdef _WIN32
			Sleep(0);
		#endif
			success = true;
			break;
		}
	}
	if (!success) {
		return false;
	}

	// Read reply
	::AuthenticateReply rpl;
	tp_begin = std::chrono::high_resolution_clock::now();
	success = false;
	while ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - tp_begin).count() < 500000)
		&& sock->good()) {
		if (sock->read_avail() == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		size_t avail = sock->read_avail();
		buf.resize(avail);
		os::error err = sock->read(buf.data(), buf.size(), avail);
		if (err == os::error::Ok) {
			success = true;
			break;
		}
	}
	if (!success) {
		return false;
	}

	success = rpl.ParsePartialFromArray(buf.data(), (int)buf.size());
	if (!success) {
		return false;
	}

	m_readSignal = os::signal::create(rpl.write_event());
	m_writeSignal = os::signal::create(rpl.read_event());

	m_worker = std::thread(worker_thread, this);
	return false;
}

bool ipc::client::call(std::string cname, std::string fname, std::vector<ipc::value> args, call_return_t fn, void* data) {
	int64_t test = 0;
	return call(cname, fname, args, fn, data, test);
}

bool ipc::client::cancel(int64_t const& id) {
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

bool ipc::client::call(std::string cname, std::string fname, std::vector<ipc::value> args, call_return_t fn, void* data, int64_t& cbid) {
	auto sock = m_socket->get_connection();
	if (sock->bad())
		return false;

	static std::mutex mtx;
	static uint64_t timestamp = 0;

	::FunctionCall msg;
	{
		std::unique_lock<std::mutex> ulock(mtx);
		timestamp++;
		msg.set_timestamp(timestamp);
	}
	msg.set_classname(cname);
	msg.set_functionname(fname);
	auto b = msg.mutable_arguments();
	for (ipc::value& v : args) {
		::Value* val = b->Add();
		switch (v.type) {
			case type::Float:
				val->set_val_float(v.value_union.fp32);
				break;
			case type::Double:
				val->set_val_double(v.value_union.fp64);
				break;
			case type::Int32:
				val->set_val_int32(v.value_union.i32);
				break;
			case type::Int64:
				val->set_val_int64(v.value_union.i64);
				break;
			case type::UInt32:
				val->set_val_uint32(v.value_union.ui32);
				break;
			case type::UInt64:
				val->set_val_uint64(v.value_union.ui64);
				break;
			case type::String:
				val->set_val_string(v.value_str);
				break;
			case type::Binary:
				val->set_val_binary(v.value_bin.data(), v.value_bin.size());
				break;
		}
	}

	std::vector<char> buf(msg.ByteSizeLong());
	if (!msg.SerializePartialToArray(buf.data(), (int)buf.size())) {
		return false;
	}

	if (fn != nullptr) {
		std::unique_lock<std::mutex> ulock(m_lock);
		m_cb.insert(std::make_pair(msg.timestamp(), std::make_pair(fn, data)));
		cbid = msg.timestamp();
	}

	os::error write_error = os::error::Ok;
	size_t write_length = 0;
	for (size_t attempt = 0; attempt < 5; attempt++) {
		if (sock->bad())
			break;

		write_error = sock->write(buf.data(), buf.size(), write_length);
		if ((write_error == os::error::Ok) && (write_length == buf.size())) {
			sock->flush();
			m_writeSignal->set();
		#ifdef _WIN32
			Sleep(0);
		#endif
			return true;
		}
	}

	if (fn != nullptr) {
		std::unique_lock<std::mutex> ulock(m_lock);
		m_cb.erase(msg.timestamp());
		cbid = 0;
	}
	return false;
}

std::vector<ipc::value> ipc::client::call_synchronous_helper(std::string cname, std::string fname, std::vector<ipc::value> args,
	std::chrono::nanoseconds timeout) {
	// Set up call reference data.
	struct CallData {
		std::condition_variable cv;
		std::mutex mtx;
		bool called = false;
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		std::vector<ipc::value> values;
	} cd;

	auto cb = [](const void* data, const std::vector<ipc::value>& rval) {
		CallData& cd = const_cast<CallData&>(*static_cast<const CallData*>(data));

		// This copies the data off of the reply thread to the main thread.
		cd.values.reserve(rval.size());
		std::copy(rval.begin(), rval.end(), std::back_inserter(cd.values));

		cd.called = true;
		cd.cv.notify_all();
	#ifdef _WIN32
		Sleep(0);
	#endif
	};

	std::unique_lock<std::mutex> ulock(cd.mtx);

	int64_t cbid = 0;
	bool success = call(cname, fname, std::move(args), cb, &cd, cbid);
	if (!success) {
		return {};
	}

	cd.cv.wait_for(ulock, timeout, [&cd]() { return cd.called; });
	if (!cd.called) {
		cancel(cbid);
		return {};
	}

	return std::move(cd.values);
}

void ipc::client::worker_thread(client* ptr) {
	// Variables
	auto conn = ptr->m_socket->get_connection();
	/// Reading
	os::error read_error = os::error::Ok;
	size_t read_length = 0, read_full_length = 0;
	char read_buffer_temp = 0;
	std::vector<char> read_buffer;
	/// Processing
	::FunctionResult proc_pb_result;
	std::vector<ipc::value> proc_rval;

	// Prepare Buffers
	read_buffer.reserve(ptr->m_socket->get_receive_buffer_size());

	while ((!ptr->m_stopWorkers) && conn->good()) {
		if (conn->read_avail() == 0) {
			if (ptr->m_readSignal->wait(std::chrono::milliseconds(10)) != os::error::Ok) {
			#ifdef _WIN32
				Sleep(0);
			#endif
				continue;
			}
		}
		ptr->m_readSignal->clear();

		read_full_length = conn->read_avail();
		read_buffer.resize(read_full_length);
		read_full_length = conn->read(read_buffer);
		if (read_full_length != read_buffer.size()) {
		#ifdef _WIN32
			Sleep(0);
		#endif
			continue;
		}

		// Process read message.
		{
			bool success = proc_pb_result.ParsePartialFromArray(read_buffer.data(), (int)read_full_length);
			if (!success) {
				continue;
			}

			// Find the callback function.
			std::pair<call_return_t, void*> cb;
			std::unique_lock<std::mutex> ulock(ptr->m_lock);
			auto cb2 = ptr->m_cb.find(proc_pb_result.timestamp());
			if (cb2 == ptr->m_cb.end()) {
				continue;
			}
			cb = cb2->second;

			// Decode return values or errors.
			if (proc_pb_result.error().length() > 0) {
				proc_rval.resize(1);
				proc_rval.at(0).type = ipc::type::Null;
				proc_rval.at(0).value_str = proc_pb_result.error();
			} else if (proc_pb_result.value_size() > 0) {
				proc_rval.resize(proc_pb_result.value_size());
				for (size_t n = 0; n < proc_rval.size(); n++) {
					auto& v = proc_pb_result.value((int)n);
					switch (v.value_case()) {
						case ::Value::ValueCase::kValBool:
							proc_rval.at(n).type = ipc::type::Int32;
							proc_rval.at(n).value_union.i32 = v.val_bool() ? 1 : 0;
							break;
						case ::Value::ValueCase::kValFloat:
							proc_rval.at(n).type = ipc::type::Float;
							proc_rval.at(n).value_union.fp32 = v.val_float();
							break;
						case ::Value::ValueCase::kValDouble:
							proc_rval.at(n).type = ipc::type::Double;
							proc_rval.at(n).value_union.fp64 = v.val_double();
							break;
						case ::Value::ValueCase::kValInt32:
							proc_rval.at(n).type = ipc::type::Int32;
							proc_rval.at(n).value_union.i32 = v.val_int32();
							break;
						case ::Value::ValueCase::kValInt64:
							proc_rval.at(n).type = ipc::type::Int64;
							proc_rval.at(n).value_union.i64 = v.val_int64();
							break;
						case ::Value::ValueCase::kValUint32:
							proc_rval.at(n).type = ipc::type::UInt32;
							proc_rval.at(n).value_union.ui32 = v.val_uint32();
							break;
						case ::Value::ValueCase::kValUint64:
							proc_rval.at(n).type = ipc::type::UInt64;
							proc_rval.at(n).value_union.ui64 = v.val_uint64();
							break;
						case ::Value::ValueCase::kValString:
							proc_rval.at(n).type = ipc::type::String;
							proc_rval.at(n).value_str = v.val_string();
							break;
						case ::Value::ValueCase::kValBinary:
							proc_rval.at(n).type = ipc::type::Binary;
							proc_rval.at(n).value_bin.resize(v.val_binary().size());
							memcpy(proc_rval.at(n).value_bin.data(), v.val_binary().data(), v.val_binary().size());
							break;
					}
				}
			}

			// Call Callback
			cb.first(cb.second, proc_rval);

			// Remove cb entry
			/// ToDo: Figure out better way of registering functions, perhaps even a way to have "events" across a IPC connection.
			ptr->m_cb.erase(proc_pb_result.timestamp());
		}
	}

	// Call any remaining callbacks.
	proc_rval.resize(1);
	proc_rval[0].type = ipc::type::Null;
	proc_rval[0].value_str = "Lost IPC Connection";

	{ // ToDo: Figure out better way of registering functions, perhaps even a way to have "events" across a IPC connection.
		std::unique_lock<std::mutex> ulock(ptr->m_lock);
		for (auto& cb : ptr->m_cb) {
			cb.second.first(cb.second.second, proc_rval);
		}

		ptr->m_cb.clear();
	}
}
