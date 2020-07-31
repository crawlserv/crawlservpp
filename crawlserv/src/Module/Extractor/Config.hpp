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
 * Config.hpp
 *
 * Extracting configuration.
 *
 *  Created on: May 9, 2019
 *      Author: ans
 */

#ifndef MODULE_EXTRACTOR_CONFIG_HPP_
#define MODULE_EXTRACTOR_CONFIG_HPP_

#include "../../Network/Config.hpp"

#include <algorithm>	// std::min std::replace_if
#include <array>		// std::array
#include <cstdint>		// std::int64_t, std::uint8_t, std::uint32_t, std::uint64_t
#include <string>		// std::string
#include <string_view>	// std::string_view, std::string_view_literals
#include <vector>		// std::vector

//! Namespace for extractor classes.
namespace crawlservpp::Module::Extractor {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! Logging is disabled.
	constexpr std::uint8_t crawlerLoggingVerbose{0};

	//! Default logging is enabled.
	constexpr std::uint8_t generalLoggingDefault{1};

	//! Extended logging is enabled.
	constexpr std::uint8_t generalLoggingExtended{2};

	//! Verbose logging is enabled.
	constexpr std::uint8_t generalLoggingVerbose{3};

	//! Extract variable value from parsed data.
	constexpr std::uint8_t variablesSourcesParsed{0};

	//! Extract variable value from the content of a crawled web page.
	constexpr std::uint8_t variablesSourcesContent{1};

	//! Extract variable value from the URL of a crawled web page.
	constexpr std::uint8_t variablesSourcesUrl{2};

	//! Extract data from other extracted data.
	constexpr std::uint8_t expectedSourceExtracting{0};

	//! Extract data from parsed data.
	constexpr std::uint8_t expectedSourceParsed{1};

	//! Extract data from the content of a crawled web page.
	constexpr std::uint8_t expectedSourceContent{2};

	//! HTTP status codes to retry by default.
	constexpr std::array defaultRetryHttpStatusCodes{429, 502, 503, 504};

	//! Protocols to remove from URLs
	constexpr std::array protocolsToRemove{"http://"sv, "https://"sv};

	//! Default cache size.
	constexpr std::uint64_t defaultCacheSize{2500};

	//! Default locking time, in seconds.
	constexpr std::uint32_t defaultLockS{300};

	//! Default re-tries on connection error.
	constexpr std::int64_t defaultReTries{720};

	//! Default sleeping time on connection errors, in milliseconds.
	constexpr std::uint64_t defaultSleepErrorMs{10000};

	//! Default time that will be waited between HTTP requests, in milliseconds.
	constexpr std::uint64_t defaultSleepHttpMs{0};

	//! Default time to wait before checking for new URLs when all URLs have been processed, in milliseconds.
	constexpr std::uint64_t defaultSleepIdleMs{5000};

	//! Default time to wait before last try to re-connect to mySQL server, in seconds.
	constexpr std::uint64_t defaultSleepMySqlS{20};

	//! Default name of the paging variable.
	/*!
	 *  To be used in
	 *  Extractor::Config::Entries::sourceUrl,
	 *  Extractor::Config::Entries::sourceCookies,
	 *  and
	 *  Extractor::Config::Entries::sourceHeaders.
	 *  Will be overwritten with either the
	 *   number, or the name of the current page.
	 */
	constexpr auto defaultPagingVariable{"$p"sv};

	//! Default maximum depth of recursive extracting.
	constexpr std::uint64_t defaultRecursiveMaxDepth{100};

	///@}

	/*
	 * DECLARATION
	 */

	//! Configuration for extractors.
	class Config : protected Network::Config {
		// for convenience
		using ConfigItem = Struct::ConfigItem;

	public:
		///@name Configuration
		///@{

		//! Configuration entries for extractor threads.
		/*!
		 * \warning Changing the configuration requires
		 *   updating @c json/extractor.json in @c
		 *   crawlserv_frontend!
		 */
		struct Entries {
			///@name Extractor Configuration
			///@{

			//! Number of URLs fetched and extracted from before saving results.
			/*!
			 * Set to zero to cache all URLs at once.
			 */
			std::uint64_t generalCacheSize{defaultCacheSize};

			//! Specifies whether to include custom URLs when extracting.
			bool generalExtractCustom{false};

			//! URL locking time, in seconds.
			std::uint32_t generalLock{defaultLockS};

			//! Level of logging activity.
			std::uint8_t generalLogging{generalLoggingDefault};

			//! Specifies whether to free small amounts of unused memory more often, at the expense of performance.
			bool generalMinimizeMemory{false};

			//! Specifies whether to re-extract data from already processed URLs.
			bool generalReExtract{false};

			//! Name of table to save extracted data to.
			std::string generalTargetTable;

			//! Number of re-tries on connection errors.
			/*!
			 * Set to @c -1, if you want to re-try an
			 *  infinite number of times on connection
			 *  errors.
			 */
			std::int64_t generalReTries{defaultReTries};

			//! HTTP errors that will be handled like connection errors.
			std::vector<std::uint32_t> generalRetryHttp{
				defaultRetryHttpStatusCodes.cbegin(),
				defaultRetryHttpStatusCodes.cend()
			};

			//! Sleeping time (in ms) on connection errors, in milliseconds.
			std::uint64_t generalSleepError{defaultSleepErrorMs};

			//! Time that will be waited between HTTP requests, in milliseconds.
			std::uint64_t generalSleepHttp{defaultSleepHttpMs};

			//! Time to wait before checking for new URLs when all URLs have been processed, in milliseconds.
			std::uint64_t generalSleepIdle{defaultSleepIdleMs};

			//! Time to wait before last try to re-connect to mySQL server, in seconds.
			std::uint64_t generalSleepMySql{defaultSleepMySqlS};

			//! Number of @c tidyhtml errors to write to the log.
			/*!
			 * \note Logging needs to be enabled in order
			 *   for this option to have any effect.
			 */
			std::uint32_t generalTidyErrors{0};

