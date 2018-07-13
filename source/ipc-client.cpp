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
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include "source/os/error.hpp"
#include "source/os/tags.hpp"
#include "source/os/windows/semaphore.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Objbase.h>
#endif

using namespace std::placeholders;

static const size_t buffer_size = 128 * 1024 * 1024;

void ipc::client::worker() {
	os::error ec = os::error::Success;
	std::vector<ipc::value> proc_rval;

	while (m_socket->is_connected() && !m_watcher.stop) {
		if (!m_rop || !m_rop->is_valid()) {
			m_rbuf.resize(sizeof(ipc_size_t));
			ec = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&client::read_callback_init, this, _1, _2));
			if (ec != os::error::Pending && ec != os::error::Success) {
				if (ec == os::error::Disconnected) {
					break;
				} else {
					throw std::exception("Unexpected error.");
				}
			}
		}

		os::waitable * waits[] = { m_rop.get() };
		size_t                      wait_index = -1;
		for (size_t idx = 0; idx < 1; idx++) {
			if (waits[idx] != nullptr) {
				if (waits[idx]->wait(std::chrono::milliseconds(0)) == os::error::Success) {
					wait_index = idx;
					break;
				}
			}
		}
		if (wait_index == -1) {
			os::error code = os::waitable::wait_any(waits, 1, wait_index, std::chrono::milliseconds(20));
			if (code == os::error::TimedOut) {
				continue;
			} else if (code == os::error::Disconnected) {
				break;
			} else if (code == os::error::Error) {
				throw std::exception("Error");
			}
		}
	}

	// Call any remaining callbacks.
	proc_rval.resize(1);
	proc_rval[0].type = ipc::type::Null;
	proc_rval[0].value_str = "Lost IPC Connection";

	{ // ToDo: Figure out better way of registering functions, perhaps even a way to have "events" across a IPC connection.
		std::unique_lock<std::mutex> ulock(m_lock);
		for (auto& cb : m_cb) {
			cb.second.first(cb.second.second, proc_rval);
		}

		m_cb.clear();
	}
}

void ipc::client::read_callback_init(os::error ec, size_t size) {
	os::error ec2 = os::error::Success;

	m_rop->invalidate();

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(m_rbuf);
		if (n_size != 0) {
			m_rbuf.resize(n_size);
			ec2 = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, std::bind(&client::read_callback_msg, this, _1, _2));
			if (ec2 != os::error::Pending && ec2 != os::error::Success) {
				if (ec2 == os::error::Disconnected) {
					return;
				} else {
					throw std::exception("Unexpected error.");
				}
			}
		}
	}
}

void ipc::client::read_callback_msg(os::error ec, size_t size) {
	::FunctionResult proc_pb_result;
	std::vector<ipc::value> proc_rval;
	std::pair<call_return_t, void*> cb;

	m_rop->invalidate();

	bool success = proc_pb_result.ParsePartialFromArray(m_rbuf.data(), int(m_rbuf.size()));
	if (!success) {
		return;
	}

	// Find the callback function.
	std::unique_lock<std::mutex> ulock(m_lock);
	auto cb2 = m_cb.find(proc_pb_result.timestamp());
	if (cb2 == m_cb.end()) {
		return;
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
	m_cb.erase(proc_pb_result.timestamp());
}

ipc::client::client(std::string socketPath) {
	m_socket = std::make_unique<os::windows::named_pipe>(os::open_only, socketPath, os::windows::pipe_read_mode::Byte);
}

ipc::client::~client() {
	m_watcher.stop = true;
	if (m_watcher.worker.joinable()) {
		m_watcher.worker.join();
	}
	m_socket = nullptr;
}

bool ipc::client::authenticate() {
	os::error ec = os::error::Success;
	::Authenticate msg;
	::AuthenticateReply rpl;

	if (!m_socket)
		return false;

	if (m_authenticated)
		return true;

	// Build Message
	msg.set_password("Hello World"); // Eventually will be used.
	msg.set_name("");

	std::vector<char> buf(msg.ByteSizeLong());
	if (!msg.SerializePartialToArray(buf.data(), (int)buf.size())) {
		return false;
	}

	ipc::make_sendable(m_wbuf, buf);
	ec = m_socket->write(m_wbuf.data(), m_wbuf.size(), m_wop, nullptr);
	if (ec != os::error::Success && ec != os::error::Pending) {
		m_wop->invalidate();
		return false;
	}

	ec = m_wop->wait(std::chrono::milliseconds(5000));
	if (ec != os::error::Success) {
		m_wop->invalidate();
		return false;
	}
	m_wop->invalidate();

	m_rbuf.resize(sizeof(ipc_size_t));
	ec = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, nullptr);
	if (ec != os::error::Success && ec != os::error::Pending) {
		m_rop->invalidate();
		return false;
	}

	ec = m_rop->wait(std::chrono::milliseconds(5000));
	if (ec != os::error::Success) {
		m_rop->invalidate();
		return false;
	}
	m_rop->invalidate();

	ipc_size_t size = ipc::read_size(m_rbuf);
	m_rbuf.resize(size);
	ec = m_socket->read(m_rbuf.data(), m_rbuf.size(), m_rop, nullptr);
	if (ec != os::error::Success && ec != os::error::Pending) {
		m_rop->invalidate();
		return false;
	}

	ec = m_rop->wait(std::chrono::milliseconds(5000));
	if (ec != os::error::Success) {
		m_rop->invalidate();
		return false;
	}
	m_rop->invalidate();

	if (!rpl.ParsePartialFromArray(buf.data(), (int)buf.size())) {
		return false;
	}

	m_watcher.stop = false;
	m_watcher.worker = std::thread(std::bind(&client::worker, this));
	return true;
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
	static std::mutex mtx;
	static uint64_t timestamp = 0;
	::FunctionCall msg;
	os::error ec;

	if (!m_socket)
		return false;

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

	ipc::make_sendable(m_wbuf, buf);
	ec = m_socket->write(m_wbuf.data(), m_wbuf.size(), m_wop, nullptr);
	if (ec != os::error::Success && ec != os::error::Pending) {
		cancel(cbid);
		if (ec == os::error::Disconnected) {
			return false;
		} else {
			throw std::exception("Unexpected Error");
		}
	}

	ec = m_wop->wait(std::chrono::milliseconds(5000));
	if (ec != os::error::Success) {
		cancel(cbid);
		if (ec == os::error::Disconnected) {
			return false;
		} else {
			throw std::exception("Unexpected Error");
		}
	}

	return true;
}

std::vector<ipc::value> ipc::client::call_synchronous_helper(std::string cname, std::string fname, std::vector<ipc::value> args,
	std::chrono::nanoseconds timeout) {
	// Set up call reference data.
	struct CallData {
		std::shared_ptr<os::windows::semaphore> sgn = std::make_shared<os::windows::semaphore>();
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
		cd.sgn->signal();
	};

	std::unique_lock<std::mutex> ulock(cd.mtx);

	int64_t cbid = 0;
	bool success = call(cname, fname, std::move(args), cb, &cd, cbid);
	if (!success) {
		return {};
	}

	cd.sgn->wait(timeout);
	if (!cd.called) {
		cancel(cbid);
		return {};
	}

	return std::move(cd.values);
}
