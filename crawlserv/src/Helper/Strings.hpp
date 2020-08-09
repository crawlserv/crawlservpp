/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * Strings.hpp
 *
 * Namespace for global string helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_STRINGS_HPP_
#define HELPER_STRINGS_HPP_

#include <boost/algorithm/string.hpp>

#include <algorithm>	// std::equal, std::find_if, std::mismatch, std::remove_if,
						// std::replace, std::sort, std::transform, std::unique
#include <array>		// std::array
#include <cctype>		// std::isspace, std::tolower
#include <ios>			// std::boolalpha
#include <queue>		// std::queue
#include <random>		// std::default_random_engine, std::random_device, std::uniform_int_distribution
#include <sstream>		// std::istringstream
#include <string>		// std::string
#include <string_view>	// std::string_view, std::string_view_literals
#include <utility>		// std::pair
#include <vector>		// std::vector

//! Namespace for global string helper functions.
namespace crawlservpp::Helper::Strings {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! UTF-8 whitespaces used by utfTidy().
	inline constexpr std::array utfWhitespaces {
		"\u0085"sv, // next line (NEL)
		"\u00a0"sv, // no-break space
		"\u1680"sv, // Ogham space mark
		"\u2000"sv, // en quad
		"\u2001"sv, // em quad
		"\u2002"sv, // en space
		"\u2003"sv, // em space
		"\u2004"sv, // three-per-em space
		"\u2005"sv, // four-per-em space
		"\u2006"sv, // six-per-em space
		"\u2007"sv, // figure space
		"\u2008"sv, // punctuation space
		"\u2009"sv, // thin space
		"\u200a"sv, // hair space
		"\u2028"sv, // line separator
		"\u2029"sv, // paragraph separator
		"\u202f"sv, // narrow no-break space
		"\u205f"sv, // medium mathematical space
		"\u3000"sv, // ideographic space
	};

	//! Length of a two-digit hexademical number including the preceding percentage sign.
	inline constexpr auto checkHexLength{3};

	//! Characters to be chosen from for random string generation performed by generateRandom().
	inline constexpr auto randCharSet{
		"01234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"sv
	};

	///@}

	/*
	 * DECLARATION
	 */

	///@name Replacing
	///@{

	void replaceAll(
			std::string& strInOut,
			std::string_view needle,
			std::string_view replacement
	);

	///@}
	///@name Conversion
	///@{

	bool stringToBool(std::string inputString);

	///@}
	///@name Number Format Checking
	///@{

	bool isHex(std::string_view inputString);

	///@}
	///@name Trimming
	///@{

	void trim(std::string& stringToTrim);

	///@}
	///@name Joining
	///@{

	std::string join(
			const std::vector<std::string>& strings,
			char delimiter,
			bool ignoreEmpty
	);
	std::string join(
			const std::vector<std::string>& strings,
			std::string_view delimiter,
			bool ignoreEmpty
	);
	std::string join(
			std::queue<std::string>& strings,
			char delimiter,
			bool ignoreEmpty
	);
	std::string join(
			std::queue<std::string>& strings,
			std::string_view delimiter,
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
			std::string_view delimiter,
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
			std::string_view delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	);

	///@}
	///@name Splitting
	///@{

	std::vector<std::string> split(const std::string& str, char delimiter);
	std::vector<std::string> split(std::string_view str, std::string_view delimiter);

	std::queue<std::string> splitToQueue(
			std::string_view str,
			char delimiter,
			bool removeEmpty
	);
	std::queue<std::string> splitToQueue(
			std::string_view str,
			std::string_view delimiter,
			bool removeEmpty
	);

	///@}
	///@name Sorting
	///@{

	void sortAndRemoveDuplicates(std::vector<std::string>& vectorOfStrings, bool caseSensitive);

	///@}
	///@name Escape Characters
	///@{

	char getFirstOrEscapeChar(std::string_view from);

	///@}
	///@name Encoding
	///@{

	void encodePercentage(std::string& stringToEncode);

	///@}
	///@name Tidying
	///@{

	void utfTidy(std::string& stringToTidy);

	///@}
	///@name Name Checking
	///@{

	bool checkDomainName(std::string_view name);
	bool checkSQLName(std::string_view name);

	///@}
	///@name Random String Generation
	///@{

