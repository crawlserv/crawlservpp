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
 * RegEx.hpp
 *
 * Using the PCRE2 library to implement a Perl-Compatible Regular Expressions
 *  query with boolean, single and/or multiple results. An expression is only
 *  created when needed.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef QUERY_REGEX_HPP_
#define QUERY_REGEX_HPP_

#include "../Main/Exception.hpp"
#include "../Wrapper/PCRE.hpp"
#include "../Wrapper/PCREMatch.hpp"

#include <pcre2.h>

#include <array>		// std::array
#include <cstdint>		// std::int32_t, std::uint32_t
#include <string>		// std::string, std::to_string
#include <vector>		// std::vector

namespace crawlservpp::Query {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The length of the error buffer used by the PCRE2 library, in bytes.
	inline constexpr auto pcre2ErrorBufferLength{1024};

	//! Bit mask to extract the first bit of a multibyte character.
	inline constexpr auto bitmaskTopBit{0x80};

	//! Bit mask to extract the top two bits of a multibyte character.
	inline constexpr auto bitmaskTopTwoBits{0xc0};

	///@}

	/*
	 * DECLARATION
	 */

	//! Implements a %RegEx query using the PCRE2 library.
	/*!
	 * For more information about the PCRE2 library, see
	 *  its <a href="https://www.pcre.org/">website</a>.
	 */
	class RegEx {
	public:
		///@name Construction
		///@{

		RegEx(const std::string& expression, bool single, bool multi);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] bool getBool(const std::string& text) const;
		void getFirst(const std::string& text, std::string& resultTo) const;
		void getAll(const std::string& text, std::vector<std::string>& resultTo) const;
		[[nodiscard]] bool valid() const noexcept;

		///@}

		//! Class for JSONPath exceptions.
		/*!
		 * Throws an exception when
		 * - the given RegEx expression is empty
		 * - no result type has been specified
		 * - the compilation of the RegEx
		 *    expression failed
		 * - no RegEx expression has been defined
		 *    prior to performing it
		 * - an error occurs during execution
		 *    of the query
		 * - the output vector is unexpectedly
		 *    too small to contain the result
		 */
		MAIN_EXCEPTION_CLASS();

	private:
		Wrapper::PCRE expressionSingle;
		Wrapper::PCRE expressionMulti;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor setting a RegEx string and whether the query will return single and/or multiple results.
	/*!
	 * \note Cannot use string view, because
	 *   the underlying API requires a null-
	 *   terminated string.
	 *
	 * \param expression Const reference to
	 *   a string containing the RegEx query.
	 *   Newlines at the end of the expression
	 *   will be removed.
	 * \param single Set whether the query
	 *   can return single results.
	 * \param multi Set whether the query
	 *   can return multiple results.
	 *
	 * \throws RegEx::Exception if the given
	 *   expression is empty, no result
	 *   type has been specified, or the
	 *   compilation of the RegEx expression
	 *   failed.
	 */
	inline RegEx::RegEx(const std::string& expression, bool single, bool multi) {
		std::string queryString(expression);
		std::int32_t errorNumber{0};
		PCRE2_SIZE errorOffset{0};

		// remove newlines at the end of the expression
		while(queryString.back() == '\n') {
			queryString.pop_back();
		}

		// check arguments
		if(queryString.empty()) {
			throw RegEx::Exception("Expression is empty");
		}

		if(!single && !multi) {
			throw RegEx::Exception("No result type for expression specified");
		}

		// compile expression(s)
		if(single) {
			this->expressionSingle.set(
					pcre2_compile(
							static_cast<PCRE2_SPTR>(
									static_cast<const void *>(
											queryString.c_str()
									)
							),
							PCRE2_ZERO_TERMINATED,
							PCRE2_UTF | PCRE2_UCP,
							&errorNumber,
							&errorOffset,
							nullptr
					)
			);

			if(!(this->expressionSingle.valid())) {
				// RegEx error
				std::array<char, pcre2ErrorBufferLength> errorBuffer{};

				pcre2_get_error_message(
						errorNumber,
						static_cast<PCRE2_UCHAR8 *>(
								static_cast<void *>(
										errorBuffer.data()
								)
						),
						pcre2ErrorBufferLength
				);

				std::string exceptionString{"Compilation error at "};

				exceptionString += std::to_string(errorOffset);
				exceptionString += 	": ";
				exceptionString += errorBuffer.data();

				throw RegEx::Exception(exceptionString);
			}
		}

		if(multi) {
			this->expressionMulti.set(
					pcre2_compile(
							static_cast<PCRE2_SPTR>(
									static_cast<const void *>(
											expression.c_str()
									)
							),
							PCRE2_ZERO_TERMINATED,
							PCRE2_UTF | PCRE2_UCP | PCRE2_MULTILINE,
							&errorNumber,
							&errorOffset,
							nullptr
					)
			);

			if(!(this->expressionMulti.valid())) {
				// RegEx error
				std::array<char, pcre2ErrorBufferLength> errorBuffer{};

				pcre2_get_error_message(
						errorNumber,
						static_cast<PCRE2_UCHAR8 *>(
								static_cast<void *>(
										errorBuffer.data()
								)
						),
						pcre2ErrorBufferLength
				);

				std::string exceptionString{"Compilation error at "};

				exceptionString += std::to_string(errorOffset);
				exceptionString += ": ";
				exceptionString += errorBuffer.data();

				throw RegEx::Exception(exceptionString);
			}
		}
	}

