/*
 * Config.hpp
 *
 * Parsing configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/parser.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_CONFIG_HPP_
#define MODULE_PARSER_CONFIG_HPP_

#include "../../Main/Exception.hpp"
#include "../../Module/Config.hpp"

#include <algorithm>	// std::min, std::replace_if
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Module::Parser {

	/*
	 * DECLARATION
	 */

	class Config : public Module::Config {
	public:
		Config() {};
		virtual ~Config() {};

		// configuration constants
		static const unsigned char generalLoggingSilent = 0;
		static const unsigned char generalLoggingDefault = 1;
		static const unsigned char generalLoggingExtended = 2;
		static const unsigned char generalLoggingVerbose = 3;

		static const unsigned char parsingSourceUrl = 0;
		static const unsigned char parsingSourceContent = 1;

		// configuration entries
		struct Entries {
			// constructor with default values
			Entries();

			// general entries
			unsigned long generalCacheSize;
			unsigned int generalLock;
			unsigned char generalLogging;
			bool generalNewestOnly;
			bool generalParseCustom;
			bool generalReParse;
			std::string generalResultTable;
			std::vector<unsigned long> generalSkip;
			unsigned long generalSleepIdle;
			unsigned long generalSleepMySql;
			bool generalTiming;

			// parsing entries
			std::vector<std::string> parsingDateTimeFormats;
			std::vector<std::string> parsingDateTimeLocales;
			std::vector<unsigned long> parsingDateTimeQueries;
			std::vector<unsigned short> parsingDateTimeSources;
			std::vector<char> parsingFieldDelimiters;
			std::vector<bool> parsingFieldIgnoreEmpty;
			std::vector<bool> parsingFieldJSON;
			std::vector<std::string> parsingFieldNames;
			std::vector<unsigned long> parsingFieldQueries;
			std::vector<unsigned char> parsingFieldSources;
			std::vector<bool> parsingFieldTidyTexts;
			std::vector<bool> parsingFieldWarningsEmpty;
			std::vector<std::string> parsingIdIgnore;
			std::vector<unsigned long> parsingIdQueries;
			std::vector<unsigned char> parsingIdSources;
			bool parsingRepairCData;
		} config;

		// sub-class for Parser::Config exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	protected:
		// parsing of parsing-specific configuration
		void parseOption() override;
		void checkOptions() override;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set default values
	inline Config::Entries::Entries() :	generalCacheSize(2500),
										generalLock(300),
										generalLogging(Config::generalLoggingDefault),
										generalNewestOnly(true),
										generalParseCustom(false),
										generalReParse(false),
										generalSleepIdle(5000),
										generalSleepMySql(20),
										generalTiming(false),
										parsingRepairCData(true) {}

	// parse parsing-specific configuration option
	inline void Config::parseOption() {
		this->category("general");
		this->option("cache.size", this->config.generalCacheSize);
		this->option("logging", this->config.generalLogging);
		this->option("newest.only", this->config.generalNewestOnly);
		this->option("parse.custom", this->config.generalParseCustom);
		this->option("reparse", this->config.generalReParse);
		this->option("result.table", this->config.generalResultTable, StringParsingOption::SQL);
		this->option("skip", this->config.generalSkip);
		this->option("sleep.idle", this->config.generalSleepIdle);
		this->option("sleep.mysql", this->config.generalSleepMySql);
		this->option("timing", this->config.generalTiming);

		this->category("parser");
		this->option("datetime.formats", this->config.parsingDateTimeFormats);
		this->option("datetime.locales", this->config.parsingDateTimeLocales);
		this->option("datetime.queries", this->config.parsingDateTimeQueries);
		this->option("datetime.sources", this->config.parsingDateTimeSources);
		this->option("field.delimiters", this->config.parsingFieldDelimiters, CharParsingOption::FromString);
		this->option("field.ignore.empty", this->config.parsingFieldIgnoreEmpty);
		this->option("field.json", this->config.parsingFieldJSON);
		this->option("field.names", this->config.parsingFieldNames, StringParsingOption::SQL);
		this->option("field.queries", this->config.parsingFieldQueries);
		this->option("field.sources", this->config.parsingFieldSources);
		this->option("field.tidy.texts", this->config.parsingFieldTidyTexts);
		this->option("field.warnings.empty", this->config.parsingFieldWarningsEmpty);
		this->option("id.ignore", this->config.parsingIdIgnore);
		this->option("id.queries", this->config.parsingIdQueries);
		this->option("id.sources", this->config.parsingIdSources);
		this->option("repair.cdata", this->config.parsingRepairCData);
	}

	// check parsing-specific configuration, throws Config::Exception
	inline void Config::checkOptions() {
		// check for result table
		if(this->config.generalResultTable.empty())
			throw Exception("Parser::Config::checkOptions(): No result table specified.");
		
		// check properties of date/time queries
		const unsigned long completeDateTimes = std::min( // number of complete date/time queries (= minimum size of all arrays)
				this->config.parsingDateTimeQueries.size(),
				this->config.parsingDateTimeSources.size()
		);

		bool incompleteDateTimes = false;

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

			this->warning("Incomplete date/time queries removed from configuration.");

			incompleteDateTimes = false;
		}

		// remove date/time formats that are not used, add empty format where none is specified
		if(this->config.parsingDateTimeFormats.size() > completeDateTimes)
			incompleteDateTimes = true;

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
		if(this->config.parsingDateTimeLocales.size() > completeDateTimes)
			incompleteDateTimes = true;

		this->config.parsingDateTimeLocales.resize(completeDateTimes);

		// warn about unused properties
		if(incompleteDateTimes)
			this->warning("Unused date/time properties removed from configuration.");

		// check properties of parsing fields
		const unsigned long completeFields = std::min({ // number of complete fields (= minimum size of all arrays)
				this->config.parsingFieldNames.size(),
				this->config.parsingFieldQueries.size(),
				this->config.parsingFieldSources.size()
		});

		bool incompleteFields = false;

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

		// remove field delimiters that are not used, add empty delimiter (\0) where none is specified
		if(this->config.parsingFieldDelimiters.size() > completeFields)
			incompleteFields = true;

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
		if(this->config.parsingFieldIgnoreEmpty.size() > completeFields)
			incompleteFields = true;

		this->config.parsingFieldIgnoreEmpty.resize(completeFields, true);

		// remove 'save field entry' properties that are not used, set to 'false' where none is specified
		if(this->config.parsingFieldJSON.size() > completeFields)
			incompleteFields = true;

		this->config.parsingFieldJSON.resize(completeFields, false);

		// remove 'tidy text' properties that are not used, set to 'false' where none is specified
		if(this->config.parsingFieldTidyTexts.size() > completeFields)
			incompleteFields = true;

		this->config.parsingFieldTidyTexts.resize(completeFields, false);

		// remove 'warning if empty' properties that are not used, set to 'false' where none is specified
		if(this->config.parsingFieldWarningsEmpty.size() > completeFields)
			incompleteFields = true;

		this->config.parsingFieldWarningsEmpty.resize(completeFields, false);

		// warn about unused properties
		if(incompleteFields)
			this->warning("Unused field properties removed from configuration.");

		// check properties of ID queries
		const unsigned long completeIds = std::min( // number of complete ID queries (= minimum size of all arrays)
				this->config.parsingIdQueries.size(),
				this->config.parsingIdSources.size()
		);

		bool incompleteIds = false;

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


} /* crawlservpp::Module::Parser */

#endif /* MODULE_PARSER_CONFIG_HPP_ */
