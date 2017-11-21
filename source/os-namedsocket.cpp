// A custom IPC solution to bypass electron IPC.
// Copyright(C) 2017 Streamlabs
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

#include "os-namedsocket.hpp"

#define MINIMUM_TIMEOUT 1000000
#define MINIMUM_BUFFER_SIZE 32767

#pragma region Named Socket
OS::NamedSocket::NamedSocket() {
	// Socket is neither initialized or listening.
	m_isInitialized =
		m_isListening = false;

	// Timing out defaults to 2ms.
	m_timeOutWait = std::chrono::nanoseconds(2000000);
	m_timeOutReceive = std::chrono::nanoseconds(2000000);
	m_timeOutSend = std::chrono::nanoseconds(2000000);

	// Buffers default to 1 MB Size.
	m_bufferReceiveSize = MINIMUM_BUFFER_SIZE;
	m_bufferSendSize = MINIMUM_BUFFER_SIZE;
}

OS::NamedSocket::~NamedSocket() {
	Close();
}

#pragma region Options
bool OS::NamedSocket::SetReceiveBufferSize(size_t size) {
	if (m_isInitialized)
		return false;
	if (size < MINIMUM_BUFFER_SIZE)
		return false;
	m_bufferReceiveSize = size;
	return true;
}

size_t OS::NamedSocket::GetReceiveBufferSize() {
	return m_bufferReceiveSize;
}

bool OS::NamedSocket::SetSendBufferSize(size_t size) {
	if (m_isInitialized)
		return false;
	if (size < MINIMUM_BUFFER_SIZE)
		return false;
	m_bufferSendSize = size;
	return true;
}

size_t OS::NamedSocket::GetSendBufferSize() {
	return m_bufferSendSize;
}

bool OS::NamedSocket::SetWaitTimeOut(std::chrono::nanoseconds time) {
	if (m_isInitialized)
		return false;
	if (time < std::chrono::nanoseconds(MINIMUM_TIMEOUT))
		return false;
	m_timeOutWait = time;
	return true;
}

std::chrono::nanoseconds OS::NamedSocket::GetWaitTimeOut() {
	return m_timeOutWait;
}

bool OS::NamedSocket::SetReceiveTimeOut(std::chrono::nanoseconds time) {
	if (m_isInitialized)
		return false;
	if (time < std::chrono::nanoseconds(MINIMUM_TIMEOUT))
		return false;
	m_timeOutReceive = time;
	return true;
}

std::chrono::nanoseconds OS::NamedSocket::GetReceiveTimeOut() {
	return m_timeOutReceive;
}

bool OS::NamedSocket::SetSendTimeOut(std::chrono::nanoseconds time) {
	if (m_isInitialized)
		return false;
	if (time < std::chrono::nanoseconds(MINIMUM_TIMEOUT))
		return false;
	m_timeOutSend = time;
	return true;
}

std::chrono::nanoseconds OS::NamedSocket::GetSendTimeOut() {
	return m_timeOutSend;
}
#pragma endregion Options

#pragma region Listen/Connect/Close
bool OS::NamedSocket::Listen(std::string path, size_t backlog) {
	if (m_isInitialized)
		return false;

	if (backlog == 0)
		backlog = 1;

	if (!_listen(path, backlog)) {
		_close();
		return false;
	}

	m_isInitialized = true;
	m_isListening = true;
	return true;
}

bool OS::NamedSocket::Connect(std::string path) {
	if (m_isInitialized)
		return false;

	if (!_connect(path)) {
		_close();
		return false;
	}

	m_isInitialized = true;
	m_isListening = false;
	return true;
}

bool OS::NamedSocket::Close() {
	if (!m_isInitialized)
		return false;

	if (!_close())
		return false;

	m_isInitialized = false;
	return true;
}
#pragma endregion Listen/Connect/Close

#pragma region Server & Client
bool OS::NamedSocket::IsInitialized() {
	return m_isInitialized;
}

bool OS::NamedSocket::IsServer() {
	return m_isInitialized && m_isListening;
}

bool OS::NamedSocket::IsClient() {
	return m_isInitialized && !m_isListening;
}

std::weak_ptr<OS::NamedSocketConnection> OS::NamedSocket::Accept() {
	for (auto frnt = m_connections.begin(); frnt != m_connections.end(); frnt++) {
		if ((*frnt)->IsWaiting()) {
			return std::weak_ptr<OS::NamedSocketConnection>(*frnt);
		}
	}
	return std::weak_ptr<OS::NamedSocketConnection>();
}
#pragma endregion Server & Client

#pragma region Client Only
std::shared_ptr<OS::NamedSocketConnection> OS::NamedSocket::GetConnection() {
	return m_connections.front();
}
#pragma endregion Client Only
#pragma endregion Named Socket

#pragma region Named Socket Connection
bool OS::NamedSocketConnection::Bad() {
	return !Good();
}
#pragma endregion Named Socket Connection
