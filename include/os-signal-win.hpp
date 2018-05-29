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

#pragma once

#include "os-error.hpp"
#include "os-signal.hpp"
#include <string>
#include <vector>

#undef WINVER
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#define WINVER _WIN32_WINNT_WIN7
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

extern "C" {
#include <windows.h>
#include <AclAPI.h>
#include <AccCtrl.h>
}

namespace os {
	class signal_win : public signal {
		HANDLE handle;
		struct {
			SECURITY_ATTRIBUTES sa;
			PSECURITY_DESCRIPTOR pSD = NULL;
			PSID pEveryoneSID = NULL, pAuthenticatedUsersSID = NULL, pAdminSID = NULL;
			PACL pACL = NULL;
			EXPLICIT_ACCESS ea[3];			
			SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_WORLD_SID_AUTHORITY;
			SID_IDENTIFIER_AUTHORITY SIDAuthUsers = SECURITY_NT_AUTHORITY;
			SID_IDENTIFIER_AUTHORITY SIDAuthNT = SECURITY_NT_AUTHORITY;
		} sd;

		protected:
		void create_security_descriptor();
		void destroy_security_descriptor();

		public:
		virtual ~signal_win() override;
		signal_win(bool initial_state = false, bool auto_reset = true);
		signal_win(std::string name, bool initial_state = false, bool auto_reset = true);

		virtual os::error clear() override;
		virtual os::error set(bool state = true) override;
		virtual os::error pulse() override;
		virtual os::error wait(std::chrono::nanoseconds timeout) override;

		HANDLE raw();
	};
}
