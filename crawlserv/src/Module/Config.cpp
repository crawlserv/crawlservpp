/*
 * Config.cpp
 *
 * Abstract class as base for module-specific configuration classes.
 *
 *  Created on: Jan 7, 2019
 *      Author: ans
 */

#include "Config.h"

namespace crawlservpp::Module {

// constructor stub
Config::Config() {}

// destructor stub
Config::~Config() {}

// load configuration
void Config::loadConfig(const std::string& configJson, std::vector<std::string>& warningsTo) {
	// parse JSON
	rapidjson::Document json;
	if(json.Parse(configJson.c_str()).HasParseError())
		throw Config::Exception("Could not parse configuration JSON.");

	if(!json.IsArray())
		throw Config::Exception("Invalid configuration JSON (is no array).");

	// load module-specific configuration
	this->loadModule(json, warningsTo);
}

}
