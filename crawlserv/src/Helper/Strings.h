/*
 * Strings.h
 *
 * Namespace with global string helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_STRINGS_H_
#define HELPER_STRINGS_H_

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <string>
#include <vector>

namespace Helper::Strings {
	// Unicode white spaces
	const std::string utfWhitespaces[] = {
		"\u0085", // next line (NEL)
		"\u00a0", // no-break space
		"\u1680", // Ogham space mark
		"\u2000", // en quad
		"\u2001", // em quad
		"\u2002", // en space
		"\u2003", // em space
		"\u2004", // three-per-em space
		"\u2005", // four-per-em space
		"\u2006", // six-per-em space
		"\u2007", // figure space
		"\u2008", // punctuation space
		"\u2009", // thin space
		"\u200a", // hair space
		"\u2028", // line separator
		"\u2029", // paragraph separator
		"\u202f", // narrow no-break space
		"\u205f", // medium mathematical space
		"\u3000", // ideographic space
	};

	void replaceAll(std::string& strInOut, const std::string& from, const std::string& to, bool onlyOnce);
	bool stringToBool(std::string inputString);
	void trim(std::string& stringToTrim);
	std::string concat(const std::vector<std::string>& vectorToConcat, char delimiter, bool ignoreEmpty);
	char getFirstOrEscapeChar(const std::string& from);
	void utfTidy(std::string& stringToTidy);
	bool checkSQLName(const std::string& name);
}


#endif /* HELPER_STRINGS_H_ */
