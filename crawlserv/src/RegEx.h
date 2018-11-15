/*
 * RegEx.h
 *
 * Using the PCRE2 library to implement a Perl-Compatible Regular Expressions query with boolean, single and/or multiple results.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef REGEX_H_
#define REGEX_H_

#define PCRE2_CODE_UNIT_WIDTH 8
#define PCRE2_ERROR_BUFFER_LENGTH 1024

#include <pcre2.h>

#include <sstream>
#include <string>
#include <vector>

class RegEx {
public:
	RegEx();
	virtual ~RegEx();

	bool compile(const std::string& expression, bool single, bool multi);
	bool getBool(const std::string& text, bool& resultTo) const;
	bool getFirst(const std::string& text, std::string& resultTo) const;
	bool getAll(const std::string& text, std::vector<std::string>& resultTo) const;

	std::string getErrorMessage() const;

protected:
	pcre2_code * expressionSingle;
	pcre2_code * expressionMulti;

private:
	mutable std::string errorMessage;
};

#endif /* REGEX_H_ */
