// A custom IPC solution to bypass electron IPC.
// Copyright(C) 2018 Streamlabs (General Workings Inc)
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

#pragma once

namespace os {
	enum class error {
		// Everything went right
		Ok = 0,
		Success = Ok,

		// Generic Error
		Error,
		
		// The buffer you passed is invalid.
		InvalidBuffer,

		// Too small buffer, element count, etc.
		BufferTooSmall,
		TooSmall = BufferTooSmall,
		TooTiny = BufferTooSmall,
		TooFew = BufferTooSmall,
		
		// Too large buffer, element count, etc.
		BufferTooLarge,
		TooLarge = BufferTooLarge,
		TooBig = BufferTooLarge,
		TooMany = BufferTooLarge,

		// read() has more data available.
		MoreData, 

		// Timed out
		TimedOut,

		// Disconnected
		Disconnected,

		// Abandoned Object, Ownership retrieved
		Abandoned,


	};

	inline char const* to_string(os::error error_code) {
		using os::error;
		using namespace os;
		switch (error_code) {
			case error::Ok:
				return "Ok";
			case error::Error:
				return "Error";
			case error::InvalidBuffer:
				return "Invalid Buffer";
			case error::BufferTooSmall:
				return "Buffer Too Small";
			case error::BufferTooLarge:
				return "Buffer Too Large";
			case error::MoreData:
				return "More Data";
			case error::TimedOut:
				return "Timed Out";
			case error::Disconnected:
				return "Disconnected";
		}
		return "";
	}
}
