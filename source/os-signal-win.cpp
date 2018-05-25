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
 
#include "os-signal-win.hpp"
#include <iostream>

void os::signal_win::create_security_descriptor() {
	DWORD dwRes;

	// Create a well-known SID for the Everyone group.
	if (!AllocateAndInitializeSid(&sd.SIDAuthWorld, 1,
		SECURITY_WORLD_RID,
		0, 0, 0, 0, 0, 0, 0,
		&sd.pEveryoneSID)) {
		std::cerr << "AllocateAndInitializeSid Error" << GetLastError() << std::endl;
		return;
	}

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow Everyone read access to the key.
	ZeroMemory(&sd.ea, 2 * sizeof(EXPLICIT_ACCESS));
	sd.ea[0].grfAccessPermissions = KEY_READ | SYNCHRONIZE | SPECIFIC_RIGHTS_ALL; // SPECIFIC_RIGHTS_ALL is required.
	sd.ea[0].grfAccessMode = SET_ACCESS;
	sd.ea[0].grfInheritance = NO_INHERITANCE;
	sd.ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	sd.ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	sd.ea[0].Trustee.ptstrName = (LPTSTR)sd.pEveryoneSID;

	// Create a SID for the BUILTIN\Administrators group.
	if (!AllocateAndInitializeSid(&sd.SIDAuthNT, 2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&sd.pAdminSID)) {
		std::cerr << "AllocateAndInitializeSid Error" << GetLastError() << std::endl;
		return;
	}

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow the Administrators group full access to
	// the key.
	sd.ea[1].grfAccessPermissions = KEY_ALL_ACCESS | SPECIFIC_RIGHTS_ALL | STANDARD_RIGHTS_ALL;
	sd.ea[1].grfAccessMode = SET_ACCESS;
	sd.ea[1].grfInheritance = NO_INHERITANCE;
	sd.ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	sd.ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
	sd.ea[1].Trustee.ptstrName = (LPTSTR)sd.pAdminSID;

	// Create a new ACL that contains the new ACEs.
	dwRes = SetEntriesInAcl(2, sd.ea, NULL, &sd.pACL);
	if (ERROR_SUCCESS != dwRes) {
		std::cerr << "SetEntriesInAcl Error" << GetLastError() << std::endl;
		return;
	}

	// Initialize a security descriptor.  
	sd.pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR,
		SECURITY_DESCRIPTOR_MIN_LENGTH);
	if (NULL == sd.pSD) {
		std::cerr << "LocalAlloc Error" << GetLastError() << std::endl;
		return;
	}

	if (!InitializeSecurityDescriptor(sd.pSD,
		SECURITY_DESCRIPTOR_REVISION)) {
		std::cerr << "InitializeSecurityDescriptor Error" << GetLastError() << std::endl;
		return;
	}

	// Add the ACL to the security descriptor. 
	if (!SetSecurityDescriptorDacl(sd.pSD,
		TRUE,     // bDaclPresent flag   
		sd.pACL,
		FALSE))   // not a default DACL 
	{
		std::cerr << "SetSecurityDescriptorDacl Error" << GetLastError() << std::endl;
		return;
	}

	// Initialize a security attributes structure.
	sd.sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sd.sa.lpSecurityDescriptor = sd.pSD;
	sd.sa.bInheritHandle = FALSE;
}

void os::signal_win::destroy_security_descriptor() {
	if (sd.pEveryoneSID)
		FreeSid(sd.pEveryoneSID);
	if (sd.pAdminSID)
		FreeSid(sd.pAdminSID);
	if (sd.pACL)
		LocalFree(sd.pACL);
	if (sd.pSD)
		LocalFree(sd.pSD);
}

os::signal_win::~signal_win() {
	CloseHandle(handle);
	destroy_security_descriptor();
}

os::signal_win::signal_win(bool initial_state /*= false*/, bool auto_reset /*= true*/) {
	create_security_descriptor();

	SetLastError(ERROR_SUCCESS);
	handle = CreateEvent(&sd.sa, !auto_reset, initial_state, NULL);
	switch (GetLastError()) {
		case ERROR_ALREADY_EXISTS:
			break;
		default:
			throw std::runtime_error("Failed to create signal.");
			break;
		case ERROR_SUCCESS:
			break;
	}
}

os::signal_win::signal_win(std::string name, bool initial_state /*= false*/, bool auto_reset /*= true*/) {
	create_security_descriptor();

#ifdef _UNICODE
	std::wstring fullname = name;
#else
	std::string fullname = name;
#endif

	SetLastError(ERROR_SUCCESS);
	handle = OpenEvent(EVENT_MODIFY_STATE, false, fullname.c_str());
	DWORD error = GetLastError();
	if (handle) {
		return;
	}

	SetLastError(ERROR_SUCCESS);
	handle = CreateEvent(&sd.sa, !auto_reset, initial_state, fullname.c_str());
	error = GetLastError();
	switch (GetLastError()) {
		case ERROR_ALREADY_EXISTS:
			break;
		case ERROR_INVALID_HANDLE:
			throw std::runtime_error("Named object already exists.");
			break;
		default:
			throw std::runtime_error("Failed to create signal.");
			break;
		case ERROR_SUCCESS:
			break;
	}
}

os::error os::signal_win::clear() {
	BOOL success = ResetEvent(handle);
	return (success ? os::error::Ok : os::error::Error);
}

os::error os::signal_win::set(bool state /*= true*/) {
	BOOL success = 0;
	if (state) {
		success = SetEvent(handle);
	} else {
		success = ResetEvent(handle);
	}
	return (success ? os::error::Ok : os::error::Error);
}

os::error os::signal_win::pulse() {
	BOOL success = PulseEvent(handle);
	return (success ? os::error::Ok : os::error::Error);
}

os::error os::signal_win::wait(std::chrono::nanoseconds timeout) {
	DWORD status = WaitForSingleObjectEx(handle, 
		(DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count(),
		false);
	switch (status) {
		case WAIT_IO_COMPLETION:
			return os::error::Error;
		case WAIT_TIMEOUT:
			return os::error::TimedOut;
		case WAIT_ABANDONED:
			return os::error::Abandoned;
		case WAIT_FAILED:
			return os::error::Error;
		case WAIT_OBJECT_0:
			return os::error::Ok;
	}
}

HANDLE os::signal_win::raw() {
	return handle;
}

os::error os::signal::wait_multiple(std::chrono::nanoseconds timeout, std::vector<std::shared_ptr<os::signal>> signals, size_t& signalled_index, bool wait_all) {
	if (signals.size() == 0) {
		return os::error::TooFew;
	}
	if (signals.size() > MAXIMUM_WAIT_OBJECTS) {
		return os::error::TooMany;
	}

	std::vector<HANDLE> handles(signals.size());
	for (size_t idx = 0; idx < signals.size(); idx++) {
		handles[idx] = std::dynamic_pointer_cast<os::signal_win>(signals[idx])->raw();
	}

	DWORD time = (DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
	DWORD status = WaitForMultipleObjects((DWORD)handles.size(), handles.data(), wait_all, time);
	if (status == WAIT_TIMEOUT) {
		return os::error::TimedOut;
	} else if (status >= WAIT_OBJECT_0 && status < (WAIT_OBJECT_0 + handles.size())) {
		signalled_index = status - WAIT_OBJECT_0;
		return os::error::Ok;
	} else if (status >= WAIT_ABANDONED_0 && status < (WAIT_ABANDONED_0 + handles.size())) {
		signalled_index = status - WAIT_ABANDONED_0;
		return os::error::Abandoned;
	}

	return os::error::Error;
}
