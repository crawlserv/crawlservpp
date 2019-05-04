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
#include <queue>
#include <random>
#include <string>
#include <vector>

namespace crawlservpp::Helper::Strings {

	/*
	 * DECLARATION
	 */

	void replaceAll(
			std::string& strInOut,
			const std::string& from,
			const std::string& to,
			bool onlyOnce
	);

	bool stringToBool(std::string inputString);

	void trim(std::string& stringToTrim);

	std::string join(
			const std::vector<std::string>& strings,
			char delimiter,
			bool ignoreEmpty
	);
	std::string join(
			const std::vector<std::string>& strings,
			const std::string& delimiter,
			bool ignoreEmpty
	);
	std::string join(
			std::queue<std::string>& strings,
			char delimiter,
			bool ignoreEmpty
	);
	std::string join(
			std::queue<std::string>& strings,
			const std::string& delimiter,
			bool ignoreEmpty
	);
	void join(
			const std::vector<std::string>& strings,
			char delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	);
	void join(
			const std::vector<std::string>& strings,
			const std::string& delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	);
	void join(
			std::queue<std::string>& strings,
			char delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	);
	void join(
			std::queue<std::string>& strings,
			const std::string& delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	);

	std::vector<std::string> split(const std::string& str, char delimiter);
	std::vector<std::string> split(const std::string& str, const std::string& delimiter);
	std::queue<std::string> splitToQueue(const std::string& str, char delimiter);
	std::queue<std::string> splitToQueue(const std::string& str, const std::string& delimiter);

	void sortAndRemoveDuplicates(std::vector<std::string>& vectorOfStrings, bool caseSensitive);

	char getFirstOrEscapeChar(const std::string& from);

	void utfTidy(std::string& stringToTidy);

	bool checkDomainName(const std::string& name);
	bool checkSQLName(const std::string& name);

