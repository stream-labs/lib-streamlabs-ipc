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

#pragma region SIDs
	// Create a well-known SID for the Everyone group.
	if (!AllocateAndInitializeSid(&sd.SIDAuthWorld, 1,
		SECURITY_WORLD_RID,
		0, 0, 0, 0, 0, 0, 0,
		&sd.pEveryoneSID)) {
		std::cerr << "AllocateAndInitializeSid Error" << GetLastError() << std::endl;
		return;
	}

	// Create a SID for the BUILTIN\AuthenticatedUsers group.
	if (!AllocateAndInitializeSid(&sd.SIDAuthUsers, 2,
		SECURITY_BUILTIN_DOMAIN_RID,
		SECURITY_AUTHENTICATED_USER_RID,
		0, 0, 0, 0, 0, 0,
		&sd.pAuthenticatedUsersSID)) {
		std::cerr << "AllocateAndInitializeSid Error" << GetLastError() << std::endl;
		return;
	}

	// Create a SID for the BUILTIN\Administrators group.
	if (!AllocateAndInitializeSid(&sd.SIDAuthNT, 2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&sd.pAdminSID)) {
		std::cerr << "AllocateAndInitializeSid Error" << GetLastError() << std::endl;
		return;
	}
#pragma endregion SIDs

	ZeroMemory(&sd.ea, sizeof(sd.ea));

	DWORD access = GENERIC_ALL | GENERIC_EXECUTE | GENERIC_READ | GENERIC_WRITE | KEY_ALL_ACCESS | SPECIFIC_RIGHTS_ALL | STANDARD_RIGHTS_ALL | SYNCHRONIZE;

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow Everyone read access to the key.
	sd.ea[0].grfAccessPermissions = access;
	sd.ea[0].grfAccessMode = SET_ACCESS;
	sd.ea[0].grfInheritance = NO_INHERITANCE;
	sd.ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	sd.ea[0].Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
	sd.ea[0].Trustee.ptstrName = (LPTSTR)sd.pEveryoneSID;

	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow the Administrators group full access to
	// the key.
	sd.ea[1].grfAccessPermissions = access;
	sd.ea[1].grfAccessMode = SET_ACCESS;
	sd.ea[1].grfInheritance = NO_INHERITANCE;
	sd.ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	sd.ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
	sd.ea[1].Trustee.ptstrName = (LPTSTR)sd.pAdminSID;
	
	// Initialize an EXPLICIT_ACCESS structure for an ACE.
	// The ACE will allow the Administrators group full access to
	// the key.
	sd.ea[2].grfAccessPermissions = access;
	sd.ea[2].grfAccessMode = SET_ACCESS;
	sd.ea[2].grfInheritance = NO_INHERITANCE;
	sd.ea[2].Trustee.TrusteeForm = TRUSTEE_IS_SID;
	sd.ea[2].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
	sd.ea[2].Trustee.ptstrName = (LPTSTR)sd.pAdminSID;

	DWORD aclsize = sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) * 3
		+ GetLengthSid(sd.pEveryoneSID)
		+ GetLengthSid(sd.pAuthenticatedUsersSID)
		+ GetLengthSid(sd.pAdminSID);
	sd.pACL = (PACL)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, aclsize);

	// initialize the DACL
	if (!InitializeAcl(sd.pACL, aclsize, ACL_REVISION)) {
		std::cerr << "InitializeAcl() failed with error " << GetLastError() << std::endl;
		return;
	}

	// Create a new ACL that contains the new ACEs.
	PACL newACL;
	dwRes = SetEntriesInAcl(3, sd.ea, sd.pACL, &newACL);
	if (ERROR_SUCCESS != dwRes) {
		std::cerr << "SetEntriesInAcl Error" << GetLastError() << std::endl;
		return;
	}
	if (newACL != sd.pACL) {
		HeapFree(GetProcessHeap(), 0, sd.pACL);
		sd.pACL = newACL;
	}

	// add the Authenticated Users group ACE to the DACL with
	// GENERIC_READ, GENERIC_WRITE, and GENERIC_EXECUTE access
	if (!AddAccessAllowedAce(sd.pACL, ACL_REVISION,
		GENERIC_ALL,
		sd.pEveryoneSID)) {
		printf("AddAccessAllowedAce() failed with error %d/n",
			GetLastError());
		return;
	}

	// add the Authenticated Users group ACE to the DACL with
	// GENERIC_READ, GENERIC_WRITE, and GENERIC_EXECUTE access
	if (!AddAccessAllowedAce(sd.pACL, ACL_REVISION,
		GENERIC_ALL,
		sd.pAuthenticatedUsersSID)) {
		printf("AddAccessAllowedAce() failed with error %d/n",
			GetLastError());
		return;
	}

	// add the Authenticated Users group ACE to the DACL with
	// GENERIC_READ, GENERIC_WRITE, and GENERIC_EXECUTE access
	if (!AddAccessAllowedAce(sd.pACL, ACL_REVISION,
		GENERIC_ALL,
		sd.pAdminSID)) {
		printf("AddAccessAllowedAce() failed with error %d/n",
			GetLastError());
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
	sd.sa.bInheritHandle = TRUE;
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
	handle = CreateSemaphore(&sd.sa, initial_state ? 1 : 0, 1, NULL);
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
	handle = OpenSemaphore(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, fullname.c_str());
	DWORD error = GetLastError();
	if (handle) {
		return;
	}

	SetLastError(ERROR_SUCCESS);
	handle = CreateSemaphore(&sd.sa, initial_state ? 1 : 0, 1, fullname.c_str());
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
	return (wait(std::chrono::nanoseconds(0)) == os::error::Success) ? os::error::Success : os::error::Error;
}

os::error os::signal_win::set(bool state /*= true*/) {
	if (state) {
		return (ReleaseSemaphore(handle, 1, NULL) != 0) ? os::error::Success : os::error::Error;
	} else {
		return clear();
	}
}

os::error os::signal_win::pulse() {
	if (set(true) != os::error::Success) {
		return os::error::Error;
	}
	Sleep(0);
	set(false);
	return os::error::Success;
}

os::error os::signal_win::wait(std::chrono::nanoseconds timeout) {
	DWORD status = WaitForSingleObjectEx(handle, 
		(DWORD)std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count(),
		false);
	DWORD error = GetLastError();
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

	return os::error::Error;
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
