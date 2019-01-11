/*
 * Strings.cpp
 *
 * Namespace with global string helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#include "Strings.h"

// replace all occurences of a string with another string (onlyOnce avoids replacing parts of the replacements)
void Strings::replaceAll(std::string& strInOut, const std::string& from, const std::string& to, bool onlyOnce) {
	unsigned long startPos = 0;
	if(from.empty()) return;

	while((startPos = strInOut.find(from, startPos)) != std::string::npos) {
		strInOut.replace(startPos, from.length(), to);
		if(onlyOnce) startPos += to.length();
	}
}

// convert string to boolean value
bool Strings::stringToBool(std::string inputString) {
	std::transform(inputString.begin(), inputString.end(), inputString.begin(), ::tolower);
	std::istringstream strStr(inputString);
	bool result;
	strStr >> std::boolalpha >> result;
	return result;
}

// trim a string (NOTE: Only ASCII white spaces will be processed!)
void Strings::trim(std::string & stringToTrim) {
	stringToTrim.erase(stringToTrim.begin(), std::find_if(stringToTrim.begin(), stringToTrim.end(), [](int ch) {
		return !std::isspace(ch);
	}));
	stringToTrim.erase(std::find_if(stringToTrim.rbegin(), stringToTrim.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), stringToTrim.end());
}

// concatenate all elements of a vector into a single string
std::string Strings::concat(const std::vector<std::string>& vectorToConcat, char delimiter, bool ignoreEmpty) {
	std::string result;
	for(auto i = vectorToConcat.begin(); i != vectorToConcat.end(); ++i) {
		if(!ignoreEmpty || i->length())	result += *i + delimiter;
	}
	if(result.length()) result.pop_back();
	return result;
}

// get the first character of the string or an escaped character (\n, \t or \\) (NOTE: Only ASCII supported!)
char Strings::getFirstOrEscapeChar(const std::string& from) {
	if(from.length()) {
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
		else return from.at(0);
	}
	return 0;
}

// remove new lines and unnecessary spaces (including Unicode white spaces)
void Strings::utfTidy(std::string& stringToTidy) {
	// replace Unicode white spaces with spaces
	for(unsigned long n = 0; n < sizeof(Strings::utfWhitespaces) / sizeof(std::string); n++)
		Strings::replaceAll(stringToTidy, Strings::utfWhitespaces[n], " ", true);

	// replace special ASCII characters with spaces
	std::replace(stringToTidy.begin(), stringToTidy.end(), '\t', ' '); // horizontal tab
	std::replace(stringToTidy.begin(), stringToTidy.end(), '\n', ' '); // line feed
	std::replace(stringToTidy.begin(), stringToTidy.end(), '\v', ' '); // vertical tab
	std::replace(stringToTidy.begin(), stringToTidy.end(), '\f', ' '); // form feed
	std::replace(stringToTidy.begin(), stringToTidy.end(), '\r', ' '); // carriage return

	// replace unnecessary spaces
	Strings::replaceAll(stringToTidy, " .", ".", true);
	Strings::replaceAll(stringToTidy, " ,", ",", true);
	Strings::replaceAll(stringToTidy, " :", ":", true);
	Strings::replaceAll(stringToTidy, " ;", ";", true);
	Strings::replaceAll(stringToTidy, "( ", "(", true);
	Strings::replaceAll(stringToTidy, " )", ")", true);

	// replace double spaces
	Strings::replaceAll(stringToTidy, "  ", " ", false);

	// trim result
	Strings::trim(stringToTidy);
}
