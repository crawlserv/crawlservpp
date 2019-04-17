/*
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
		std::string locales = Helper::System::exec("locale -a");
#endif

		return Helper::Strings::split(locales, "\n");
	}

} /* crawlservpp::Helper::Portability */


#endif /* HELPER_PORTABILITY_LOCALE_H_ */
