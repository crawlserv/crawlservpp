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
#include <cstdint>		// std::uint8_t, std::int64_t, std::uint16_t, std::uint64_t
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

	//! The keyword to use a UNIX time format minus an offset.
	inline constexpr auto unixTimeFormatMinus{"UNIX-"sv};

	//! The length of the keyword to use a UNIX time format with offset.
	inline constexpr auto unixTimeFormatXLength{5};

	//! The position of the beginning of a UNIX time format offset.
	inline constexpr auto unixTimeFormatXOffset{4};

	//! An array containing English ordinal suffixes to be stripped from numbers.
	inline constexpr std::array englishOrdinalSuffixes{"st"sv, "nd"sv, "rd"sv, "th"sv};

	//! An array containing French ordinal suffix to be stripped from numbers.
	inline constexpr std::array frenchOrdinalSuffixes{"e"sv, "er"sv};

	//! An array containing Russian ordinal suffixes to be stripped from numbers.
	inline constexpr std::array russianOrdinalSuffixes{
		"-ый"sv, "-го"sv, "-му"sv, "-ми"sv, "-й"sv, "-я"sv, "-е"sv, "-м"sv, "-х"sv
	};

	//! An array containing Ukrainian ordinal suffixes to be stripped from numbers.
	inline constexpr std::array ukrainianOrdinalSuffixes{"-а"sv, "-е"sv, "-і"sv, "-я"sv, "-є"sv};

	//! The date/time format used by the MySQL database (as @c C string).
	inline constexpr auto sqlTimeStamp{"%F %T"};

	//! The length of a formatted time stamp in the MySQL database.
	inline constexpr auto sqlTimeStampLength{19};

	//! The prefix for English locales.
	inline constexpr auto englishLocalePrefix{"en"sv};

	//! The prefix for French locales.
	inline constexpr auto frenchLocalePrefix{"fr"sv};

	//! The prefix for Russian locales.
	inline constexpr auto russianLocalePrefix{"ru"sv};

	//! The prefix for Ukrainian locales.
	inline constexpr auto ukrainianLocalePrefix{"uk"sv};

	//! The length of the 12-h suffix (AM/PM).
	inline constexpr auto amPmLength{2};

	//! The number of hours to be added to a PM time, or to be subtracted from a 12th hour AM time.
	inline constexpr auto hourChange{12};

	//! The hour of noon and midnight.
	inline constexpr auto hourNoonMidnight{12};

	//! The two digits from which two-digit years will be interpreted as years after 2000.
	inline constexpr auto centuryFrom{69};

	//! The number of years in a century.
	inline constexpr auto yearsPerCentury{100};

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

	//! The length of a date in valid ISO Format (@c YYYY-MM-DD).
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
	void convert12hTo24h(int& hour, bool isPm);

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

	template<std::size_t N>
	void removeOrdinals(
			const std::array<std::string_view, N>& suffixes,
			std::string& strInOut
	);
	template<std::size_t N>
	void removeOrdinals(
			std::string_view currentLocale,
			std::string_view localePrefix,
			const std::array<std::string_view, N>& suffixes,
			std::string& strInOut
	);
	void fixFrenchMonths(std::string_view locale, std::string& strInOut);
	void fixRussianMonths(std::string_view locale, std::string& strInOut, std::string& formatInOut);
	void fixUkrainianMonths(std::string_view locale, std::string& strInOut, std::string& formatInOut);
	void extendSingleDigits(std::string& dateTimeString);
	void fixYear(std::string_view format, int& year);
	void handle12hTime(
			std::string& formatString,
			const std::string& dateTimeString,
			bool& outIsAm,
			bool& outIsPm
	);

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

	//! Converts a date/time formatted in a “long” format into the format @c YYYY-MM-DD HH:MM:SS.
	/*!
	 * Calling this function with an empty input
	 *  string will have no consequences.
	 *
	 * \param dateTime String containing the
	 *   date/time formatted in a “long” format and
	 *   to be converted in situ.
	 *
	 * \throws DateTime::Exception if the conversion
	 *   fails.
	 *
	 * \sa convertCustomDateTimeToSQLTimeStamp
	 */
	inline void convertLongDateTimeToSQLTimeStamp(std::string& dateTime) {
		convertCustomDateTimeToSQLTimeStamp(dateTime, longDateTime);
	}

	//! Converts date/time with a custom format into the format @c YYYY-MM-DD HH:MM:SS.
	/*!
	 *
	 * Calling this function with an empty input
	 *  string will have no consequences.
	 *
	 * For more information about the format string,
	 *  see
	 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
	 *  Howard Hinnant's paper about his date.h
	 *  library</a>.
	 *
	 * Alternatively, @c UNIX, @c UNIX+&lt;offset&gt;, or
	 *  @c UNIX&lt;-offset&gt; can be used to convert from
	 *  a UNIX time plus/minus the given offset.
	 *
	 * \note A string view cannot be used, because
	 *   the underlying @c C function requires a
	 *   null-terminated string.
	 *
	 * \param dateTime Reference to a string
	 *   containing the date/time formatted in the
	 *   custom format and to be converted in situ.
	 *
	 * \param customFormat A const reference to the
	 *   string containing the date/time format to be
	 *   converted from.
	 *
	 * \throws DateTime::Exception of the custom
	 *   format is invalid or empty, or if the
	 *   date/time conversion fails.
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
						+ "\'] to date/time"
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
			removeOrdinals(englishOrdinalSuffixes, dateTime);

			std::istringstream in(dateTime);
			date::sys_seconds tp;

			in >> date::parse(customFormat, tp);

			if(bool(in)) {
				dateTime = date::format(sqlTimeStamp, tp);
			}
			else {
				// try good old C time
				std::string formatString{customFormat};
				bool isAm{false};
				bool isPm{false};
				std::tm cTime{};

				extendSingleDigits(dateTime);
				handle12hTime(formatString, dateTime, isAm, isPm);

				std::istringstream inStringStream(dateTime);

				inStringStream.imbue(std::locale(std::setlocale(LC_ALL, nullptr)));

				inStringStream >> std::get_time(&cTime, formatString.c_str());

				if(inStringStream.fail()) {
					throw Exception(
							"Could not convert '"
							+ dateTime
							+ "' [expected format: '"
							+ formatString
							+ "'] to date/time"
					);
				}

				if(isAm || isPm) {
					convert12hTo24h(cTime.tm_hour, isPm);
				}

				fixYear(formatString, cTime.tm_year);

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
							+ "'] to date/time"
					);
				}
			}
		}
	}

	//! Converts date/time with a custom format into the format @c YYYY-MM-DD HH:MM:SS.
	/*!
	 * Calling this function with an empty input
	 *  string will have no consequences.
	 *
	 * For more information about the format string,
	 *  see
	 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
	 *  Howard Hinnant's paper about his date.h
	 *  library</a>.
	 *
	 * \note String views cannot be used, because
	 *   the underlying functions require
	 *   null-terminated strings.
	 *
	 * \param dateTime A reference to the string
	 *   containing the date/time formatted in the
	 *   custom format and to be converted in situ.
	 *
	 * \param customFormat A const reference to the
	 *   string containing the date/time format to
	 *   be converted from.
	 *
	 * \param locale A const reference to the string
	 *   containing the locale to be used during the
	 *   conversion.
	 *
	 * \throws DateTime::Exception of the custom
	 *   format is invalid or empty, or if the
	 *   date/time conversion fails.
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

		// fix Russian and Ukrainian months
		std::string formatString{customFormat};
		fixRussianMonths(locale, dateTime, formatString);
		fixUkrainianMonths(locale, dateTime, formatString);

		// remove ordinals
		removeOrdinals(englishOrdinalSuffixes, dateTime);
		removeOrdinals(locale, frenchLocalePrefix, frenchOrdinalSuffixes, dateTime);
		removeOrdinals(locale, russianLocalePrefix, russianOrdinalSuffixes, dateTime);
		removeOrdinals(locale, ukrainianLocalePrefix, ukrainianOrdinalSuffixes, dateTime);

		std::istringstream in(dateTime);
		date::sys_seconds tp;

		try {
			in.imbue(std::locale(locale));
		}
		catch(const std::runtime_error& e) {
			throw LocaleException("Unknown locale \'" + locale + "\'");
		}

		in >> date::parse(formatString, tp);

		if(bool(in)) {
			dateTime = date::format(sqlTimeStamp, tp);
		}
		else {
			// try good old C time
			bool isAm{false};
			bool isPm{false};
			std::tm cTime{};

			extendSingleDigits(dateTime);
			handle12hTime(formatString, dateTime, isAm, isPm);

			std::istringstream inStringStream(dateTime);

			inStringStream.imbue(std::locale(locale));

			inStringStream >> std::get_time(&cTime, formatString.c_str());

			if(inStringStream.fail()) {
				throw Exception(
						"Could not convert '"
						+ dateTime
						+ "' [expected format: '"
						+ formatString
						+ "'] to date/time"
				);
			}

			if(isAm || isPm) {
				convert12hTo24h(cTime.tm_hour, isPm);
			}

			fixYear(formatString, cTime.tm_year);

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
						+ formatString
						+ "', locale: '"
						+ locale
						+ "'] to date/time"
				);
			}
		}
	}

	//! Converts a timestamp in the @c YYYYMMDDHHMMSS format to a MySQL timestamp in the @c YYYY-MM-DD HH:MM:SS format.
	/*!
	 * Calling this function with an empty input
	 *  string will have no consequences.
	 *
	 * \param timeStamp A reference to the string
	 *   containing a timestamp in the @c
	 *   YYYYMMDDHHMMSS format to be converted in
	 *   situ.
	 *
	 * \throws DateTime::Exception if the conversion
	 *   fails.
	 *
	 * \sa convertCustomDateTimeToSQLTimeStamp
	 */
	inline void convertTimeStampToSQLTimeStamp(std::string& timeStamp) {
		convertCustomDateTimeToSQLTimeStamp(timeStamp, "%Y%m%d%H%M%S");
	}

	//! Converts a MySQL timestamp in the @c YYYY-MM-DD HH:MM:SS format to a timestamp in the @c YYYYMMDDHHMMSS format.
	/*!
	 * Calling this function with an empty input
	 *  string will have no consequences.
	 *
	 * \param timeStamp A reference to the string
	 *   containing a MySQL timestamp in the @c
	 *   YYYY-MM-DD HH:MM:SS format to be converted in
	 *   situ.
	 *
	 * \throws DateTime::Exception if the conversion
	 *   fails.
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

		in >> date::parse(sqlTimeStamp, tp);

		if(!bool(in)) {
			throw Exception(
					"Could not convert SQL timestamp \'"
					+ timeStamp
					+ "\' to date/time"
			);
		}

		timeStamp = date::format("%Y%m%d%H%M%S", tp);
	}

	//! Converts an hour from the 12h to the 24h system.
	/*!
	 * \param hour Reference to the hour which will be
	 *   changed accordingly, if necessary.
	 * \param isPm Indicates whether the given hour is
	 *   @c PM. If false, it will be interpreted as @c
	 *   AM.
	 */
	inline void convert12hTo24h(
			int& hour,
			bool isPm
	) {
		if(isPm) {
			if(hour < hourNoonMidnight) { /* not 12 PM [noon] */
				hour += hourChange;
			}
		}
		else if(hour == hourNoonMidnight) { /* at 12 AM [midnight] */
			hour = 0;
		}
	}

	//! Converts microseconds into a well-formatted string.
	/*!
	 * \param microseconds The number of microseconds
	 *   to convert.
	 *
	 * \returns A copy of the string containing the
	 *   well-formatted output.
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
	 * \param milliseconds The number of milliseconds
	 *   to convert.
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

	//! Formats the current date/time as string in the format @c YYYY-MM-DD HH:MM:SS
	/*!
	 * \returns A copy of the string containing the
	 *   current date/time of the system as string in
	 *   the format @c YYYY-MM-DD HH:MM:SS.
	 */
	inline std::string now() {
		return date::format(
				sqlTimeStamp,
				date::floor<std::chrono::seconds>(
						std::chrono::system_clock::now()
				)
		);
	}

	//! Checks whether a string contains a valid date in the ISO format.
	/*!
	 * \note A string view is not being used, because
	 *   the underlying string stream requires a copy
	 *   of the string.
	 *
	 * \param isoDate A string containing the date to
	 *   check.
	 *
	 * \returns True if the given string contains a
	 *   valid date in ISO format, i.e. a date in the
	 *   format @c YYY-MM-DD. False otherwise.
	 */
	inline bool isValidISODate(const std::string& isoDate) {
		std::istringstream in(isoDate);
		date::sys_days tp;

		in >> date::parse("%F", tp);

		return bool(in);
	}

	//! Checks whether the given ISO date is in the given range of dates.
	/*!
	 * \note Only the first ten characters of the
	 *    given dates will be considered.
	 *
	 * \param isoDate A string view containing the date
	 *   in valid ISO format, i.e. a date in the format
	 *   @c YYY-MM-DD.
	 * \param rangeFrom A string view containing the
	 *   start date of the range in valid ISO format,
	 *   i.e. a date in the format @c YYY-MM-DD.
	 * \param rangeTo A string view containing the end
	 *   date of the range in valid ISO format, i.e. a
	 *   date in the format @c YYY-MM-DD.
	 *
	 * \returns True, if the given date falls into the
	 *   given range of dates or one of the two dates
	 *   defining this range are too short. False, if
	 *   the given date does not fall into the given
	 *   range of dates, or the given date is too short.
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

	//! Removes all ordinal suffixes after numbers in the given string.
	/*!
	 * \param suffixes Constant reference to an array
	 *   containing the suffixes to be removed from
	 *   the end of numbers in the given string.
	 * \param strInOut A reference to the string
	 *   containing the date/time, from which the
	 *   ordinal suffixes will be removed in situ.
	 */
	template<std::size_t N> void removeOrdinals(
			const std::array<std::string_view, N>& suffixes,
			std::string& strInOut
	) {
		std::size_t pos{0};

		while(pos < strInOut.length()) {
			auto next{std::string::npos};
			std::size_t len{0};

			for(const auto& suffix : suffixes) {
				const auto search{strInOut.find(suffix, pos)};

				if(search < next) {
					next = search;
					len = suffix.length();
				}
			}

			pos = next;

			if(pos == std::string::npos) {
				break;
			}

			if(pos > 0) {
				if(std::isdigit(strInOut.at(pos - 1)) != 0) {
					const auto end{pos + len};

					if(
							end == strInOut.length()
							|| std::isspace(strInOut.at(end)) != 0
							|| std::ispunct(strInOut.at(end)) != 0
					) {
						// remove st, nd, rd or th
						strInOut.erase(pos, len);

						// skip whitespace/punctuation afterwards
						++pos;
					}
					else {
						pos += len + 1;
					}
				}
				else {
					pos += len + 1;
				}
			}
			else {
				pos += len + 1;
			}
		}
	}

	//! Removes all ordinal suffixes after numbers in the given string, if the current locale matches the given locale.
	/*!
	 * \param currentLocale View of a string
	 *   containing the current locale.
	 * \param localePrefix View of a string containing
	 *   the language prefix of the locale of the
	 *   given suffixes.
	 * \param suffixes Constant reference to an array
	 *   containing the suffixes to be removed from
	 *   the end of numbers in the given string, if
	 *   the current locale fits the given locale
	 *   prefix.
	 * \param strInOut A reference to the string
	 *   containing the date/time, from which the
	 *   ordinal suffixes will be removed in situ, if
	 *   the current locale starts with the given
	 *   locale prefix.
	 */
	template<std::size_t N> void removeOrdinals(
			std::string_view currentLocale,
			std::string_view localePrefix,
			const std::array<std::string_view, N>& suffixes,
			std::string& strInOut
	) {
		if(currentLocale.length() >= localePrefix.length()) {
			std::string prefix(currentLocale, 0, localePrefix.length());

			std::transform(
					prefix.begin(),
					prefix.end(),
					prefix.begin(),
					[](const auto c) {
						return std::tolower(c);
					}
			);

			if(prefix == localePrefix) {
				removeOrdinals<N>(suffixes, strInOut);
			}
		}
	}

	//! Replaces the abbreviation @c avr. for the month of april (avril) in the given string, if the locale is French.
	/*!
	 * If the given locale is not French, the function
	 *  call will be without consequences.
	 *
	 * \param locale A string view containing the
	 *   locale to be checked for French.
	 * \param strInOut A reference to the string
	 *   containing the date/time, in which the
	 *   abbreviation will be replaced if the given
	 *   locale is French.
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

	//! Shortens Russian month names and replaces the abbreviations @c май and @c сент, if the locale is Russian.
	/*!
	 * If the given locale is English, and the format
	 *  contains @c %b or @c %B, the function will
	 *  replace the Russified month names @c maj,
	 *  @c Maj and @c MAJ with the English @c May.
	 *
	 * Otherwise, the function call will be without
	 *  consequences.
	 *
	 * \param locale A string view containing the
	 *   locale to be checked for Russian or English.
	 * \param strInOut A reference to the string
	 *   containing the date/time, in which the months
	 *   will be replaced, if the given locale is
	 *   Russian or English.
	 * \param formatInOut A reference to the string
	 *   containing the formatting string that will be
	 *   changed accordingly, if necessary.
	 */
	inline void fixRussianMonths(std::string_view locale, std::string& strInOut, std::string& formatInOut) {
		if(locale.length() >= russianLocalePrefix.length()) {
			std::string prefix(locale, 0, russianLocalePrefix.length());

			std::transform(
					prefix.begin(),
					prefix.end(),
					prefix.begin(),
					[](const auto c) {
						return std::tolower(c);
					}
			);

			if(prefix == russianLocalePrefix) {
				std::string oldString;

				const bool bigB{
					formatInOut.find("%B") != std::string::npos
				};

				if(bigB) {
					oldString = strInOut;
				}

				Helper::Strings::replaceAll(strInOut, "январь", "янв");
				Helper::Strings::replaceAll(strInOut, "Январь", "янв");
				Helper::Strings::replaceAll(strInOut, "ЯНВАРЬ", "янв");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Янв", "янв");
					Helper::Strings::replaceAll(strInOut, "ЯНВ", "янв");
				}

				Helper::Strings::replaceAll(strInOut, "февраль", "фев");
				Helper::Strings::replaceAll(strInOut, "Февраль", "фев");
				Helper::Strings::replaceAll(strInOut, "ФЕВРАЛЬ", "фев");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Фев", "фев");
					Helper::Strings::replaceAll(strInOut, "ФЕВ", "фев");
				}

				Helper::Strings::replaceAll(strInOut, "марта", "мар");
				Helper::Strings::replaceAll(strInOut, "Марта", "мар");
				Helper::Strings::replaceAll(strInOut, "МАРТА", "мар");
				Helper::Strings::replaceAll(strInOut, "март", "мар");
				Helper::Strings::replaceAll(strInOut, "Март", "мар");
				Helper::Strings::replaceAll(strInOut, "МАРТ", "мар");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Мар", "мар ");
					Helper::Strings::replaceAll(strInOut, "МАР", "мар ");
				}

				Helper::Strings::replaceAll(strInOut, "апрель", "апр");
				Helper::Strings::replaceAll(strInOut, "Апрель", "апр");
				Helper::Strings::replaceAll(strInOut, "АПРЕЛЬ", "апр");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Апр", "апр");
					Helper::Strings::replaceAll(strInOut, "АПР", "апр");
				}

				Helper::Strings::replaceAll(strInOut, "май", "мая");
				Helper::Strings::replaceAll(strInOut, "Май", "мая");
				Helper::Strings::replaceAll(strInOut, "МАЙ", "мая");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Мая", "мая");
					Helper::Strings::replaceAll(strInOut, "МАЯ", "мая");
				}

				Helper::Strings::replaceAll(strInOut, "июнь", "июн");
				Helper::Strings::replaceAll(strInOut, "Июнь", "июн");
				Helper::Strings::replaceAll(strInOut, "ИЮНь", "июн");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Июн", "июн");
					Helper::Strings::replaceAll(strInOut, "ИЮН", "июн");
				}

				Helper::Strings::replaceAll(strInOut, "июль", "июл");
				Helper::Strings::replaceAll(strInOut, "Июль", "июл");
				Helper::Strings::replaceAll(strInOut, "ИЮЛь", "июл");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Июл", "июл");
					Helper::Strings::replaceAll(strInOut, "ИЮЛ", "июл");
				}

				Helper::Strings::replaceAll(strInOut, "августа", "авг");
				Helper::Strings::replaceAll(strInOut, "Августа", "авг");
				Helper::Strings::replaceAll(strInOut, "АВГУСТА", "авг");
				Helper::Strings::replaceAll(strInOut, "август", "авг");
				Helper::Strings::replaceAll(strInOut, "Август", "авг");
				Helper::Strings::replaceAll(strInOut, "АВГУСТ", "авг");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Авг", "авг");
					Helper::Strings::replaceAll(strInOut, "АВГ", "авг");
				}

				Helper::Strings::replaceAll(strInOut, "сентябрь", "сен");
				Helper::Strings::replaceAll(strInOut, "Сентябрь", "сен");
				Helper::Strings::replaceAll(strInOut, "СЕНТЯБРЬ", "сен");
				Helper::Strings::replaceAll(strInOut, "сентября", "сен");
				Helper::Strings::replaceAll(strInOut, "Сентября", "сен");
				Helper::Strings::replaceAll(strInOut, "СЕНТЯБРя", "сен");
				Helper::Strings::replaceAll(strInOut, "Сен", "сен");
				Helper::Strings::replaceAll(strInOut, "СЕН", "сен");
				Helper::Strings::replaceAll(strInOut, "сент", "сен");
				Helper::Strings::replaceAll(strInOut, "сенТ", "сен");

				Helper::Strings::replaceAll(strInOut, "октябрь", "окт");
				Helper::Strings::replaceAll(strInOut, "Октябрь", "окт");
				Helper::Strings::replaceAll(strInOut, "ОКТЯБРЬ", "окт");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Окт", "окт");
					Helper::Strings::replaceAll(strInOut, "ОКТ", "окт");
				}

				Helper::Strings::replaceAll(strInOut, "ноябрь", "ноя");
				Helper::Strings::replaceAll(strInOut, "Ноябрь", "ноя");
				Helper::Strings::replaceAll(strInOut, "НОЯБРЬ", "ноя");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Ноя", "ноя");
					Helper::Strings::replaceAll(strInOut, "НОЯ", "ноя");
				}

				Helper::Strings::replaceAll(strInOut, "декабрь", "дек");
				Helper::Strings::replaceAll(strInOut, "Декабрь", "дек");
				Helper::Strings::replaceAll(strInOut, "ДЕКАБРЬ", "дек");

				if(!bigB) {
					Helper::Strings::replaceAll(strInOut, "Дек", "дек");
					Helper::Strings::replaceAll(strInOut, "ДЕК", "дек");
				}

				if(bigB && strInOut != oldString) {
					Helper::Strings::replaceAll(formatInOut, "%B", "%b");
				}

				return;
			}
		}

		// if the locale is English, replace the Russified "maj"/"Maj"/"MAJ" with English "May"
		if(
				formatInOut.find("%b") != std::string::npos
				|| formatInOut.find("%B") != std::string::npos
		) {
			if(locale.length() >= englishLocalePrefix.length()) {
				std::string prefix(locale, 0, russianLocalePrefix.length());

				std::transform(
						prefix.begin(),
						prefix.end(),
						prefix.begin(),
						[](const auto c) {
							return std::tolower(c);
						}
				);

				if(prefix == englishLocalePrefix) {
					Helper::Strings::replaceAll(strInOut, "maj", "May");
					Helper::Strings::replaceAll(strInOut, "Maj", "May");
					Helper::Strings::replaceAll(strInOut, "MAJ", "May");
				}
			}
		}
	}

	//! Shortens Ukrainian month names, if the locale is Ukrainian.
	/*!
	 * If the given locale is not Ukrainian, the function
	 *  call will be without consequences.
	 *
	 * \param locale A string view containing the locale to
	 *   be checked for Ukrainian.
	 * \param strInOut A reference to the string containing
	 *   the date/time, in which the months will be
		  replaced, if the given locale is Ukrainian.
	 * \param formatInOut A reference to the string
	 *   containing the formatting string that will be
	 *   changed accordingly, if necessary.
	 */
	inline void fixUkrainianMonths(std::string_view locale, std::string& strInOut, std::string& formatInOut) {
		if(locale.length() >= ukrainianLocalePrefix.length()) {
			std::string prefix(locale, 0, ukrainianLocalePrefix.length());

			std::transform(
					prefix.begin(),
					prefix.end(),
					prefix.begin(),
					[](const auto c) {
						return std::tolower(c);
					}
			);

			if(prefix == ukrainianLocalePrefix) {
				std::string oldString{strInOut};

				Helper::Strings::replaceAll(strInOut, "січень", "січ");
				Helper::Strings::replaceAll(strInOut, "Січень", "січ");
				Helper::Strings::replaceAll(strInOut, "СІЧЕНЬ", "січ");

				Helper::Strings::replaceAll(strInOut, "стд", "січ");
				Helper::Strings::replaceAll(strInOut, "Стд", "січ");
				Helper::Strings::replaceAll(strInOut, "СТД", "січ");

				Helper::Strings::replaceAll(strInOut, "лютий", "лют");
				Helper::Strings::replaceAll(strInOut, "Лютий", "лют");
				Helper::Strings::replaceAll(strInOut, "ЛЮТИЙ", "лют");

				Helper::Strings::replaceAll(strInOut, "березень", "бер");
				Helper::Strings::replaceAll(strInOut, "Березень", "бер");
				Helper::Strings::replaceAll(strInOut, "БЕРЕЗЕНЬ", "бер");

				Helper::Strings::replaceAll(strInOut, "квітень", "кві");
				Helper::Strings::replaceAll(strInOut, "Квітень", "кві");
				Helper::Strings::replaceAll(strInOut, "КВІТЕНЬ", "кві");

				Helper::Strings::replaceAll(strInOut, "травень", "тра");
				Helper::Strings::replaceAll(strInOut, "Травень", "тра");
				Helper::Strings::replaceAll(strInOut, "ТРАВЕНЬ", "тра");

				Helper::Strings::replaceAll(strInOut, "червень", "чер");
				Helper::Strings::replaceAll(strInOut, "Червень", "чер");
				Helper::Strings::replaceAll(strInOut, "ЧЕРВЕНЬ", "чер");

				Helper::Strings::replaceAll(strInOut, "липень", "лип");
				Helper::Strings::replaceAll(strInOut, "Липень", "лип");
				Helper::Strings::replaceAll(strInOut, "ЛИПЕНЬ", "лип");

				Helper::Strings::replaceAll(strInOut, "серпень", "сер");
				Helper::Strings::replaceAll(strInOut, "Серпень", "сер");
				Helper::Strings::replaceAll(strInOut, "СЕРПЕНЬ", "сер");

				Helper::Strings::replaceAll(strInOut, "вересень", "вер");
				Helper::Strings::replaceAll(strInOut, "Вересень", "вер");
				Helper::Strings::replaceAll(strInOut, "ВЕРЕСЕНЬ", "вер");

				Helper::Strings::replaceAll(strInOut, "жовтень", "жов");
				Helper::Strings::replaceAll(strInOut, "Жовтень", "жов");
				Helper::Strings::replaceAll(strInOut, "ЖОВТЕНЬ", "жов");

				Helper::Strings::replaceAll(strInOut, "листопада", "лис");
				Helper::Strings::replaceAll(strInOut, "Листопада", "лис");
				Helper::Strings::replaceAll(strInOut, "ЛИСТОПАДА", "лис");
				Helper::Strings::replaceAll(strInOut, "листопад", "лис");
				Helper::Strings::replaceAll(strInOut, "Листопад", "лис");
				Helper::Strings::replaceAll(strInOut, "ЛИСТОПАД", "лис");

				Helper::Strings::replaceAll(strInOut, "грудень", "гру");
				Helper::Strings::replaceAll(strInOut, "Грудень", "гру");
				Helper::Strings::replaceAll(strInOut, "ГРУДЕНЬ", "гру");

				if(strInOut != oldString) {
					Helper::Strings::replaceAll(formatInOut, "%B", "%b");
				}
			}
		}
	}

	//! Extends single digits (@c 1-9) by adding a leading zero to each of them.
	/*!
	 * \param dateTimeString Reference to the string
	 *   in which all single digits will be extended.
	 */
	inline void extendSingleDigits(std::string& dateTimeString) {
		std::size_t pos{0};

		while(pos < dateTimeString.length()) {
			pos = dateTimeString.find_first_of("123456789", pos);

			if(pos == std::string::npos) {
				break;
			}

			if(
					(
							pos == 0
							|| std::isdigit(dateTimeString[pos - 1]) == 0
					) &&
					(
							pos == dateTimeString.length() - 1
							|| std::isdigit(dateTimeString[pos + 1]) == 0
					)
			) {
				// extend digit by adding leading zero
				dateTimeString.insert(pos, 1, '0');

				++pos;
			}

			++pos;
		}
	}

	//! Changes a year before 1969 into a year after 2000, if it has been parsed from two digits.
	/*!
	 * \param format View to the string containing the
	 *   format. If it does not contain @c %y, the
	 *   call to the function will have no effect.
	 * \param year Reference to the year to be changed,
	 *   if necessary.
	 */
	inline void fixYear(std::string_view format, int& year) {
		if(
				format.find("%y") != std::string_view::npos
				&& year < centuryFrom
		) {
			year += yearsPerCentury;
		}
	}

	//! Handles 12h-time manually to avoid buggy standard library implementations.
	/*!
	 * If the format contains a 12h suffix (@c %p),
	 *  it will be replaced by the actual content
	 *  of the suffix. For times after noon (@c pm),
	 *  the 12 hours before noon need to be manually
	 *  added during conversion. For times in the
	 *  12th hour, 12 hours need to be subtracted
	 *  manually from the AM time.
	 *
	 * \param formatString Reference to a string
	 *   containing the format of the date/time.
	 * \param dateTimeString Constant reference to a
	 *   string containing the date/time.
	 * \param outIsAm Reference to a boolean value
	 *   that will be set to @c true, if an AM time
	 *   has been encountered, i.e. during conversion,
	 *   twelve hours need to be manually subtracted
	 *   if the given time is in the 12th hour.
	 *   Otherwise its value will not be changed.
	 * \param outIsPm Reference to a boolean value
	 *   that will be set to @c true, if an PM time
	 *   has been encountered, i.e. during conversion,
	 *   twelve hours need to be manually added.
	 *   Otherwise its value will not be changed.
	 *
	 * \note Replaced will be the first @c am, @c AM,
	 *   @c pm, or @c PM that is surrounded by white
	 *   spaces, punctuation, digits and/or the
	 *   beginning/end of the string, regardless
	 *   of its actual position.
	 */
	inline void handle12hTime(
			std::string& formatString,
			const std::string& dateTimeString,
			bool& outIsAm,
			bool& outIsPm
	) {
		const auto formatPos{formatString.find("%p")};

		if(formatPos == std::string::npos) {
			return;
		}

		std::size_t pos{0};

		while(pos < dateTimeString.length()) {
			auto newPos{pos};
			auto amPos1{dateTimeString.find("am", pos)};
			auto amPos2{dateTimeString.find("AM", pos)};
			auto amPos{std::min(amPos1, amPos2)};

			if(amPos != std::string::npos) {
				newPos = amPos;

				if(
						(
								amPos == 0
								|| std::isspace(dateTimeString[amPos - 1]) != 0
								|| std::ispunct(dateTimeString[amPos - 1]) != 0
								|| std::isdigit(dateTimeString[amPos - 1]) != 0
						) && (
								amPos == dateTimeString.length() - amPmLength
								|| std::isspace(dateTimeString[amPos + amPmLength]) != 0
								|| std::ispunct(dateTimeString[amPos + amPmLength]) != 0
								|| std::isdigit(dateTimeString[amPos + amPmLength]) != 0
						)
				) {
					// found am/AM -> replace it in format string
					Helper::Strings::replaceAll(
							formatString,
							"%p",
							dateTimeString.substr(amPos, amPmLength)
					);

					outIsAm = true;

					return;
				}
			}

			auto pmPos1{dateTimeString.find("pm", pos)};
			auto pmPos2{dateTimeString.find("PM", pos)};
			auto pmPos{std::min(pmPos1, pmPos2)};

			if(pmPos != std::string::npos) {
				if(pmPos > newPos) {
					newPos = pmPos;
				}

				if(
						(
								pmPos == 0
								|| std::isspace(dateTimeString[pmPos - 1]) != 0
								|| std::ispunct(dateTimeString[pmPos - 1]) != 0
								|| std::isdigit(dateTimeString[pmPos - 1]) != 0
						) && (
								pmPos == dateTimeString.length() - amPmLength
								|| std::isspace(dateTimeString[pmPos + amPmLength]) != 0
								|| std::ispunct(dateTimeString[pmPos + amPmLength]) != 0
								|| std::isdigit(dateTimeString[pmPos + amPmLength]) != 0
						)
				) {
					// found pm/PM -> replace it in format string
					Helper::Strings::replaceAll(
							formatString,
							"%p",
							dateTimeString.substr(pmPos, amPmLength)
					);

					outIsPm = true;

					return;
				}
			}

			if(newPos == pos) {
				break;
			}

			pos = newPos;
		}
	}

} /* namespace crawlservpp::Helper::DateTime */

#endif /* HELPER_DATETIME_HPP_ */