	std::string generateRandom(std::size_t length);

	///@}

	/*
	 * IMPLEMENTATION
	 */

	//! Replaces all occurences within a string with another string.
	/*!
	 * \note No recursive replacement will be performed. Use a @c while loop for that.
	 *
	 * \param strInOut A reference to the string in which the occurences will be replaced.
	 * \param needle A string view defining the occurence to be replaced.
	 * \param replacement A string view defining the replacement.
	 */
	inline void replaceAll(
			std::string& strInOut,
			std::string_view needle,
			std::string_view replacement
	) {
		std::size_t startPos{0};

		if(needle.empty()) {
			return;
		}

		while((startPos = strInOut.find(needle, startPos)) != std::string::npos) {
			strInOut.replace(startPos, needle.length(), replacement);

			startPos += replacement.length();
		}
	}

	//! Converts a string into a boolean value.
	/*!
	 * Only case-insensitive variations of @c "true"
	 *   will be converted into @c true.
	 *
	 * \note In order for the conversion to be case-insensitive,
	 *  a copy of the given string will be made.
	 *
	 * \param inputString The string to be converted into a boolean value.
	 *
	 * \returns True, if the given string represents @c true.
	 *   False otherwise.
	 */
	inline bool stringToBool(std::string inputString) {
		std::transform(
				inputString.begin(),
				inputString.end(),
				inputString.begin(),
				[](const auto c) {
					return std::tolower(c);
				}
		);

		std::istringstream strStr(inputString);

		bool result{false};

		strStr >> std::boolalpha >> result;

		return result;
	}

	//! Checks whether a string contains only hexadecimal digits.
	/*!
	 * Case-insensitive.
	 *
	 * \param inputString A view into the string to check for hexadecimal digits.
	 *
	 * \returns True, if the string only contains hexadecimal digits (@c 0-@c F)
	 *   and no whitespaces. False otherwise.
	 */
	inline bool isHex(std::string_view inputString) {
		return inputString.find_first_not_of(
				"0123456789AaBbCcDdEeFf"
		) == std::string_view::npos;
	}

	//! Removes whitespaces around a string.
	/*!
	 * \warning Only ASCII whitespaces will be processed.
	 *
	 * \param stringToTrim Reference to the string to be trimmed in-situ.
	 *
	 * \sa utfTidy
	 */
	inline void trim(std::string& stringToTrim) {
		stringToTrim.erase(
				stringToTrim.begin(),
				std::find_if(
						stringToTrim.begin(),
						stringToTrim.end(),
						[](int ch) {
							return std::isspace(ch) == 0;
						}
				)
		);

		stringToTrim.erase(
				std::find_if(
						stringToTrim.rbegin(),
						stringToTrim.rend(),
						[](int ch) {
							return std::isspace(ch) == 0;
						}
		).base(), stringToTrim.end());
	}

	//! Concatenates all elements of a vector into a single string.
	/*!
	 * \param strings Constant reference to a vector
	 *   containing the strings to be concatenated.
	 * \param delimiter A character to be inserted
	 *   inbetween the concatenated strings.
	 * \param ignoreEmpty Ignore empty strings when
	 *   concatenating the given elements.
	 *
	 * \returns The string containing the concatenated
	 *   elements, separated by the given delimiter,
	 *   or an empty string if no elements have been
	 *   concatenated.
	 */
	inline std::string join(
			const std::vector<std::string>& strings,
			char delimiter,
			bool ignoreEmpty
	) {
		std::string result;
		std::size_t size{0};

		// calculate and reserve needed memory
		for(const auto& string : strings) {
			if(!ignoreEmpty || !string.empty()) {
				size += string.size() + 1;
			}
		}

		result.reserve(size);

		// create string
		for(const auto& string : strings) {
			if(!ignoreEmpty || !string.empty()) {
				result += string;
				result += delimiter;
			}
		}

		if(!result.empty()) {
			result.pop_back();
		}

		// return string
		return result;
	}

