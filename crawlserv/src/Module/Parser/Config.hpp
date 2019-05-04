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
#include "../../Network/Config.hpp"

#include <algorithm>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Module::Parser {

	/*
	 * DECLARATION
	 */

	class Config : public Module::Config {
	public:
		Config() {};
		virtual ~Config() {};

		// configuration constants
		static const unsigned short generalLoggingSilent = 0;
		static const unsigned short generalLoggingDefault = 1;
		static const unsigned short generalLoggingExtended = 2;
		static const unsigned short generalLoggingVerbose = 3;
		static const unsigned short parsingSourceUrl = 0;
		static const unsigned short parsingSourceContent = 1;

		// configuration entries
		struct Entries {
			// constructor with default values
			Entries();

			// general entries
			unsigned long generalCacheSize;
			unsigned int generalLock;
			unsigned short generalLogging;

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
			std::vector<unsigned short> parsingFieldSources;
			std::vector<bool> parsingFieldTidyTexts;
			std::vector<bool> parsingFieldWarningsEmpty;
			std::vector<std::string> parsingIdIgnore;
			std::vector<unsigned long> parsingIdQueries;
			std::vector<unsigned short> parsingIdSources;
			bool parsingRepairCData;
		} config;

		// sub-class for Module::Config exceptions
		class Exception : public Main::Exception {
			public:
				Exception(const std::string& description) : Main::Exception(description) {}
				virtual ~Exception() {}
			};

	protected:
		// parsing-specific configuration parsing
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
		
		// check properties of datetime queries
		unsigned long completeDateTimes = std::min( // number of complete datetime queries (= minimum size of all arrays)
				this->config.parsingDateTimeQueries.size(),
				this->config.parsingDateTimeSources.size()
		);

		bool incompleteDateTimes = false;

		// the 'date/time format' property will be ignored if array is too large or set to "%F %T" if entry is missing
		if(this->config.parsingDateTimeFormats.size() > completeDateTimes)
			this->config.parsingDateTimeFormats.resize(completeDateTimes);
		else {
			this->config.parsingDateTimeFormats.reserve(completeDateTimes);

			while(this->config.parsingDateTimeFormats.size() < completeDateTimes)
				this->config.parsingDateTimeFormats.emplace_back("%F %T");
		}

		// ...and empty 'date/time format' properties will also be replaced by the default value "%F %T"
		for(auto i = this->config.parsingDateTimeFormats.begin(); i != this->config.parsingDateTimeFormats.end(); ++i)
			if(i->empty()) *i = "%F %T";

		// the 'locales' property will be ignored if array is too large or set to "" if entry is missing
		if(this->config.parsingDateTimeLocales.size() > completeDateTimes)
			this->config.parsingDateTimeLocales.resize(completeDateTimes);
		else {
			this->config.parsingDateTimeLocales.reserve(completeDateTimes);

			while(this->config.parsingDateTimeLocales.size() < completeDateTimes)
				this->config.parsingDateTimeLocales.emplace_back();
		}

		if(this->config.parsingDateTimeQueries.size() > completeDateTimes) {
			// remove queries of incomplete datetime queries
			this->config.parsingDateTimeQueries.resize(completeDateTimes);

			incompleteDateTimes = true;
		}

		if(this->config.parsingDateTimeSources.size() > completeDateTimes) {
			// remove sources of incomplete datetime queries
			this->config.parsingDateTimeSources.resize(completeDateTimes);

			incompleteDateTimes = true;
		}

		if(incompleteDateTimes) {
			// warn about incomplete datetime queries
			this->warning("\'datetime.queries\' and \'.sources\' should have the same number of elements.");
			this->warning("Incomplete datetime queries removed.");
		}

		// check properties of parsing fields (arrays with field names, queries and sources should have the same number of elements)
		unsigned long completeFields = std::min({ // number of complete fields (= minimum size of all arrays)
				this->config.parsingFieldNames.size(),
				this->config.parsingFieldQueries.size(),
				this->config.parsingFieldSources.size()
		});

		bool incompleteFields = false;

		// the 'delimiter' property will be ignored if array is too large or set to '\n' if entry is missing
		if(this->config.parsingFieldDelimiters.size() > completeFields)
			this->config.parsingFieldDelimiters.resize(completeFields);
		else {
			this->config.parsingFieldDelimiters.reserve(completeFields);

			while(this->config.parsingFieldDelimiters.size() < completeFields)
				this->config.parsingFieldDelimiters.push_back('\n');
		}

		// the 'ignore empty values' property will be ignored if array is too large or set to 'true' if entry is missing
		if(this->config.parsingFieldIgnoreEmpty.size() > completeFields)
			this->config.parsingFieldIgnoreEmpty.resize(completeFields);
		else {
			this->config.parsingFieldIgnoreEmpty.reserve(completeFields);

			while(this->config.parsingFieldIgnoreEmpty.size() < completeFields)
				this->config.parsingFieldIgnoreEmpty.push_back(true);
		}

		// the 'save field entry as JSON' property will be ignored if array is too large or set to 'false' if entry is missing
		if(this->config.parsingFieldJSON.size() > completeFields)
			this->config.parsingFieldJSON.resize(completeFields);
		else {
			this->config.parsingFieldJSON.reserve(completeFields);

			while(this->config.parsingFieldJSON.size() < completeFields)
				this->config.parsingFieldJSON.push_back(false);
		}

		// the 'tidy text' property will be ignored if array is too large or set to 'false' if entry is missing
		if(this->config.parsingFieldTidyTexts.size() > completeFields)
			this->config.parsingFieldTidyTexts.resize(completeFields);
		else {
			this->config.parsingFieldTidyTexts.reserve(completeFields);

			while(this->config.parsingFieldTidyTexts.size() < completeFields)
				this->config.parsingFieldTidyTexts.push_back(false);
		}

		// the 'warning if empty' property will be ignored if array is too large or set to 'false' if entry is missing
		if(this->config.parsingFieldWarningsEmpty.size() > completeFields)
			this->config.parsingFieldWarningsEmpty.resize(completeFields);
		else {
			this->config.parsingFieldWarningsEmpty.reserve(completeFields);
			while(this->config.parsingFieldWarningsEmpty.size() < completeFields)
				this->config.parsingFieldWarningsEmpty.push_back(false);
		}

		if(this->config.parsingFieldNames.size() > completeFields) {
			// remove names of incomplete parsing fields
			this->config.parsingFieldNames.resize(completeFields);

			incompleteFields = true;
		}

		if(this->config.parsingFieldQueries.size() > completeFields) {
			// remove queries of incomplete parsing fields
			this->config.parsingFieldQueries.resize(completeFields);

			incompleteFields = true;
		}

		if(this->config.parsingFieldSources.size() > completeFields) {
			// remove sources of incomplete parsing fields
			this->config.parsingFieldSources.resize(completeFields);

			incompleteFields = true;
		}

		if(incompleteFields) {
			// warn about incomplete parsing fields
			this->warning("\'field.names\', \'.queries\' and \'.sources\' should have the same number of elements.");
			this->warning("Incomplete field(s) removed.");
		}

		// check properties of ID queries
		unsigned long completeIds = std::min( // number of complete ID queries (= minimum size of all arrays)
				this->config.parsingIdQueries.size(),
				this->config.parsingIdSources.size()
		);

		bool incompleteIds = false;

		if(this->config.parsingIdQueries.size() > completeIds) {
			// remove queries of incomplete ID queries
			this->config.parsingIdQueries.resize(completeIds);

			incompleteIds = true;
		}

		if(this->config.parsingIdSources.size() > completeIds) {
			// remove sources of incomplete ID queries
			this->config.parsingIdSources.resize(completeIds);

			incompleteIds = true;
		}

		if(incompleteIds) {
			// warn about incomplete ID queries
			this->warning("\'id.queries\' and \'.sources\' should have the same number of elements.");
			this->warning("Incomplete ID queries removed.");
		}
	}


} /* crawlservpp::Module::Parser */

#endif /* MODULE_PARSER_CONFIG_HPP_ */