			//! Specifies whether to write @c tidyhtml warnings to the log.
			/*!
			 * \note Logging needs to be enabled in order
			 *   for this option to have any effect.
			 */
			bool generalTidyWarnings{false};

			//! Specifies whether to calculate timing statistics for the extractor.
			bool generalTiming{false};

			///@}
			///@name Variables
			///@{

			//! Alias for the variable with same array index.
			/*!
			 * Variable aliases allow additions to (and
			 *  subtractions from, via negative values)
			 *  the value of variables. The name of
			 *  the variable alias will be replaced
			 *  with the resulting value.
			 *
			 *  \sa variablesName, variablesAliasAdd
			 */
			std::vector<std::string> variablesAlias;

			//! Value to add to the variable alias with the same array index.
			/*!
			 * Use negative values to subtract from the
			 *  original value.
			 *
			 *  \sa variablesName, variablesAlias
			 */
			std::vector<std::int64_t> variablesAliasAdd;

			//! Date/time format to be used for the variable with the same array index.
			/*!
			 * If empty, no date/time conversion will be
			 *  performed.
			 *
			 * See Howard E. Hinnant's
			 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
			 *  C++ @c date.h library
			 *  documentation</a> for details.
			 *
			 * Set a string to @c UNIX to parse Unix timestamps,
			 *  i.e. seconds since the Unix epoch, instead.
			 *
			 *  \todo Not implemented yet.
			 *
			 *  \sa variablesName,
			 *    variablesDateTimeLocale,
			 *    Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> variablesDateTimeFormat;

			//! Date/time locale to be used for the variable with the same array index.
			/*!
			 * Will be ignored, if no corresponding
			 *  date/time format is given.
			 *
			 * \todo Not implemented yet.
			 *
			 * \sa variablesName,
			 *   variablesDateTimeFormat,
			 *   Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> variablesDateTimeLocale;

			//! Variable names.
			/*!
			 * Strings to be replaced by the respective variable
			 *  values in
			 *  Extractor::Config::Entries::variablesTokensSource,
			 *  Extractor::Config::variablesTokensHeaders,
			 *  Extractor::Config::sourceUrl,
			 *  Extractor::Config::sourceCookies, and
			 *  Extractor::Config::sourceHeaders.
			 */
			std::vector<std::string> variablesName;

			//! Parsed column for the value of the variable with the same array index.
			/*!
			 * \note Will only be used, if parsed data is
			 *   the source of the variable.
			 *
			 * \sa variablesSource
			 */
			std::vector<std::string> variablesParsedColumn;

			//! Name of the table containing the parsed data for the variable with the same array index.
			/*!
			 * \note Will only be used, if parsed data is
			 *   the source of the variable.
			 *
			 * \sa variablesSource,
			 *   variablesQuery
			 */
			std::vector<std::string> variablesParsedTable;

			//! Query on the content or URL for the variable with the same array index.
			/*!
			 * \note Will only be used, if the content
			 *   or the URL is the source of the variable.
			 *
			 * \sa variablesSource,
			 *   variablesQuery
			 */
			std::vector<std::uint64_t> variablesQuery;

			//! Source of the variable with the same array index.
			/*!
			 * Determines whether to use the table column stored
			 *  in
			 *  Extractor::Config::Entries::variablesParsedTable
			 *  and
			 *  Extractor::Config::Entries::variablesParsedColumn,
			 *  or the query stored in
			 *  Extractor::Config::Entries::variablesQuery to
			 *  determine the value of the variable with the same
			 *  array index.
			 *
			 * \sa variablesSourcesParsed,
			 *   variablesSourcesContent,
			 *   variablesSourcesUrl
			 */
			std::vector<std::uint8_t> variablesSource;

			//! List of token variables.
			/*!
			 * Strings to be replaced with the value of the
			 *  respective token variable in
			 *  Extractor::Config::Entries::sourceUrl,
			 *  Extractor::Config::Entries::sourceCookies, and
			 *  Extractor::Config::Entries::sourceHeaders.
			 *
			 * The values of token variables are determined
			 *  by requesting data from external soures.
			 *
			 * \sa variablesTokensSource
			 */
			std::vector<std::string> variablesTokens;

			//! Custom HTTP @c Cookie header for the token variable with the same array index.
			/*!
			 * \sa variablesTokensSource
			 */
			std::vector<std::string> variablesTokensCookies;

			//! Query to extract token variable with the same array index.
			/*!
			 * \sa variablesTokensSource
			 */
			std::vector<std::uint64_t> variablesTokensQuery;

			//! Source URL for the token variable with the same array index.
			/*!
			 * \note The URL needs to be absolute, but
			 *   without protocol, e.g.
			 *   @c en.wikipedia.org/wiki/Main_Page.
			 *
			 * To retrieve the content of the URL,
			 *  the headers specified in
			 *  Extractor::Config::Entries::variablesTokenHeaders,
			 *  and the cookies specified in the
			 *  string with the same array index in
			 *  Extractor::Config::Entries::variablesTokensCookies
			 *  will be used.
			 *
			 * Extractor::Config::Entries::variablesTokensUsePost
			 *  specifies whether to use HTTP POST,
			 *  instead of HTTP GET, when retrieving
			 *  the content. When HTTP POST is used,
			 *  arguments attached to the URL (e.g.
			 *  @c ?var1&var2=valueOfVar2) will be
			 *  sent as arguments of the HTTP POST
			 *  request instead of parts of the URL.
			 *
			 * Afterwards, the query with the same
			 *  array index in
			 *  Extractor::Config::Entries::variablesTokensQuery
			 *  will be used to determine the value
			 *  of the respective token variable.
			 */
			std::vector<std::string> variablesTokensSource;

			//! Specifies whether to use HTTP POST instead of GET for the token variable with the same array index.
			/*!
			 * \sa variablesTokensSource
			 */
			std::vector<bool> variablesTokensUsePost;

			//! Custom HTTP headers to be used for ALL token variables.
			/*!
			 * \sa variablesTokensSource
			 */
			std::vector<std::string> variablesTokenHeaders;

			///@}
			///@name Paging
			///@{