	//! Concatenates all elements of a vector into a single string.
	/*!
	 * \param strings Constant reference to a vector
	 *   containing the strings to be concatenated.
	 * \param delimiter View of a string to be inserted
	 *   inbetween the concatenated strings.
	 * \param ignoreEmpty Ignore empty strings when
	 *   concatenating the given elements.
	 *
	 * \returns The string containing the concatenated
	 *   elements, separated by the given delimiter,
	 *   or an empty string if no elements have been
	 *   concatenated.
	 */
	inline std::string join(
			const std::vector<std::string>& strings,
			std::string_view delimiter,
			bool ignoreEmpty
	) {
		// calculate and reserve needed memory
		std::string result;
		std::size_t size{0};

		for(const auto& string : strings) {
			if(!ignoreEmpty || !string.empty()) {
				size += string.size() + delimiter.size();
			}
		}

		result.reserve(size);

		// create string
		for(const auto& string : strings) {
			if(!ignoreEmpty || !string.empty()) {
				result += string;
				result += delimiter;
			}
		}

		if(!result.empty()) {
			result.pop_back();
		}

		// return string
		return result;
	}

	//! Concatenates all elements of a queue into a single string.
	/*!
	 * \note The queue will be completely emptied in
	 *   the process, even if elements will be ignored.
	 *
	 * \param strings Constant reference to a vector
	 *   containing the strings to be concatenated.
	 * \param delimiter A character to be inserted
	 *   inbetween the concatenated strings.
	 * \param ignoreEmpty Ignore empty strings when
	 *   concatenating the given elements.
	 *
	 * \returns The string containing the concatenated
	 *   elements, separated by the given delimiter,
	 *   or an empty string if no elements have been
	 *   concatenated.
	 */
	inline std::string join(
			std::queue<std::string>& strings,
			char delimiter,
			bool ignoreEmpty
	) {
		// create string
		std::string result;

		while(!strings.empty()) {
			if(!ignoreEmpty || !(strings.front().empty())) {
				result += strings.front() + delimiter;
			}

			strings.pop();
		}

		if(!result.empty()) {
			result.pop_back();
		}

		// return string
		return result;
	}

	//! Concatenates all elements of a queue into a single string.
	/*!
	 * \note The queue will be completely emptied in
	 *   the process, even if elements will be ignored.
	 *
	 * \param strings Constant reference to a vector
	 *   containing the strings to be concatenated.
	 * \param delimiter View of a string to be inserted
	 *   inbetween the concatenated strings.
	 * \param ignoreEmpty Ignore empty strings when
	 *   concatenating the given elements.
	 *
	 * \returns The string containing the concatenated
	 *   elements, separated by the given delimiter,
	 *   or an empty string if no elements have been
	 *   concatenated.
	 */
	inline std::string join(
			std::queue<std::string>& strings,
			std::string_view delimiter,
			bool ignoreEmpty
	) {
		// create string
		std::string result;

		while(!strings.empty()) {
			if(!ignoreEmpty || !(strings.front().empty())) {
				result += strings.front();
				result += delimiter;
			}

			strings.pop();
		}

		if(!result.empty()) {
			result.pop_back();
		}

		// return string
		return result;
	}

	//! Concatenates all elements of a vector and appends them to a string.
	/*!
	 * \param strings Constant reference to a vector
	 *   containing the strings to be concatenated.
	 * \param delimiter A character to be inserted
	 *   inbetween the concatenated strings.
	 * \param ignoreEmpty Ignore empty strings when
	 *   concatenating the given elements.
	 * \param appendTo The string that will be appended
         with the concatenated elements, separated by
         the given delimiter. It will remain unchanged,
         if no elements have been concatenated.
	 */
	inline void join(
			const std::vector<std::string>& strings,
			char delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	) {
		// save old size of the string
		const auto oldSize{appendTo.size()};

		// calculate and reserve needed memory
		auto size{oldSize};

		for(const auto& string : strings) {
			if(!ignoreEmpty || !string.empty()) {
				size += string.size() + 1;
			}
		}

		appendTo.reserve(size);

		// append string
		for(const auto& string : strings) {
			if(!ignoreEmpty || !string.empty()) {
				appendTo += string + delimiter;
			}
		}

		if(appendTo.size() > oldSize) {
			appendTo.pop_back();
		}
	}

