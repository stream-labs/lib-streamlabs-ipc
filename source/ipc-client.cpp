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

#include "ipc-client.hpp"

IPC::Client::Client(std::string socketPath) {
	m_socket = OS::NamedSocket::Create();
	if (!m_socket->Connect(socketPath)) {
		throw std::exception("Failed to initialize socket.");
	}
}

IPC::Client::~Client() {
	m_socket->Close();
}

void IPC::Client::RawWrite(const std::vector<char>& buf) {
	m_socket->GetConnection()->Write(buf);
}

std::vector<char> IPC::Client::RawRead() {
	return m_socket->GetConnection()->Read();
}

size_t IPC::Client::RawAvail() {
	return m_socket->GetConnection()->ReadAvail();
}
