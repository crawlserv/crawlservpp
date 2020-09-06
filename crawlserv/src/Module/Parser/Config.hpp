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
 * Parsing configuration.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_CONFIG_HPP_
#define MODULE_PARSER_CONFIG_HPP_

#include "../../Main/Exception.hpp"
#include "../../Module/Config.hpp"

#include <algorithm>	// std::min, std::replace_if
#include <cstdint>		// std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <string>		// std::string
#include <vector>		// std::vector

//! Namespace for parser classes.
namespace crawlservpp::Module::Parser {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Logging is disabled.
	inline constexpr std::uint8_t crawlerLoggingVerbose{0};

	//! Default logging is enabled.
	inline constexpr std::uint8_t generalLoggingDefault{1};

	//! Extended logging is enabled.
	inline constexpr std::uint8_t generalLoggingExtended{2};

	//! Verbose logging is enabled.
	inline constexpr std::uint8_t generalLoggingVerbose{3};

	//! Parse data from the URL of a crawled web page.
	inline constexpr std::uint8_t parsingSourceUrl{0};

	//! Parse data from the content of a crawled web page.
	inline constexpr std::uint8_t parsingSourceContent{1};

	//! Default cache size.
	inline constexpr std::uint64_t defaultCacheSize{2500};

	//! Default URL locking time, in seconds.
	inline constexpr std::uint32_t defaultLockS{300};

	//! Default maximum number of URLs to be processed in one MySQL query.
	inline constexpr std::uint16_t defaultMaxBatchSize{500};

	//! Default time to wait before checking for new URLs when all URLs have been parsed, in milliseconds.
	inline constexpr std::uint64_t defaultSleepIdleMs{5000};

	//! Default time to wait before last try to re-connect to MySQL server, in seconds.
	inline constexpr std::uint64_t defaultSleepMySqlS{20};

	///@}

	/*
	 * DECLARATION
	 */

	//! Configuration for parsers.
	class Config : protected Module::Config {
	public:
		///@name Configuration
		///@{

		//! Configuration entries for parser threads.
		/*!
		 * \warning Changing the configuration requires
		 *   updating @c json/parser.json in @c
		 *   crawlserv_frontend!
		 */
		struct Entries {
			///@name Parser Configuration
			///@{

			//! Number of URLs fetched and parsed before saving results.
			/*!
			 * Set to zero to cache all URLs at once.
			 */
			std::uint64_t generalCacheSize{defaultCacheSize};

			//! Timeout on MySQL query execution, in milliseconds.
			std::uint64_t generalDbTimeOut{0};

			//! URL locking time, in seconds.
			std::uint32_t generalLock{defaultLockS};

			//! Level of logging activity.
			std::uint8_t generalLogging{generalLoggingDefault};

			//! Maximum number of URLs processed in one MySQL query.
			std::uint16_t generalMaxBatchSize{defaultMaxBatchSize};

			//! Specifies whether to parse only the newest content for each URL.
			bool generalNewestOnly{true};

			//! Specifies whether to include custom URLs when parsing.
			bool generalParseCustom{false};

			//! Specifies whether to re-parse already parsed URLs.
			bool generalReParse{false};

			//! Table name to save parsed data to.
			std::string generalResultTable;

			//! Queries on URLs that will not be parsed.
			std::vector<std::uint64_t> generalSkip;

			//! Time to wait before checking for new URLs when all URLs have been parsed, in milliseconds.
			std::uint64_t generalSleepIdle{defaultSleepIdleMs};

			//! Time to wait before last try to re-connect to MySQL server, in seconds.
			std::uint64_t generalSleepMySql{defaultSleepMySqlS};

			//! Specifies whether to calculate timing statistics.
			bool generalTiming{false};

			///@}
			///@name Parsing
			///@{

			//! Content matching one of these queries will be excluded from parsing.
			std::vector<std::uint64_t> parsingContentIgnoreQueries;

			//! Format of the date/time to be parsed by the date/time query with the same array index.
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
			 *  \sa parsingDateTimeSources,
			 *    parsingDateTimeQueries,
			 *    parsingDateTimeLocale,
			 *    Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> parsingDateTimeFormats;

			//! Locale to be used by the date/time query with the same array index.
			/*!
			 * \sa parsingDateTimeSources,
			 *    parsingDateTimeQueries,
			 *    parsingDateTimeLocale,
			 *    Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> parsingDateTimeLocales;

			//! Queries used for parsing the date/time.
			/*!
			 * The first query that returns a non-empty result will be used.
			 *
			 * \sa parsingDateTimeSources
			 */
			std::vector<std::uint64_t> parsingDateTimeQueries;

