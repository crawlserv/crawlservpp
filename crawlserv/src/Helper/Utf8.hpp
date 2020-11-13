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
 * Utf8.hpp
 *
 * Namespace for global UTF-8 helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_UTF8_HPP_
#define HELPER_UTF8_HPP_

#include "../Main/Exception.hpp"

#include "../_extern/utf8/source/utf8.h"

#include <cstddef>		// std::size_t
#include <string>		// std::string
#include <string_view>	// std::string_view

//! Namespace for global UTF-8 encoding functions.
namespace crawlservpp::Helper::Utf8 {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Factor for guessing the maximum amount of memory used for UTF-8 compared to ISO-8859-1.
	inline constexpr auto utf8MemoryFactor{2};

	//! Bit mask to extract the first bit of a multibyte character.
	inline constexpr auto bitmaskTopBit{0x80};

	//! Bit mask to extract the top two bits of a multibyte character.
	inline constexpr auto bitmaskTopTwoBits{0xc0};

	//! Shift six bits.
	inline constexpr auto shiftSixBits{6};

	//! Bit mask to check the last six bits for 0b000001.
	inline constexpr auto bitmaskLastSixBits0b000001{0x3F};

	//! One byte.
	inline constexpr auto oneByte{1};

	//! Two bytes.
	inline constexpr auto twoBytes{2};

	//! Three bytes.
	inline constexpr auto threeBytes{3};

	//! Four bytes.
	inline constexpr auto fourBytes{4};

	///@}

	/*
	 * DECLARATION
	 */

	///@name Conversion
	///@{

	std::string iso88591ToUtf8(std::string_view strIn);

	///@}
	///@name Validation
	///@{

	bool isValidUtf8(std::string_view stringToCheck, std::string& errTo);
	bool isLastCharValidUtf8(std::string_view stringToCheck);
	bool isSingleUtf8Char(std::string_view stringToCheck);

	///@}
	///@name Repair
	///@{

	bool repairUtf8(std::string_view strIn, std::string& strOut);

	///@}
	///@name Length
	///@{

	std::size_t length(std::string_view str);

	///@}

	/*
	 * EXCEPTION CLASS
	 */

	//! Class for UTF-8 exceptions.
	/*!
	 * Will be thrown when
	 * - invalid UTF-8 characters could not be
	 *    replaced.
	 */
	MAIN_EXCEPTION_CLASS();

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONVERSION
	 */

	//! Converts a string from ISO-8859-1 to UTF-8.
	/*!
	 * \param strIn View of the string to be
	 *   converted.
	 *
	 * \returns A copy of the converted string.
	 */
	inline std::string iso88591ToUtf8(std::string_view strIn) {
		std::string strOut;

		// guess maximum memory requirement
		strOut.reserve(strIn.size() * utf8MemoryFactor);

		for(const uint8_t c : strIn) {
			if(c < bitmaskTopBit) {
				strOut.push_back(c);
			}
			else {
				strOut.push_back(bitmaskTopTwoBits | c >> shiftSixBits);
				strOut.push_back(bitmaskTopBit | (c & bitmaskLastSixBits0b000001));
			}
		}

		return strOut;
	}

	/*
	 * VALIDATION
	 */

	//! Checks whether a string contains valid UTF-8.
	/*!
	 * Uses the @c UTF8-CPP library for UTF-8
	 *  validation. See its
	 *  <a href="https://github.com/nemtrif/utfcpp">
	 *  GitHub repository</a> for more information.
	 *
	 * \param stringToCheck View of the string
	 *   to check for valid UTF-8.
	 * \param errTo Reference to a string to
	 *   which a UTF-8 error will be written.
	 *
	 * \returns True if the given string
	 *   contains valid UTF-8. False otherwise.
	 */
	inline bool isValidUtf8(std::string_view stringToCheck, std::string& errTo) {
		try {
			return utf8::is_valid(stringToCheck.cbegin(), stringToCheck.cend());
		}
		catch(const utf8::exception& e) {
			errTo = e.what();

			return false;
		}
	}

