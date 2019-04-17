/*
 * Strings.hpp
 *
 * Namespace with global string helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_STRINGS_HPP_
#define HELPER_STRINGS_HPP_

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <string>
#include <vector>

namespace crawlservpp::Helper::Strings {

	/*
	 * DECLARATION
	 */

	void replaceAll(std::string& strInOut, const std::string& from, const std::string& to, bool onlyOnce);
	bool stringToBool(std::string inputString);
	void trim(std::string& stringToTrim);
	std::string concat(const std::vector<std::string>& vectorToConcat, char delimiter, bool ignoreEmpty);
	void sortAndRemoveDuplicates(std::vector<std::string>& vectorOfStrings, bool caseSensitive);
	char getFirstOrEscapeChar(const std::string& from);
	void utfTidy(std::string& stringToTidy);
	bool checkDomainName(const std::string& name);
	bool checkSQLName(const std::string& name);

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

	/*
	 * IMPLEMENTATION
	 */

	// replace all occurences of a string with another string (onlyOnce avoids replacing parts of the replacements)
	inline void replaceAll(std::string& strInOut, const std::string& from, const std::string& to, bool onlyOnce) {
		unsigned long startPos = 0;
		unsigned long jump = 0;
		if(from.empty()) return;

		// avoid infinite loop
		if(!onlyOnce) {
			jump = to.find_last_of(from);

			if(jump == std::string::npos)
				jump = 0;
			else
				++jump;
		}

		// replace!
		while((startPos = strInOut.find(from, startPos)) != std::string::npos) {
			strInOut.replace(startPos, from.length(), to);

			if(onlyOnce)
				startPos += to.length();
			else
				startPos += jump;
		}
	}

	// convert string to boolean value
	inline bool stringToBool(std::string inputString) {
		std::transform(inputString.begin(), inputString.end(), inputString.begin(), ::tolower);

		std::istringstream strStr(inputString);

		bool result;

		strStr >> std::boolalpha >> result;

		return result;
	}

	// trim a string (NOTE: Only ASCII white spaces will be processed!)
	inline void trim(std::string & stringToTrim) {
		stringToTrim.erase(stringToTrim.begin(), std::find_if(stringToTrim.begin(), stringToTrim.end(), [](int ch) {
			return !std::isspace(ch);
		}));

		stringToTrim.erase(std::find_if(stringToTrim.rbegin(), stringToTrim.rend(), [](int ch) {
			return !std::isspace(ch);
		}).base(), stringToTrim.end());
	}

	// concatenate all elements of a vector into a single string
	inline std::string concat(const std::vector<std::string>& vectorToConcat, char delimiter, bool ignoreEmpty) {
		std::string result;

		for(auto i = vectorToConcat.begin(); i != vectorToConcat.end(); ++i)
			if(!ignoreEmpty || !(i->empty())) result += *i + delimiter;

		if(!result.empty())
			result.pop_back();

		return result;
	}

	// sort vector of strings and remove duplicates (NOTE: Only ASCII supported!)
	inline void sortAndRemoveDuplicates(std::vector<std::string>& vectorOfStrings, bool caseSensitive) {
		if(caseSensitive) {
			// case-sensitive sort
			std::sort(vectorOfStrings.begin(), vectorOfStrings.end());

			// case-sensitive removal of co-occuring duplicates
			vectorOfStrings.erase(std::unique(vectorOfStrings.begin(), vectorOfStrings.end()), vectorOfStrings.end());
		}
		else {
			// case-insensitive sort
			std::sort(vectorOfStrings.begin(), vectorOfStrings.end(),
					[](const auto& s1, const auto& s2) {
						const auto result = std::mismatch(s1.cbegin(), s1.cend(), s2.cbegin(), s2.cend(),
								[](const auto& s1, const auto& s2) {
									return (s1 == s2) || std::tolower(s1) == std::tolower(s2);
								}
						);

						return result.second != s2.cend()
								&& (
										result.first == s1.cend()
										|| std::tolower(*result.first) < std::tolower(*result.second)
								);
				}
			);

			// case-insensitive removal of co-orruring duplicates
			vectorOfStrings.erase(std::unique(vectorOfStrings.begin(), vectorOfStrings.end(),
					[](const auto& s1, const auto& s2) {
						return (s1.size() == s2.size()) && std::equal(s1.begin(), s1.end(), s2.begin(),
								[](const auto& c1, const auto& c2) {
									return (c1 == c2) || std::tolower(c1) == std::tolower(c2);
								}
						);
					}
			), vectorOfStrings.end());
		}
	}

	// get the first character of the string or an escaped character (\n, \t or \\) (NOTE: Only ASCII supported!)
	inline char getFirstOrEscapeChar(const std::string& from) {
		if(!from.empty()) {
			if(from.at(0) == '\\' && from.length() > 1) {
				switch(from.at(1)) {
				case 'n':
					return '\n';

				case 't':
					return '\t';

				case '\\':
					return '\\';
				}
			}
			else
				return from.at(0);
		}

		return 0;
	}

	// remove new lines and unnecessary spaces (including Unicode white spaces)
	inline void utfTidy(std::string& stringToTidy) {
		// replace Unicode white spaces with spaces
		for(unsigned long n = 0; n < sizeof(utfWhitespaces) / sizeof(std::string); ++n)
			replaceAll(stringToTidy, utfWhitespaces[n], " ", true);

		// replace special ASCII characters with spaces
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\t', ' '); // horizontal tab
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\n', ' '); // line feed
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\v', ' '); // vertical tab
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\f', ' '); // form feed
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\r', ' '); // carriage return

		// replace unnecessary spaces
		replaceAll(stringToTidy, " .", ".", true);
		replaceAll(stringToTidy, " ,", ",", true);
		replaceAll(stringToTidy, " :", ":", true);
		replaceAll(stringToTidy, " ;", ";", true);
		replaceAll(stringToTidy, "( ", "(", true);
		replaceAll(stringToTidy, " )", ")", true);

		// replace double spaces
		replaceAll(stringToTidy, "  ", " ", false);

		// trim result
		trim(stringToTidy);
	}

	// check whether the name is valid as domain (checking only for characters that interfere with internal SQL statements)
	inline bool checkDomainName(const std::string& name) {
		return name.find_first_of("/\'") == std::string::npos;
	}

	// check whether the name is valid for MySQL purposes (table and field names)
	inline bool checkSQLName(const std::string& name) {
		return name.find_first_not_of(
				"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$_"
		) == std::string::npos;
	}

} // /* crawlservpp::Helper::Strings */


#endif /* HELPER_STRINGS_HPP_ */
