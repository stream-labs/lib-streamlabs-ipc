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

#include "ipc-value.hpp"
#include <iostream>

ipc::value::value() {
	this->type = type::Null;
}

ipc::value::value(const std::vector<char>& p_value):type(type::Binary), value_bin(p_value) {
}

ipc::value::value(const std::string & p_value):type(type::String), value_str(p_value) {
}

ipc::value::value(uint64_t p_value) {
	this->type = type::UInt64;
	this->value_union.ui64 = p_value;
}

ipc::value::value(uint32_t p_value) {
	this->type = type::UInt32;
	this->value_union.ui32 = p_value;
}

ipc::value::value(int64_t p_value) {
	this->type = type::Int64;
	this->value_union.i64 = p_value;
}

ipc::value::value(int32_t p_value) {
	this->type = type::Int32;
	this->value_union.i32 = p_value;
}

ipc::value::value(double p_value) {
	this->type = type::Double;
	this->value_union.fp64 = p_value;
}

ipc::value::value(float p_value) {
	this->type = type::Float;
	this->value_union.fp32 = p_value;
}

size_t ipc::value::size() {
	size_t size = sizeof(uint32_t);
	switch (this->type) {
		case type::Int32:
			size += sizeof(int32_t);
			break;
		case type::UInt32:
			size += sizeof(uint32_t);
			break;
		case type::Float:
			size += sizeof(float_t);
			break;
		case type::Int64:
			size += sizeof(int64_t);
			break;
		case type::UInt64:
			size += sizeof(uint64_t);
			break;
		case type::Double:
			size += sizeof(double_t);
			break;
		case type::String:
			size += sizeof(uint32_t);
			size += this->value_str.size();
			break;
		case type::Binary:
			size += sizeof(uint32_t);
			size += this->value_bin.size();
			break;
	}
	return size;
}

size_t ipc::value::serialize(std::vector<char>& buf, size_t offset) {
	std::cout << "serialize 0" << std::endl;
	size_t buf_size = buf.size() - offset;
	size_t full_size = size();
	if (buf_size < full_size) {
		throw std::exception((const std::exception&)"Value serialization failed, buffer too small");
	}
	std::cout << "serialize 1" << std::endl;
	size_t noffset = offset;
	reinterpret_cast<uint32_t&>(buf[noffset]) = (uint32_t)this->type;
	noffset += sizeof(uint32_t);
	std::cout << "serialize 2,type: " << (uint32_t)this->type << std::endl;
	switch (this->type) {
		case type::Int32:
			std::cout << "serialize Int32" << std::endl;
			reinterpret_cast<int32_t&>(buf[noffset]) = this->value_union.i32;
			std::cout << "serialize Int32 end, value " << this->value_union.i32 << std::endl;
			noffset += sizeof(int32_t);
			std::cout << "serialize Int32 offset " << noffset << std::endl;
			break;
		case type::UInt32:
			std::cout << "serialize UInt32" << std::endl;
			reinterpret_cast<uint32_t&>(buf[noffset]) = this->value_union.ui32;
			std::cout << "serialize UInt32 end, value " << this->value_union.ui32 << std::endl;
			noffset += sizeof(uint32_t);
			std::cout << "serialize Int32 offset " << noffset << std::endl;
			break;
		case type::Float:
			std::cout << "serialize Float" << std::endl;
			reinterpret_cast<float_t&>(buf[noffset]) = this->value_union.fp32;
			std::cout << "serialize Float end, value " << this->value_union.fp32 << std::endl;
			noffset += sizeof(float_t);
			std::cout << "serialize Float offset " << noffset << std::endl;
			break;
		case type::Int64:
			std::cout << "serialize Int64" << std::endl;
			reinterpret_cast<int64_t&>(buf[noffset]) = this->value_union.i64;
			std::cout << "serialize Int64 end, value " << this->value_union.i64 << std::endl;
			noffset += sizeof(int64_t);
			std::cout << "serialize Int64 offset " << noffset << std::endl;
			break;
		case type::UInt64:
			std::cout << "serialize UInt64" << std::endl;
			reinterpret_cast<uint64_t&>(buf[noffset]) = this->value_union.ui64;
			std::cout << "serialize UInt64 end, value " << this->value_union.ui64 << std::endl;
			noffset += sizeof(uint64_t);
			std::cout << "serialize UInt64 offset " << noffset << std::endl;
			break;
		case type::Double:
			std::cout << "serialize Double" << std::endl;
			reinterpret_cast<double_t&>(buf[noffset]) = this->value_union.fp64;
			std::cout << "serialize Double end, value " << this->value_union.fp64 << std::endl;
			noffset += sizeof(double_t);
			std::cout << "serialize Double offset " << noffset << std::endl;
			break;
		case type::String:
			std::cout << "serialize String" << std::endl;
			reinterpret_cast<uint32_t&>(buf[noffset]) = static_cast<uint32_t>(this->value_str.size());
			std::cout << "serialize String end, value " << this->value_str.c_str() << std::endl;
			noffset += sizeof(uint32_t);
			if (this->value_str.size() > 0) {
			    auto str_begin        = this->value_str.begin();
			    auto str_end          = this->value_str.end();
			    auto buffer_begin     = buf.begin() + noffset;
			    std::copy(str_begin, str_end, buffer_begin);
			}
			noffset += this->value_str.size();
			std::cout << "serialize String offset " << noffset << std::endl;
			break;
		case type::Binary:
			std::cout << "serialize Binary" << std::endl;
			reinterpret_cast<uint32_t&>(buf[noffset]) = static_cast<uint32_t>(this->value_bin.size());
			std::cout << "serialize Binary end" << std::endl;
			noffset += sizeof(uint32_t);
			if (this->value_bin.size() > 0) {
			    auto bin_begin    = this->value_bin.begin();
			    auto bin_end      = this->value_bin.end();
			    auto buffer_begin = buf.begin() + noffset;
			    std::copy(bin_begin, bin_end, buffer_begin);
			}
			noffset += this->value_bin.size();
			std::cout << "serialize Binary offset " << noffset << std::endl;
			break;
	}
	std::cout << "serialize 3" << std::endl;
	return noffset - offset;
}

