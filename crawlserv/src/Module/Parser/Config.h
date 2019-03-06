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

#ifndef MODULE_PARSER_CONFIG_H_
#define MODULE_PARSER_CONFIG_H_

#include "../Config.h"

#include "../../Helper/Algos.h"
#include "../../Helper/Strings.h"

#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace crawlservpp::Module::Parser {
	class Config : public Module::Config {
	public:
		Config();
		virtual ~Config();

		// general entries
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
}

#endif /* MODULE_PARSER_CONFIG_H_ */
