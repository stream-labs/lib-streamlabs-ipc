/******************************************************************************
    Copyright (C) 2016-2019 by Streamlabs (General Workings Inc)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

******************************************************************************/

#include "ipc-client.hpp"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <iterator>
#include "../include/error.hpp"
#include "../include/tags.hpp"
#ifdef WIN32
#include "windows/semaphore.hpp"
#include <windows.h>
#include <Objbase.h>
#endif
#include <memory>

using namespace std::placeholders;

call_return_t g_fn   = NULL;
void*         g_data = NULL;
int64_t       g_cbid = NULL;

void ipc::client::worker() {
     os::error ec = os::error::Success;
     std::vector<ipc::value> proc_rval;
     std::cout << "Starting client worker" << std::endl;
    
     while (m_socket->is_connected() && !m_watcher.stop) {
          if (!m_rop || !m_rop->is_valid()) {
              m_watcher.buf.resize(255);
//              std::this_thread::sleep_for(std::chrono::milliseconds(100000));
			// std::cout << "Reader semaphore wait - start" << std::endl;
			if (sem_wait(m_reader_sem) < 0) {
				std::cout << "Failed to wait for the semaphore: " << strerror(errno) << std::endl;
				break;
			}
			// std::cout << "Reader semaphore wait - end" << std::endl;
			// std::cout << "client - read" << std::endl;
             ec = (os::error) m_socket->read(m_watcher.buf.data(),
                                 m_watcher.buf.size(),
                                 m_rop, std::bind(&client::read_callback_msg,
                                                  this,
                                                  std::placeholders::_1,
                                                  std::placeholders::_2), true);
			// std::cout << "Writer semaphore post - start - can write" << std::endl;
			
//              if (ec != os::error::Pending && ec != os::error::Success) {
//                  if (ec == os::error::Disconnected) {
//                      break;
//                  } else {
//                      throw std::exception((const std::exception&)"Unexpected error.");
//                  }
//              }
          }

 //         // ec = m_rop->wait(std::chrono::milliseconds(0));
 //         if (ec == os::error::Success) {
 //             continue;
 //         } else {
 //             // ec = m_rop->wait(std::chrono::milliseconds(20));
 //             if (ec == os::error::TimedOut) {
 //                 continue;
 //             } else if (ec == os::error::Disconnected) {
 //                 break;
 //             } else if (ec == os::error::Error) {
 //                 // throw std::exception((const std::exception&)"Error");
 //             }
          }
//     }

// 	if (!m_socket->is_connected()) {
// 		std::string test = "test";
// 	}
// #ifdef WIN32
// #endif

// 	// Call any remaining callbacks.
// 	proc_rval.resize(1);
// 	proc_rval[0].type = ipc::type::Null;
// 	proc_rval[0].value_str = "Lost IPC Connection";

// 	{ // ToDo: Figure out better way of registering functions, perhaps even a way to have "events" across a IPC connection.
// 		std::unique_lock<std::mutex> ulock(m_lock);
// 		for (auto& cb : m_cb) {
// 			cb.second.first(cb.second.second, proc_rval);
// 		}

// 		m_cb.clear();
// 	}

//    if (!m_socket->is_connected()) {
//        exit(1);
//    }
}

void ipc::client::read_callback_init(os::error ec, size_t size) {
//    std::cout << "client - read_callback_init" << std::endl;
	os::error ec2 = os::error::Success;

	m_rop->invalidate();

	if (ec == os::error::Success || ec == os::error::MoreData) {
		ipc_size_t n_size = read_size(m_watcher.buf);
#ifdef _DEBUG
		std::string hex_msg = ipc::vectortohex(m_watcher.buf);
		ipc::log("(read) ????????: %.*s => %llu", hex_msg.size(), hex_msg.data(), n_size);
#endif
		if (n_size != 0) {
			m_watcher.buf.resize(n_size);
			std::cout << "client read size: " << n_size << std::endl;
#ifdef WIN32
			ec2 = m_socket->read(m_watcher.buf.data(), m_watcher.buf.size(), m_rop, std::bind(&client::read_callback_msg, this, _1, _2));
#elif __APPLE__
             ec = (os::error) m_socket->read(m_watcher.buf.data(),
                                 m_watcher.buf.size(),
                                 m_rop, std::bind(&client::read_callback_msg,
                                                  this,
                                                  std::placeholders::_1,
                                                  std::placeholders::_2), false);
#endif
			if (ec2 != os::error::Pending && ec2 != os::error::Success) {
				if (ec2 == os::error::Disconnected) {
					return;
				} else {
					throw std::exception((const std::exception&)"Unexpected error.");
				}
			}
		}
	}
}

