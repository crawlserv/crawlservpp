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
 * DateTime.hpp
 *
 * Namespace for global date/time helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_DATETIME_HPP_
#define HELPER_DATETIME_HPP_

#include "Strings.hpp"

#include "../Main/Exception.hpp"

#include "../_extern/date/include/date/date.h"

#include <algorithm>	// std::min, std::transform
#include <array>		// std::array
#include <cctype>		// std::isdigit, std::ispunct, std::isspace, std::tolower
#include <chrono>		// std::chrono::seconds, std::chrono::system_clock
#include <clocale>		// std::setlocale
#include <cmath>		// std::lround
#include <cstdint>		// std::int64_t, std::uint16_t, std::uint64_t
#include <ctime>		// std::tm, std::strftime, std::time_t
#include <exception>	// std::exception
#include <iomanip>		// std::get_time
#include <locale>		// std::locale
#include <sstream>		// std::istringstream
#include <stdexcept>	// std::runtime_error
#include <string>		// std::stol, fstd::string, std::to_string
#include <string_view>	// std::string_view, std::string_view_literals

//! Namespace for global date/time helper functions.
namespace crawlservpp::Helper::DateTime {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! The 'long' format for date/times.
	inline constexpr auto longDateTime{"%a, %d %b %Y %T %Z"};

	//! The keyword to use a UNIX time format.
	inline constexpr auto unixTimeFormat{"UNIX"sv};

	//! The keyword to use a UNIX time format plus an offset.
	inline constexpr auto unixTimeFormatPlus{"UNIX+"sv};

	//! The keyword to use a UNIX time format minus an offset
	inline constexpr auto unixTimeFormatMinus{"UNIX-"sv};

	//! The length of the keyword to use a UNIX time format with offset
	inline constexpr auto unixTimeFormatXLength{5};

	//! The position of the beginning of a UNIX time format offset.
	inline constexpr auto unixTimeFormatXOffset{4};

	//! An array containing English ordinal suffixes to be stripped from numbers
	inline constexpr std::array englishOrdinalSuffixes{"st"sv, "nd"sv, "rd"sv, "th"sv};

	//! The length of English ordinal suffixes to be stripped from numbers
	inline constexpr auto englishOrdinalSuffixLength{2};

	//! The date/time format used by the MySQL database.
	inline constexpr auto sqlTimeStamp{"%F %T"};

	//! The length of a formatted time stamp in the MySQL database.
	inline constexpr auto sqlTimeStampLength{19};

	//! The prefix for French locale.
	inline constexpr auto frenchLocalePrefix{"fr"sv};

	//! The number of microseconds per day used for date/time formatting.
	inline constexpr auto microsecondsPerDay{86400000000};

	//! The number of milliseconds per day used for date/time formatting.
	inline constexpr auto millisecondsPerDay{86400000};

	//! The number of seconds per day used for date/time formatting.
	inline constexpr auto secondsPerDay{86400};

	//! The number of microseconds per hour used for date/time formatting.
	inline constexpr auto microsecondsPerHour{3600000000};

	//! The number of milliseconds per hour used for date/time formatting.
	inline constexpr auto millisecondsPerHour{3600000};

	//! The number of seconds per hour used for date/time formatting.
	inline constexpr auto secondsPerHour{3600};

	//! The number of microseconds per minute used for date/time formatting.
	inline constexpr auto microsecondsPerMinute{60000000};

	//! The number of milliseconds per minute used for date/time formatting.
	inline constexpr auto millisecondsPerMinute{60000};

	//! The number of seconds per minute used for date/time formatting.
	inline constexpr auto secondsPerMinute{60};

	//! The number of microseconds per second used for date/time formatting.
	inline constexpr auto microsecondsPerSecond{1000000};

	//! The number of milliseconds per second used for date/time formatting.
	inline constexpr auto millisecondsPerSecond{1000};

	//! The number of microseconds per millisecond used for date/time formatting.
	inline constexpr auto microsecondsPerMillisecond{1000};

	//! The length of a date in valid ISO Format (YYYY-MM-DD)
	inline constexpr auto isoDateLength{10};

	///@}

	/*
	 * DECLARATION
	 */

	///@name Conversion
	///@{

