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
 * DateTime.hpp
 *
 * Namespace with global helper functions for date/time and time to string conversion.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_DATETIME_HPP_
#define HELPER_DATETIME_HPP_

#include "Strings.hpp"

#include "../Main/Exception.hpp"

#include "../_extern/date/include/date/date.h"

#include <boost/lexical_cast.hpp>

#include <algorithm>	// std::min
#include <cmath>		// std::round
#include <cctype>		// ::ispunct, ::isspace, ::tolower
#include <chrono>		// std::chrono::seconds, std::chrono::system_clock
#include <clocale>		// ::setlocale
#include <ctime>		// struct ::tm, ::strftime, ::strptime
#include <locale>		// std::locale
#include <sstream>		// std::istringstream
#include <stdexcept>	// std::runtime_error
#include <string>		// std::string, std::to_string

namespace crawlservpp::Helper::DateTime {

	/*
	 * DECLARATION
	 */

	// date/time conversions
	void convertLongDateTimeToSQLTimeStamp(std::string& dateTime);
	void convertCustomDateTimeToSQLTimeStamp(std::string& dateTime, const std::string& customFormat);
	void convertCustomDateTimeToSQLTimeStamp(
			std::string& dateTime,
			const std::string& customFormat,
			const std::string& locale
	);
	void convertTimeStampToSQLTimeStamp(std::string& timeStamp);
	void convertSQLTimeStampToTimeStamp(std::string& timeStamp);
	std::string now();

	// date/time verification
	bool isValidISODate(const std::string& isoDate);

	// date/time comparison
	bool isISODateInRange(const std::string& isoDate, const std::string& rangeFrom, const std::string& rangeTo);

	// timing
	std::string microsecondsToString(unsigned long long microseconds);
	std::string millisecondsToString(unsigned long long milliseconds);
	std::string secondsToString(unsigned long long seconds);

	/*
	 * CLASSES FOR DATE/TIME EXCEPTIONS
	 */

	MAIN_EXCEPTION_CLASS();
	MAIN_EXCEPTION_SUBCLASS(LocaleException);

	/*
	 * IMPLEMENTATION
	 */

	// convert timestamp from WEEKDAY[short], DD MONTH[short] YYYY HH:MM:SS TIMEZONE[short] to YYYY-MM-DD HH:MM:SS
	inline void convertLongDateTimeToSQLTimeStamp(std::string& dateTime) {
		convertCustomDateTimeToSQLTimeStamp(dateTime, "%a, %d %b %Y %T %Z");
	}

	// convert date and time with custom format to YYYY-MM-DD HH:MM:SS
	inline void convertCustomDateTimeToSQLTimeStamp(std::string& dateTime, const std::string& customFormat) {
		// check arguments
		if(dateTime.empty())
			return;

		if(customFormat.empty())
			throw Exception("DateTime::convertCustomDateTimeToSQLTimeStamp(): No custom format specified");

		// check for UNIX time (TODO: document hidden feature)
		if(
				customFormat == "UNIX"
				|| (
						customFormat.length() > 5
						&& (
								customFormat.substr(0, 5) == "UNIX+" || customFormat.substr(0, 5) == "UNIX-"
						)
					)
		) {
			// get (optional) offset from UNIX time
			long offset = 0;

			if(customFormat.length() > 5) {
				try {
					offset = boost::lexical_cast<size_t>(customFormat.substr(4));
				}
				catch(const boost::bad_lexical_cast& e) {
					throw Exception(
							"DateTime::convertCustomDateTimeToSQLTimeStamp(): Invalid date/time format - "
							+ customFormat
							+ " [expected: UNIX, UNIX+N or UNIX-N where N is a valid number]"
					);
				}
			}

			// get UNIX time
			time_t unixTime = 0;

			try {
				if(dateTime.find('.') != std::string::npos) {
					// handle values with comma as floats (and round them)
					float f = boost::lexical_cast<float>(dateTime);

					unixTime = static_cast<time_t>(std::round(f));
				}
				else
					unixTime = boost::lexical_cast<time_t>(dateTime);
			}
			catch(const boost::bad_lexical_cast& e) {
				throw Exception(
						"Could not convert \'"
						+ dateTime
						+ "\' [expected format: \'"
						+ customFormat
						+ "\'] to date/time "
				);
			}

			// remove (optional) offset
			unixTime -= offset;

			// conversion
			dateTime = date::format("%F %T", std::chrono::system_clock::from_time_t(unixTime));
		}
		else {
			// ordinal hack: remove ordinal endings (st, nd, rd, th)
			size_t pos = 0;

			while(pos + 2 <= dateTime.length()) {
				pos = std::min(
						dateTime.find("st", pos),
						std::min(
								dateTime.find("nd", pos),
								std::min(
										dateTime.find("rd", pos),
										dateTime.find("th", pos)
								)
						)
				);

				if(pos == std::string::npos)
					break;

				if(pos > 0) {
					if(::isdigit(dateTime.at(pos - 1))) {
						if(
								pos + 2 == dateTime.length()
								|| ::isspace(dateTime.at(pos + 2))
								|| ::ispunct(dateTime.at(pos + 2))
						) {
							// remove st, nd, rd or th
							dateTime.erase(pos, 2);

							// skip whitespace/punctuation afterwards
							++pos;
						}
						else
							pos += 3;
					}
					else
						pos += 3;
				}
				else
					pos += 3;
			}
			// end of ordinal hack

			std::istringstream in(dateTime);
			date::sys_seconds tp;

			in >> date::parse(customFormat, tp);

			if(bool(in))
				dateTime = date::format("%F %T", tp);
			else {
				// try good old C time
				struct ::tm cTime = {};

				if(!::strptime(dateTime.c_str(), customFormat.c_str(), &cTime))
					throw Exception(
							"Could not convert \'"
							+ dateTime
							+ "\' [expected format: \'"
							+ customFormat
							+ "\'] to date/time "
					);

				char out[20] = { 0 };

				size_t len = ::strftime(out, 20, "%F %T", &cTime);

				if(len)
					dateTime = std::string(out, len);
				else
					throw Exception(
							"Could not convert \'"
							+ dateTime
							+ "\' [expected format: \'"
							+ customFormat
							+ "\'] to date/time "
					);
			}
		}
	}

