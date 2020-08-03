#pragma once

#include "error.hpp"
#include "tags.hpp"
#include "async_op.hpp"

namespace ipc {
    class socket {
        public:
        socket(){};
		virtual ~socket(){};


        virtual void handle_accept_callback(os::error code, size_t length) = 0;
        virtual bool is_created() = 0;
        virtual bool is_connected() = 0;
        virtual void set_connected(bool is_connected) = 0;
        virtual os::error accept(std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb) = 0;
    };
}