			//! Alias for the paging variable.
			/*!
			 * A paging alias allows additions to (and
			 *  subtractions from, via negative values)
			 *  the current value of the paging variable.
			 *  The name of the alias will be replaced
			 *  with the resulting value.
			 *
			 * \sa pagingAliasAdd
			 */
			std::string pagingAlias;

			//! Value to add to the alias for the paging variable.
			/*!
			 * Use negative values to subtract from the
			 *  original value.
			 *
			 *  \sa pagingAlias
			 */
			std::int64_t pagingAliasAdd{0};

			//! Number of the first page.
			/*!
			 * \sa pagingStep
			 */
			std::int64_t pagingFirst{0};

			//! Name of the first page.
			/*!
			 * If not empty, this string will overwrite
			 *  Extractor::Config::Entries::pagingFirst.
			 *  Extractor::Config::Entries::pagingStep
			 *  will also not be used to determine
			 *  the number of the next page, when a
			 *  page name is used instead.
			 */
			std::string pagingFirstString;

			//! Query on page content to determine whether there is another page.
			/*!
			 * Will be ignored, if no query is set,
			 *  i.e. the value is zero.
			 */
			std::uint64_t pagingIsNextFrom{0};

			//! Query on page content to find the number(s) or name(s) of additional pages.
			/*!
			 * Will be ignored, if no query is set,
			 *  i.e. the value is zero.
			 *
			 * If a query is set, it will overwrite
			 *   Extractor::Config::Entries::pagingStep,
			 *   which will no longer be used to
			 *   determine the number of the next page.
			 */
			std::uint64_t pagingNextFrom{0};

			//! Query to determine the total number of pages from the content of the first page.
			/*!
			 * Will be ignored, if no query is set,
			 *  i.e. the value is zero.
			 *
			 * If a query is set, it will overwrite
			 *  Extractor::Config::Entries::pagingStep, and
			 *  Extractor::Config::Entries::pagingNumberFrom,
			 *  which will no longer be used to determine
			 *  the number of the next page.
			 */
			std::uint64_t pagingNumberFrom{0};

			//! Number to add to page variable for retrieving the next page, if a page number is used.
			/*!
			 * \sa pagingFirst, pagingNextFrom,
			 *   pagingNumberFrom, pagingFirstString
			 */
			std::int64_t pagingStep{1};

			//! Name of the paging variable.
			/*!
			 * To be used in
			 *  Extractor::Config::Entries::sourceUrl,
			 *  Extractor::Config::Entries::sourceCookies,
			 *  and
			 *  Extractor::Config::Entries::SourceHeaders.
			 *  Will be overwritten with either the
			 *   number, or the name of the current page.
			 */
			std::string pagingVariable{defaultPagingVariable};

			///@}
			///@name Source
			///@{

			//! Custom HTTP @c Cookie header used when retrieving data.
			std::string sourceCookies;

			//! Custom HTTP headers used when retrieving data.
			std::vector<std::string> sourceHeaders;

			//! URL to retrieve data from.
			/*!
			 * \note The URL needs to be absolute, but
			 *   without protocol, e.g.
			 *   @c en.wikipedia.org/wiki/Main_Page.
			 */
			std::string sourceUrl;

			//! URL of the first page to retrieve data from.
			/*!
			 * \note The URL needs to be absolute, but
			 *   without protocol, e.g.
			 *   @c en.wikipedia.org/wiki/Main_Page.
			 *
			 * Will be ignored, when empty.
			 */
			std::string sourceUrlFirst;

			//! Specifies whether to use HTTP POST instead of HTTP GET for extracting data.
			/*!
			 * \note When HTTP POST is used,
			 *   arguments attached to the URL (e.g.
			 *   @c ?var1&var2=valueOfVar2) will be
			 *   sent as arguments of the HTTP POST
			 *   request instead of parts of the URL.
			 */
			bool sourceUsePost{false};

			///@}
			///@name Extracting
			///@{

			//! Queries to extract datasets.
			/*!
			 * The first query that returns a non-empty
			 *  result will be used.
			 */
			std::vector<std::uint64_t> extractingDatasetQueries;

			//! Format of date/time to be extracted by the date/time query with the same array index.
			/*!
			 * If not specified, the format
			 *  @c %%F @c %%T, i.e.
			 *  @c YYYY-MM-DD HH:MM:SS will be used.
			 *
			 * See Howard E. Hinnant's
			 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
			 *  C++ @c date.h library
			 *  documentation</a> for details.
			 *
			 * Set a string to @c UNIX to parse Unix timestamps,
			 *  i.e. seconds since the Unix epoch, instead.
			 *
			 * \sa extractingDateTimeQueries,
			 *   extractingDateTimeLocales,
			 *   Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> extractingDateTimeFormats;

			//! Locale used by the date/time query with the same array index for extracting date and time.
			/*!
			 * \sa extractingDateTimeQueries,
			 *   extractingDateTimeLocales,
			 *   Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> extractingDateTimeLocales;

			//! Queries used for extracting date/time from the dataset.
			/*!
			 * The first query that returns a non-empty
			 *  result will be used.
			 *
			 * \sa extractingDateTimeFormats,
			 *   extractingDateTimeLoclaes
			 */
			std::vector<std::uint64_t> extractingDateTimeQueries;

			//! Queries to detect fatal errors in the data.
			/*!
			 * The extraction will fail, if any of
			 *  these queries return @c true.
			 */
			std::vector<std::uint64_t> extractingErrorFail;

			//! Queries to detect temporary errors in the data.
			/*!
			 *  The extraction will be retried, as long
			 *   as any of  these queries return @c true.
			 */
			std::vector<std::uint64_t> extractingErrorRetry;

