/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * RegEx.cpp
 *
 * Using the PCRE2 library to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
 *  Expression is only created when needed.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "RegEx.hpp"

namespace crawlservpp::Query {

	// constructor stub
	RegEx::RegEx(const std::string& expression, bool single, bool multi) {
		int errorNumber = 0;
		PCRE2_SIZE errorOffset = 0;

		// delete old expressions if necessary
		this->expressionSingle.reset();
		this->expressionMulti.reset();

		// check arguments
		if(expression.empty())
			throw RegEx::Exception("Expression is empty");

		if(!single && !multi)
			throw RegEx::Exception("No result type for expression specified");

		// compile expression(s)
		if(single) {
			this->expressionSingle.reset(
					pcre2_compile(
							(PCRE2_SPTR) expression.c_str(),
							PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP,
							&errorNumber,
							&errorOffset,
							nullptr
					)
			);

			if(!(this->expressionSingle)) {
				// RegEx error
				PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];

				pcre2_get_error_message(errorNumber, errorBuffer, sizeof(errorBuffer));

				throw RegEx::Exception(
						"Compilation error at "
						+ std::to_string(errorOffset)
						+ ": "
						+ std::string(reinterpret_cast<const char *>(errorBuffer))
				);
			}
		}

		if(multi) {
			this->expressionMulti.reset(
					pcre2_compile(
							(PCRE2_SPTR) expression.c_str(),
							PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP | PCRE2_MULTILINE,
							&errorNumber,
							&errorOffset,
							nullptr
					)
			);

			if(!(this->expressionMulti)) {
				// RegEx error
				PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];

				pcre2_get_error_message(errorNumber, errorBuffer, sizeof(errorBuffer));

				throw RegEx::Exception(
						"Compilation error at "
						+ std::to_string(errorOffset)
						+ ": "
						+ std::string(reinterpret_cast<const char *>(errorBuffer))
				);
			}
		}
	}

	// move constructor
	RegEx::RegEx(RegEx&& other) noexcept :
			expressionSingle(std::move(other.expressionSingle)),
			expressionMulti(std::move(other.expressionMulti)) {}

	// destructor stub
	RegEx::~RegEx() {}

	// get boolean result of RegEx expression (at least one match?), throws RegEx::Exception
	bool RegEx::getBool(const std::string& text) const {
		// check compiled expression
		if(!(this->expressionSingle))
			throw RegEx::Exception("No single result expression compiled.");

		// get first match
		Wrapper::PCREMatch pcreMatch(
				pcre2_match_data_create_from_pattern(
						this->expressionSingle.get(),
						nullptr
				)
		);

		int result = pcre2_match(
				this->expressionSingle.get(),
				(PCRE2_SPTR) text.c_str(),
				text.length(),
				0,
				0,
				pcreMatch.get(),
				nullptr
		);

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
				PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];

				pcre2_get_error_message(result, errorBuffer, sizeof(errorBuffer));

				throw RegEx::Exception(reinterpret_cast<const char *>(errorBuffer));
			}
		}

		// at least one match found -> result is true
		return true;
	}

	// get single result of RegEx expression (first match), throws RegEx::Exception
	void RegEx::getFirst(const std::string& text, std::string& resultTo) const {
		// check compiled expression
		if(!(this->expressionSingle))
			throw RegEx::Exception("No single result expression compiled");

		// empty target
		resultTo.clear();

		// get first match
		Wrapper::PCREMatch pcreMatch(
				pcre2_match_data_create_from_pattern(
						this->expressionSingle.get(),
						nullptr
				)
		);

		int result = pcre2_match(
				this->expressionSingle.get(),
				(PCRE2_SPTR) text.c_str(),
				text.length(),
				0,
				0,
				pcreMatch.get(),
				nullptr
		);

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
				PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];

				pcre2_get_error_message(result, errorBuffer, sizeof(errorBuffer));

				throw RegEx::Exception(reinterpret_cast<const char *>(errorBuffer));
			}
		}

		// at least one match found -> get resulting match
		const PCRE2_SIZE * pcreOVector = pcre2_get_ovector_pointer(pcreMatch.get());

		resultTo = text.substr(pcreOVector[0], pcreOVector[1] - pcreOVector[0]);
	}

	// get all results of RegEx expression (all full matches), throws RegEx::Exception
	void RegEx::getAll(const std::string& text, std::vector<std::string>& resultTo) const {
		// check compiled expression
		if(!(this->expressionMulti))
			throw RegEx::Exception("No multi result expression compiled");

		// empty target
		resultTo.clear();

		// get first match
		Wrapper::PCREMatch pcreMatch(
				pcre2_match_data_create_from_pattern(
						this->expressionMulti.get(),
						nullptr
				)
		);

		int result = pcre2_match(
				this->expressionMulti.get(),
				(PCRE2_SPTR) text.c_str(),
				text.length(),
				0,
				0,
				pcreMatch.get(), nullptr
		);

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
				PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];

				pcre2_get_error_message(result, errorBuffer, sizeof(errorBuffer));

				throw RegEx::Exception(reinterpret_cast<const char *>(errorBuffer));
			}
		}

		// at least one match found -> save first match
		PCRE2_SIZE * pcreOVector = pcre2_get_ovector_pointer(pcreMatch.get());

		resultTo.emplace_back(text, pcreOVector[0], pcreOVector[1] - pcreOVector[0]);

		// get RegEx options
		uint32_t pcreOptions = 0;
		uint32_t pcreNewLineOption = 0;

		pcre2_pattern_info(this->expressionMulti.get(), PCRE2_INFO_ALLOPTIONS, &pcreOptions);
		pcre2_pattern_info(this->expressionMulti.get(), PCRE2_INFO_NEWLINE, &pcreNewLineOption);

		const int pcreUTF8 = (pcreOptions & PCRE2_UTF) != 0;
		const int pcreNewLine =
				pcreNewLineOption == PCRE2_NEWLINE_ANY
				|| pcreNewLineOption == PCRE2_NEWLINE_CRLF
				|| pcreNewLineOption == PCRE2_NEWLINE_ANYCRLF;

		// get more matches
		while(true) {
			PCRE2_SIZE pcreOffset = pcreOVector[1];
			pcreOptions = 0;

			// check for empty string (end of matches)
			if(pcreOVector[0] == pcreOVector[1]) {
				if(pcreOVector[0] == text.length())
					break;

				pcreOptions = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
			}

			// get next match
			result = pcre2_match(
					this->expressionMulti.get(),
					(PCRE2_SPTR) text.c_str(),
					text.length(),
					pcreOffset,
					pcreOptions,
					pcreMatch.get(),
					nullptr
			);

			// check result
			if(result == PCRE2_ERROR_NOMATCH) {
				if(!pcreOptions)
					break;

				pcreOVector[1] = pcreOffset + 1;

				if(
						pcreNewLine
						&& pcreOffset < text.length() - 1
						&& text.at(pcreOffset) == '\r'
						&& text.at(pcreOffset + 1) == '\n'
				)
					pcreOVector[1] += 1;
				else if(pcreUTF8) {
					while(pcreOVector[1] < text.length()) {
						if((text.at(pcreOVector[1]) & 0xc0) != 0x80)
							break;

						pcreOVector[1] += 1;
					}
				}

				continue;
			}

			if(result < 0) {
				// matching error
				PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];

				pcre2_get_error_message(result, errorBuffer, sizeof(errorBuffer));

				throw RegEx::Exception(reinterpret_cast<const char *>(errorBuffer));
			}

			if(!result)
				throw RegEx::Exception("Result vector unexpectedly too small");

			// get resulting match
			resultTo.emplace_back(text, pcreOVector[0], pcreOVector[1] - pcreOVector[0]);
		}
	}

	// bool operator
	RegEx::operator bool() const noexcept {
		return this->expressionSingle || this->expressionMulti;
	}

	// not operator
	bool RegEx::operator!() const noexcept {
		return !(this->expressionSingle) && !(this->expressionMulti);
	}

} /* crawlservpp::Query */
