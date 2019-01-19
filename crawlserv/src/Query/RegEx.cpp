/*
 * RegEx.cpp
 *
 * Using the PCRE2 library to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "RegEx.h"

namespace crawlservpp::Query {

// constructor
RegEx::RegEx() {
	this->expressionSingle = NULL;
	this->expressionMulti = NULL;
}

// destructor
RegEx::~RegEx() {
	if(this->expressionSingle) {
		pcre2_code_free(this->expressionSingle);
		this->expressionSingle = NULL;
	}
	if(this->expressionMulti) {
		pcre2_code_free(this->expressionMulti);
		this->expressionMulti = NULL;
	}
}

// compile RegEx expression
bool RegEx::compile(const std::string& pattern, bool single, bool multi) {
	int errorNumber = 0;
	PCRE2_SIZE errorOffset = 0;

	// delete old expressions if necessary
	if(this->expressionSingle) {
		pcre2_code_free(this->expressionSingle);
		this->expressionSingle = NULL;
	}
	if(this->expressionMulti) {
		pcre2_code_free(this->expressionMulti);
		this->expressionMulti = NULL;
	}

	// check arguments
	if(pattern.empty()) {
		this->errorMessage = "RegEx error: Expression is empty.";
		return false;
	}
	if(!single && !multi) {
		this->errorMessage = "RegEx error: No result type for expression specified.";
		return false;
	}

	// compile expression(s)
	if(single) {
		this->expressionSingle = pcre2_compile((PCRE2_SPTR) pattern.c_str(), PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP,
				&errorNumber, &errorOffset, NULL);
		if(!(this->expressionSingle)) {
			// RegEx error
			PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];
			std::ostringstream errorStrStr;
			pcre2_get_error_message(errorNumber, errorBuffer, sizeof(errorBuffer));
			errorStrStr << "RegEx compilation error at " << errorOffset << ": " << errorBuffer;
			this->errorMessage = errorStrStr.str();
			return false;
		}
	}

	if(multi) {
		this->expressionMulti = pcre2_compile((PCRE2_SPTR) pattern.c_str(), PCRE2_ZERO_TERMINATED, PCRE2_UTF | PCRE2_UCP
				| PCRE2_MULTILINE, &errorNumber, &errorOffset, NULL);
		if(!(this->expressionMulti)) {
			// RegEx error
			PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];
			std::ostringstream errorStrStr;
			pcre2_get_error_message(errorNumber, errorBuffer, sizeof(errorBuffer));
			errorStrStr << "RegEx compilation error at " << errorOffset << ": " << errorBuffer;
			this->errorMessage = errorStrStr.str();
			if(this->expressionSingle) {
				pcre2_code_free(this->expressionSingle);
				this->expressionSingle = NULL;
			}
			return false;
		}
	}

	return true;
}

// get boolean result of RegEx expression (at least one match?)
bool RegEx::getBool(const std::string& text, bool& resultTo) const {
	// check compiled expression
	if(!(this->expressionSingle)) {
		this->errorMessage = "RegEx error: No single result expression compiled.";
		return false;
	}

	// get first match
	pcre2_match_data * pcreMatch = pcre2_match_data_create_from_pattern(this->expressionSingle, NULL);
	int result = pcre2_match(this->expressionSingle, (PCRE2_SPTR) text.c_str(), text.length(), 0, 0, pcreMatch, NULL);

	// check result
	if(result <= 0) {
		switch(result) {
		case PCRE2_ERROR_NOMATCH:
			// no match found -> result is false
			resultTo = false;
			pcre2_match_data_free(pcreMatch);
			return true;

		case 0:
			// output vector was too small (should not happen when using pcre2_match_data_create_from_pattern(...))
			this->errorMessage = "Unexpected RegEx matching error: Result vector too small.";
			pcre2_match_data_free(pcreMatch);
			return false;

		default:
			// match error : set error message and delete match
			PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];
			std::ostringstream errorStrStr;
			pcre2_get_error_message(result, errorBuffer, sizeof(errorBuffer));
			this->errorMessage = "RegEx matching error: " + std::string((char *) errorBuffer);
			pcre2_match_data_free(pcreMatch);
			return false;
		}
	}

	// at least one match found -> result is true
	resultTo = true;

	// delete match
	pcre2_match_data_free(pcreMatch);
	return true;
}

// get single result of RegEx expression (first match)
bool RegEx::getFirst(const std::string& text, std::string& resultTo) const {
	// check compiled expression
	if(!(this->expressionSingle)) {
		this->errorMessage = "RegEx error: No single result expression compiled.";
		return false;
	}

	// get first match
	pcre2_match_data * pcreMatch = pcre2_match_data_create_from_pattern(this->expressionSingle, NULL);
	int result = pcre2_match(this->expressionSingle, (PCRE2_SPTR) text.c_str(), text.length(), 0, 0, pcreMatch, NULL);

	// check result
	if(result <= 0) {
		switch(result) {
		case PCRE2_ERROR_NOMATCH:
			// no match found -> result is empty string
			resultTo = "";
			pcre2_match_data_free(pcreMatch);
			return true;

		case 0:
			// output vector was too small (should not happen when using pcre2_match_data_create_from_pattern(...))
			this->errorMessage = "Unexpected RegEx matching error: Result vector too small.";
			pcre2_match_data_free(pcreMatch);
			return false;

		default:
			// matching error
			PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];
			std::ostringstream errorStrStr;
			pcre2_get_error_message(result, errorBuffer, sizeof(errorBuffer));
			this->errorMessage = "RegEx matching error: " + std::string((char *) errorBuffer);
			pcre2_match_data_free(pcreMatch);
			return false;
		}
	}

	// at least one match found -> get resulting match
	PCRE2_SIZE * pcreOVector = pcre2_get_ovector_pointer(pcreMatch);
	resultTo = text.substr(pcreOVector[0], pcreOVector[1] - pcreOVector[0]);

	// delete match
	pcre2_match_data_free(pcreMatch);
	return true;
}

// get all results of RegEx expression (all full matches)
bool RegEx::getAll(const std::string& text, std::vector<std::string>& resultTo) const {
	std::vector<std::string> resultArray;

	// check compiled expression
	if(!(this->expressionMulti)) {
		this->errorMessage = "RegEx error: No multi result expression compiled.";
		return false;
	}

	// get first match
	pcre2_match_data * pcreMatch = pcre2_match_data_create_from_pattern(this->expressionMulti, NULL);
	int result = pcre2_match(this->expressionMulti, (PCRE2_SPTR) text.c_str(), text.length(), 0, 0, pcreMatch, NULL);

	// check result
	if(result <= 0) {
		switch(result) {
		case PCRE2_ERROR_NOMATCH:
			// no match found -> result is empty array
			resultTo = resultArray;
			pcre2_match_data_free(pcreMatch);
			return true;

		case 0:
			// output vector was too small (should not happen when using pcre2_match_data_create_from_pattern(...))
			this->errorMessage = "Unexpected RegEx matching error: Result vector too small.";
			pcre2_match_data_free(pcreMatch);
			return false;

		default:
			// matching error
			PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];
			std::ostringstream errorStrStr;
			pcre2_get_error_message(result, errorBuffer, sizeof(errorBuffer));
			this->errorMessage = "RegEx matching error: " + std::string((char *) errorBuffer);
			pcre2_match_data_free(pcreMatch);
			return false;
		}
	}

	// at least one match found -> save first match
	PCRE2_SIZE * pcreOVector = pcre2_get_ovector_pointer(pcreMatch);
	resultArray.push_back(text.substr(pcreOVector[0], pcreOVector[1] - pcreOVector[0]));

	// get RegEx options
	uint32_t pcreOptions = 0;
	uint32_t pcreNewLineOption = 0;
	pcre2_pattern_info(this->expressionMulti, PCRE2_INFO_ALLOPTIONS, &pcreOptions);
	pcre2_pattern_info(this->expressionMulti, PCRE2_INFO_NEWLINE, &pcreNewLineOption);
	int pcreUTF8 = (pcreOptions & PCRE2_UTF) != 0;
	int pcreNewLine = pcreNewLineOption == PCRE2_NEWLINE_ANY || pcreNewLineOption == PCRE2_NEWLINE_CRLF
			|| pcreNewLineOption == PCRE2_NEWLINE_ANYCRLF;

	// get more matches
	while(true) {
		PCRE2_SIZE pcreOffset = pcreOVector[1];
		pcreOptions = 0;

		// check for empty string (end of matches)
		if(pcreOVector[0] == pcreOVector[1]) {
			if(pcreOVector[0] == text.length()) break;
			pcreOptions = PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED;
		}

		// get next match
		result = pcre2_match(this->expressionMulti, (PCRE2_SPTR) text.c_str(), text.length(), pcreOffset, pcreOptions,
				pcreMatch, NULL);

		// check result
		if(result == PCRE2_ERROR_NOMATCH) {
			if(!pcreOptions) break;
			pcreOVector[1] = pcreOffset + 1;
			if(pcreNewLine && pcreOffset < text.length() - 1 && text.at(pcreOffset) == '\r' && text.at(pcreOffset + 1) == '\n')
				pcreOVector[1] += 1;
			else if(pcreUTF8) {
				while(pcreOVector[1] < text.length()) {
					if((text.at(pcreOVector[1]) & 0xc0) != 0x80) break;
					pcreOVector[1] += 1;
				}
			}
			continue;
		}

		if(result < 0) {
			// matching error
			PCRE2_UCHAR errorBuffer[PCRE2_ERROR_BUFFER_LENGTH];
			std::ostringstream errorStrStr;
			pcre2_get_error_message(result, errorBuffer, sizeof(errorBuffer));
			this->errorMessage = "RegEx matching error: " + std::string((char *) errorBuffer);
			pcre2_match_data_free(pcreMatch);
			return false;
		}

		if(!result) {
			// output vector was too small (should not happen when using pcre2_match_data_create_from_pattern(...))
			this->errorMessage = "Unexpected RegEx matching error: Result vector too small.";
			pcre2_match_data_free(pcreMatch);
			return false;
		}

		// get resulting match
		resultArray.push_back(text.substr(pcreOVector[0], pcreOVector[1] - pcreOVector[0]));
	}

	// copy result
	resultTo = resultArray;

	// delete match
	pcre2_match_data_free(pcreMatch);
	return true;
}

// get error message
std::string RegEx::getErrorMessage() const {
	return this->errorMessage;
}

}