	//! Gets a boolean result from performing the query on a parsed JSON document.
	/*!
	 * \note Cannot use string view, because
	 *   the underlying API requires a null-
	 *   terminated string.
	 *
	 * \param text Const reference to a string
	 *   containing the text on which the query
	 *   should be performed.
	 *
	 * \returns True, if there is at least one
	 *   match after performing the query on the
	 *   document. False otherwise.
	 *
	 * \throws RegEx::Exception if no RegEx
	 *   expression has been defined, an error
	 *   occurs during execution of the query,
	 *   or the output vector is unexpectedly
	 *   too small to contain the result.
	 */
	inline bool RegEx::getBool(const std::string& text) const {
		// check compiled expression
		if(!(this->expressionSingle.valid())) {
			throw RegEx::Exception("No single result expression compiled");
		}

		// get first match
		Wrapper::PCREMatch pcreMatch(
				pcre2_match_data_create_from_pattern(
						this->expressionSingle.getc(),
						nullptr
				)
		);

		auto result{
			pcre2_match(
					this->expressionSingle.getc(),
					static_cast<PCRE2_SPTR>(
							static_cast<const void *>(
									text.c_str()
							)
					),
					text.length(),
					0,
					0,
					pcreMatch.get(),
					nullptr
			)
		};

		// check result
		if(result <= 0) {
			switch(result) {
			case PCRE2_ERROR_NOMATCH:
				// no match found -> result is false
				return false;

			case 0:
				// output vector was too small (should not happen when using pcre2_match_data_create_from_pattern(...))
				throw RegEx::Exception("Result vector unexpectedly too small");

			default:
				// match error: set error message and delete match
				std::array<char, pcre2ErrorBufferLength> errorBuffer{};

				pcre2_get_error_message(
						result,
						static_cast<PCRE2_UCHAR8 *>(
								static_cast<void *>(
										errorBuffer.data()
								)
						),
						pcre2ErrorBufferLength
				);

				throw RegEx::Exception(errorBuffer.data());
			}
		}

		// at least one match found -> result is true
		return true;
	}

	//! Gets the first match from performing the query on a parsed JSON document.
	/*!
	 * \note Cannot use string view, because
	 *   the underlying API requires a null-
	 *   terminated string.
	 *
	 * \param text Const reference to a string
	 *   containing the text on which the query
	 *   should be performed.
	 * \param resultTo Reference to a string
	 *   to which the result will be written.
	 *   The string will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws RegEx::Exception if no RegEx
	 *   expression has been defined, an error
	 *   occurs during execution of the query,
	 *   or the output vector is unexpectedly
	 *   too small to contain the result.
	 */
	inline void RegEx::getFirst(const std::string& text, std::string& resultTo) const {
		// empty target
		resultTo.clear();

		// check compiled expression
		if(!(this->expressionSingle.valid())) {
			throw RegEx::Exception("No single result expression compiled");
		}

		// get first match
		Wrapper::PCREMatch pcreMatch(
				pcre2_match_data_create_from_pattern(
						this->expressionSingle.getc(),
						nullptr
				)
		);

		int result{
			pcre2_match(
					this->expressionSingle.getc(),
					static_cast<PCRE2_SPTR>(
							static_cast<const void *>(
									text.c_str()
							)
					),
					text.length(),
					0,
					0,
					pcreMatch.get(),
					nullptr
			)
		};

		// check result
		if(result <= 0) {
			switch(result) {
			case PCRE2_ERROR_NOMATCH:
				// no match found -> result is empty string
				return;

			case 0:
				// output vector was too small (should not happen when using pcre2_match_data_create_from_pattern(...))
				throw RegEx::Exception("Result vector unexpectedly too small");

			default:
				// matching error
				std::array<char, pcre2ErrorBufferLength> errorBuffer{};

				pcre2_get_error_message(
						result,
						static_cast<PCRE2_UCHAR8 *>(
								static_cast<void *>(
										errorBuffer.data()
								)
						),
						pcre2ErrorBufferLength
				);

				throw RegEx::Exception(errorBuffer.data());
			}
		}

		// at least one match found -> get resulting match
		auto * pcreOVector{pcre2_get_ovector_pointer(pcreMatch.get())};

		//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		resultTo = text.substr(pcreOVector[0], pcreOVector[1] - pcreOVector[0]);
	}

