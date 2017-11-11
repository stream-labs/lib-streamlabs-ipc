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
	m_readWorker.shutdown = m_writeWorker.shutdown = true;
	m_readWorker.worker.join();
	m_writeCV.notify_all();
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
	while (!work->shutdown) {
		if (writer) {
			if (!WriterTask()) {
				break;
			}
		} else {
			if (!ReaderTask()) {
				break;
			}
		}
	}

	m_parent->OnClientDisconnected(m_clientId);
}

bool IPC::ServerInstance::ReaderTask() {
	while (m_socket->ReadAvail() > 0) {
		if (m_socket->Bad())
			return false;

		if (m_readWorker.shutdown)
			return false;

		m_parent->ExecuteMessage(m_clientId, m_socket->Read());
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	return true;
}

bool IPC::ServerInstance::WriterTask() {
	std::unique_lock<std::mutex> ulock(m_writeLock);
	m_writeCV.wait(ulock, [this] {
		return m_writeWorker.shutdown || m_writeQueue.size() > 0 || m_socket->Bad();
	});

	if (m_socket->Bad())
		return false;

	if (m_writeWorker.shutdown)
		return false;

	while (m_writeQueue.size() > 0) {
		std::vector<char> data = m_writeQueue.front();
		if (m_socket->Write(data) == data.size()) {
			m_writeQueue.pop();
		} else {
			m_writeQueue.pop();
			m_writeQueue.push(data);
		}

		if (m_socket->Bad())
			return false;
	}

	return true;
}
