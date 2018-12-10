/*
 * Utf8.cpp
 *
 * Namespace with global UTF-8 helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#include "Utf8.h"

// convert ISO-8859-1 to UTF-8
std::string Utf8::iso88591ToUtf8(const std::string& strIn) {
	std::string strOut;
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
bool Utf8::repairUtf8(const std::string& strIn, std::string& strOut) {
	if(utf8::is_valid(strIn.begin(), strIn.end())) return false;
	utf8::replace_invalid(strIn.begin(), strIn.end(), back_inserter(strOut));
	return true;
}
