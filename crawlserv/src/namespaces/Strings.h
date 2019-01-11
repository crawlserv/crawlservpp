/*
 * Strings.h
 *
 * Namespace with global string helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef SRC_NAMESPACES_STRINGS_H_
#define SRC_NAMESPACES_STRINGS_H_

#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>

namespace Strings {
	void replaceAll(std::string& strInOut, const std::string& from, const std::string& to, bool onlyOnce);
	bool stringToBool(std::string inputString);
	void trim(std::string& stringToTrim);
	std::string concat(const std::vector<std::string>& vectorToConcat, char delimiter, bool ignoreEmpty);
	char getFirstOrEscapeChar(const std::string& from);
	void tidy(std::string& stringToTidy);
}


#endif /* SRC_NAMESPACES_STRINGS_H_ */