			//! Where to parse the date/time from – the URL itself, or the crawled content belonging to the URL.
			/*!
			 * \sa parsingSourceUrl,
			 *   parsingSourceContent,
			 *   parsingDateTimeQueries
			 */
			std::vector<std::uint16_t> parsingDateTimeSources;

			//! Specifies whether to write a warning to the log if no date/time could be parsed although a query is specified.
			/*!
			 * \note Logging needs to be enabled in order
			 *   for this option to have any effect.
			 */
			bool parsingDateTimeWarningEmpty{true};

			//! Date/time format of the field with the same array index.
			/*!
			 * If not specified, no date/time
			 *  conversion will be performed.
			 *
			 * See Howard E. Hinnant's
			 *  <a href="https://howardhinnant.github.io/date/date.html#from_stream_formatting">
			 *  C++ @c date.h library
			 *  documentation</a> for details.
			 *
			 * Set a string to @c UNIX to parse Unix timestamps,
			 *  i.e. seconds since the Unix epoch, instead.
			 *
			 *  \sa parsingFieldQueries,
			 *    parsingFieldDateTimeLocales,
			 *    Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> parsingFieldDateTimeFormats;

			//! Locale to be used by the query with the same array index.
			/*!
			 * \sa parsingFieldQueries,
			 *    parsingFieldDateTimeFormats,
			 *    Helper::DateTime::convertCustomDateTimeToSQLTimeStamp
			 */
			std::vector<std::string> parsingFieldDateTimeLocales;

			//! Delimiter between multiple results for the field with the same array index, if not saved as JSON.
			/*!
			 *  Only the first character of the string, @c \\n
			 *  (default), @c \\t, or @c \\\\ will be used.
			 */
			std::vector<char> parsingFieldDelimiters;

			//! Specifies whether to ignore empty values when parsing multiple results for the field with the same array index.
			/*!
			 * Enabled by default.
			 */
			std::vector<bool> parsingFieldIgnoreEmpty;

			//! Specifies whether to save the value of the field with the same array index as a JSON array.
			std::vector<bool> parsingFieldJSON;

			//! Name of the field with the same array index.
			std::vector<std::string> parsingFieldNames;

			//! Query for the field with the same array index.
			std::vector<std::uint64_t> parsingFieldQueries;

			//! Source of the field with the same array index – the URL itself, or the crawled content belonging to the URL.
			/*!
			 * \sa parsingSourceUrl,
			 *   parsingSourceContent,
			 *   parsingFieldQueries
			 */
			std::vector<std::uint8_t> parsingFieldSources;

			//! Specifies whether to remove line breaks and unnecessary spaces when parsing the field with the same array index.
			std::vector<bool> parsingFieldTidyTexts;

			//! Specifies whether to write a warning to the log if the field with the same array index is empty.
			/*!
			 * \note Logging needs to be enabled in order
			 *   for this option to have any effect.
			 */
			std::vector<bool> parsingFieldWarningsEmpty;

			//! Parsed IDs to be ignored.
			std::vector<std::string> parsingIdIgnore;

			//! Queries to parse the ID.
			/*!
			 * The first query that returns a non-empty
			 *  result will be used. Datasets with duplicate
			 *  or empty IDs will not be parsed.
			 *
			 * \sa parsingIdSources
			 */
			std::vector<std::uint64_t> parsingIdQueries;

			//! Where to parse the ID from when using the ID query with the same array index –  – the URL itself, or the crawled content belonging to the URL.
			/*!
			 * \sa parsingSourceUrl,
			 *   parsingSourceContent,
			 *   parsingIdQueries
			 */
			std::vector<std::uint8_t> parsingIdSources;

			//! Specifies whether to (try to) repair CData when parsing HTML/XML.
			bool parsingRepairCData{true};

			//! Specifies whether to (try to) repair broken HTML/XML comments.
			bool parsingRepairComments{true};

			//! Specifies whether to remove XML processing instructions (@c <?xml:...>) before parsing HTML content.
			bool parsingRemoveXmlInstructions{true};

			//! Number of @c tidyhtml errors to write to the log.
			/*!
			 * \note Logging needs to be enabled in order
			 *   for this option to have any effect.
			 */
			std::uint16_t parsingTidyErrors{0};

			//! Specifies whether to write @c tidyhtml warnings to the log.
			/*!
			 * \note Logging needs to be enabled in order
			 *   for this option to have any effect.
			 */
			bool parsingTidyWarnings{false};

