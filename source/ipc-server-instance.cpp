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
#include "ipc.pb.h"

IPC::ServerInstance::ServerInstance() {}

IPC::ServerInstance::ServerInstance(IPC::Server* owner, std::shared_ptr<OS::NamedSocketConnection> conn) {
	m_parent = owner;
	m_socket = conn;
	m_clientId = m_socket->GetClientId();

	m_readWorker.worker = std::thread(ReaderThread, this);
	m_executeWorker.worker = std::thread(ExecuteThread, this);
	m_writeWorker.worker = std::thread(WriterThread, this);
}

IPC::ServerInstance::~ServerInstance() {
	// Threading
	m_stopWorkers = true;
	m_readWorker.cv.notify_all();
	if (m_readWorker.worker.joinable())
		m_readWorker.worker.join();
	m_executeWorker.cv.notify_all();
	if (m_executeWorker.worker.joinable())
		m_executeWorker.worker.join();
	m_writeWorker.cv.notify_all();
	if (m_writeWorker.worker.joinable())
		m_writeWorker.worker.join();
}

void IPC::ServerInstance::QueueMessage(std::vector<char> data) {
	//std::unique_lock<std::mutex> ulock(m_writeLock);
	//m_writeQueue.push(data);
	//m_writeCV.notify_all();
}

bool IPC::ServerInstance::IsAlive() {
	if (m_socket->Bad())
		return false;

	if (m_stopWorkers)
		return false;

	return true;
}

void IPC::ServerInstance::ReaderThread(ServerInstance* ptr) {
	std::vector<char> buf(65535);
	while (!ptr->m_stopWorkers) {
		if (ptr->m_socket->Bad())
			break;

		if (ptr->m_socket->ReadAvail() == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			continue;
		}

		size_t bytes = ptr->m_socket->Read(buf);
		if ((bytes != 0) && (bytes != std::numeric_limits<size_t>::max())) {
			std::unique_lock<std::mutex> ulock(ptr->m_executeWorker.lock);
			ptr->m_executeWorker.queue.push(buf);
			ptr->m_executeWorker.cv.notify_all();
		}
	}
}

void IPC::ServerInstance::ExecuteThread(ServerInstance* ptr) {
	std::unique_lock<std::mutex> ulock(ptr->m_executeWorker.lock);
	while (!ptr->m_stopWorkers) {
		ptr->m_executeWorker.cv.wait(ulock, [ptr]() {
			return (ptr->m_stopWorkers || (ptr->m_executeWorker.queue.size() > 0));
		});

		std::vector<char>& msg = ptr->m_executeWorker.queue.front();
		ptr->m_parent->HandleMessage(ptr->m_clientId, msg);
		ptr->m_executeWorker.queue.pop();
	}
}

void IPC::ServerInstance::WriterThread(ServerInstance* ptr) {
	std::unique_lock<std::mutex> ulock(ptr->m_writeWorker.lock);
	while (!ptr->m_stopWorkers) {
		if (ptr->m_socket->Bad())
			break;

		ptr->m_writeWorker.cv.wait(ulock, [ptr]() {
			return (ptr->m_stopWorkers || (ptr->m_writeWorker.queue.size() > 0));
		});

		auto& msg = ptr->m_writeWorker.queue.front();
		if (msg.size() > 0) {
			size_t written = ptr->m_socket->Write(msg);
			if (written == msg.size()) {
				ptr->m_writeWorker.queue.pop();
			} else if (written == std::numeric_limits<size_t>::max()) {
				// Notify parent of a write too large?
				ptr->m_writeWorker.queue.pop();
			}
		} else {
			ptr->m_writeWorker.queue.pop();
		}
	}
}

