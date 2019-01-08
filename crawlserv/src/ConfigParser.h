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

#ifndef CONFIGCRAWLER_H_
#define CONFIGCRAWLER_H_

#include "ConfigModule.h"

#include "external/rapidjson/document.h"

#include <sstream>
#include <string>
#include <vector>

class ConfigParser : public ConfigModule {
public:
	ConfigParser();
	virtual ~ConfigParser();

	// parser entries
	std::vector<std::string> parserDateTimeFormats;
	std::vector<std::string> parserDateTimeLocales;
	std::vector<unsigned long> parserDateTimeQueries;
	std::vector<unsigned short> parserDateTimeSources;
	std::vector<bool> parserFieldJSON;
	std::vector<std::string> parserFieldNames;
	std::vector<unsigned long> parserFieldQueries;
	std::vector<unsigned short> parserFieldSources;
	std::vector<unsigned long> parserIdQueries;
	std::vector<unsigned short> parserIdSources;
	bool parserLogging;
	static const unsigned short parserLoggingSilent = 0;
	static const unsigned short parserLoggingDefault = 1;
	static const unsigned short parserLoggingExtended = 2;
	static const unsigned short parserLoggingVerbose = 3;
	bool parserNewestOnly;
	bool parserReParse;
	static const unsigned short parserSourceUrl = 0;
	static const unsigned short parserSourceContent = 1;
	std::string parserResultTable;
	unsigned long parserSleepIdle;
	unsigned long parserSleepMySql;

protected:
	// load parsing-specific configuration from parsed JSON document
	void loadModule(const rapidjson::Document& jsonDocument, std::vector<std::string>& warningsTo) override;
};

#endif /* CONFIGCRAWLER_H_ */