	std::string randomGenerate(unsigned long length);

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
	inline void replaceAll(
			std::string& strInOut,
			const std::string& from,
			const std::string& to,
			bool onlyOnce
	) {
		unsigned long startPos = 0;
		unsigned long jump = 0;

		if(from.empty())
			return;

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
	inline std::string join(
			const std::vector<std::string>& strings,
			char delimiter,
			bool ignoreEmpty
	) {
		unsigned long size = 0;
		std::string result;

		// calculate and reserve needed memory
		for(auto i = strings.begin(); i != strings.end(); ++i)
			if(!ignoreEmpty || !(i->empty()))
				size += i->size() + 1;

		result.reserve(size);

		// create string
		for(auto i = strings.begin(); i != strings.end(); ++i)
			if(!ignoreEmpty || !(i->empty()))
				result += *i + delimiter;

		if(!result.empty())
			result.pop_back();

		// return string
		return result;
	}

	// concatenate all elements of a vector into a single string
	inline std::string join(
			const std::vector<std::string>& strings,
			const std::string& delimiter,
			bool ignoreEmpty
	) {
		std::string result;

		// calculate and reserve needed memory
		unsigned long size = 0;

		for(auto i = strings.begin(); i != strings.end(); ++i)
			if(!ignoreEmpty || !(i->empty()))
				size += i->size() + delimiter.size();

		result.reserve(size);

		// create string

		for(auto i = strings.begin(); i != strings.end(); ++i)
			if(!ignoreEmpty || !(i->empty()))
				result += *i + delimiter;

		if(!result.empty())
			result.pop_back();

		// return string
		return result;
	}

	// concatenate all elements of a queue into a single string, emptying the queue in the process
	inline std::string join(
			std::queue<std::string>& strings,
			char delimiter,
			bool ignoreEmpty
	) {
		// create string
		std::string result;

		while(!strings.empty()) {
			if(!ignoreEmpty || !(strings.front().empty()))
				result += strings.front() + delimiter;

			strings.pop();
		}

		if(!result.empty())
			result.pop_back();

		// return string
		return result;
	}

	// concatenate all elements of a queue into a single string, emptying the queue in the process
	inline std::string join(
			std::queue<std::string>& strings,
			const std::string& delimiter,
			bool ignoreEmpty
	) {
		// create string
		std::string result;

		while(!strings.empty()) {
			if(!ignoreEmpty || !(strings.front().empty()))
				result += strings.front() + delimiter;

			strings.pop();
		}

		if(!result.empty())
			result.pop_back();

		// return string
		return result;
	}

	// concatenate all elements of a vector and append them to a single string
	inline void join(
			const std::vector<std::string>& strings,
			char delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	) {
		// save old size of the string
		const unsigned long oldSize = appendTo.size();

		// calculate and reserve needed memory
		unsigned long size = oldSize;

		for(auto i = strings.begin(); i != strings.end(); ++i)
			if(!ignoreEmpty || !(i->empty()))
				size += i->size() + 1;

		appendTo.reserve(size);

		// append string
		for(auto i = strings.begin(); i != strings.end(); ++i)
			if(!ignoreEmpty || !(i->empty()))
				appendTo += *i + delimiter;

		if(appendTo.size() > oldSize)
			appendTo.pop_back();
	}

	// concatenate all elements of a vector and append them to a single string
	inline void join(
			const std::vector<std::string>& strings,
			const std::string& delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	) {
		// save old size of the string
		const unsigned long oldSize = appendTo.size();

		// calculate and reserve needed memory
		unsigned long size = oldSize;

		for(auto i = strings.begin(); i != strings.end(); ++i)
			if(!ignoreEmpty || !(i->empty()))
				size += i->size() + delimiter.size();

		appendTo.reserve(size);

		// append string
		for(auto i = strings.begin(); i != strings.end(); ++i)
			if(!ignoreEmpty || !(i->empty()))
				appendTo += *i + delimiter;

		if(appendTo.size() > oldSize)
			appendTo.pop_back();
	}

	// concatenate all elements of a queue and append them to a single string, emptying the queue in the process
	inline void join(
			std::queue<std::string>& strings,
			char delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	) {
		// save old size of the string
		const unsigned long oldSize = appendTo.size();

		// append string
		while(!strings.empty()) {
			if(!ignoreEmpty || !(strings.front().empty()))
				appendTo += strings.front() + delimiter;

			strings.pop();
		}

		if(appendTo.size() > oldSize)
			appendTo.pop_back();
	}

	// concatenate all elements of a queue and append them to a single string, emptying the queue in the process
	inline void join(
			std::queue<std::string>& strings,
			const std::string& delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	) {
		// save old size of the string
		const unsigned long oldSize = appendTo.size();

		// append string
		while(!strings.empty()) {
			if(!ignoreEmpty || !(strings.front().empty()))
				appendTo += strings.front() + delimiter;

			strings.pop();
		}

		if(appendTo.size() > oldSize)
			appendTo.pop_back();
	}

	// split string into vector of strings
	inline std::vector<std::string> split(const std::string& str, char delimiter) {
		std::string tmp(str);
		std::vector<std::string> result;

		while(!tmp.empty()) {
			auto index = tmp.find(delimiter);

			if(index != std::string::npos) {
				result.emplace_back(tmp, 0, index);

				tmp = tmp.substr(index + 1);
			}
			else if(!tmp.empty()) {
				result.emplace_back(tmp);

				tmp.clear();
			}
		}

		return result;
	}

	// split string into vector of strings
	inline std::vector<std::string> split(const std::string& str, const std::string& delimiter) {
		std::string tmp(str);
		std::vector<std::string> result;

		while(!tmp.empty()) {
			auto index = tmp.find(delimiter);

			if(index != std::string::npos) {
				result.emplace_back(tmp, 0, index);

				tmp = tmp.substr(index + delimiter.size());
			}
			else if(!tmp.empty()) {
				result.emplace_back(tmp);

				tmp.clear();
			}
		}

		return result;
	}

	// split string into queue of strings
	inline std::queue<std::string> splitToQueue(const std::string& str, char delimiter) {
		std::string tmp(str);
		std::queue<std::string> result;

		while(!tmp.empty()) {
			auto index = tmp.find(delimiter);

			if(index != std::string::npos) {
				result.emplace(tmp, 0, index);

				tmp = tmp.substr(index + 1);
			}
			else if(!tmp.empty()) {
				result.emplace(tmp);

				tmp.clear();
			}
		}

		return result;
	}

	// split string into queue of strings
	inline std::queue<std::string> splitToQueue(const std::string& str, const std::string& delimiter) {
		std::string tmp(str);
		std::queue<std::string> result;

		while(!tmp.empty()) {
			auto index = tmp.find(delimiter);

			if(index != std::string::npos) {
				result.emplace(tmp, 0, index);

				tmp = tmp.substr(index + delimiter.size());
			}
			else if(!tmp.empty()) {
				result.emplace(tmp);

				tmp.clear();
			}
		}

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

	// generate a random alphanumerical string of a specific length
	inline std::string generateRandom(unsigned long length) {
		static const std::string charSet(
				"01234567890"
				"abcdefghijklmnopqrstuvwxyz"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		);

		thread_local static std::default_random_engine rengine(std::random_device{}());
		thread_local static std::uniform_int_distribution<std::string::size_type> distribution(0, charSet.length() - 1);

		std::string result;

		result.reserve(length);

		while(length--)
			result += charSet[distribution(rengine)];

		return result;
	}

} /* crawlservpp::Helper::Strings */


#endif /* HELPER_STRINGS_HPP_ */
