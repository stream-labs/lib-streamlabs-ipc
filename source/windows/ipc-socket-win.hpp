#ifndef SOCKET_WIN_H
#define SOCKET_WIN_H

#include "../include/ipc-socket.hpp"
#include "utility.hpp"
#include "async_request.hpp"

namespace os {
	namespace windows {
		enum class pipe_type : int8_t {
			Byte = 0x00,
			//reserved = 0x01,
			//reserved = 0x02,
			//reserved = 0x03,
			Message = 0x04,
		};

		enum class pipe_read_mode : int8_t {
			Byte = 0x00,
			//reserved = 0x01,
			Message = 0x02,
		};

		class socket_win : public ipc::socket {
			HANDLE              handle;
			bool                created = false;
			SECURITY_ATTRIBUTES security_attributes;
			struct {
				ULONG sessionId;
				ULONG processId;
			} remoteId;

			public:
			static std::unique_ptr<os::windows::socket_win> create(os::create_only_t, const std::string& name);
			static std::unique_ptr<os::windows::socket_win> create(os::open_only_t, const std::string& name);

			socket_win();
			socket_win(os::create_only_t, const std::string & name, size_t max_instances = PIPE_UNLIMITED_INSTANCES,
					   pipe_type type = pipe_type::Byte, pipe_read_mode mode = pipe_read_mode::Byte,
					   bool is_unique = true);
			socket_win(os::open_only_t, const std::string& name, pipe_read_mode mode = pipe_read_mode::Byte);
			~socket_win();

			os::error read(char *buffer, size_t buffer_length, std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb);
			os::error write(const char *buffer, size_t buffer_length, std::shared_ptr<os::async_op> &op,
							os::async_op_cb_t cb);

			virtual void handle_accept_callback(os::error code, size_t length) override;
			virtual bool is_created() override;
			virtual bool is_connected() override;
			virtual void set_connected(bool is_connected) override;
			virtual os::error accept(std::shared_ptr<os::async_op> &op, os::async_op_cb_t cb) override;
		};
	}
}

#endif