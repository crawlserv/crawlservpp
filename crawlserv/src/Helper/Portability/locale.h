/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
	//! The size of the buffer to receive the description of a locale from Windows.
	inline constexpr auto localeBufferSize{1024};

	//! Callback for adding a locale in Windows.
	/*!
	 * \param pStr Pointer to string containing
	 *   the system name of the locale.
	 * \param dwFlags Flags to be ignored.
	 * \param lparam Pointer to custom data,
	 *   i.e. the vector containing all locales.
	 *
	 * \returns True.
	 */
	BOOL CALLBACK addLocale(LPWSTR pStr, DWORD dwFlags, LPARAM lparam) {
		WCHAR wcBuffer[localeBufferSize];
		int iResult{};

		if(GetLocaleInfoEx(pStr, LOCALE_SENGLANGUAGE, wcBuffer, localeBufferSize)) {
			GetLocaleInfoEx(pStr, LOCALE_SENGLANGUAGE, wcBuffer, localeBufferSize);

			std::wstring wstr(wcBuffer);

			reinterpret_cast<std::vector<std::string> *>(
					lparam)->emplace_back(wstr.cbegin(), wstr.cend()
			);
		}

		return TRUE;
	}
#endif

	//! Enumerates all available locale on the system.
	/*!
	 * \returns A vector of strings containing all available locale
	 *   on the current system.
	 */
	inline std::vector<std::string> enumLocales() {
		std::vector<std::string> result;

#ifdef _WIN32
		EnumSystemLocalesEx(addLocale, LOCALE_ALL, reinterpret_cast<LPARAM>(&result), NULL);
#else
		const std::string locales = Helper::System::exec("locale -a");
#endif

		return Helper::Strings::split(locales, "\n");
	}

} /* namespace crawlservpp::Helper::Portability */

#endif /* HELPER_PORTABILITY_LOCALE_H_ */
