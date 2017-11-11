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

std::string IPC::Base::MakeFunctionUniqueId(std::string name, std::vector<Type> parameters) {
	// Implement similar behavior to C/C++ compilers, which put parameter type
	//  into the generated function name in order to allow overloading of the
	//  same function, even when exported.
	// This behavior might not be desired, but allows some amount of flexibility.

	std::string uq = name + "_";
	for (Type p : parameters) {
		switch (p) {
			case IPC::Type::Null:
				uq += "N0";
				break;
			case IPC::Type::Float:
				uq += "F4";
				break;
			case IPC::Type::Double:
				uq += "F8";
				break;
			case IPC::Type::Int32:
				uq += "I4";
				break;
			case IPC::Type::Int64:
				uq += "I8";
				break;
			case IPC::Type::UInt32:
				uq += "U4";
				break;
			case IPC::Type::UInt64:
				uq += "U8";
				break;
			case IPC::Type::String:
				uq += "PS";
				break;
			case IPC::Type::Binary:
				uq += "PB";
				break;
		}
	}
	return uq;
}