	// convert date and time with custom format to YYYY-MM-DD HH:MM:SS (using specific locale),
	inline void convertCustomDateTimeToSQLTimeStamp(
			std::string& dateTime,
			const std::string& customFormat,
			const std::string& locale
	) {
		// check arguments
		if(dateTime.empty())
			return;

		if(customFormat.empty())
			throw Exception("DateTime::convertCustomDateTimeToSQLTimeStamp(): No custom format specified");

		if(locale.empty()) {
			convertCustomDateTimeToSQLTimeStamp(dateTime, customFormat);

			return;
		}

		// locale hack: The French abbreviation "avr." for April is not stringently supported
		if(
				locale.size() > 1
				&& ::tolower(locale.at(0) == 'f')
				&& ::tolower(locale.at(1) == 'r')
		)
			Helper::Strings::replaceAll(dateTime, "avr.", "avril", true);
		// end of locale hack

		// ordinal hack: remove ordinal endings (st, nd, rd, th) after numbers
		size_t pos = 0;

		while(pos + 2 <= dateTime.length()) {
			pos = std::min(
					dateTime.find("st", pos),
					std::min(
							dateTime.find("nd", pos),
							std::min(
									dateTime.find("rd", pos),
									dateTime.find("th", pos)
							)
					)
			);

			if(pos == std::string::npos)
				break;

			if(pos > 0) {
				if(::isdigit(dateTime.at(pos - 1))) {
					if(
							pos + 2 == dateTime.length()
							|| ::isspace(dateTime.at(pos + 2))
							|| ::ispunct(dateTime.at(pos + 2))
					) {
						// remove st, nd, rd or th
						dateTime.erase(pos, 2);

						// skip whitespace/punctuation afterwards
						++pos;
					}
					else
						pos += 3;
				}
				else
					pos += 3;
			}
			else
				pos += 3;
		}
		// end of ordinal hack

		std::istringstream in(dateTime);
		date::sys_seconds tp;

		try {
			in.imbue(std::locale(locale));
		}
		catch(const std::runtime_error& e) {
			throw LocaleException("Unknown locale \'" + locale + "\'");
		}

		in >> date::parse(customFormat, tp);

		if(bool(in))
			dateTime = date::format("%F %T", tp);
		else {
			// try good old C time
			struct ::tm cTime = {};

			const char * oldLocale = ::setlocale(LC_TIME, locale.c_str());

			if(!oldLocale)
				throw LocaleException("Could not set C locale \'" + locale + "\'");

			if(!::strptime(dateTime.c_str(), customFormat.c_str(), &cTime))
				throw Exception(
						"Could not convert \'"
						+ dateTime
						+ "\' [expected format: \'"
						+ customFormat
						+ "\', locale: \'"
						+ locale
						+ "\'] to date/time"
				);

			::setlocale(LC_TIME, oldLocale);

			char out[20] = { 0 };

			size_t len = ::strftime(out, 20, "%F %T", &cTime);

			if(len)
				dateTime = std::string(out, len);
			else
				throw Exception(
						"Could not convert \'"
						+ dateTime
						+ "\' [expected format: \'"
						+ customFormat
						+ "\', locale: \'"
						+ locale
						+ "\'] to date/time"
				);
		}
	}

	// convert timestamp from YYYYMMDDHHMMSS to YYYY-MM-DD HH:MM:SS
	inline void convertTimeStampToSQLTimeStamp(std::string& timeStamp) {
		convertCustomDateTimeToSQLTimeStamp(timeStamp, "%Y%m%d%H%M%S");
	}

	// convert timestamp from YYYY-MM-DD HH:MM:SS to YYYYMMDDHHMMSS
	inline void convertSQLTimeStampToTimeStamp(std::string& timeStamp) {
		// check argument
		if(timeStamp.empty())
			return;

		std::istringstream in(timeStamp);
		date::sys_seconds tp;

		in >> date::parse("%F %T", tp);

		if(!bool(in))
			throw Exception(
					"Could not convert SQL timestamp \'"
					+ timeStamp
					+ "\' to date/time"
			);

		timeStamp = date::format("%Y%m%d%H%M%S", tp);
	}

	// get the current date/time as YYYY-MM-DD HH:MM:SS
	inline std::string now() {
		return date::format("%F %T", date::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
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
		size_t days = rest / 86400000000;
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

		const size_t days = rest / 86400000;

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

		const size_t days = rest / 86400;

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