			///@}
		}

		//! Configuration of the parser.
		config;

		///@}

		//! Class for parser configuration exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		// parsing of parsing-specific configuration
		void parseOption() override;
		void checkOptions() override;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Parses an parser-specific configuration option.
	inline void Config::parseOption() {
		this->category("general");
		this->option("cache.size", this->config.generalCacheSize);
		this->option("db.timeout", this->config.generalDbTimeOut);
		this->option("logging", this->config.generalLogging);
		this->option("max.batch.size", this->config.generalMaxBatchSize);
		this->option("newest.only", this->config.generalNewestOnly);
		this->option("parse.custom", this->config.generalParseCustom);
		this->option("reparse", this->config.generalReParse);
		this->option("skip", this->config.generalSkip);
		this->option("sleep.idle", this->config.generalSleepIdle);
		this->option("sleep.mysql", this->config.generalSleepMySql);
		this->option(
				"target.table",
				this->config.generalResultTable,
				StringParsingOption::SQL
		);
		this->option("timing", this->config.generalTiming);

		this->category("parser");
		this->option("content.ignore.queries", this->config.parsingContentIgnoreQueries);
		this->option("datetime.formats", this->config.parsingDateTimeFormats);
		this->option("datetime.locales", this->config.parsingDateTimeLocales);
		this->option("datetime.queries", this->config.parsingDateTimeQueries);
		this->option("datetime.sources", this->config.parsingDateTimeSources);
		this->option("datetime.warning.empty", this->config.parsingDateTimeWarningEmpty);
		this->option("field.datetime.formats", this->config.parsingFieldDateTimeFormats);
		this->option("field.datetime.locales", this->config.parsingFieldDateTimeLocales);
		this->option(
				"field.delimiters",
				this->config.parsingFieldDelimiters,
				CharParsingOption::FromString
		);
		this->option("field.ignore.empty", this->config.parsingFieldIgnoreEmpty);
		this->option("field.json", this->config.parsingFieldJSON);
		this->option(
				"field.names",
				this->config.parsingFieldNames,
				StringParsingOption::SQL
		);
		this->option("field.queries", this->config.parsingFieldQueries);
		this->option("field.sources", this->config.parsingFieldSources);
		this->option("field.tidy.texts", this->config.parsingFieldTidyTexts);
		this->option("field.warnings.empty", this->config.parsingFieldWarningsEmpty);
		this->option("id.ignore", this->config.parsingIdIgnore);
		this->option("id.queries", this->config.parsingIdQueries);
		this->option("id.sources", this->config.parsingIdSources);
		this->option("remove.xml.instructions", this->config.parsingRemoveXmlInstructions);
		this->option("repair.cdata", this->config.parsingRepairCData);
		this->option("repair.comments", this->config.parsingRepairComments);
		this->option("tidy.errors", this->config.parsingTidyErrors);
		this->option("tidy.warnings", this->config.parsingTidyWarnings);
	}