void ipc::client::read_callback_msg(os::error ec, size_t size) {
//    std::cout << "client - read_callback_msg" << std::endl;
	std::pair<call_return_t, void*> cb;
	ipc::message::function_reply fnc_reply_msg;

	m_rop->invalidate();

#ifdef _DEBUG
	ipc::log("(read) ????????: Deserializing Function Reply, ec = %lld, size = %llu", (int64_t)ec, (uint64_t)size);
	std::string hex_msg = ipc::vectortohex(m_watcher.buf);
	ipc::log("(read) ????????: %.*s.", hex_msg.size(), hex_msg.data());
#endif

	try {
		fnc_reply_msg.deserialize(m_watcher.buf, 0);
	} catch (std::exception& e) {
		ipc::log("Deserialize failed with error %s.", e.what());
		throw e;
	}

#ifdef _DEBUG
	ipc::log("(read) %8llu: Deserialized with %lld return values.", 
		fnc_reply_msg.uid.value_union.ui64,
		fnc_reply_msg.values.size());
	for (size_t idx = 0; idx < fnc_reply_msg.values.size(); idx++) {
		ipc::value& arg = fnc_reply_msg.values[idx];
		switch (arg.type) {
			case type::Int32:
				ipc::log("(read) \t%llu: %ld", idx, arg.value_union.i32);
				break;
			case type::Int64:
				ipc::log("(read) \t%llu: %lld", idx, arg.value_union.i64);
				break;
			case type::UInt32:
				ipc::log("(read) \t%llu: %lu", idx, arg.value_union.ui32);
				break;
			case type::UInt64:
				ipc::log("(read) \t%llu: %llu", idx, arg.value_union.ui64);
				break;
			case type::Float:
				ipc::log("(read) \t%llu: %f", idx, (double)arg.value_union.fp32);
				break;
			case type::Double:
				ipc::log("(read) \t%llu: %f", idx, arg.value_union.fp64);
				break;
			case type::String:
				ipc::log("(read) \t%llu: %.*s", idx, arg.value_str.size(), arg.value_str.c_str());
				break;
			case type::Binary:
				ipc::log("(read) \t%llu: (Binary)", idx);
				break;
			case type::Null:
				ipc::log("(read) \t%llu: (Null)", idx);
				break;
		}
	}
	ipc::log("(read) \terror: %.*s", fnc_reply_msg.error.value_str.size(), fnc_reply_msg.error.value_str.c_str());
#endif
	sem_post(m_writer_sem);
	// Find the callback function.
	std::unique_lock<std::mutex> ulock(m_lock);
	auto cb2 = m_cb.find(fnc_reply_msg.uid.value_union.ui64);
	if (cb2 == m_cb.end()) {
#ifdef _DEBUG
		ipc::log("(read) %8llu: No callback, returning.", fnc_reply_msg.uid.value_union.ui64);
#endif
		return;
	}
	cb = cb2->second;

	// Decode return values or errors.
	if (fnc_reply_msg.error.value_str.size() > 0) {
		fnc_reply_msg.values.resize(1);
		fnc_reply_msg.values.at(0).type = ipc::type::Null;
		fnc_reply_msg.values.at(0).value_str = fnc_reply_msg.error.value_str;
	}

#ifdef _DEBUG
	ipc::log("(read) %8llu: Calling callback Function Reply...", fnc_reply_msg.uid.value_union.ui64);
#endif

	// Call Callback
	cb.first(cb.second, fnc_reply_msg.values);

	// Remove cb entry
	/// ToDo: Figure out better way of registering functions, perhaps even a way to have "events" across a IPC connection.
	m_cb.erase(fnc_reply_msg.uid.value_union.ui64);

#ifdef _DEBUG
	ipc::log("(read) %8llu: Done.", fnc_reply_msg.uid.value_union.ui64);
#endif
	// std::cout << "release semaphore" << std::endl;
}

