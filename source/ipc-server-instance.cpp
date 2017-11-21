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

#include "ipc-server-instance.hpp"

IPC::ServerInstance::ServerInstance() {}

IPC::ServerInstance::ServerInstance(IPC::Server* owner, std::shared_ptr<OS::NamedSocketConnection> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = m_socket->GetClientId();

	m_readWorker.worker = std::thread(WorkerMain, this, false);
	m_writeWorker.worker = std::thread(WorkerMain, this, true);
}

IPC::ServerInstance::~ServerInstance() {
	m_readWorker.shutdown = true;
	if (m_readWorker.worker.joinable())
		m_readWorker.worker.join();
	m_writeWorker.shutdown = true;
	m_writeCV.notify_all();
	if (m_writeWorker.worker.joinable())
		m_writeWorker.worker.join();
}

void IPC::ServerInstance::QueueMessage(std::vector<char> data) {
	std::unique_lock<std::mutex> ulock(m_writeLock);
	m_writeQueue.push(data);
	m_writeCV.notify_all();
}

void IPC::ServerInstance::WorkerMain(ServerInstance* ptr, bool writer) {
	ptr->WorkerLocal(writer);
}

void IPC::ServerInstance::WorkerLocal(bool writer) {
	WorkerStruct* work = (writer ? &m_writeWorker : &m_readWorker);
	while ((m_readWorker.shutdown == false) && IsAlive()) {
		if (writer) {
			if (!WriterTask())
				break;
		} else {
			if (!ReaderTask())
				break;
		}
	}
}

bool IPC::ServerInstance::ReaderTask() {
	while (m_socket->ReadAvail() > 0)
		m_parent->handle_message(m_clientId, m_socket->Read());

	if (!IsAlive())
		return false;

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	return true;
}

bool IPC::ServerInstance::WriterTask() {
	std::queue<std::vector<char>> dataQueue;
	{
		std::unique_lock<std::mutex> ulock(m_writeLock);
		m_writeCV.wait(ulock, [this] {
			return m_writeWorker.shutdown || m_writeQueue.size() > 0 || !IsAlive();
		});

		if (m_writeWorker.shutdown || !IsAlive())
			return false;

		if (m_writeQueue.size() > 0) {
			dataQueue.swap(m_writeQueue);
		}
	}

	while (dataQueue.size() > 0) {
		std::vector<char> data = std::move(dataQueue.front());
		if (m_socket->Write(data) == data.size()) {
			m_writeQueue.pop();
		} else {
			return false;
		}
	}

	if (!IsAlive())
		return false;

	return true;
}

bool IPC::ServerInstance::IsAlive() {
	if (m_socket->Bad())
		return false;

	if (m_writeWorker.shutdown)
		return false;

	return true;
}
