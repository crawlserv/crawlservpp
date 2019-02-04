/*
 * Config.h
 *
 * Abstract class as base for module-specific configuration classes.
 *
 *  Created on: Jan 7, 2019
 *      Author: ans
 */

#ifndef MODULE_CONFIG_H_
#define MODULE_CONFIG_H_

#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace crawlservpp::Module {
	class Config {
	public:
		Config();
		virtual ~Config();

		// get error message
		const std::string& getErrorMessage() const;

		// configuration loader
		void loadConfig(const std::string& configJson, std::vector<std::string>& warningsTo);

	protected:
		// load module-specific configuration from parsed JSON document
		virtual void loadModule(const rapidjson::Document& jsonDocument, std::vector<std::string>& warningsTo) = 0;
	};
}

#endif /* MODULE_CONFIG_H_ */