			//! Date/time format of the field with the same array index.
			/*!
			 * If empty, no date/time conversion will be
			 *  performed.
			 *
			 * See Howard E. Hinnant's
			 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
			 *  C++ @c date.h library
			 *  documentation</a> for details.
			 *
			 * Set a string to @c UNIX to parse Unix timestamps,
			 *  i.e. seconds since the Unix epoch, instead.
			 *
			 * \sa extractingFieldNames,
			 *   extractingFieldDateTimeLocales,
			 *   Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> extractingFieldDateTimeFormats;

			//! Locale used when converting the field with the same array index to a date/time.
			/*!
			 * Will be ignored, if no date/time format
			 *  has been specified for the field.
			 *
			 * \sa extractingFieldNames,
			 *   extractingFieldDateTimeFormats,
			 *   Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> extractingFieldDateTimeLocales;

			//! Delimiter between multiple results for the field with the same array index, if not saved as JSON.
			/*!
			 * Only the first character of the string, @c \\n
			 *  (default), @c \\t, or @c \\\\ will be used.
			 *
			 *  \sa extractingFieldNames
			 */
			std::vector<char> extractingFieldDelimiters;

			//! Specifies whether to ignore empty values when parsing multiple results for the field with the same array index.
			/*!
			 * Enabled by default.
			 *
			 * \sa extractingFieldNames
			 */
			std::vector<bool> extractingFieldIgnoreEmpty;

			//! Save the value of the field with the same array index as a JSON array.
			/*!
			 * \sa extractingFieldNames
			 */
			std::vector<bool> extractingFieldJSON;

			//! The names of the custom fields to extract.
			/*!
			 * These fields will be extracted from
			 *  the content of the current page,
			 *  using the queries specified in
			 *  Extractor::Config::Entries::extractingFieldQueries.
			 *
			 * Field options are matched via the array
			 *  index in the respective vectors.
			 *
			 * If
			 *  Extractor::Config::Entries::extractingFieldDateTimeFormats
			 *  contains a non-empty string, a
			 *  date/time will be parsed for the
			 *  respective field, using the locale
			 *  defined in
			 *  Extractor::Config::Entries::extractingFieldDateTimeLocale.
			 *
			 * Multiple values for one field will
			 *  be detected via the delimiter in
			 *  Extractor::Config::Entries::extractingFieldDelimiters,
			 *  Extractor::Config::Entries::extractingFieldIgnoreEmpty
			 *  determines whether to ignore empty
			 *  values, and
			 *  Extractor::Config::Entries::extractingFieldJSON
			 *  whether to store them as a JSON array.
			 *
			 * If the value of a field is empty,
			 *  Extractor::Config::Entries::extractingFieldWarningsEmpty
			 *  determines whether to write a warning
			 *  to the log.
			 *
			 * Extractor::Config::Entries::extractingFieldTidyTexts
			 *  specifies whether to tidy up the
			 *  resulting text before being stored
			 *  to the respective field.
			 */
			std::vector<std::string> extractingFieldNames;

			//! The query used to extract the custom field with the same array index from the data.
			/*!
			 * \sa extractingFieldNames
			 */
			std::vector<std::uint64_t> extractingFieldQueries;

			//! Specifies whether to remove line breaks and unnecessary spaces when extracting the field with the same array index.
			/*!
			 * \sa extractingFieldNames
			 */
			std::vector<bool> extractingFieldTidyTexts;

			//! Specifies whether to write a warning to the log when the field with the same array index is empty.
			/*!
			 * \note Logging needs to be enabled in order
			 *   for this option to have any effect.
			 *
			 * \sa extractingFieldNames
			 */
			std::vector<bool> extractingFieldWarningsEmpty;

			//! Extracted IDs to be ignored.
			std::vector<std::string> extractingIdIgnore;

			//! Queries to extract the ID from the dataset.
			/*!
			 *  The first query that returns a non-empty
			 *   result will be used. Datasets with duplicate
			 *   or empty IDs will not be extracted.
			 */
			std::vector<std::uint64_t> extractingIdQueries;

			//! Specifies whether, if a dataset with the same ID already exists, it will be overwritten.
			bool extractingOverwrite{true};

			//! Queries for extracting more datasets from a dataset.
			/*!
			 * The first query that returns a non-empty
			 *  result will be used.
			 */
			std::vector<std::uint64_t> extractingRecursive;

			//! Maximum depth of recursive extracting.
			std::uint64_t extractingRecursiveMaxDepth{defaultRecursiveMaxDepth};

			//! Specifies whether to remove duplicate datasets over multiple pages before checking the expected number of datasets.
			bool extractingRemoveDuplicates{true};

			//! Specifies whether to (try to) repair CData when parsing HTML/XML.
			bool extractingRepairCData{true};

			//! Specifies whether to (try to) repair broken HTML/XML comments.
			bool extractingRepairComments{true};

			///@}
			///@name Linked Data
			///@{

			//! Queries to extract linked datasets.
			/*!
			 * The first query that returns a non-empty
			 *  result will be used.
			 *
			 *  \todo Not implemented yet.
			 *
			 * \sa linkedFieldNames
			 */
			std::vector<std::uint64_t> linkedDatasetQueries;

			//! Date/time format of the linked field with the same array index.
			/*!
			 * If empty, no date/time conversion will be
			 *  performed.
			 *
			 * See Howard E. Hinnant's
			 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
			 *  C++ @c date.h library
			 *  documentation</a> for details.
			 *
			 * Set a string to @c UNIX to parse Unix timestamps,
			 *  i.e. seconds since the Unix epoch, instead.
			 *
			 *  \sa linkedFieldNames,
			 *    linkedDateTimeLocales,
			 *    Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> linkedDateTimeFormats;

			//! Date/time locale of the linked field with the same array index.
			/*!
			 * Will be ignored, if no corresponding
			 *  date/time format is given.
			 *
			 * \sa linkedFieldNames,
			 *   linkedDateTimeFormat,
			 *   Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> linkedDateTimeLocales;

			//! Delimiter between multiple results for the field with the same array index, if not saved as JSON.
			/*!
			 * Only the first character, @c \\n (default),
			 *  @c \\t, or @c \\\\ will be used.
			 *
			 *  \sa linkedFieldNames
			 */
			std::vector<char> linkedDelimiters;

