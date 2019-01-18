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

// get error message
const std::string& Config::getErrorMessage() const {
	return this->errorMessage;
}

// load configuration
bool Config::loadConfig(const std::string& configJson, std::vector<std::string>& warningsTo) {
	// parse JSON
	rapidjson::Document json;
	if(json.Parse(configJson.c_str()).HasParseError()) {
		this->errorMessage = "Could not parse configuration JSON.";
		return false;
	}
	if(!json.IsArray()) {
		this->errorMessage = "Invalid configuration JSON (is no array).";
		return false;
	}

	// load module-specific configuration
	this->loadModule(json, warningsTo);
	return true;
}

}
