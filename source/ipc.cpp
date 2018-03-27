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

#include "ipc.hpp"

std::string ipc::base::make_unique_id(std::string name, std::vector<type> parameters) {
	// Implement similar behavior to C/C++ compilers, which put parameter type
	//  into the generated function name in order to allow overloading of the
	//  same function, even when exported.
	// This behavior might not be desired, but allows some amount of flexibility.

	std::string uq = name;
	if (parameters.size() > 0) {
		uq += "_";
		for (type p : parameters) {
			switch (p) {
				case ipc::type::Null:
					uq += "N0";
					break;
				case ipc::type::Float:
					uq += "F4";
					break;
				case ipc::type::Double:
					uq += "F8";
					break;
				case ipc::type::Int32:
					uq += "I4";
					break;
				case ipc::type::Int64:
					uq += "I8";
					break;
				case ipc::type::UInt32:
					uq += "U4";
					break;
				case ipc::type::UInt64:
					uq += "U8";
					break;
				case ipc::type::String:
					uq += "PS";
					break;
				case ipc::type::Binary:
					uq += "PB";
					break;
			}
		}
	}
	return uq;
}
