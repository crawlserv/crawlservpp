/*
 *
 * ---
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
 * Utf8.hpp
 *
 * Namespace with global UTF-8 helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_UTF8_HPP_
#define HELPER_UTF8_HPP_

#include "../Main/Exception.hpp"

#include "../_extern/utf8/source/utf8.h"

#include <string>	// std::string

namespace crawlservpp::Helper::Utf8 {

	/*
	 * DECLARATION
	 */

	std::string iso88591ToUtf8(const std::string& strIn);
	bool isValidUtf8(const std::string& stringToCheck, std::string& errTo);
	bool repairUtf8(const std::string& strIn, std::string& strOut);

	/*
	 * CLASS FOR UTF-8 EXCEPTIONS
	 */

	class Exception : public Main::Exception {
	public:
		Exception(const std::string& description) : Main::Exception(description) {}
	};

	/*
	 * IMPLEMENTATION
	 */

	// convert ISO-8859-1 to UTF-8
	inline std::string iso88591ToUtf8(const std::string& strIn) {
		std::string strOut;

		strOut.reserve(strIn.size() * 2);

		for(const uint8_t c : strIn) {
			if(c < 0x80)
				strOut.push_back(c);
			else {
				strOut.push_back(0xc0 | c >> 6);
				strOut.push_back(0x80 | (c & 0x3f));
			}
		}
		return strOut;
	}

	// check for valid UTF-8 string
	inline bool isValidUtf8(const std::string& stringToCheck, std::string& errTo) {
		try {
			return utf8::is_valid(stringToCheck.begin(), stringToCheck.end());
		}
		catch(const utf8::exception& e) {
			errTo = e.what();

			return false;
		}
	}

	// replace invalid UTF-8 characters, return whether invalid characters occured, throws Utf8::Exception
	inline bool repairUtf8(const std::string& strIn, std::string& strOut) {
		try {
			if(utf8::is_valid(strIn.begin(), strIn.end()))
				return false;

			utf8::replace_invalid(strIn.begin(), strIn.end(), back_inserter(strOut));

			return true;
		}
		catch(const utf8::exception& e) {
			throw Exception("UTF-8 error: " + std::string(e.what()));
		}
	}

} /* crawlservpp::Helper::Utf8 */

#endif /* HELPER_UTF8_HPP_ */