	void convertLongDateTimeToSQLTimeStamp(std::string& dateTime);
	void convertCustomDateTimeToSQLTimeStamp(
			std::string& dateTime,
			const std::string& customFormat
	);
	void convertCustomDateTimeToSQLTimeStamp(
			std::string& dateTime,
			const std::string& customFormat,
			const std::string& locale
	);
	void convertTimeStampToSQLTimeStamp(std::string& timeStamp);
	void convertSQLTimeStampToTimeStamp(std::string& timeStamp);

	///@}
	///@name Formatting
	///@{

	[[nodiscard]] std::string microsecondsToString(std::uint64_t microseconds);
	[[nodiscard]] std::string millisecondsToString(std::uint64_t milliseconds);
	[[nodiscard]] std::string secondsToString(std::uint64_t seconds);
	[[nodiscard]] std::string now();

	///@}
	///@name Verification
	///@{

	[[nodiscard]] bool isValidISODate(const std::string& isoDate);

	///@}
	///@name Comparison
	///@{

	[[nodiscard]] bool isISODateInRange(std::string_view isoDate, std::string_view rangeFrom, std::string_view rangeTo);

	///@}
	///@name Helpers
	///@{

	void removeEnglishOrdinals(std::string& inOut);
	void fixFrenchMonths(std::string_view locale, std::string& inOut);

	///@}

	/*
	 * CLASSES FOR DATE/TIME EXCEPTIONS
	 */

	//! Class for date/time exceptions.
	/*!
	 * This exception is being thrown when
	 * - a specified date/time format is invalid
	 * - a date/time conversion fails
	 */
	MAIN_EXCEPTION_CLASS();

	//! Class for date/time locale exception
	/*!
	 * This exception is being thrown when
	 *  the specified locale is invalid.
	 */
	MAIN_EXCEPTION_SUBCLASS(LocaleException);

	/*
	 * IMPLEMENTATION
	 */

	//! Converts a date/time formatted in a 'long' format into the format YYYY-MM-DD HH:MM:SS.
	/*!
	 * Calling this function with an empty input string
	 *  will have no consequences.
	 *
	 * \param dateTime String containing the date/time formatted
	 *   in a 'long' format and to be converted in situ.
	 *
	 * \throws DateTime::Exception if the conversion fails.
	 *
	 * \sa convertCustomDateTimeToSQLTimeStamp
	 */
	inline void convertLongDateTimeToSQLTimeStamp(std::string& dateTime) {
		convertCustomDateTimeToSQLTimeStamp(dateTime, longDateTime);
	}

