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

#include "../Main/Exception.h"

#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <queue>
#include <string>

namespace crawlservpp::Module {
	class Config {
	public:
		Config();
		virtual ~Config();

		// configuration loader
		void loadConfig(const std::string& configJson, std::queue<std::string>& warningsTo);

		// sub-class for configuration exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// not moveable, not copyable
		Config(Config&) = delete;
		Config(Config&&) = delete;
		Config& operator=(Config&) = delete;
		Config& operator=(Config&&) = delete;

	protected:
		// load module-specific configuration from parsed JSON document
		virtual void loadModule(const rapidjson::Document& jsonDocument, std::queue<std::string>& warningsTo) = 0;
	};
}

#endif /* MODULE_CONFIG_H_ */
