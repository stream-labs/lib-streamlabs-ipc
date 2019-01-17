/* Copyright(C) 2018 Michael Fabian Dirks <info@xaymar.com>
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef OS_ASYNC_OP_HPP
#define OS_ASYNC_OP_HPP

#include <functional>
#include <inttypes.h>
#include "error.hpp"
#include "waitable.hpp"

namespace os {
	typedef std::function<void(os::error code, size_t length)> async_op_cb_t;

	class async_op : public os::waitable {
		protected:
		bool          valid = false;
		async_op_cb_t callback;
		bool          callback_called = false;
		struct {
			async_op_cb_t callback;
			bool          callback_called = false;
		} system;

		virtual void *get_waitable() override = 0;

		public:
		async_op(){};
		async_op(async_op_cb_t u_callback) : callback(u_callback){};
		virtual ~async_op(){};

		virtual bool is_valid() = 0;

		virtual void invalidate() = 0;

		virtual bool is_complete() = 0;

		virtual bool cancel() = 0;

		virtual void set_callback(async_op_cb_t u_callback);

		virtual void set_system_callback(async_op_cb_t u_callback);

		virtual void call_callback() = 0;

		virtual void call_callback(os::error ec, size_t length) = 0;
	};
} // namespace os

#endif // OS_ASYNC_OP_HPP
