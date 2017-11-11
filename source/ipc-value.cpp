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

#include "ipc-value.hpp"

IPC::Value::Value() {
	this->type = Type::Null;
}

IPC::Value::Value(std::vector<char> p_value) {
	this->type = Type::Binary;
	this->value_bin = p_value;
}

IPC::Value::Value(std::string p_value) {
	this->type = Type::String;
	this->value_str = p_value;
}

IPC::Value::Value(uint64_t p_value) {
	this->type = Type::UInt64;
	this->value.ui64 = p_value;
}

IPC::Value::Value(uint32_t p_value) {
	this->type = Type::UInt32;
	this->value.ui32 = p_value;
}

IPC::Value::Value(int64_t p_value) {
	this->type = Type::Int64;
	this->value.i64 = p_value;
}

IPC::Value::Value(int32_t p_value) {
	this->type = Type::Int32;
	this->value.i32 = p_value;
}

IPC::Value::Value(double p_value) {
	this->type = Type::Double;
	this->value.fp64 = p_value;
}

IPC::Value::Value(float p_value) {
	this->type = Type::Float;
	this->value.fp32 = p_value;
}