	//! Concatenates all elements of a vector and appends them to a string.
	/*!
	 * \param strings Constant reference to a vector
	 *   containing the strings to be concatenated.
	 * \param delimiter A view of the string to be inserted
	 *   inbetween the concatenated strings.
	 * \param ignoreEmpty Ignore empty strings when
	 *   concatenating the given elements.
	 * \param appendTo The string that will be appended
		 with the concatenated elements, separated by
		 the given delimiter. It will remain unchanged,
		 if no elements have been concatenated.
	 */
	inline void join(
			const std::vector<std::string>& strings,
			std::string_view delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	) {
		// save old size of the string
		const auto oldSize{appendTo.size()};

		// calculate and reserve needed memory
		auto size{oldSize};

		for(const auto& string : strings) {
			if(!ignoreEmpty || !string.empty()) {
				size += string.size() + delimiter.size();
			}
		}

		appendTo.reserve(size);

		// append string
		for(const auto& string : strings) {
			if(!ignoreEmpty || !string.empty()) {
				appendTo += string;
				appendTo += delimiter;
			}
		}

		if(appendTo.size() > oldSize) {
			appendTo.pop_back();
		}
	}

	//! Concatenates all elements of a queue into a single string.
	/*!
	 * \note The queue will be completely emptied in
	 *   the process, even if elements will be ignored.
	 *
	 * \param strings Constant reference to a vector
	 *   containing the strings to be concatenated.
	 * \param delimiter A character to be inserted
	 *   inbetween the concatenated strings.
	 * \param ignoreEmpty Ignore empty strings when
	 *   concatenating the given elements.
	 * \param appendTo The string that will be appended
		 with the concatenated elements, separated by
		 the given delimiter. It will remain unchanged,
		 if no elements have been concatenated.
	 */
	inline void join(
			std::queue<std::string>& strings,
			char delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	) {
		// save old size of the string
		const auto oldSize{appendTo.size()};

		// append string
		while(!strings.empty()) {
			if(!ignoreEmpty || !(strings.front().empty())) {
				appendTo += strings.front() + delimiter;
			}

			strings.pop();
		}

		if(appendTo.size() > oldSize) {
			appendTo.pop_back();
		}
	}

	//! Concatenates all elements of a queue into a single string.
	/*!
	 * \note The queue will be completely emptied in
	 *   the process, even if elements will be ignored.
	 *
	 * \param strings Constant reference to a vector
	 *   containing the strings to be concatenated.
	 * \param delimiter A view of the string to be inserted
	 *   inbetween the concatenated strings.
	 * \param ignoreEmpty Ignore empty strings when
	 *   concatenating the given elements.
	 * \param appendTo The string that will be appended
		 with the concatenated elements, separated by
		 the given delimiter. It will remain unchanged,
		 if no elements have been concatenated.
	 */
	inline void join(
			std::queue<std::string>& strings,
			std::string_view delimiter,
			bool ignoreEmpty,
			std::string& appendTo
	) {
		// save old size of the string
		const auto oldSize{appendTo.size()};

		// append string
		while(!strings.empty()) {
			if(!ignoreEmpty || !(strings.front().empty())) {
				appendTo += strings.front();
				appendTo += delimiter;
			}

			strings.pop();
		}

		if(appendTo.size() > oldSize) {
			appendTo.pop_back();
		}
	}

	//! Splits a string into a vector of strings using the given delimiter.
	/*!
	 * \param str A const reference to the string
	 *   to be split up.
	 * \param delimiter The character around which
	 *  the resulting elements will be splitted.
	 *
	 * \returns A new vector containing the splitted
	 *   elements.
	 */
	inline std::vector<std::string> split(const std::string& str, char delimiter) {
		std::vector<std::string> result;

		boost::split(result, str, [&delimiter](char c) {
			return c == delimiter;
		});

		return result;
	}

