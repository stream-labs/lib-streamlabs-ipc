// Shared code for all tests.
// 

#include "lib.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <vector>
#include <stdarg.h>
#include <ctime>
#include <iomanip>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#pragma region shared::logger
static bool log_timestamp_relative_to_start = true;
static std::chrono::high_resolution_clock::time_point global_log_start = std::chrono::high_resolution_clock::now();

static bool log_stdout_enabled = true;
static bool log_stderr_enabled = false;
static bool log_debug_enabled = true;
static bool log_file_enabled = false;
static std::string log_file_path = "";
static std::ofstream log_file_stream;

bool shared::logger::is_timestamp_relative_to_start() {
	return log_timestamp_relative_to_start;
}

void shared::logger::is_timestamp_relative_to_start(bool v) {
	log_timestamp_relative_to_start = v;
}

bool shared::logger::to_stdout() {
	return log_stdout_enabled;
}

void shared::logger::to_stdout(bool v) {
	log_stdout_enabled = v;
}

bool shared::logger::to_stderr() {
	return log_stderr_enabled;
}

void shared::logger::to_stderr(bool v) {
	log_stderr_enabled = v;
}

bool shared::logger::to_debug() {
	return log_debug_enabled;
}

void shared::logger::to_debug(bool v) {
	log_debug_enabled = v;
}

bool shared::logger::to_file() {
	return log_file_enabled;
}

std::string shared::logger::to_file_path() {
	return log_file_path;
}

bool shared::logger::to_file(bool v, std::string path) {
	log_file_enabled = v;
	if (log_file_enabled) {
		if ((path != log_file_path) || (!log_file_stream.is_open())) {
			log_file_path = path;
			if (log_file_stream.is_open()) {
				log_file_stream.close();
			}

			log_file_stream.open(path, std::ios::out);
			if (!log_file_stream.is_open()) {
				log_file_enabled = false;
				return false;
			}
		}
	} else {
		if (log_file_stream.is_open()) {
			log_file_stream.close();
		}
	}
}

void shared::logger::log(std::string format, ...) {
	std::vector<char> message_buffer;
	std::vector<char> timestamp_buffer;

	// Generate Timestamp
	if (log_timestamp_relative_to_start) {
		auto timeSinceStart = (std::chrono::high_resolution_clock::now() - global_log_start);
		auto hours = std::chrono::duration_cast<std::chrono::hours>(timeSinceStart);
		timeSinceStart -= hours;
		auto minutes = std::chrono::duration_cast<std::chrono::minutes>(timeSinceStart);
		timeSinceStart -= minutes;
		auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeSinceStart);
		timeSinceStart -= seconds;
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceStart);
		timeSinceStart -= milliseconds;
		auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(timeSinceStart);
		timeSinceStart -= microseconds;
		auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceStart);

		const char* timestamp_format = "%.2d:%.2d:%.2d.%.3d.%.3d.%.3d";
	#define timestamp_args hours.count(), minutes.count(), seconds.count(),	milliseconds.count(), microseconds.count(), nanoseconds.count()
		timestamp_buffer.resize(_scprintf(timestamp_format, timestamp_args) + 1);
		sprintf_s(timestamp_buffer.data(), timestamp_buffer.size(), timestamp_format, timestamp_args);
	#undef timestamp_args
	} else {
		std::time_t t = std::time(0);
		timestamp_buffer.resize(sizeof("0000-00-00T00:00:00+0000"));
		std::strftime(timestamp_buffer.data(), timestamp_buffer.size(), "%FT%T%z", std::localtime(&t));
	}

	// Generate Message
	va_list args;
	va_start(args, format);
	message_buffer.resize(_vscprintf(format.c_str(), args) + 1);
	vsprintf_s(message_buffer.data(), message_buffer.size(), format.c_str(), args);
	va_end(args);

	// Log each line individually
	size_t message_begin = 0;
	size_t message_end = 0;
	std::string message;
	const char* final_format = "[%.*s] %.*s";
	std::vector<char> final_buffer;
	for (size_t p = 0, end = message_buffer.size(); p < end; p++) {
		char c = message_buffer[p];
		if (p < (end - 1) && (c == '\r' && (message_buffer[p + 1] == '\n'))) {
			message_end = p;
			message = std::string(message_buffer.data() + message_begin, message_buffer.data() + message_end);
			message_begin = p + 2;
		} else if (c == '\n' || c == '\r') {
			message_end = p;
			message = std::string(message_buffer.data() + message_begin, message_buffer.data() + message_end);
			message_begin = p + 1;
		} else if (p == end - 1) {
			message_end = end;
			message = std::string(message_buffer.data() + message_begin, message_buffer.data() + message_end);
		}

		if (message_end != 0) {
			final_buffer.resize(_scprintf(final_format,
				timestamp_buffer.size(), timestamp_buffer.data(),
				message.length(), message.data()) + 1);
			snprintf(final_buffer.data(), final_buffer.size(), final_format,
				timestamp_buffer.size(), timestamp_buffer.data(),
				message.length(), message.data());
			final_buffer[final_buffer.size() - 1] = '\n';

			if (log_stdout_enabled) {
				fwrite(final_buffer.data(), sizeof(char), final_buffer.size(), stdout);
			}
			if (log_stderr_enabled) {
				fwrite(final_buffer.data(), sizeof(char), final_buffer.size(), stderr);
			}
			if (log_debug_enabled) {
			#ifdef _WIN32
				if (IsDebuggerPresent()) {
					int wNum = MultiByteToWideChar(CP_UTF8, 0, final_buffer.data(), -1, NULL, 0);
					if (wNum > 1) {
						std::wstring wide_buf;
						wide_buf.reserve(wNum + 1);
						wide_buf.resize(wNum - 1);
						MultiByteToWideChar(CP_UTF8, 0, final_buffer.data(), -1, &wide_buf[0],
							wNum);

						OutputDebugStringW(wide_buf.c_str());
					}
				}
			#endif
			}
			if (log_file_enabled) {
				if (log_file_stream.is_open()) {
					log_file_stream << final_buffer.data();
				}
			}

			message_end = 0;
		}
	}
}
#pragma endregion shared::logger

