/*
 * DateTime.h
 *
 * Namespace with global helper functions for date/time and time to string conversion.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef SRC_NAMESPACES_DATETIME_H_
#define SRC_NAMESPACES_DATETIME_H_

#include "../external/date.h"

#include <locale>
#include <sstream>
#include <string>

namespace DateTime {
	// date/time conversions
	bool convertLongDateTimeToSQLTimeStamp(std::string& dateTime);
	bool convertCustomDateTimeToSQLTimeStamp(std::string& dateTime, const std::string& customFormat);
	bool convertCustomDateTimeToSQLTimeStamp(const std::string& locale, std::string& dateTime, const std::string& customFormat);
	bool convertTimeStampToSQLTimeStamp(std::string& timeStamp);
	bool convertSQLTimeStampToTimeStamp(std::string& timeStamp);

	// timing
	std::string microsecondsToString(unsigned long long microseconds);
	std::string millisecondsToString(unsigned long long milliseconds);
	std::string secondsToString(unsigned long long seconds);
}

#endif /* SRC_NAMESPACES_DATETIME_H_ */
