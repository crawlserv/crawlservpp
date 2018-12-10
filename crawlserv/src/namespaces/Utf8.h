/*
 * Utf8.h
 *
 * Namespace with global UTF-8 helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef SRC_NAMESPACES_UTF8_H_
#define SRC_NAMESPACES_UTF8_H_

#include "../external/utf8.h"

#include <string>

namespace Utf8 {
	std::string iso88591ToUtf8(const std::string& strIn);
	bool repairUtf8(const std::string& strIn, std::string& strOut);
}

#endif /* SRC_NAMESPACES_UTF8_H_ */
