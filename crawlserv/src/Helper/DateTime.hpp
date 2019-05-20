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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * DateTime.hpp
 *
 * Namespace with global helper functions for date/time and time to string conversion.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_DATETIME_HPP_
#define HELPER_DATETIME_HPP_

#include "../_extern/date/include/date/date.h"

#include <locale>	// std::locale
#include <sstream>	// std::istringstream
#include <string>	// std::string, std::to_string

namespace crawlservpp::Helper::DateTime {

	/*
	 * DECLARATION
	 */

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

	/*
	 * IMPLEMENTATION
	 */

	// convert time stamp from WEEKDAY[short], DD MONTH[short] YYYY HH:MM:SS TIMEZONE[short] to YYYY-MM-DD HH:MM:SS
	inline bool convertLongDateTimeToSQLTimeStamp(std::string& dateTime) {
		return convertCustomDateTimeToSQLTimeStamp(dateTime, "%a, %d %b %Y %T %Z");
	}

	// convert date and time with custom format to YYYY-MM-DD HH:MM:SS
	inline bool convertCustomDateTimeToSQLTimeStamp(std::string& dateTime, const std::string& customFormat) {
		std::istringstream in(dateTime);
		date::sys_seconds tp;

		in >> date::parse(customFormat, tp);

		if(!bool(in))
			return false;

		dateTime = date::format("%F %T", tp);

		return true;
	}

	// convert date and time with custom format to YYYY-MM-DD HH:MM:SS (using specific locale),
	//  return whether conversion was successful
	inline bool convertCustomDateTimeToSQLTimeStamp(
			std::string& dateTime,
			const std::string& customFormat,
			const std::locale& locale
	) {
		std::istringstream in(dateTime);
		date::sys_seconds tp;

		in.imbue(locale);

		in >> date::parse(customFormat, tp);

		if(!bool(in))
			return false;

		dateTime = date::format("%F %T", tp);

		return true;
	}

	// convert time stamp from YYYYMMDDHHMMSS to YYYY-MM-DD HH:MM:SS
	inline bool convertTimeStampToSQLTimeStamp(std::string& timeStamp) {
		return convertCustomDateTimeToSQLTimeStamp(timeStamp, "%Y%m%d%H%M%S");
	}

	// convert time stamp from YYYY-MM-DD HH:MM:SS to YYYYMMDDHHMMSS
	inline bool convertSQLTimeStampToTimeStamp(std::string& timeStamp) {
		std::istringstream in(timeStamp);
		date::sys_seconds tp;

		in >> date::parse("%F %T", tp);

		if(!bool(in))
			return false;

		timeStamp = date::format("%Y%m%d%H%M%S", tp);

		return true;
	}

	// check whether a string contains a valid ISO date
	inline bool isValidISODate(const std::string& isoDate) {
		std::istringstream in(isoDate);
		date::sys_days tp;

		in >> date::parse("%F", tp);

		return bool(in);
	}

	// check whether a ISO date (YYYY-MM-DD) is in a specific date range
	inline bool isISODateInRange(
			const std::string& isoDate,
			const std::string& rangeFrom,
			const std::string& rangeTo
	) {
		if(isoDate.length() < 10)
			return false;

		if(rangeFrom.length() < 10 && rangeTo.length() < 10)
			return true;

		if(rangeTo.length() < 10)
			return isoDate.substr(0, 10) >= rangeFrom.substr(0, 10);

		if(rangeFrom.length() < 10)
			return isoDate.substr(0, 10) <= rangeTo.substr(0, 10);

		return isoDate.substr(0, 10) >= rangeFrom.substr(0, 10) && isoDate <= rangeTo.substr(0, 10);
	}

	// convert microseconds to string
	inline std::string microsecondsToString(unsigned long long microseconds) {
		unsigned long long rest = microseconds;
		unsigned long days = rest / 86400000000;
		std::string result;

		rest -= days * 86400000000;

		unsigned short hours = rest / 3600000000;

		rest -= hours * 3600000000;

		unsigned short minutes = rest / 60000000;

		rest -= minutes * 60000000;

		unsigned short seconds = rest / 1000000;

		rest -= seconds * 1000000;

		unsigned short milliseconds = rest / 1000;

		rest -= milliseconds * 1000;

		if(days)
			result += std::to_string(days) + "d ";

		if(hours)
			result += std::to_string(hours) + "h ";

		if(minutes)
			result += std::to_string(minutes) + "min ";

		if(seconds)
			result += std::to_string(seconds) + "s ";

		if(milliseconds)
			result += std::to_string(milliseconds) + "ms ";

		if(rest)
			result += std::to_string(rest) + "μs ";

		if(result.empty())
			return "<1μs";

		result.pop_back();

		return result;
	}

	// convert milliseconds to string
	inline std::string millisecondsToString(unsigned long long milliseconds) {
		unsigned long long rest = milliseconds;
		std::string result;

		const unsigned long days = rest / 86400000;

		rest -= days * 86400000;

		const unsigned short hours = rest / 3600000;

		rest -= hours * 3600000;

		const unsigned short minutes = rest / 60000;

		rest -= minutes * 60000;

		const unsigned short seconds = rest / 1000;

		rest -= seconds * 1000;

		if(days)
			result += std::to_string(days) + "d ";

		if(hours)
			result += std::to_string(hours) + "h ";

		if(minutes)
			result += std::to_string(minutes) + "min ";

		if(seconds)
			result += std::to_string(seconds) + "s ";

		if(rest)
			result += std::to_string(rest) + "ms ";

		if(result.empty())
			return "<1ms";

		result.pop_back();

		return result;
	}

	// convert seconds to string
	inline std::string secondsToString(unsigned long long seconds) {
		unsigned long long rest = seconds;
		std::string result;

		const unsigned long days = rest / 86400;

		rest -= days * 86400;

		const unsigned short hours = rest / 3600;

		rest -= hours * 3600;

		const unsigned short minutes = rest / 60;

		rest -= minutes * 60;

		if(days)
			result += std::to_string(days) + "d ";

		if(hours)
			result += std::to_string(hours) + "h ";

		if(minutes)
			result += std::to_string(minutes) + "min ";

		if(rest)
			result += std::to_string(rest) + "s ";

		if(result.empty())
			return "<1s";

		result.pop_back();

		return result;
	}

} /* crawlservpp::Helper::DateTime */

#endif /* HELPER_DATETIME_HPP_ */