ipc::client::client(std::string socketPath) {
#ifdef WIN32
	m_socket = std::make_unique<os::windows::named_pipe>(os::open_only, socketPath, os::windows::pipe_read_mode::Byte);
#elif __APPLE__
	m_socket = std::make_unique<os::apple::named_pipe>(os::open_only, socketPath);
#endif
	m_watcher.stop   = false;

	sem_unlink(reader_sem_name.c_str());
	remove(reader_sem_name.c_str());
	m_reader_sem = sem_open(reader_sem_name.c_str(), O_CREAT | O_EXCL, 0644, 0);

	sem_unlink(writer_sem_name.c_str());
	remove(writer_sem_name.c_str());
	m_writer_sem = sem_open(writer_sem_name.c_str(), O_CREAT | O_EXCL, 0644, 1);
	
	m_watcher.worker = std::thread(std::bind(&client::worker, this));
}

ipc::client::~client() {
	m_watcher.stop = true;
	if (m_watcher.worker.joinable()) {
		m_watcher.worker.join();
	}
	sem_close(m_reader_sem);
	sem_close(m_writer_sem);
	m_socket = nullptr;
}


bool ipc::client::cancel(int64_t const& id) {
	std::unique_lock<std::mutex> ulock(m_lock);
	return m_cb.erase(id) != 0;
}

bool ipc::client::call(const std::string& cname, const std::string& fname, std::vector<ipc::value> args, call_return_t fn, void* data, int64_t& cbid) {
	static std::mutex mtx;
	static uint64_t timestamp = 0;
	os::error ec;

	std::shared_ptr<os::async_op> write_op;
	ipc::message::function_call fnc_call_msg;
	std::vector<char> outbuf;

	if (!m_socket)
		return false;

	{
		std::unique_lock<std::mutex> ulock(mtx);
		timestamp++;
		fnc_call_msg.uid = ipc::value(timestamp);
	}

	// Set	
	fnc_call_msg.class_name = ipc::value(cname);
	fnc_call_msg.function_name = ipc::value(fname);
	fnc_call_msg.arguments = std::move(args);

#ifdef _DEBUG
	ipc::log("(write) %8llu: Serializing Function Call for class '%.*s' with function '%.*s' and %llu arguments.",
		fnc_call_msg.uid.value_union.ui64,
		(uint64_t)fnc_call_msg.class_name.value_str.size(), fnc_call_msg.class_name.value_str.c_str(),
		(uint64_t)fnc_call_msg.function_name.value_str.size(), fnc_call_msg.function_name.value_str.c_str(),
		(uint64_t)fnc_call_msg.arguments.size());
	for (size_t idx = 0; idx < fnc_call_msg.arguments.size(); idx++) {
		ipc::value& arg = fnc_call_msg.arguments[idx];
		switch (arg.type) {
			case type::Int32:
				ipc::log("(write) \t%llu: %ld", idx, arg.value_union.i32);
				break;
			case type::Int64:
				ipc::log("(write) \t%llu: %lld", idx, arg.value_union.i64);
				break;
			case type::UInt32:
				ipc::log("(write) \t%llu: %lu", idx, arg.value_union.ui32);
				break;
			case type::UInt64:
				ipc::log("(write) \t%llu: %llu", idx, arg.value_union.ui64);
				break;
			case type::Float:
				ipc::log("(write) \t%llu: %f", idx, (double)arg.value_union.fp32);
				break;
			case type::Double:
				ipc::log("(write) \t%llu: %f", idx, arg.value_union.fp64);
				break;
			case type::String:
				ipc::log("(write) \t%llu: %.*s", idx, arg.value_str.size(), arg.value_str.c_str());
				break;
			case type::Binary:
				ipc::log("(write) \t%llu: (Binary)", idx);
				break;
			case type::Null:
				ipc::log("(write) \t%llu: (Null)", idx);
				break;
		}
	}
#endif

	// Serialize
	std::vector<char> buf(fnc_call_msg.size());
	try {
		fnc_call_msg.serialize(buf, 0);
	} catch (std::exception& e) {
		ipc::log("(write) %8llu: Failed to serialize, error %s.", fnc_call_msg.uid.value_union.ui64, e.what());
		throw e;
	}

	if (fn != nullptr) {
		std::unique_lock<std::mutex> ulock(m_lock);
		m_cb.insert(std::make_pair(fnc_call_msg.uid.value_union.ui64, std::make_pair(fn, data)));
		cbid = fnc_call_msg.uid.value_union.ui64;
	}

#ifdef _DEBUG
	ipc::log("(write) %8llu: Sending %llu bytes...", fnc_call_msg.uid.value_union.ui64, buf.size());
	std::string hex_msg = ipc::vectortohex(buf);
	ipc::log("(write) %8llu: %.*s", fnc_call_msg.uid.value_union.ui64, hex_msg.size(), hex_msg.data());
#endif

	// ipc::make_sendable(outbuf, buf);
#ifdef _DEBUG
	hex_msg = ipc::vectortohex(outbuf);
	ipc::log("(write) %8llu: %.*s", fnc_call_msg.uid.value_union.ui64, hex_msg.size(), hex_msg.data());
#endif
#ifdef WIN32
	ec = m_socket->write(outbuf.data(), outbuf.size(), write_op, nullptr);
	if (ec != os::error::Success && ec != os::error::Pending) {
		cancel(cbid);
		//write_op->cancel();
		return false;
	}

	ec = write_op->wait();
	if (ec != os::error::Success) {
		cancel(cbid);
		write_op->cancel();
		return false;
	}
#elif __APPLE__
    // std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	// std::cout << "Writer semaphore wait - start" << std::endl;
	// sem_wait(m_writer_sem);

	// if (sem_wait(m_writer_sem) < 0) {
	// 	std::cout << "Failed to wait for the semaphore: " << strerror(errno) << std::endl;
	// 	// return;
	// }
	std::cout << "client - wait for writer to be available" << std::endl;
	int ret = 0;
	do {
	    ret = sem_wait(m_writer_sem);
	}
 	while (ret == -1 && errno == EINTR);

	// std::cout << "Decremented semaphore" << std::endl;

	// std::cout << "Writer semaphore wait - end " << std::endl;
	std::cout << "client - write " << buf.size() << std::endl;
    ec = (os::error) m_socket->write(buf.data(), buf.size());
	// std::cout << "client - wrote " << std::endl;
	// std::cout << "Reader semaphore post - start - can read" << std::endl;
	sem_post(m_reader_sem);
	// if (ec != os::error::Success && ec != os::error::Pending) {
	// 	cancel(cbid);
	// 	return false;
	// }

	// ec = write_op->wait();
	// if (ec != os::error::Success) {
	// 	cancel(cbid);
	// 	write_op->cancel();
	// 	return false;
	// }
#endif
#ifdef _DEBUG
	ipc::log("(write) %8llu: Sent.", fnc_call_msg.uid.value_union.ui64);
#endif

	return true;
}

