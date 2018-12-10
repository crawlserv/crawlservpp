/*
 * Strings.h
 *
 * Namespace with string helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef SRC_NAMESPACES_STRINGS_H_
#define SRC_NAMESPACES_STRINGS_H_

#include <algorithm>
#include <iomanip>
#include <string>

namespace Strings {
	void replaceAll(std::string& strInOut, const std::string& from, const std::string& to);
	bool stringToBool(std::string inputString);
	void trim(std::string& stringToTrim);
}


#endif /* SRC_NAMESPACES_STRINGS_H_ */