	//! Checks the parser-specific configuration options.
	/*!
	 * \throws Module::Extractor::Config::Exception
	 *   if no target table has been specified.
	 */
	inline void Config::checkOptions() {
		// check for target table
		if(this->config.generalResultTable.empty()) {
			throw Exception(
					"Parser::Config::checkOptions():"
					" No target table has been specified."
			);
		}
		
		// check properties of date/time queries
		const auto completeDateTimes{
			std::min( // number of complete date/time queries (= min. size of all arrays)
					this->config.parsingDateTimeQueries.size(),
					this->config.parsingDateTimeSources.size()
			)
		};

		bool incompleteDateTimes{false};

		// remove date/time queries or sources that are not used
		if(this->config.parsingDateTimeQueries.size() > completeDateTimes) {
			this->config.parsingDateTimeQueries.resize(completeDateTimes);

			incompleteDateTimes = true;
		}
		else if(this->config.parsingDateTimeSources.size() > completeDateTimes) {
			this->config.parsingDateTimeSources.resize(completeDateTimes);

			incompleteDateTimes = true;
		}

		// warn about incomplete date/time queries
		if(incompleteDateTimes) {
			this->warning(
					"\'datetime.queries\', \'.sources\'"
					" should have the same number of elements."
			);

			this->warning(
					"Incomplete date/time queries removed from configuration."
			);

			incompleteDateTimes = false;
		}

		// remove date/time formats that are not used, add empty format where none is specified
		if(this->config.parsingDateTimeFormats.size() > completeDateTimes) {
			incompleteDateTimes = true;
		}

		this->config.parsingDateTimeFormats.resize(completeDateTimes);

		// replace empty date/time formats with "%F %T"
		std::replace_if(
				this->config.parsingDateTimeFormats.begin(),
				this->config.parsingDateTimeFormats.end(),
				[](const auto& str) {
					return str.empty();
				},
				"%F %T"
		);

		// remove date/time locales that are not used, add empty locale where none is specified
		if(this->config.parsingDateTimeLocales.size() > completeDateTimes) {
			incompleteDateTimes = true;
		}

		this->config.parsingDateTimeLocales.resize(completeDateTimes);

		// warn about unused properties
		if(incompleteDateTimes) {
			this->warning("Unused date/time properties removed from configuration.");
		}

		// check properties of parsing fields
		const auto completeFields{
			std::min({ // number of complete fields (= min. size of all arrays)
					this->config.parsingFieldNames.size(),
					this->config.parsingFieldQueries.size(),
					this->config.parsingFieldSources.size()
			})
		};

		bool incompleteFields{false};

		// remove names of incomplete parsing fields
		if(this->config.parsingFieldNames.size() > completeFields) {
			this->config.parsingFieldNames.resize(completeFields);

			incompleteFields = true;
		}

		// remove queries of incomplete parsing fields
		if(this->config.parsingFieldQueries.size() > completeFields) {
			this->config.parsingFieldQueries.resize(completeFields);

			incompleteFields = true;
		}

		// remove sources of incomplete parsing fields
		if(this->config.parsingFieldSources.size() > completeFields) {
			this->config.parsingFieldSources.resize(completeFields);

			incompleteFields = true;
		}

		// warn about incomplete parsing fields
		if(incompleteFields) {
			this->warning(
					"\'field.names\', \'.queries\' and \'.sources\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete field(s) removed from configuration.");

			incompleteFields = false;
		}

		// remove date/time formats that are not used, add empty format where none is specified
		if(this->config.parsingFieldDateTimeFormats.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.parsingFieldDateTimeFormats.resize(completeFields);

		// remove date/time locales that are not used, add empty locale where none is specified
		if(this->config.parsingFieldDateTimeLocales.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.parsingFieldDateTimeLocales.resize(completeFields);

		// remove field delimiters that are not used, add empty delimiter (\0) where none is specified
		if(this->config.parsingFieldDelimiters.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.parsingFieldDelimiters.resize(completeFields, '\0');

		// replace all empty field delimiters with '\n'
		std::replace_if(
				this->config.parsingFieldDelimiters.begin(),
				this->config.parsingFieldDelimiters.end(),
				[](char c) {
					return c == '\0';
				},
				'\n'
		);

		// remove 'ignore empty values' properties that are not used, set to 'true' where none is specified
		if(this->config.parsingFieldIgnoreEmpty.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.parsingFieldIgnoreEmpty.resize(completeFields, true);

		// remove 'save field as JSON' properties that are not used, set to 'false' where none is specified
		if(this->config.parsingFieldJSON.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.parsingFieldJSON.resize(completeFields, false);

		// remove 'tidy text' properties that are not used, set to 'false' where none is specified
		if(this->config.parsingFieldTidyTexts.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.parsingFieldTidyTexts.resize(completeFields, false);

		// remove 'warning if empty' properties that are not used, set to 'false' where none is specified
		if(this->config.parsingFieldWarningsEmpty.size() > completeFields) {
			incompleteFields = true;
		}

		this->config.parsingFieldWarningsEmpty.resize(completeFields, false);

		// warn about unused properties
		if(incompleteFields) {
			this->warning("Unused field properties removed from configuration.");
		}

		// check properties of ID queries
		const auto completeIds{
			std::min( // number of complete ID queries (= min. size of all arrays)
					this->config.parsingIdQueries.size(),
					this->config.parsingIdSources.size()
			)
		};

		bool incompleteIds{false};

		// remove ID queries or sources that are not used
		if(this->config.parsingIdQueries.size() > completeIds) {
			this->config.parsingIdQueries.resize(completeIds);

			incompleteIds = true;
		}
		else if(this->config.parsingIdSources.size() > completeIds) {
			this->config.parsingIdSources.resize(completeIds);

			incompleteIds = true;
		}

		// warn about incomplete ID queries
		if(incompleteIds) {
			this->warning(
					"\'id.queries\' and \'.sources\'"
					" should have the same number of elements."
			);

			this->warning("Incomplete ID queries removed from configuration.");
		}
	}

} /* namespace crawlservpp::Module::Parser */

#endif /* MODULE_PARSER_CONFIG_HPP_ */