	//! Converts date/time with a custom format into the format YYYY-MM-DD HH:MM:SS.
	/*!
	 *
	 * Calling this function with an empty input string
	 *  will have no consequences.
	 *
	 * For more information about the format string, see
	 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
	 *  Howard Hinnant's paper about his date.h library</a>.
	 *
	 * Alternatively, "UNIX", "UNIX+<offset>", or "UNIX<-offset>"
	 *  can be used to convert from a UNIX time plus/minus the given offset.
	 *
	 * \note A string view cannot be used, because the underlying
	 *   C function requires a null-terminated string.
	 *
	 * \param dateTime Reference to a string containing the date/time
	 *   formatted in the custom format and to be converted in situ.
	 *
	 * \param customFormat A const reference to the string containing
	 *   the date/time format to be converted from.
	 *
	 * \throws DateTime::Exception of the custom format is invalid
	 *   or empty, or if the date/time conversion fails.
	 */
	inline void convertCustomDateTimeToSQLTimeStamp(
			std::string& dateTime,
			const std::string& customFormat
	) {
		// check arguments
		if(dateTime.empty()) {
			return;
		}

		if(customFormat.empty()) {
			throw Exception(
					"DateTime::convertCustomDateTimeToSQLTimeStamp():"
					" No custom format specified"
			);
		}

		// check for UNIX time format
		if(
				customFormat == unixTimeFormat
				|| (
						customFormat.length() > unixTimeFormatXLength
						&& (
								customFormat.substr(0, unixTimeFormatXLength) == unixTimeFormatPlus
								|| customFormat.substr(0, unixTimeFormatXLength) == unixTimeFormatMinus
						)
					)
		) {
			// get (optional) offset from UNIX time
			std::int64_t offset{0};

			if(customFormat.length() > unixTimeFormatXLength) {
				try {
					offset = std::stol(customFormat.substr(unixTimeFormatXOffset));
				}
				catch(const std::exception& e) {
					throw Exception(
							"DateTime::convertCustomDateTimeToSQLTimeStamp(): Invalid date/time format - "
							+ customFormat
							+ " [expected: UNIX, UNIX+N or UNIX-N where N is a valid number]"
					);
				}
			}

			// get UNIX time
			std::time_t unixTime{0};

			try {
				if(dateTime.find('.') != std::string::npos) {
					// handle values with comma as floats (and round them)
					float f{std::stof(dateTime)};

					unixTime = static_cast<std::time_t>(std::lround(f));
				}
				else {
					unixTime = static_cast<std::time_t>(std::stol(dateTime));
				}
			}
			catch(const std::exception& e) {
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
			dateTime = date::format(
					sqlTimeStamp,
					std::chrono::system_clock::from_time_t(unixTime)
			);
		}
		else {
			// remove English ordinal endings (st, nd, rd, th)
			removeEnglishOrdinals(dateTime);

			std::istringstream in(dateTime);
			date::sys_seconds tp;

			in >> date::parse(customFormat, tp);

			if(bool(in)) {
				dateTime = date::format(sqlTimeStamp, tp);
			}
			else {
				// try good old C time
				struct std::tm cTime{};

				{
					std::istringstream inStringStream(dateTime);

					inStringStream.imbue(std::locale(std::setlocale(LC_ALL, nullptr)));

					inStringStream >> std::get_time(&cTime, customFormat.c_str());

					if(inStringStream.fail()) {
						throw Exception(
								"Could not convert '"
								+ dateTime
								+ "' [expected format: '"
								+ customFormat
								+ "'] to date/time "
						);
					}
				}

				std::array<char, sqlTimeStampLength + 1> out{};

				const auto len{
					std::strftime(out.data(), sqlTimeStampLength + 1, sqlTimeStamp, &cTime)
				};

				if(len > 0) {
					dateTime = std::string(out.data(), len);
				}
				else {
					throw Exception(
							"Could not convert '"
							+ dateTime
							+ "' [expected format: '"
							+ customFormat
							+ "'] to date/time "
					);
				}
			}
		}
	}

	//! Converts date/time with a custom format into the format YYYY-MM-DD HH:MM:SS.
	/*!
	 * Calling this function with an empty input string
	 *  will have no consequences.
	 *
	 * For more information about the format string, see
	 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
	 *  Howard Hinnant's paper about his date.h library</a>.
	 *
	 * \note String views cannot be used, because the underlying
	 *   functions require null-terminated strings.
	 *
	 * \param dateTime A reference to the string containing the date/time
	 *   formatted in the custom format and to be converted in situ.
	 *
	 * \param customFormat A const reference to the string containing
	 *   the date/time format to be converted from.
	 *
	 * \param locale A const reference to the string containing the
	 *   locale to be used during the conversion.
	 *
	 * \throws DateTime::Exception of the custom format is invalid
	 *   or empty, or if the date/time conversion fails.
	 */
	inline void convertCustomDateTimeToSQLTimeStamp(
			std::string& dateTime,
			const std::string& customFormat,
			const std::string& locale
	) {
		// check arguments
		if(dateTime.empty()) {
			return;
		}

		if(customFormat.empty()) {
			throw Exception(
					"DateTime::convertCustomDateTimeToSQLTimeStamp():"
					" No custom format specified"
			);
		}

		if(locale.empty()) {
			convertCustomDateTimeToSQLTimeStamp(dateTime, customFormat);

			return;
		}

		// fix French months ("avr." -> "avril")
		fixFrenchMonths(locale, dateTime);

		removeEnglishOrdinals(dateTime);

		std::istringstream in(dateTime);
		date::sys_seconds tp;

		try {
			in.imbue(std::locale(locale));
		}
		catch(const std::runtime_error& e) {
			throw LocaleException("Unknown locale \'" + locale + "\'");
		}

		in >> date::parse(customFormat, tp);

		if(bool(in)) {
			dateTime = date::format("%F %T", tp);
		}
		else {
			// try good old C time
			struct ::tm cTime{};

			{
				std::istringstream inStringStream(dateTime);

				inStringStream.imbue(std::locale(std::setlocale(LC_TIME, locale.c_str())));

				inStringStream >> std::get_time(&cTime, customFormat.c_str());

				if(inStringStream.fail()) {
					throw Exception(
							"Could not convert '"
							+ dateTime
							+ "' [expected format: '"
							+ customFormat
							+ "'] to date/time "
					);
				}
			}

			std::array<char, sqlTimeStampLength + 1> out{};

			const auto len{
				std::strftime(out.data(), sqlTimeStampLength + 1, "%F %T", &cTime)
			};

			if(len > 0) {
				dateTime = std::string(out.data(), len);
			}
			else {
				throw Exception(
						"Could not convert '"
						+ dateTime
						+ "' [expected format: '"
						+ customFormat
						+ "', locale: '"
						+ locale
						+ "'] to date/time"
				);
			}
		}
	}

	//! Converts a timestamp in YYYYMMDDHHMMSS format to a MySQL timestamp in YYYY-MM-DD HH:MM:SS format.
	/*!
	 * Calling this function with an empty input string
	 *  will have no consequences.
	 *
	 * \param timeStamp A reference to the string containing a timestamp
	 *  in the YYYYMMDDHHMMSS format to be converted in situ.
	 *
	 * \throws DateTime::Exception if the conversion fails.
	 *
	 * \sa convertCustomDateTimeToSQLTimeStamp
	 */
	inline void convertTimeStampToSQLTimeStamp(std::string& timeStamp) {
		convertCustomDateTimeToSQLTimeStamp(timeStamp, "%Y%m%d%H%M%S");
	}

	//! Converts a MySQL timestamp in YYYY-MM-DD HH:MM:SS format to a timestamp in YYYYMMDDHHMMSS format.
	/*!
	 * Calling this function with an empty input string
	 *  will have no consequences.
	 *
	 * \param timeStamp A reference to the string containing a
	 *   MySQL timestamp in the YYYY-MM-DD HH:MM:SS format to be
	 *   converted in situ.
	 *
	 * \throws DateTime::Exception if the conversion fails.
	 *
	 * \sa convertTimeStampToSQLTimeStamp
	 */
	inline void convertSQLTimeStampToTimeStamp(std::string& timeStamp) {
		// check argument
		if(timeStamp.empty()) {
			return;
		}

		std::istringstream in(timeStamp);
		date::sys_seconds tp;

		in >> date::parse("%F %T", tp);

		if(!bool(in)) {
			throw Exception(
					"Could not convert SQL timestamp \'"
					+ timeStamp
					+ "\' to date/time"
			);
		}

		timeStamp = date::format("%Y%m%d%H%M%S", tp);
	}

	//! Converts microseconds into a well-formatted string.
	/*!
	 * \param microseconds The number of microseconds to convert.
	 *
	 * \returns A copy of the string containing the well-formatted output.
	 *
	 * \sa millisecondsToString, secondsToString
	 */
	inline std::string microsecondsToString(std::uint64_t microseconds) {
		auto rest{microseconds};
		std::string result;

		const auto days{
			rest / microsecondsPerDay
		};

		rest -= days * microsecondsPerDay;

		// narrowing cast as hours are never more than hours per day
		const auto hours{static_cast<std::uint8_t>(rest / microsecondsPerHour)};

		rest -= hours * microsecondsPerHour;

		// narrowing cast as minutes are never more than minutes per hour
		const auto minutes{static_cast<std::uint8_t>(rest / microsecondsPerMinute)};

		rest -= minutes * microsecondsPerMinute;

		// narrowing cast as seconds are never more than seconds per minute
		const auto seconds{static_cast<std::uint8_t>(rest / microsecondsPerSecond)};

		rest -= seconds * microsecondsPerSecond;

		// narrowing cast as milliseconds are never more than milliseconds per second
		const auto milliseconds{static_cast<std::uint16_t>(rest / microsecondsPerMillisecond)};

		rest -= milliseconds * microsecondsPerMillisecond;

		if(days > 0) {
			result += std::to_string(days) + "d ";
		}

		if(hours > 0) {
			result += std::to_string(hours) + "h ";
		}

		if(minutes > 0) {
			result += std::to_string(minutes) + "min ";
		}

		if(seconds > 0) {
			result += std::to_string(seconds) + "s ";
		}

		if(milliseconds > 0) {
			result += std::to_string(milliseconds) + "ms ";
		}

		if(rest > 0) {
			result += std::to_string(rest) + "μs ";
		}

		if(result.empty()) {
			return "<1μs";
		}

		// remove last space
		result.pop_back();

		return result;
	}

	//! Converts milliseconds into a well-formatted string.
	/*!
	 * \param milliseconds The number of milliseconds to convert.
	 *
	 * \returns A copy of the string containing the
	 *   well-formatted output.
	 *
	 * \sa microsecondsToString, secondsToString
	 */
	inline std::string millisecondsToString(std::uint64_t milliseconds) {
		auto rest{milliseconds};
		std::string result;

		const auto days{rest / millisecondsPerDay};

		rest -= days * millisecondsPerDay;

		// narrowing cast as hours will never be more than hours per day
		const auto hours{static_cast<std::uint8_t>(rest / millisecondsPerHour)};

		rest -= hours * millisecondsPerHour;

		// narrowing cast as minutes will never be more than minutes per hour
		const auto minutes{static_cast<std::uint8_t>(rest / millisecondsPerMinute)};

		rest -= minutes * millisecondsPerMinute;

		// narrowing cast as seconds will never be more than seconds per minute
		const auto seconds{static_cast<std::uint8_t>(rest / millisecondsPerSecond)};

		rest -= seconds * millisecondsPerSecond;

		if(days > 0) {
			result += std::to_string(days) + "d ";
		}

		if(hours > 0) {
			result += std::to_string(hours) + "h ";
		}

		if(minutes > 0) {
			result += std::to_string(minutes) + "min ";
		}

		if(seconds > 0) {
			result += std::to_string(seconds) + "s ";
		}

		if(rest > 0) {
			result += std::to_string(rest) + "ms ";
		}

		if(result.empty()) {
			return "<1ms";
		}

		// remove last space
		result.pop_back();

		return result;
	}

	//! Converts seconds into a well-formatted string.
	/*!
	 * \param seconds The number of seconds to convert.
	 *
	 * \returns A copy of the string containing the
	 *   well-formatted output.
	 *
	 * \sa microsecondsToString, millisecondsToString
	 */
	inline std::string secondsToString(std::uint64_t seconds) {
		auto rest{seconds};
		std::string result;

		const auto days{rest / secondsPerDay};

		rest -= days * secondsPerDay;

		// narrowing cast as hours are never more than hours per day
		const auto hours{static_cast<std::uint8_t>(rest / secondsPerHour)};

		rest -= hours * secondsPerHour;

		// narrowing cast as minutes are never more than minutes per hour
		const auto minutes{static_cast<std::uint8_t>(rest / secondsPerMinute)};

		rest -= minutes * secondsPerMinute;

		if(days > 0) {
			result += std::to_string(days) + "d ";
		}

		if(hours > 0) {
			result += std::to_string(hours) + "h ";
		}

		if(minutes > 0) {
			result += std::to_string(minutes) + "min ";
		}

		if(rest > 0) {
			result += std::to_string(rest) + "s ";
		}

		if(result.empty()) {
			return "<1s";
		}

		// remove last space
		result.pop_back();

		return result;
	}

	//! Formats the current date/time as string in the format YYYY-MM-DD HH:MM:SS
	/*!
	 * \returns A copy of the string containing the current date/time
	 *   of the system as string in the format YYYY-MM-DD HH:MM:SS.
	 */
	inline std::string now() {
		return date::format(
				"%F %T",
				date::floor<std::chrono::seconds>(
						std::chrono::system_clock::now()
				)
		);
	}

	//! Checks whether a string contains a valid date in ISO format
	/*!
	 * \note A string view is not being used, because the underlying
	 *   string stream requires a copy of the string.
	 *
	 * \param isoDate A string containing the date to check.
	 *
	 * \returns True if the given string contains a valid date
	 *   in ISO format, i.e. a date in the format YYY-MM-DD.
	 *   False otherwise.
	 */
	inline bool isValidISODate(const std::string& isoDate) {
		std::istringstream in(isoDate);
		date::sys_days tp;

		in >> date::parse("%F", tp);

		return bool(in);
	}

	//! Checks whether the given ISO date is in the given range of dates.
	/*!
	 *
	 * \note Only the first ten characters of the given dates will be
	 *    considered.
	 *
	 * \param isoDate A string view containing the date in valid ISO format,
	 *   i.e. a date in the format YYY-MM-DD.
	 * \param rangeFrom A string view containing the start date of the range
	 *   in valid ISO format, i.e. a date in the format YYY-MM-DD.
	 * \param rangeTo A string view containing the end date of the range
	 *   in valid ISO format, i.e. a date in the format YYY-MM-DD.
	 *
	 * \returns True, if the given date falls into the given range of dates
	 *   or one of the two dates defining this range are too short.
	 *   False, if the given date does not fall into the given range of
	 *   dates, or the given date is too short.
	 */
	inline bool isISODateInRange(
			std::string_view isoDate,
			std::string_view rangeFrom,
			std::string_view rangeTo
	) {
		if(isoDate.length() < isoDateLength) {
			return false;
		}

		if(rangeFrom.length() < isoDateLength && rangeTo.length() < isoDateLength) {
			return true;
		}

		if(rangeTo.length() < isoDateLength) {
			return isoDate.substr(0, isoDateLength) >= rangeFrom.substr(0, isoDateLength);
		}

		if(rangeFrom.length() < isoDateLength) {
			return isoDate.substr(0, isoDateLength) <= rangeTo.substr(0, isoDateLength);
		}

		return isoDate.substr(0, isoDateLength) >= rangeFrom.substr(0, isoDateLength)
				&& isoDate <= rangeTo.substr(0, isoDateLength);
	}

	//! Removes all English ordinal suffixes after numbers in the given string.
	/*!
	 * \param strInOut A reference to the string containing the date/time,
	 *   from which the English ordinal suffixes will be removed in situ.
	 */
	inline void removeEnglishOrdinals(std::string& strInOut) {
		std::size_t pos{0};

		while(pos + englishOrdinalSuffixLength <= strInOut.length()) {
			std::size_t next{std::string::npos};

			for(const auto& suffix : englishOrdinalSuffixes) {
				const auto search{strInOut.find(suffix, pos)};

				if(search < next) {
					next = search;
				}
			}

			pos = next;

			if(pos == std::string::npos) {
				break;
			}

			if(pos > 0) {
				if(std::isdigit(strInOut.at(pos - 1)) != 0) {
					const auto end{pos + englishOrdinalSuffixLength};

					if(
							end == strInOut.length()
							|| std::isspace(strInOut.at(end)) != 0
							|| std::ispunct(strInOut.at(end)) != 0
					) {
						// remove st, nd, rd or th
						strInOut.erase(pos, englishOrdinalSuffixLength);

						// skip whitespace/punctuation afterwards
						++pos;
					}
					else {
						pos += englishOrdinalSuffixLength + 1;
					}
				}
				else {
					pos += englishOrdinalSuffixLength + 1;
				}
			}
			else {
				pos += englishOrdinalSuffixLength + 1;
			}
		}
	}

	//! Replaces the abbreviation "avr." for the month of april (avril) in the given string, if the locale is French.
	/*!
	 * If the given locale is not French, the function call will
	 *  be without consequences.
	 *
	 * \param locale A string view containing the locale to be checked for French.
	 * \param strInOut A reference to the string containing the date/time,
	 *   in which the abbreviation will be replaced if the given locale is French.
	 */
	inline void fixFrenchMonths(std::string_view locale, std::string& strInOut) {
		if(locale.length() >= frenchLocalePrefix.length()) {
			std::string prefix(locale, 0, frenchLocalePrefix.length());

			std::transform(
					prefix.begin(),
					prefix.end(),
					prefix.begin(),
					[](const auto c) {
						return std::tolower(c);
					}
			);

			if(prefix == frenchLocalePrefix) {
				Helper::Strings::replaceAll(strInOut, "avr.", "avril");
			}
		}
	}

} /* namespace crawlservpp::Helper::DateTime */

#endif /* HELPER_DATETIME_HPP_ */
