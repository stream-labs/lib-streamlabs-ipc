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

#pragma once
#include <functional>
#include <string>
#include <memory>
#include "ipc.hpp"
#include "ipc-socket.hpp"

typedef void (*call_return_t)(void *data, const std::vector<ipc::value> &rval);
extern call_return_t g_fn;
extern void *g_data;
extern int64_t g_cbid;

typedef void (*call_on_freez_t)(bool freez_detected, std::string app_state_path, std::string call_name, int timeout);

namespace ipc {
class client {
public:
	using call_on_disconnect_t = std::function<void()>;

	// |disconnectionCallback| is called when the server disconnection is detected.
	// If the callback is not set or you use the other constructor,
	// the client will just call |exit(1)| when the server disconnects.
	static std::shared_ptr<client> create(const std::string &socketPath, call_on_disconnect_t disconnectionCallback);

	static std::shared_ptr<client> create(std::string socketPath);
	client(){};
	virtual ~client(){};

	// Stop all internal threads and the background disconnection detection.
	// Call this if you do not plan to use the object anymore
	// but also do not want to completely destroy it.
	// The destructor will call the method anyways.
	virtual void stop() = 0;

	virtual bool call(const std::string &cname, const std::string &fname, std::vector<ipc::value> args, call_return_t fn = g_fn, void *data = g_data, int64_t &cbid = g_cbid) = 0;

	virtual std::vector<ipc::value> call_synchronous_helper(const std::string &cname, const std::string &fname, const std::vector<ipc::value> &args) = 0;

	call_on_freez_t freez_cb = nullptr;
	std::string app_state_path;
	void set_freez_callback(call_on_freez_t cb, std::string app_state);
};
}