	//! Checks the last character (i.e. up to four bytes at the end) of the given string for valid UTF-8.
	/*!
	 * Uses the @c UTF8-CPP library for UTF-8
	 *  validation. See its
	 *  <a href="https://github.com/nemtrif/utfcpp">
	 *  GitHub repository</a> for more information.
	 *
	 * \param stringToCheck Constant reference
	 *   to the string whose last character
	 *   will be checked for valid UTF-8.
	 *
	 * \returns True if the last character of
	 *   the given string is valid UTF-8 or
	 *   the given string is empty. False
	 *   otherwise.
	 */
	inline bool isLastCharValidUtf8(const std::string& stringToCheck) {
		if(stringToCheck.empty()) {
			return true;
		}

		// check for valid one-byte character
		auto pos{stringToCheck.size() - 1};

		if(utf8::is_valid(stringToCheck.substr(pos, oneByte))) {
			return true;
		}

		if(stringToCheck.size() < twoBytes) {
			return false;
		}

		--pos;

		// check for valid two-byte character
		if(utf8::is_valid(stringToCheck.substr(pos, twoBytes))) {
			return true;
		}

		if(stringToCheck.size() < threeBytes) {
			return false;
		}

		--pos;

		// check for valid three-byte character
		if(utf8::is_valid(stringToCheck.substr(pos, threeBytes))) {
			return true;
		}

		if(stringToCheck.size() < fourBytes) {
			return false;
		}

		--pos;

		// check for valid four-byte character
		if(utf8::is_valid(stringToCheck.substr(pos, fourBytes))) {
			return true;
		}

		return false;
	}

	//! Returns whether the given string contains exactly one UTF-8 code point.
	/*!
	 *  \param stringToCheck String view to a
	 *    string that will be checked for
	 *    containing exactly one UTF-8 code
	 *    point.
	 *
	 *  \returns True, if the given string
	 *    contains exactly one UTF-8 code
	 *    point. False otherwise.
	 */
	inline bool isSingleUtf8Char(std::string_view stringToCheck) {
		return utf8::distance(stringToCheck.begin(), stringToCheck.end()) == 1;
	}

	/*
	 * REPAIR
	 */

	//! Replaces invalid UTF-8 characters in the given string and returns whether invalid characters occured.
	/*!
	 * Uses the @c UTF8-CPP library for UTF-8
	 *  validation and replacement. See its
	 *  <a href="https://github.com/nemtrif/utfcpp">
	 *  GitHub repository</a> for more information.
	 *
	 * \param strIn View of the string in
	 *   which invalid UTF-8 characters
	 *   will be replaced.
	 * \param strOut Reference to a string
	 *   that will be replaced with the
	 *   resulting string, if invalid UTF-8
	 *   characters have been encountered.
	 *
	 * \returns True, if the given string
	 *   contains invalid UTF-8 characters
	 *   that have been replaced in the
	 *   resulting string.
	 *
	 * \throws Utf8::Exception if invalid
	 *   characters could not be replaced.
	 */
	inline bool repairUtf8(std::string_view strIn, std::string& strOut) {
		try {
			if(utf8::is_valid(strIn.cbegin(), strIn.cend())) {
				return false;
			}

			utf8::replace_invalid(strIn.begin(), strIn.end(), back_inserter(strOut));

			return true;
		}
		catch(const utf8::exception& e) {
			throw Exception("UTF-8 error: " + std::string(e.what()));
		}
	}

	/*
	 * LENGTH
	 */

	/*!
	 * Returns the number of UTF-8 codepoints in the given string.
	 *
	 * \param str The string to be checked
	 *   for UTF-8 codepoints.
	 *
	 * \returns The number of UTF-8
	 *   codepoints found in the
	 *   string.
	 *
	 * \throws Utf8::Exception if the
	 *   string contains invalid
	 *   UTF-8 codepoints.
	 */
	inline std::size_t length(std::string_view str) {
		constexpr unsigned char maxOneByte{127};
		constexpr unsigned char checkTwoBytes{0xE0};
		constexpr unsigned char isTwoBytes{0xC0};
		constexpr unsigned char checkThreeBytes{0xF0};
		constexpr unsigned char isThreeBytes{0xE0};
		constexpr unsigned char checkFourBytes{0xF8};
		constexpr unsigned char isFourBytes{0xF0};

		constexpr std::size_t skipTwoBytes{2};
		constexpr std::size_t skipThreeBytes{3};

		std::size_t result{0};

		const auto bytes{
			str.length()
		};

		for(std::size_t pos{0}; pos < bytes; ++pos) {
			++result;

			const unsigned char byte{
				static_cast<unsigned char>(str[pos])
			};

			if(byte <= maxOneByte) {
				// one byte
				continue;
			}

			if((byte & checkTwoBytes) == isTwoBytes) {
				// two bytes
				++pos;
			}
			else if((byte & checkThreeBytes) == isThreeBytes) {
				// three bytes
				pos += skipTwoBytes;
			}
			else if((byte & checkFourBytes) == isFourBytes) {
				// four bytes
				pos += skipThreeBytes;
			}
			else {
				std::string exceptionString{"Invalid UTF-8 in '"};

				exceptionString += str;
				exceptionString += "'";

				throw Exception(exceptionString);
			}
		}

		return result;
	}

} /* namespace crawlservpp::Helper::Utf8 */

#endif /* HELPER_UTF8_HPP_ */