#pragma region shared::os
std::string shared::os::get_working_directory() {
#ifdef _WIN32
	std::vector<wchar_t> bufUTF16 = std::vector<wchar_t>(65535);
	std::vector<char> bufUTF8;

	_wgetcwd(bufUTF16.data(), int(bufUTF16.size()));

	// Convert from Wide-char to UTF8
	DWORD bufferSize = WideCharToMultiByte(CP_UTF8, 0,
		bufUTF16.data(), int(bufUTF16.size()),
		nullptr, 0,
		NULL, NULL);
	bufUTF8.resize(bufferSize + 1);
	DWORD finalSize = WideCharToMultiByte(CP_UTF8, 0,
		bufUTF16.data(), int(bufUTF16.size()),
		bufUTF8.data(), int(bufUTF8.size()),
		NULL, NULL);
	if (finalSize == 0) {
		// Conversion failed.
		DWORD errorCode = GetLastError();
		return false;
	}

	return bufUTF8.data();
#endif
}
#pragma endregion shared::os

#pragma region shared::time
shared::time::measure_timer::measure_timer() {

}

shared::time::measure_timer::~measure_timer() {

}

std::unique_ptr<shared::time::measure_timer::instance> shared::time::measure_timer::track() {
	return std::make_unique<instance>(this);
}

void shared::time::measure_timer::track(std::chrono::nanoseconds dur) {
	auto el = timings.find(dur);
	if (el != timings.end()) {
		el->second++;
	} else {
		timings.insert(std::make_pair(dur, 1));
	}
	calls++;
}

uint64_t shared::time::measure_timer::count() {
	return calls;
}

std::chrono::nanoseconds shared::time::measure_timer::total() {
	if (timings.size() == 0) {
		return std::chrono::nanoseconds(0);
	}

	std::chrono::nanoseconds val = std::chrono::nanoseconds(0);
	for (auto el : timings) {
		val += el.first * el.second;
	}
	return val;
}

double_t shared::time::measure_timer::average() {
	if (timings.size() == 0) {
		return 0;
	}

	double_t val = 0;
	for (auto el : timings) {
		val += el.first.count() * el.second;
	}
	return (val / calls);
}

std::chrono::nanoseconds shared::time::measure_timer::percentile(double_t pct, bool by_time /*= false*/) {
	if (timings.size() == 0) {
		return std::chrono::nanoseconds(0);
	}

	// Should we gather a percentile by time, or by calls?
	if (by_time) {
		// By time, so find the largest and smallest value.
		// This can be used for median, but not average.

		std::chrono::nanoseconds smallest, largest;
		smallest = timings.begin()->first;
		largest = timings.rbegin()->first;

		for (auto el : timings) {
			double_t el_pct = (double_t((el.first - smallest).count()) / double_t(largest.count()));

			if (is_equal(pct, el_pct, 0.0005)) {
				return el.first;
			}
		}

		return timings.rbegin()->first;
	} else {
		uint64_t curr_accu = 0;
		if (is_equal(pct, 0.0, 0.0005)) {
			return timings.begin()->first;
		}

		for (auto el : timings) {
			uint64_t last_accu = curr_accu;
			curr_accu += el.second;

			double_t last_pct = double_t(last_accu) / double_t(calls);
			double_t curr_pct = double_t(curr_accu) / double_t(calls);

			if ((last_pct < pct) && ((curr_pct > pct) || is_equal(pct, curr_pct, 0.0005))) {
				return el.first;
			}
		}

		return timings.rbegin()->first;
	}
}

shared::time::measure_timer::instance::instance(measure_timer* parent) : parent(parent) {
	begin = std::chrono::high_resolution_clock::now();
}

shared::time::measure_timer::instance::~instance() {
	auto end = std::chrono::high_resolution_clock::now();
	if (parent) {
		auto dur = end - begin;
		parent->track(std::chrono::duration_cast<std::chrono::nanoseconds>(dur));
	}
}

void shared::time::measure_timer::instance::cancel() {
	parent = nullptr;
}

void shared::time::measure_timer::instance::reparent(measure_timer* new_parent) {
	parent = new_parent;
}

#pragma endregion shared::time