	//! Gets all matches from performing the query on a parsed JSON document.
	/*!
	 * \note Cannot use string view, because
	 *   the underlying API requires a null-
	 *   terminated string.
	 *
	 * \param text Const reference to a string
	 *   containing the text on which the query
	 *   should be performed.
	 * \param resultTo Reference to a vector
	 *   to which the results will be written.
	 *   The vector will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws RegEx::Exception if no RegEx
	 *   expression has been defined, an error
	 *   occurs during execution of the query,
	 *   or the output vector is unexpectedly
	 *   too small to contain the result.
	 */
	inline void RegEx::getAll(const std::string& text, std::vector<std::string>& resultTo) const {
		// empty target
		resultTo.clear();

		// check compiled expression
		if(!(this->expressionMulti.valid())) {
			throw RegEx::Exception("No multi result expression compiled");
		}

		// get first match
		Wrapper::PCREMatch pcreMatch(
				pcre2_match_data_create_from_pattern(
						this->expressionMulti.getc(),
						nullptr
				)
		);

		auto result{
			pcre2_match(
					this->expressionMulti.getc(),
					static_cast<PCRE2_SPTR>(
							static_cast<const void *>(
									text.c_str()
							)
					),
					text.length(),
					0,
					0,
					pcreMatch.get(),
					nullptr
			)
		};

		// check result
		if(result <= 0) {
			switch(result) {
			case PCRE2_ERROR_NOMATCH:
				// no match found -> result is empty array
				return;

			case 0:
				// output vector was too small (should not happen when using pcre2_match_data_create_from_pattern(...))
				throw RegEx::Exception("Result vector unexpectedly too small");

			default:
				// matching error
				std::array<char, pcre2ErrorBufferLength> errorBuffer{};

				pcre2_get_error_message(
						result,
						static_cast<PCRE2_UCHAR8 *>(
								static_cast<void *>(
										errorBuffer.data()
								)
						),
						pcre2ErrorBufferLength
				);

				throw RegEx::Exception(errorBuffer.data());
			}
		}

		// at least one match found -> save first match
		auto * pcreOVector{pcre2_get_ovector_pointer(pcreMatch.get())};

		//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
		resultTo.emplace_back(text, pcreOVector[0], pcreOVector[1] - pcreOVector[0]);

		// get RegEx options
		std::uint32_t pcreOptions{0};
		std::uint32_t pcreNewLineOption{0};

		pcre2_pattern_info(this->expressionMulti.getc(), PCRE2_INFO_ALLOPTIONS, &pcreOptions);
		pcre2_pattern_info(this->expressionMulti.getc(), PCRE2_INFO_NEWLINE, &pcreNewLineOption);

		const auto pcreUTF8{(pcreOptions & PCRE2_UTF) != 0};
		const auto pcreNewLine{
				pcreNewLineOption == PCRE2_NEWLINE_ANY
				|| pcreNewLineOption == PCRE2_NEWLINE_CRLF
				|| pcreNewLineOption == PCRE2_NEWLINE_ANYCRLF
		};

		// get more matches
		while(true) {
			//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			auto pcreOffset{pcreOVector[1]};
			pcreOptions = 0;

			// check for empty string (end of matches)
			//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			if(pcreOVector[0] == pcreOVector[1]) {
				//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
				if(pcreOVector[0] == text.length()) {
					break;
				}

				pcreOptions = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
			}

			// get next match
			result = pcre2_match(
					this->expressionMulti.getc(),
					static_cast<PCRE2_SPTR>(
							static_cast<const void *>(
									text.c_str()
							)
					),
					text.length(),
					pcreOffset,
					pcreOptions,
					pcreMatch.get(),
					nullptr
			);

			// check result
			if(result == PCRE2_ERROR_NOMATCH) {
				if(pcreOptions == 0) {
					break;
				}

				//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
				pcreOVector[1] = pcreOffset + 1;

				if(
						pcreNewLine
						&& pcreOffset < text.length() - 1
						&& text.at(pcreOffset) == '\r'
						&& text.at(pcreOffset + 1) == '\n'
				) {
					//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
					pcreOVector[1] += 1;
				}
				else if(pcreUTF8) {
					//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
					while(pcreOVector[1] < text.length()) {
						//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic, hicpp-signed-bitwise)
						if((text.at(pcreOVector[1]) & bitmaskTopTwoBits) != bitmaskTopBit) {
							break;
						}

						//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
						pcreOVector[1] += 1;
					}
				}

				continue;
			}

			if(result < 0) {
				// matching error
				std::array<char, pcre2ErrorBufferLength> errorBuffer{};

				pcre2_get_error_message(
						result,
						static_cast<PCRE2_UCHAR8 *>(
								static_cast<void *>(
										errorBuffer.data()
								)
						),
						pcre2ErrorBufferLength
				);

				throw RegEx::Exception(errorBuffer.data());
			}

			if(result == 0) {
				throw RegEx::Exception("Result vector unexpectedly too small");
			}

			// get resulting match
			//NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
			resultTo.emplace_back(text, pcreOVector[0], pcreOVector[1] - pcreOVector[0]);
		}
	}

	//! Gets whether the query is valid.
	/*!
	 * \returns True, if the unerlying expression
	 *   is valid. False otherwise.
	 */
	inline bool RegEx::valid() const noexcept {
		return this->expressionSingle.valid() || this->expressionMulti.valid();
	}

} /* namespace crawlservpp::Query */

#endif /* QUERY_REGEX_HPP_ */
