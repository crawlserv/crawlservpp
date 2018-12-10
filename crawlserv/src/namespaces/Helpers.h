/*
 * Helpers.h
 *
 * Global helper functions encapsulated into one namespace.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef NAMESPACES_HELPERS_H_
#define NAMESPACES_HELPERS_H_

#include "../structs/Memento.h"

#include "../external/date.h"
#include "../external/utf8.h"

#include <experimental/filesystem>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#ifdef __unix
	#include <termios.h>
	#include <stdio.h>
#elif
	#include <conio.h>
#endif

namespace Helpers {
	// timing
	std::string microsecondsToString(unsigned long long microseconds);
	std::string millisecondsToString(unsigned long long milliseconds);
	std::string secondsToString(unsigned long long seconds);

	// file system
	std::vector<std::string> listFilesInPath(const std::string& pathToDir, const std::string& fileExtension);

	// string functions
	bool stringToBool(std::string inputString);
	void replaceAll(std::string& strInOut, const std::string& from, const std::string& to);
	std::string iso88591ToUtf8(const std::string& strIn);
	bool repairUtf8(const std::string& strIn, std::string& strOut);
	void trim(std::string& stringToTrim);

	// Memento parsing
	std::string parseMementos(std::string mementoContent, std::vector<std::string>& warningsTo,
			std::vector<Memento>& mementosTo);

	// date/time conversions
	bool convertLongDateToSQLTimeStamp(std::string& timeStamp);
	bool convertTimeStampToSQLTimeStamp(std::string& timeStamp);
	bool convertSQLTimeStampToTimeStamp(std::string& timeStamp);

#ifdef __unix
	// portable getch
	char getch(void);
#endif
}


#endif /* NAMESPACES_HELPERS_H_ */