size_t ipc::value::deserialize(const std::vector<char>& buf, size_t offset) {
	std::cout << "value deserialize 0" << std::endl;
	if ((buf.size() - offset) < sizeof(uint32_t)) {
		abort();
		// throw std::exception((const std::exception&)"Buffer too small");
	}
	std::cout << "value deserialize 1" << std::endl;
	this->type = (ipc::type) *(reinterpret_cast<const uint32_t*>(&buf[offset]));
	size_t   noffset = offset + sizeof(uint32_t);
	uint32_t length;
	std::cout << "value deserialize 2, type: " << (uint32_t)this->type << std::endl;
	switch (this->type) {
		case type::Int32:
		case type::UInt32:
		case type::Float:
			std::cout << "value deserialize Int32" << std::endl;
			if ((buf.size() - noffset) < sizeof(int32_t)) {
				abort();
				// throw std::exception((const std::exception&)"Deserialize of 32-bit value failed");
			}
			std::cout << "value deserialize Int32 end" << std::endl;
			memcpy(&this->value_union.i32, &buf[noffset], sizeof(int32_t));
			std::cout << "value deserialize Int32 " << (int32_t)this->value_union.i32 << std::endl;
			noffset += sizeof(int32_t);
			break;
		case type::Int64:
		case type::UInt64:
		case type::Double:
			std::cout << "value deserialize Int64" << std::endl;
			if ((buf.size() - noffset) < sizeof(int64_t)) {
				abort();
				// throw std::exception((const std::exception&)"Deserialize of 64-bit value failed");
			}
			std::cout << "value deserialize Int64 end" << std::endl;
			memcpy(&this->value_union.ui64, &buf[noffset], sizeof(uint64_t));
			std::cout << "value deserialize Int64 " << (int64_t)this->value_union.ui64 << std::endl;
			noffset += sizeof(int64_t);
			break;
		case type::String:
			std::cout << "value deserialize String" << std::endl;
			if ((buf.size() - noffset) < sizeof(uint32_t)) {
				abort();
				// throw std::exception((const std::exception&)"Deserialize of string value failed, length missing");
			}
			length = reinterpret_cast<const uint32_t&>(buf[noffset]);
			noffset += sizeof(uint32_t);
			if ((buf.size() - noffset) < length) {
				abort();
				// throw std::exception((const std::exception&)"Deserialize of string value failed, string missing");
			}

			std::cout << "value deserialize String end" << std::endl;
			this->value_str.clear();
			this->value_str.resize(length);
			if (length > 0) {
			    auto begin      = buf.begin() + noffset;
			    auto end        = buf.begin() + noffset + static_cast<size_t>(length);
			    this->value_str = std::string(begin, end);
			}
			std::cout << "value deserialize String " << this->value_str.c_str() << std::endl;
			noffset += length;
			break;
		case type::Binary:
			std::cout << "value deserialize Binary" << std::endl;
			if ((buf.size() - noffset) < sizeof(uint32_t)) {
				abort();
				// throw std::exception((const std::exception&)"Deserialize of buffer value failed, length missing");
			}
			length = reinterpret_cast<const uint32_t&>(buf[noffset]);
			noffset += sizeof(uint32_t);
			if ((buf.size() - noffset) < length) {
				abort();
				// throw std::exception((const std::exception&)"Deserialize of buffer value failed, buffer missing");
			}
			std::cout << "value deserialize Binary end" << std::endl;
			this->value_bin.clear();
			this->value_bin.resize(length);
			if (length > 0) {
			    auto begin      = buf.begin() + noffset;
			    auto end        = buf.begin() + noffset + static_cast<size_t>(length);
			    this->value_bin = std::vector<char>(begin, end);
			}
			std::cout << "value deserialize Binary end 1" << std::endl;
			noffset += length;
			break;
	}
	std::cout << "value deserialize 3" << std::endl;
	return (noffset - offset);
}
