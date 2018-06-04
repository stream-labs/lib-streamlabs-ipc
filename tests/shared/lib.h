// Shared code for all tests.
// 

#include <inttypes.h>
#include <string>
#include <memory>
#include <map>
#include <chrono>

namespace shared {
	template<typename T>
	bool is_equal(T V1, T V2, T Edge) {
		if (V1 > V2) {
			return (V1 - V2) <= Edge;
		} else if (V1 < V2) {
			return (V2 - V1) <= Edge;
		} else {
			return true;
		}
	}

	namespace logger {
		bool is_timestamp_relative_to_start();
		void is_timestamp_relative_to_start(bool v);

		bool to_stdout();
		void to_stdout(bool v);

		bool to_stderr();
		void to_stderr(bool v);

		bool to_debug();
		void to_debug(bool v);

		bool to_file();
		std::string to_file_path();
		bool to_file(bool v, std::string path);

		void log(std::string format, ...);
	};

	namespace os {
		std::string get_working_directory();

		class process {
			uint64_t id;
			uint64_t handle;

			public:
			process(std::string program, std::string command_line, std::string working_directory);
			~process();

			bool is_alive(process proc);

			bool kill(int32_t exit_code);
			int32_t get_exit_code();

			void detach();
			bool is_detached();
		};
	};

	namespace time {
		class measure_timer {
			std::map<std::chrono::nanoseconds, size_t> timings;
			size_t calls = 0;

			protected:
			inline void track(std::chrono::nanoseconds dur);

			public:
			class instance {
				measure_timer* parent;
				std::chrono::high_resolution_clock::time_point begin;

				public:
				instance(measure_timer* parent);

				~instance();

				void cancel();

				void reparent(measure_timer* new_parent);

			};

			public:
			measure_timer();

			~measure_timer();

			std::unique_ptr<instance> track();

			uint64_t count();

			std::chrono::nanoseconds total();

			double_t average();

			std::chrono::nanoseconds percentile(double_t pct, bool by_time = false);

		};
	}
};