int count = 0;

std::vector<ipc::value> ipc::client::call_synchronous_helper(const std::string & cname, const std::string & fname, const std::vector<ipc::value>& args) {
	// Set up call reference data.
	struct CallData {
#ifdef WIN32
		std::shared_ptr<os::windows::semaphore> sgn = std::make_shared<os::windows::semaphore>();
#elif __APPLE__
		sem_t *sem;
#endif
		bool called = false;
		std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

		std::vector<ipc::value> values;
	} cd;

	auto cb = [](void* data, const std::vector<ipc::value>& rval) {
		CallData& cd = *static_cast<CallData*>(data);

		// This copies the data off of the reply thread to the main thread.
		cd.values.reserve(rval.size());
		// std::cout << "response size: " << rval.size() << std::endl;
		std::copy(rval.begin(), rval.end(), std::back_inserter(cd.values));
		// std::cout << "Response from the server " << rval[1].value_str.c_str() << std::endl;
		// std::cout << "Count " << count++ << std::endl;
		cd.called = true;
#ifdef WIN32
		cd.sgn->signal();
#elif __APPLE__
		sem_post(cd.sem);
#endif  
	};

#ifdef __APPLE__
	std::string sem_name = "semapahore-callback";
	sem_unlink(sem_name.c_str());
	remove(sem_name.c_str());
	cd.sem = sem_open(sem_name.c_str(), O_CREAT | O_EXCL, 0644, 0);
#endif

	int64_t cbid = 0;
	bool success = call(cname, fname, std::move(args), cb, &cd, cbid);
	if (!success) {
		return {};
	}

#ifdef WIN32
	cd.sgn->wait();
#elif __APPLE__
	sem_wait(cd.sem);
	sem_close(cd.sem);
#endif

	// if (!cd.called) {
	// 	cancel(cbid);
	// 	return {};
	// }
	return std::move(cd.values);
}