	//! Splits a string into a vector of strings using the given delimiter.
	/*!
	 * \param str A const reference to the string
	 *   to be split up.
	 * \param delimiter A view of the string around
	 *  which the resulting elements will be splitted.
	 *
	 * \returns A new vector containing the splitted
	 *   elements.
	 */
	inline std::vector<std::string> split(std::string_view str, std::string_view delimiter) {
		std::string tmp(str);
		std::vector<std::string> result;

		while(!tmp.empty()) {
			auto index{tmp.find(delimiter)};

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

	//! Splits a string into a queue of strings using the given delimiter.
	/*!
	 * \param str A const reference to the string
	 *   to be split up.
	 * \param delimiter The character around which
	 *   the resulting elements will be splitted.
	 * \param removeEmpty Set whether to ignore
	 *   empty strings and not add them to the
	 *   resulting queue.
	 *
	 * \returns A new queue containing the splitted
	 *   elements.
	 */
	inline std::queue<std::string> splitToQueue(
			std::string_view str,
			char delimiter,
			bool removeEmpty
	) {
		std::queue<std::string>::container_type result;

		boost::split(result, str, [&delimiter](char c) {
			return c == delimiter;
		});

		if(removeEmpty) {
			result.erase(
					std::remove_if(
							result.begin(),
							result.end(),
							[](const auto& str) {
								return str.empty();
							}
					),
					result.end()
			);
		}

		return std::queue<std::string>(result);
	}

	//! Splits a string into a queue of strings using the given delimiter.
	/*!
	 * \param str A const reference to the string
	 *   to be split up.
	 * \param delimiter A view of the string around
	 *   which the resulting elements will be splitted.
	 * \param removeEmpty Set whether to ignore
	 *   empty strings and not add them to the
	 *   resulting queue.
	 *
	 * \returns A new queue containing the splitted
	 *   elements.
	 */
	inline std::queue<std::string> splitToQueue(
			std::string_view str,
			std::string_view delimiter,
			bool removeEmpty
	) {
		std::string tmp(str);
		std::queue<std::string> result;

		while(!tmp.empty()) {
			auto index{tmp.find(delimiter)};

			if(index != std::string::npos) {
				if(!removeEmpty || index > 0) {
					result.emplace(tmp, 0, index);
				}

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
	//! Sorts the given vector of strings and removes duplicates.
	/*!
	 * \note Only ASCII characters are supported when sorting the string.
	 *   Non-ASCII characters may result in a wrong sorting order.
	 *
	 * \param vectorOfStrings Reference to the vector of strings,
	 *   which will be sorted and from which duplicates will be
	 *   removed in-situ.
	 * \param caseSensitive True, if the removal should be performed
	 *   case-sensitive. False otherwise.
	 */
	inline void sortAndRemoveDuplicates(
			std::vector<std::string>& vectorOfStrings,
			bool caseSensitive
	) {
		if(caseSensitive) {
			// case-sensitive sort
			std::sort(vectorOfStrings.begin(), vectorOfStrings.end());

			// case-sensitive removal of co-occuring duplicates
			vectorOfStrings.erase(
					std::unique(
							vectorOfStrings.begin(),
							vectorOfStrings.end()
					),
					vectorOfStrings.end()
			);
		}
		else {
			// case-insensitive sort
			std::sort(
					vectorOfStrings.begin(),
					vectorOfStrings.end(),
					[](const auto& s1, const auto& s2) {
						const auto result{
							std::mismatch(
									s1.cbegin(),
									s1.cend(),
									s2.cbegin(),
									s2.cend(),
									[](const auto& s1, const auto& s2) {
										return (s1 == s2) || std::tolower(s1) == std::tolower(s2);
									}
							)
						};

						return result.second != s2.cend()
								&& (
										result.first == s1.cend()
										|| std::tolower(*result.first) < std::tolower(*result.second)
								);
				}
			);

			// case-insensitive removal of co-occuring duplicates
			vectorOfStrings.erase(std::unique(vectorOfStrings.begin(), vectorOfStrings.end(),
					[](const auto& s1, const auto& s2) {
						return (s1.size() == s2.size()) && std::equal(s1.cbegin(), s1.cend(), s2.cbegin(),
								[](const auto& c1, const auto& c2) {
									return (c1 == c2) || std::tolower(c1) == std::tolower(c2);
								}
						);
					}
			), vectorOfStrings.end());
		}
	}

	//! Gets the first character or an escaped character from the beginning of the given string.
	/*!
	 * Escaped characters that are supported:
	 *  @c \\n, @c \\t, and @c \\\\
	 *
	 * \note Only ASCII characters are supported.
	 *   The returning character will be invalid,
	 *   if the given string starts with a UTF-8
	 *   character.
	 *
	 * \param from A view of the string to extract
	 *   the character from.
	 *
	 * \returns the escaped character if the first
	 *   two characters of the given string equal
	 *   one of the supported escape sequences.
	 *   The first character of the given string
	 *   otherwise.
	 */
	inline char getFirstOrEscapeChar(std::string_view from) {
		if(!from.empty()) {
			if(from.at(0) == '\\' && from.length() > 1) {
				switch(from.at(1)) {
				case 'n':
					return '\n';

				case 't':
					return '\t';

				case '\\':
				default:
					// ignore invalid escape sequence
					return '\\';
				}
			}
			else {
				return from.at(0);
			}
		}

		return 0;
	}

	//! Encodes percentage signs that are not followed by a two-digit hexadecimal number with @c %25.
	/*!
	 * \param stringToEncode Reference to the string in which
	 *   the percentage signs will be encoded in-situ.
	 */
	inline void encodePercentage(std::string& stringToEncode) {
		std::size_t pos{0};

		do {
			pos = stringToEncode.find('%', pos);

			if(pos == std::string::npos) {
				break;
			}

			if(
					pos + checkHexLength > stringToEncode.length()
					|| !isHex(stringToEncode.substr(pos + 1, 2))
			) {
				stringToEncode.insert(pos + 1, "25");

				pos += checkHexLength;
			}
			else {
				++pos;
			}
		} while(pos < stringToEncode.length());
	}

	//! Removes new lines and unnecessary spaces, including UTF-8 whitespaces.
	/*!
	 * \param stringToTidy Reference to the string from which
	 *   new lines and unnecessary spaces will be removed
	 *   in-situ.
	 */
	inline void utfTidy(std::string& stringToTidy) {
		// replace Unicode white spaces with spaces
		for(const auto whitespace : utfWhitespaces) {
			replaceAll(stringToTidy, whitespace, " ");
		}

		// replace special ASCII characters with spaces
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\t', ' '); // horizontal tab
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\n', ' '); // line feed
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\v', ' '); // vertical tab
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\f', ' '); // form feed
		std::replace(stringToTidy.begin(), stringToTidy.end(), '\r', ' '); // carriage return

		// replace double spaces
		while(stringToTidy.find("  ") != std::string::npos) {
			replaceAll(stringToTidy, "  ", " ");
		}

		// replace unnecessary spaces around punctuation
		replaceAll(stringToTidy, " .", ".");
		replaceAll(stringToTidy, " ,", ",");
		replaceAll(stringToTidy, " :", ":");
		replaceAll(stringToTidy, " ;", ";");
		replaceAll(stringToTidy, "( ", "(");
		replaceAll(stringToTidy, " )", ")");

		// trim result
		trim(stringToTidy);
	}

	// check whether the name is valid as domain (checking only for characters that interfere with internal SQL statements)
	//! Checks whether the given string is a a valid domain name.
	/*!
	 * \note Checks only for characters that would interfer with
	 *   internal MySQL statements: @c /, @c \, and @c '.
	 *
	 * \param name View of the string to be checked for
	 *   a valid domain name.
	 *
	 * \returns True, if the string might be used as a domain name.
	 *   False otherwise.
	 */
	inline bool checkDomainName(std::string_view name) {
		return name.find_first_of("/\'") == std::string_view::npos;
	}

	//! Checks whether the given string is a valid name for MySQL tables and fields.
	/*!
	 * \param name View of the string to be checked for
	 *  a valid MySQL table or field name.
	 *
	 * \returns True if the string might be used as a MySQL table
	 *   or field name. False otherwise.
	 */
	inline bool checkSQLName(std::string_view name) {
		return name.find_first_not_of(
				"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$_"
		) == std::string_view::npos;
	}

	//! Generates a random alpha-numerical string of the given length.
	/*!
	 * \param length Length of the string to be generated.
	 *
	 * \returns The generated alpha-numerical string
	 *   of the given length.
	 */
	inline std::string generateRandom(std::size_t length) {
		thread_local static std::default_random_engine rengine(
				std::random_device{}()
		);

		thread_local static std::uniform_int_distribution<std::size_t> distribution(
				0,
				randCharSet.length() - 1
		);

		std::string result;

		result.reserve(length);

		while(length-- > 0) {
			result += randCharSet[distribution(rengine)];
		}

		return result;
	}

} /* namespace crawlservpp::Helper::Strings */

#endif /* HELPER_STRINGS_HPP_ */