			//! Names of the linked data fields.
			/*!
			 * Linked data is additionally extracted data
			 *  that is linked via its ID field to one of
			 *  the originally extracted data fields, as
			 *  specified in
			 *  Extractor::Config::Entries::linkedLink.
			 *
			 * The ID field, as well as the additional
			 *  data fields will be extracted from the
			 *  subset retrieved by using the query in
			 *  Extractor::Config::Entries::linkedDataSetQueries
			 *  on the content of the current page,
			 *  using the queries specified in
			 *  Extractor::Config::Entries::linkedIdQueries
			 *  for the ID, and
			 *  Extractor::Config::Entries::linkedFieldQueries
			 *  for each of the other fields.
			 *
			 * Linked data with the IDs specified in
			 *  Extractor::Config::Entries::linkedIdIgnore
			 *  will be ignored.
			 *
			 * Linked field options are matched via the
			 *  array index in the respective vectors.
			 *
			 * If
			 *  Extractor::Config::Entries::linkedFieldDateTimeFormats
			 *  contains a non-empty string, a
			 *  date/time will be parsed for the
			 *  respective field, using the locale
			 *  defined in
			 *  Extractor::Config::Entries::linkedFieldDateTimeLocale.
			 *
			 * Multiple values for one field will
			 *  be detected via the delimiter in
			 *  Extractor::Config::Entries::linkedFieldDelimiters,
			 *  Extractor::Config::Entries::linkedFieldIgnoreEmpty
			 *  determines whether to ignore empty
			 *  values, and
			 *  Extractor::Config::Entries::linkedFieldJSON
			 *  whether to store them as a JSON array.
			 *
			 * If the value of a field is empty,
			 *  Extractor::Config::Entries::linkedWarningsEmpty
			 *  determines whether to write a warning
			 *  to the log.
			 *
			 * Extractor::Config::Entries::linkedTidyTexts
			 *  specifies whether to tidy up the
			 *  resulting text before being stored
			 *  to the respective field.
			 *
			 *  \todo Not implemented yet.
			 */
			std::vector<std::string> linkedFieldNames;

			//! Query used to extract the custom field with the same array index from the dataset.
			/*!
			 * \todo Not implemented yet.
			 *
			 * \sa linkedFieldNames
			 */
			std::vector<std::uint64_t> linkedFieldQueries;

			//! IDs of linked data to be ignored.
			/*!
			 * \sa linkedFieldNames
			 */
			std::vector<std::string> linkedIdIgnore;

			//! Queries to extract the linked ID from the dataset.
			/*!
			 *  The first query that returns a non-empty result will be used.
			 *
			 *  Datasets with duplicate or empty IDs will not be extracted.
			 *
			 * \sa linkedFieldNames
			 */
			std::vector<std::uint64_t> linkedIdQueries;

			//! Specifies whether to ignore empty values when parsing multiple results for the field with the same array index.
			/*!
			 * Enabled by default.
			 *
			 * \sa linkedFieldNames
			 */
			std::vector<bool> linkedIgnoreEmpty;

			//! Specfies whether to save the value of the field with the same array index as a JSON array.
			/*!
			 * \sa linkedFieldNames
			 */
			std::vector<bool> linkedJSON;

			//! Name of the extracted field that links an extracted dataset to the ID of a linked dataset.
			/*!
			 * \todo Not implemented yet.
			 *
			 * \sa linkedFieldNames
			 */
			std::string linkedLink;

			//! Specifies whether, if a linked dataset with the same ID already exists, it will be overwritten.
			/*!
			 * \todo Not implemented yet.
			 */
			bool linkedOverwrite{true};

			//! Name of the table to save linked data to.
			/*!
			 * \todo Not implemented yet.
			 */
			std::string linkedTargetTable;

			//! Specifies whether to remove line breaks and unnecessary spaces when extracting the linked field with the same array index.
			/*!
			 * \sa linkedFieldNames
			 */
			std::vector<bool> linkedTidyTexts;

			//! Specifies whether to write a warning to the log when the field with the same array index is empty.
			/*!
			 * \note Logging needs to be enabled in order
			 *   for this option to have any effect.
			 *
			 * \sa linkedFieldNames
			 */
			std::vector<bool> linkedWarningsEmpty;

			///@}
			///@name Expected Number of Results
			///@{

			//! Specifies whether to throw an exception when the number of expected datasets is exceeded.
			bool expectedErrorIfLarger{false};

			//! Specifies whether to throw an exception when the number of expected datasets is subceeded.
			bool expectedErrorIfSmaller{false};

			//! Parsed column containing the expected number of datasets.
			/*!
			 * \note Will only be used, if parsed data is
			 *   the source of the expected number of
			 *   datasets.
			 *
			 * \sa expectedSource
			 */
			std::string expectedParsedColumn;

			//! Name of the table containing the expected number of datasets.
			/*!
			 * \note Will only be used, if parsed data is
			 *   the source of the expected number of
			 *   datasets.
			 *
			 * \sa expectedSource
			 */
			std::string expectedParsedTable;

			//! Query to be performed to retrieve the expected number of datasets.
			/*!
			 * \note Will only be used, if the content
			 *   or the URL is the source of the expected
			 *   number of datasets.
			 *
			 * \sa expectedSource
			 */
			std::uint64_t expectedQuery{0};

			//! Source of the query to retrieve the expected number of datasets.
			/*!
			 * \sa expectedSourceExtracting,
			 *   expectedSourceParsed,
			 *   expectedSourceContent,
			 *   expectedQuery,
			 *   expectedParsedTable,
			 *   expectedParsedColumn
			 */
			std::uint8_t expectedSource{expectedSourceExtracting};

			///@}
		}

		//! Configuration of the extractor
		config;

		///@}

		//! Class for extractor configuration exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		///@name Extractor-Specific Configuration Parsing
		///@{

		void parseOption() override;
		void checkOptions() override;

		///@}

	private:
		// internal helper function
		static void removeProtocolsFromUrl(std::string& inOut);
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * EXTRACTOR-SPECIFIC CONFIGURATION PARSING
	 */

