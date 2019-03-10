/*
 * Utf8.hpp
 *
 * Namespace with global UTF-8 helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_UTF8_HPP_
#define HELPER_UTF8_HPP_

#include "../_extern/utf8/source/utf8.h"

#include <string>

namespace crawlservpp::Helper::Utf8 {

	/*
	 * DECLARATION
	 */

	std::string iso88591ToUtf8(const std::string& strIn);
	bool repairUtf8(const std::string& strIn, std::string& strOut);

	/*
	 * IMPLEMENTATION
	 */

	// convert ISO-8859-1 to UTF-8
	inline std::string iso88591ToUtf8(const std::string& strIn) {
		std::string strOut;
		strOut.reserve(strIn.size() * 2);
		for(auto i = strIn.begin(); i != strIn.end(); ++i) {
			uint8_t c = *i;
			if(c < 0x80) strOut.push_back(c);
			else {
				strOut.push_back(0xc0 | c >> 6);
				strOut.push_back(0x80 | (c & 0x3f));
			}
		}
		return strOut;
	}

	// replace invalid UTF-8 characters, return whether invalid characters occured
	inline bool repairUtf8(const std::string& strIn, std::string& strOut) {
		if(utf8::is_valid(strIn.begin(), strIn.end())) return false;
		utf8::replace_invalid(strIn.begin(), strIn.end(), back_inserter(strOut));
		return true;
	}

} /* crawlservpp::Helper::Utf8 */

#endif /* HELPER_UTF8_HPP_ */
