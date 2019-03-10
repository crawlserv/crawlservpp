/*
 * Config.h
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
		Config();
		virtual ~Config();

		// general entries
		unsigned long generalCacheSize;
		unsigned int generalLock;
		unsigned short generalLogging;
		static const unsigned short generalLoggingSilent = 0;
		static const unsigned short generalLoggingDefault = 1;
		static const unsigned short generalLoggingExtended = 2;
		static const unsigned short generalLoggingVerbose = 3;
		bool generalNewestOnly;
		bool generalParseCustom;
		bool generalReParse;
		bool generalResetOnFinish;
		std::string generalResultTable;
		std::vector<unsigned long> generalSkip;
		unsigned long generalSleepIdle;
		unsigned long generalSleepMySql;
		unsigned long generalTimeoutTargetLock;
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
		static const unsigned short parsingSourceUrl = 0;
		static const unsigned short parsingSourceContent = 1;

	protected:
		// load parsing-specific configuration from parsed JSON document
		void loadModule(const rapidjson::Document& jsonDocument, std::queue<std::string>& warningsTo) override;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set default values
	inline Config::Config() :	generalCacheSize(2500),
								generalLock(300),
								generalLogging(Config::generalLoggingDefault),
								generalNewestOnly(true),
								generalParseCustom(false),
								generalReParse(false),
								generalResetOnFinish(false),
								generalSleepIdle(500),
								generalSleepMySql(20),
								generalTimeoutTargetLock(30),
								generalTiming(false) {}

	// destructor stub
	inline Config::~Config() {}

	// load parsing-specific configuration from parsed JSON document
	inline void Config::loadModule(const rapidjson::Document& jsonDocument, std::queue<std::string>& warningsTo) {
		// set logging queue
		this->setLog(warningsTo);

		// parse configuration entries
		for(rapidjson::Value::ConstValueIterator i = jsonDocument.Begin(); i != jsonDocument.End(); i++) {
			this->begin(i);

			this->category("general");
			this->option("cache.size", this->generalCacheSize);
			this->option("logging", this->generalLogging);
			this->option("newest.only", this->generalNewestOnly);
			this->option("parse.custom", this->generalParseCustom);
			this->option("reparse", this->generalReParse);
			this->option("reset.on.finish", this->generalResetOnFinish);
			this->option("result.table", this->generalResultTable, StringParsingOption::SQL);
			this->option("skip", this->generalSkip);
			this->option("sleep.idle", this->generalSleepIdle);
			this->option("sleep.mysql", this->generalSleepMySql);
			this->option("timeout.target.lock", this->generalTimeoutTargetLock);
			this->option("timing", this->generalTiming);

			this->category("parser");
			this->option("datetime.formats", this->parsingDateTimeFormats);
			this->option("datetime.locales", this->parsingDateTimeLocales);
			this->option("datetime.queries", this->parsingDateTimeQueries);
			this->option("datetime.sources", this->parsingDateTimeSources);
			this->option("field.delimiters", this->parsingFieldDelimiters, CharParsingOption::FromString);
			this->option("field.ignore.empty", this->parsingFieldIgnoreEmpty);
			this->option("field.json", this->parsingFieldJSON);
			this->option("field.names", this->parsingFieldNames, StringParsingOption::SQL);
			this->option("field.queries", this->parsingFieldQueries);
			this->option("field.sources", this->parsingFieldSources);
			this->option("field.tidy.texts", this->parsingFieldTidyTexts);
			this->option("field.warnings.empty", this->parsingFieldWarningsEmpty);
			this->option("id.ignore", this->parsingIdIgnore);
			this->option("id.queries", this->parsingIdQueries);
			this->option("id.sources", this->parsingIdSources);

			this->end();
		}

		// check properties of datetime queries
		unsigned long completeDateTimes = std::min( // number of complete datetime queries (= minimum size of all arrays)
				this->parsingDateTimeQueries.size(),
				this->parsingDateTimeSources.size()
		);

		bool incompleteDateTimes = false;

		// the 'date/time format' property will be ignored if array is too large or set to "%F %T" if entry is missing
		if(this->parsingDateTimeFormats.size() > completeDateTimes) this->parsingDateTimeFormats.resize(completeDateTimes);
		else {
			this->parsingDateTimeFormats.reserve(completeDateTimes);
			while(this->parsingDateTimeFormats.size() < completeDateTimes)
				this->parsingDateTimeFormats.emplace_back("%F %T");
		}

		// ...and empty 'date/time format' properties will also be replaced by the default value "%F %T"
		for(auto i = this->parsingDateTimeFormats.begin(); i != this->parsingDateTimeFormats.end(); ++i)
			if(i->empty()) *i = "%F %T";

		// the 'locales' property will be ignored if array is too large or set to "" if entry is missing
		if(this->parsingDateTimeLocales.size() > completeDateTimes) this->parsingDateTimeLocales.resize(completeDateTimes);
		else {
			this->parsingDateTimeLocales.reserve(completeDateTimes);
			while(this->parsingDateTimeLocales.size() < completeDateTimes)
				this->parsingDateTimeLocales.emplace_back();
		}

		if(this->parsingDateTimeQueries.size() > completeDateTimes) {
			// remove queries of incomplete datetime queries
			this->parsingDateTimeQueries.resize(completeDateTimes);
			incompleteDateTimes = true;
		}
		if(this->parsingDateTimeSources.size() > completeDateTimes) {
			// remove sources of incomplete datetime queries
			this->parsingDateTimeSources.resize(completeDateTimes);
			incompleteDateTimes = true;
		}
		if(incompleteDateTimes) {
			// warn about incomplete datetime queries
			warningsTo.emplace("\'datetime.queries\' and \'.sources\' should have the same number of elements.");
			warningsTo.emplace("Incomplete datetime queries removed.");
		}

		// check properties of parsing fields (arrays with field names, queries and sources should have the same number of elements)
		unsigned long completeFields = std::min({ // number of complete fields (= minimum size of all arrays)
				this->parsingFieldNames.size(),
				this->parsingFieldQueries.size(),
				this->parsingFieldSources.size()
		});
		bool incompleteFields = false;

		// the 'delimiter' property will be ignored if array is too large or set to '\n' if entry is missing
		if(this->parsingFieldDelimiters.size() > completeFields) this->parsingFieldDelimiters.resize(completeFields);
		else {
			this->parsingFieldDelimiters.reserve(completeFields);
			while(this->parsingFieldDelimiters.size() < completeFields)
				this->parsingFieldDelimiters.push_back('\n');
		}

		// the 'ignore empty values' property will be ignored if array is too large or set to 'true' if entry is missing
		if(this->parsingFieldIgnoreEmpty.size() > completeFields) this->parsingFieldIgnoreEmpty.resize(completeFields);
		else {
			this->parsingFieldIgnoreEmpty.reserve(completeFields);
			while(this->parsingFieldIgnoreEmpty.size() < completeFields)
				this->parsingFieldIgnoreEmpty.push_back(true);
		}

		// the 'save field entry as JSON' property will be ignored if array is too large or set to 'false' if entry is missing
		if(this->parsingFieldJSON.size() > completeFields) this->parsingFieldJSON.resize(completeFields);
		else {
			this->parsingFieldJSON.reserve(completeFields);
			while(this->parsingFieldJSON.size() < completeFields)
				this->parsingFieldJSON.push_back(false);
		}

		// the 'tidy text' property will be ignored if array is too large or set to 'false' if entry is missing
		if(this->parsingFieldTidyTexts.size() > completeFields) this->parsingFieldTidyTexts.resize(completeFields);
		else {
			this->parsingFieldTidyTexts.reserve(completeFields);
			while(this->parsingFieldTidyTexts.size() < completeFields)
				this->parsingFieldTidyTexts.push_back(false);
		}

		// the 'warning if empty' property will be ignored if array is too large or set to 'false' if entry is missing
		if(this->parsingFieldWarningsEmpty.size() > completeFields)
			this->parsingFieldWarningsEmpty.resize(completeFields);
		else {
			this->parsingFieldWarningsEmpty.reserve(completeFields);
			while(this->parsingFieldWarningsEmpty.size() < completeFields)
				this->parsingFieldWarningsEmpty.push_back(false);
		}

		if(this->parsingFieldNames.size() > completeFields) {
			// remove names of incomplete parsing fields
			this->parsingFieldNames.resize(completeFields);
			incompleteFields = true;
		}
		if(this->parsingFieldQueries.size() > completeFields) {
			// remove queries of incomplete parsing fields
			this->parsingFieldQueries.resize(completeFields);
			incompleteFields = true;
		}
		if(this->parsingFieldSources.size() > completeFields) {
			// remove sources of incomplete parsing fields
			this->parsingFieldSources.resize(completeFields);
			incompleteFields = true;
		}
		if(incompleteFields) {
			// warn about incomplete parsing fields
			warningsTo.emplace("\'field.names\', \'.queries\' and \'.sources\' should have the same number of elements.");
			warningsTo.emplace("Incomplete field(s) removed.");
		}

		// check properties of id queries (arrays defining queries should have the same number of elements - one for each query)
		unsigned long completeIds =
				std::min(this->parsingIdQueries.size(), this->parsingIdSources.size());	// number of complete id queries
																						// (= minimum size of all arrays)
		bool incompleteIds = false;
		if(this->parsingIdQueries.size() > completeIds) {
			// remove queries of incomplete id queries
			this->parsingIdQueries.resize(completeIds);
			incompleteIds = true;
		}
		if(this->parsingIdSources.size() > completeIds) {
			// remove sources of incomplete id queries
			this->parsingIdSources.resize(completeIds);
			incompleteIds = true;
		}
		if(incompleteIds) {
			// warn about incomplete id queries
			warningsTo.emplace("\'id.queries\' and \'.sources\' should have the same number of elements.");
			warningsTo.emplace("Incomplete id queries removed.");
		}
	}

} /* crawlservpp::Module::Parser */

#endif /* MODULE_PARSER_CONFIG_HPP_ */