	//! Parses an extractor-specific configuration option.
	inline void Config::parseOption() {
		// general options
		this->category("general");
		this->option("cache.size", this->config.generalCacheSize);
		this->option("extract.custom", this->config.generalExtractCustom);
		this->option("lock", this->config.generalLock);
		this->option("logging", this->config.generalLogging);
		this->option("minimize.memory", this->config.generalMinimizeMemory);
		this->option("reextract", this->config.generalReExtract);
		this->option("retries", this->config.generalReTries);
		this->option("retry.http", this->config.generalRetryHttp);
		this->option("sleep.error", this->config.generalSleepError);
		this->option("sleep.http", this->config.generalSleepHttp);
		this->option("sleep.idle", this->config.generalSleepIdle);
		this->option("sleep.mysql", this->config.generalSleepMySql);
		this->option("target.table", this->config.generalTargetTable);
		this->option("tidy.errors", this->config.generalTidyErrors);
		this->option("tidy.warnings", this->config.generalTidyWarnings);
		this->option("timing", this->config.generalTiming);

		// variables
		this->category("variables");
		this->option("alias", this->config.variablesAlias);
		this->option("alias.add", this->config.variablesAliasAdd);
		this->option("datetime.format", this->config.variablesDateTimeFormat);
		this->option("datetime.locale", this->config.variablesDateTimeLocale);
		this->option("name", this->config.variablesName);
		this->option("parsed.column", this->config.variablesParsedColumn);
		this->option("parsed.table", this->config.variablesParsedTable);
		this->option("query", this->config.variablesQuery);
		this->option("source", this->config.variablesSource);
		this->option("tokens", this->config.variablesTokens);
		this->option("tokens.cookies", this->config.variablesTokensCookies);
		this->option("tokens.query", this->config.variablesTokensQuery);
		this->option("tokens.source", this->config.variablesTokensSource);
		this->option("tokens.use.post", this->config.variablesTokensUsePost);
		this->option("token.headers", this->config.variablesTokenHeaders);

		// paging
		this->category("paging");
		this->option("alias", this->config.pagingAlias);
		this->option("alias.add", this->config.pagingAliasAdd);
		this->option("first", this->config.pagingFirst);
		this->option("first.string", this->config.pagingFirstString);
		this->option("is.next.from", this->config.pagingIsNextFrom);
		this->option("next.from", this->config.pagingNextFrom);
		this->option("number.from", this->config.pagingNumberFrom);
		this->option("step", this->config.pagingStep);
		this->option("variable", this->config.pagingVariable);

		// source
		this->category("source");
		this->option("cookies", this->config.sourceCookies);
		this->option("headers", this->config.sourceHeaders);
		this->option("url", this->config.sourceUrl);
		this->option("url.first", this->config.sourceUrlFirst);
		this->option("use.post", this->config.sourceUsePost);

		// extracting
		this->category("extracting");
		this->option("dataset.queries", this->config.extractingDatasetQueries);
		this->option("datetime.formats", this->config.extractingDateTimeFormats);
		this->option("datetime.locales", this->config.extractingDateTimeLocales);
		this->option("datetime.queries", this->config.extractingDateTimeQueries);
		this->option("error.fail", this->config.extractingErrorFail);
		this->option("error.retry", this->config.extractingErrorRetry);
		this->option("field.datetime.formats", this->config.extractingFieldDateTimeFormats);
		this->option("field.datetime.locales", this->config.extractingFieldDateTimeLocales);
		this->option(
				"field.delimiters",
				this->config.extractingFieldDelimiters,
				CharParsingOption::FromString
		);
		this->option("field.ignore.empty", this->config.extractingFieldIgnoreEmpty);
		this->option("field.json", this->config.extractingFieldJSON);
		this->option("field.names", this->config.extractingFieldNames);
		this->option("field.queries", this->config.extractingFieldQueries);
		this->option("field.tidy.texts", this->config.extractingFieldTidyTexts);
		this->option("field.warnings.empty", this->config.extractingFieldWarningsEmpty);
		this->option("id.ignore", this->config.extractingIdIgnore);
		this->option("id.queries", this->config.extractingIdQueries);
		this->option("overwrite", this->config.extractingOverwrite);
		this->option("recursive", this->config.extractingRecursive);
		this->option("recursive.max.depth", this->config.extractingRecursiveMaxDepth);
		this->option("remove.duplicates", this->config.extractingRemoveDuplicates);
		this->option("repair.cdata", this->config.extractingRepairCData);
		this->option("repair.comments", this->config.extractingRepairComments);

		// linked data
		this->category("linked");
		this->option("dataset.queries", this->config.linkedDatasetQueries);
		this->option("datetime.formats", this->config.linkedDateTimeFormats);
		this->option("datetime.locales", this->config.linkedDateTimeLocales);
		this->option("delimiters", this->config.linkedDelimiters, CharParsingOption::FromString);
		this->option("field.names", this->config.linkedFieldNames);
		this->option("field.queries", this->config.linkedFieldQueries);
		this->option("id.ignore", this->config.linkedIdIgnore);
		this->option("id.queries", this->config.linkedIdQueries);
		this->option("ignore.empty", this->config.linkedIgnoreEmpty);
		this->option("json", this->config.linkedJSON);
		this->option("link", this->config.linkedLink);
		this->option("overwrite", this->config.linkedOverwrite);
		this->option("target.table", this->config.linkedTargetTable);
		this->option("tidy.texts", this->config.linkedTidyTexts);
		this->option("warnings.empty", this->config.linkedWarningsEmpty);

		// expected [number of results]
		this->category("expected");
		this->option("error.if.larger", this->config.expectedErrorIfLarger);
		this->option("error.if.smaller", this->config.expectedErrorIfSmaller);
		this->option("parsed.column", this->config.expectedParsedColumn);
		this->option("parsed.table", this->config.expectedParsedTable);
		this->option("query", this->config.expectedQuery);
		this->option("source", this->config.expectedSource);
	}

