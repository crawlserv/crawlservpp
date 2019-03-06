/*
 * DateTime.h
 *
 * Namespace with global helper functions for date/time and time to string conversion.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_DATETIME_H_
#define HELPER_DATETIME_H_

#include "../_extern/date/include/date/date.h"

#include <locale>
#include <sstream>
#include <string>

namespace Helper::DateTime {
	// date/time conversions
	bool convertLongDateTimeToSQLTimeStamp(std::string& dateTime);
	bool convertCustomDateTimeToSQLTimeStamp(std::string& dateTime, const std::string& customFormat);
	bool convertCustomDateTimeToSQLTimeStamp(std::string& dateTime, const std::string& customFormat, const std::locale& locale);
	bool convertTimeStampToSQLTimeStamp(std::string& timeStamp);
	bool convertSQLTimeStampToTimeStamp(std::string& timeStamp);

	// date/time verification
	bool isValidISODate(const std::string& isoDate);

	// date/time comparison
	bool isISODateInRange(const std::string& isoDate, const std::string& rangeFrom, const std::string& rangeTo);

	// timing
	std::string microsecondsToString(unsigned long long microseconds);
	std::string millisecondsToString(unsigned long long milliseconds);
	std::string secondsToString(unsigned long long seconds);
}

#endif /* HELPER_DATETIME_H_ */
