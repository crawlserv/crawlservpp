/*
 * ConfigParser.h
 *
 * Parsing configuration.
 *
 * WARNING:	Changing the configuration requires updating 'json/parser.json' in crawlserv_frontend!
 * 			See there for details on the specific configuration entries.
 *
 *  Created on: Oct 25, 2018
 *      Author: ans
 */

#ifndef CONFIGPARSER_H_
#define CONFIGPARSER_H_

#include "ConfigModule.h"

#include "namespaces/Strings.h"

#include "external/rapidjson/document.h"

#include <sstream>
#include <string>
#include <vector>

class ConfigParser : public ConfigModule {
public:
	ConfigParser();
	virtual ~ConfigParser();

	// general entries
	unsigned int generalLock;
	unsigned short generalLogging;
	static const unsigned short generalLoggingSilent = 0;
	static const unsigned short generalLoggingDefault = 1;
	static const unsigned short generalLoggingExtended = 2;
	static const unsigned short generalLoggingVerbose = 3;
	bool generalNewestOnly;
	bool generalReParse;
	bool generalResetOnFinish;
	std::string generalResultTable;
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
	std::vector<unsigned long> parsingIdQueries;
	std::vector<unsigned short> parsingIdSources;
	static const unsigned short parsingSourceUrl = 0;
	static const unsigned short parsingSourceContent = 1;

protected:
	// load parsing-specific configuration from parsed JSON document
	void loadModule(const rapidjson::Document& jsonDocument, std::vector<std::string>& warningsTo) override;
};

#endif /* CONFIGPARSER_H_ */
