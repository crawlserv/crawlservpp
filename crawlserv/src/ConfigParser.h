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

#include "external/rapidjson/document.h"

#include <sstream>
#include <string>
#include <vector>

class ConfigParser {
public:
	ConfigParser();
	virtual ~ConfigParser();

	// configuration loader
	bool loadConfig(const std::string& configJson, std::vector<std::string>& warningsTo);

	// get error message
	const std::string& getErrorMessage() const;

	// parser entries
	std::vector<std::string> parserDateTimeFormats;
	std::vector<std::string> parserDateTimeLocales;
	std::vector<unsigned long> parserDateTimeQueries;
	std::vector<unsigned short> parserDateTimeSources;
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
	std::string parserResultMultiSeparator;
	std::string parserResultMultiQuotes;
	static const unsigned short parserSourceUrl = 0;
	static const unsigned short parserSourceContent = 1;
	std::string parserResultTable;
	unsigned long parserSleepIdle;
	unsigned long parserSleepMySql;

private:
	std::string errorMessage;
};

#endif /* CONFIGCRAWLER_H_ */
