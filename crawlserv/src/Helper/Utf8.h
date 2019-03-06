/*
 * Utf8.h
 *
 * Namespace with global UTF-8 helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_UTF8_H_
#define HELPER_UTF8_H_

#include "../_extern/utf8/source/utf8.h"

#include <string>

namespace Helper::Utf8 {
	std::string iso88591ToUtf8(const std::string& strIn);
	bool repairUtf8(const std::string& strIn, std::string& strOut);
}

#endif /* HELPER_UTF8_H_ */
