/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * locale.h
 *
 * List available locales.
 *
 *  Created on: Apr 17, 2019
 *      Author: ans
 */

#ifndef HELPER_PORTABILITY_LOCALE_H_
#define HELPER_PORTABILITY_LOCALE_H_

#include <string>
#include <vector>

#ifdef _WIN32
#include "stdafx.h"
#include "windows.h"
#include <wstring>
#else
#include "../Strings.hpp"
#include "../System.hpp"
#endif

namespace crawlservpp::Helper::Portability {
#ifdef _WIN32
#define BUFFER_SIZE 1024
	BOOL CALLBACK addLocale(LPWSTR pStr, DWORD dwFlags, LPARAM lparam) {
		WCHAR wcBuffer[BUFFER_SIZE];
		int iResult;

		if(GetLocaleInfoEx(pStr, LOCALE_SENGLANGUAGE, wcBuffer, BUFFER_SIZE)) {
			GetLocaleInfoEx(pStr, LOCALE_SENGLANGUAGE, wcBuffer, BUFFER_SIZE);

			std::wstring wstr(wcBuffer);

			((std::vector<std::string> *) lparam)->emplace_back(wstr.begin(), wstr.end());
		}

		return (TRUE);
	}
#endif

	inline std::vector<std::string> enumLocales() {
		std::vector<std::string> result;

#ifdef _WIN32
		EnumSystemLocalesEx(addLocale, LOCALE_ALL, (LPARAM) &result, NULL);
#else
		const std::string locales = Helper::System::exec("locale -a");
#endif

		return Helper::Strings::split(locales, "\n");
	}

} /* crawlservpp::Helper::Portability */


#endif /* HELPER_PORTABILITY_LOCALE_H_ */