	//! Checks the extractor-specific configuration options.
	/*!
	 * \throws Module::Extractor::Config::Exception
	 *   if no target table has been specified.
	 */
	inline void Config::checkOptions() {
		// check for target table
		if(this->config.generalTargetTable.empty()) {
			throw Exception(
					"Parser::Config::checkOptions():"
					" No target table has been specified."
			);
		}

		// remove obvious protocols from given URLs
		for(auto& url : this->config.variablesTokensSource) {
			Config::removeProtocolsFromUrl(url);
		}

		Config::removeProtocolsFromUrl(this->config.sourceUrl);
		Config::removeProtocolsFromUrl(this->config.sourceUrlFirst);

		// check properties of variables
		bool incompleteVariables{false};

		const auto completeVariables{
			std::min({
					/* number of complete variables (= min. size of name and source arrays) */
					this->config.variablesName.size(),
					this->config.variablesSource.size()
			})
		};

		// remove variable names that are not used
		if(this->config.variablesName.size() > completeVariables) {
			this->config.variablesName.resize(completeVariables);

			incompleteVariables = true;
		}

		// remove variable sources that are not used
		if(this->config.variablesSource.size() > completeVariables) {
			this->config.variablesSource.resize(completeVariables);

			incompleteVariables = true;
		}

		// warn about incomplete variables
		if(incompleteVariables) {
			this->warning(
					"\'variables.name\' and \'.source\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete variable(s) removed from configuration.");

			incompleteVariables = false;
		}

		// remove variable queries that are not used, add empty query where none is specified
		if(this->config.variablesQuery.size() > completeVariables) {
			incompleteVariables = true;
		}

		this->config.variablesQuery.resize(completeVariables);

		// remove variable tables that are not used, add empty table where none is specified
		if(this->config.variablesParsedTable.size() > completeVariables) {
			incompleteVariables = true;
		}

		this->config.variablesParsedTable.resize(completeVariables);

		// remove variable columns that are not used, add empty column where none is specified
		if(this->config.variablesParsedColumn.size() > completeVariables) {
			incompleteVariables = true;
		}

		this->config.variablesParsedColumn.resize(completeVariables);

		// remove variable date/time formats that are not used, add empty format where none is specified
		if(this->config.variablesDateTimeFormat.size() > completeVariables) {
			incompleteVariables = true;
		}

		this->config.variablesDateTimeFormat.resize(completeVariables);

		// replace empty date/time formats with "%F %T"
		std::replace_if(
				this->config.variablesDateTimeFormat.begin(),
				this->config.variablesDateTimeFormat.end(),
				[](const auto& str) {
					return str.empty();
				},
				"%F %T"
		);

		// remove variable date/time locales that are not used, add empty locale where none is specified
		if(this->config.variablesDateTimeLocale.size() > completeVariables) {
			incompleteVariables = true;
		}

		this->config.variablesDateTimeLocale.resize(completeVariables);

		// remove variable aliases that are not used, add empty alias where none is specified
		if(this->config.variablesAlias.size() > completeVariables) {
			incompleteVariables = true;
		}

		this->config.variablesAlias.resize(completeVariables);

		// remove variable alias values that are not used, add empty alias value where none is specified
		if(this->config.variablesAliasAdd.size() > completeVariables) {
			incompleteVariables = true;
		}

		this->config.variablesAliasAdd.resize(completeVariables);

		// warn about unused properties
		if(incompleteVariables) {
			this->warning("Unused variable properties removed from configuration.");
		}

		// check properties of tokens
		bool incompleteTokens{false};

		const auto completeTokens{
			std::min({
					/* number of complete tokens (= min. size of arrays) */
					this->config.variablesTokens.size(),
					this->config.variablesTokensSource.size(),
					this->config.variablesTokensQuery.size()
			})
		};

		// remove token variable names that are not used
		if(this->config.variablesTokens.size() > completeTokens) {
			this->config.variablesTokens.resize(completeTokens);

			incompleteTokens = true;
		}

		// remove token sources that are not used
		if(this->config.variablesTokensSource.size() > completeTokens) {
			this->config.variablesTokensSource.resize(completeTokens);

			incompleteTokens = true;
		}

		// remove token queries that are not used
		if(this->config.variablesTokensQuery.size() > completeTokens) {
			this->config.variablesTokensQuery.resize(completeTokens);

			incompleteTokens = true;
		}

		// warn about incomplete tokens
		if(incompleteTokens) {
			this->warning(
					"\'variables.tokens\', \'.tokens.source\' and \'.tokens.query\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete token(s) removed from configuration.");

			incompleteTokens = false;
		}

		// remove cookie headers that are not used, set to empty string where none is specified
		if(this->config.variablesTokensCookies.size() > completeTokens) {
			incompleteTokens = true;
		}

		this->config.variablesTokensCookies.resize(completeTokens);

		// remove token POST options that are not used, set to 'false' where none is specified
		if(this->config.variablesTokensUsePost.size() > completeTokens) {
			incompleteTokens = true;
		}

		this->config.variablesTokensUsePost.resize(completeTokens, false);

		// warn about unused property
		if(incompleteTokens) {
			this->warning(
					"Unused token properties removed from configuration."
			);
		}

		// check properties of date/time queries
		bool incompleteDateTimes{false};

		// remove date/time formats that are not used, add empty format where none is specified
		if(
				this->config.extractingDateTimeFormats.size()
				> this->config.extractingDateTimeQueries.size()
		) {
			incompleteDateTimes = true;
		}

		this->config.extractingDateTimeFormats.resize(
				this->config.extractingDateTimeQueries.size()
		);

		// remove date/time locales that are not used, add empty locale where none is specified
		if(
				this->config.extractingDateTimeLocales.size()
				> this->config.extractingDateTimeQueries.size()
		) {
			incompleteDateTimes = true;
		}

		this->config.extractingDateTimeLocales.resize(
				this->config.extractingDateTimeQueries.size()
		);

		// warn about unused properties
		if(incompleteDateTimes) {
			this->warning(
					"Unused date/time properties removed from configuration."
			);
		}

		// replace empty date/time formats with "%F %T"
		std::replace_if(
				this->config.extractingDateTimeFormats.begin(),
				this->config.extractingDateTimeFormats.end(),
				[](const auto& str) {
					return str.empty();
				},
				"%F %T"
		);

		// check properties of fields
		const auto completeFields{
			std::min(
					this->config.extractingFieldNames.size(),
					this->config.extractingFieldQueries.size()
			)
		};

		bool incompleteFields{false};

		// remove field names or queries that are not used
		if(this->config.extractingFieldNames.size() > completeFields) {
			incompleteFields = true;

			this->config.extractingFieldNames.resize(completeFields);
		}
		else if(this->config.extractingFieldQueries.size() > completeFields) {
			incompleteFields = true;

			this->config.extractingFieldQueries.resize(completeFields);
		}

		// warn about incomplete fields
		if(incompleteFields) {
			this->warning(
					"\'variables.field.names\' and \'.field.queries\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete field(s) removed from configuration.");

			incompleteFields = false;
		}

		// remove date/time formats that are not used, add empty format where none is specified
		if(this->config.extractingFieldDateTimeFormats.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.extractingFieldDateTimeFormats.resize(completeFields);

		// remove date/time locales that are not used, add empty locale where none is specified
		if(this->config.extractingFieldDateTimeLocales.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.extractingFieldDateTimeLocales.resize(completeFields);

		// remove field delimiters that are not used, add empty delimiter (\0) where none is specified
		if(this->config.extractingFieldDelimiters.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.extractingFieldDelimiters.resize(completeFields, '\0');

		// replace all empty field delimiters with '\n'
		std::replace_if(
				this->config.extractingFieldDelimiters.begin(),
				this->config.extractingFieldDelimiters.end(),
				[](char c) {
					return c == '\0';
				},
				'\n'
		);

		// remove 'ignore empty values' properties that are not used, set to 'true' where none is specified
		if(this->config.extractingFieldIgnoreEmpty.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.extractingFieldIgnoreEmpty.resize(completeFields, true);

		// remove 'save field as JSON' properties that are not used, set to 'false' where none is specified
		if(this->config.extractingFieldJSON.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.extractingFieldJSON.resize(completeFields, false);

		// remove 'tidy text' properties that are not used, set to 'false' where none is specified
		if(this->config.extractingFieldTidyTexts.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.extractingFieldTidyTexts.resize(completeFields, false);

		// remove 'warning if empty' properties that are not used, set to 'false' where none is specified
		if(this->config.extractingFieldWarningsEmpty.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.extractingFieldWarningsEmpty.resize(completeFields, false);

		// warn about unused properties
		if(incompleteFields) {
			this->warning(
					"Unused field properties for extraction removed from configuration."
			);
		}

		// check properties of linked fields
		const auto completeLinkedFields{
			std::min(
					this->config.linkedFieldNames.size(),
					this->config.linkedFieldQueries.size()
			)
		};

		incompleteFields = false;

		// remove field names or queries that are not used
		if(this->config.linkedFieldNames.size() > completeLinkedFields) {
			incompleteFields = true;

			this->config.linkedFieldNames.resize(completeLinkedFields);
		}
		else if(this->config.linkedFieldQueries.size() > completeLinkedFields) {
			incompleteFields = true;

			this->config.linkedFieldQueries.resize(completeLinkedFields);
		}

		// warn about incomplete fields
		if(incompleteFields) {
			this->warning(
					"\'linked.field.names\' and \'.field.queries\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete field(s) removed from configuration.");

			incompleteFields = false;
		}

		// remove date/time formats that are not used, add empty format where none is specified
		if(this->config.linkedDateTimeFormats.size() > completeLinkedFields) {
			incompleteFields = true;
		}

		this->config.linkedDateTimeFormats.resize(completeFields);

		// remove date/time locales that are not used, add empty locale where none is specified
		if(this->config.linkedDateTimeLocales.size() > completeLinkedFields) {
			incompleteFields = true;
		}

		this->config.linkedDateTimeLocales.resize(completeLinkedFields);

		// remove field delimiters that are not used, add empty delimiter (\0) where none is specified
		if(this->config.linkedDelimiters.size() > completeLinkedFields) {
			incompleteFields = true;
		}

		this->config.linkedDelimiters.resize(completeLinkedFields, '\0');

		// replace all empty field delimiters with '\n'
		std::replace_if(
				this->config.linkedDelimiters.begin(),
				this->config.linkedDelimiters.end(),
				[](char c) {
					return c == '\0';
				},
				'\n'
		);

		// remove 'ignore empty values' properties that are not used, set to 'true' where none is specified
		if(this->config.linkedIgnoreEmpty.size() > completeLinkedFields) {
			incompleteFields = true;
		}

		this->config.linkedIgnoreEmpty.resize(completeLinkedFields, true);

		// remove 'save field as JSON' properties that are not used, set to 'false' where none is specified
		if(this->config.linkedJSON.size() > completeLinkedFields) {
			incompleteFields = true;
		}

		this->config.linkedJSON.resize(completeLinkedFields, false);

		// remove 'tidy text' properties that are not used, set to 'false' where none is specified
		if(this->config.linkedTidyTexts.size() > completeLinkedFields) {
			incompleteFields = true;
		}

		this->config.linkedTidyTexts.resize(completeLinkedFields, false);

		// remove 'warning if empty' properties that are not used, set to 'false' where none is specified
		if(this->config.linkedWarningsEmpty.size() > completeLinkedFields) {
			incompleteFields = true;
		}

		this->config.linkedWarningsEmpty.resize(completeLinkedFields, false);

		// warn about unused properties
		if(incompleteFields) {
			this->warning(
					"Unused field properties for linked data removed from configuration."
			);
		}
	}

	/*
	 * INTERNAL HELPER FUNCTION (private)
	 */

	// remove obvious protocol(s) from beginning of URL
	inline void Config::removeProtocolsFromUrl(std::string& inOut) {
		for(const auto& protocol : protocolsToRemove) {
			while(
					inOut.length() >= protocol.length() &&
					inOut.substr(0, protocol.length()) == protocol.data()
			) {
				inOut = inOut.substr(protocol.length());
			}
		}
	}

} /* namespace crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_CONFIG_HPP_ */
