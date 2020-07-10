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
#include <string>

#include "ipc.hpp"
#include "ipc-socket.hpp"

typedef void (*call_return_t)(void* data, const std::vector<ipc::value>& rval);
extern call_return_t g_fn;
extern void*         g_data;
extern int64_t       g_cbid;

typedef void (*call_on_freez_t)(bool freez_detected, std::string app_state_path);

namespace ipc {
	class client {
		public:
		static std::shared_ptr<client> create(std::string socketPath);
		client() {};
		virtual ~client() {};

		virtual bool call(
		    const std::string&      cname,
		    const std::string&      fname,
		    std::vector<ipc::value> args,
		    call_return_t           fn   = g_fn,
		    void*                   data = g_data,
		    int64_t&                cbid = g_cbid
		) = 0;

		virtual std::vector<ipc::value> call_synchronous_helper(
			const std::string & cname,
			const std::string &fname,
			const std::vector<ipc::value> & args
		) = 0;

		call_on_freez_t freez_cb = nullptr;
		std::string app_state_path;
		void set_freez_callback(call_on_freez_t cb, std::string app_state);
	};
}
