/*
 * ConfigModule.h
 *
 * Abstract class as base for module-specific configuration classes.
 *
 *  Created on: Jan 7, 2019
 *      Author: ans
 */

#ifndef SRC_CONFIGMODULE_H_
#define SRC_CONFIGMODULE_H_

#include "external/rapidjson/document.h"

#include <string>
#include <vector>

class ConfigModule {
public:
	ConfigModule();
	virtual ~ConfigModule();

	// get error message
	const std::string& getErrorMessage() const;

	// configuration loader
	bool loadConfig(const std::string& configJson, std::vector<std::string>& warningsTo);

protected:
	// error message
	std::string errorMessage;

	// load module-specific configuration from parsed JSON document
	virtual void loadModule(const rapidjson::Document& jsonDocument, std::vector<std::string>& warningsTo) = 0;
};

#endif /* SRC_CONFIGMODULE_H_ */
