/*
 * ConfigProperties.h
 *
 * Basic configuration properties (module, name and configuration).
 *
 * NOTE: module will be ignored on update!
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */


#ifndef STRUCT_CONFIGPROPERTIES_H_
#define STRUCT_CONFIGPROPERTIES_H_

#include <string>

namespace crawlservpp::Struct {
	struct ConfigProperties {
		std::string module;
		std::string name;
		std::string config;

		// constructors
		ConfigProperties() {}
		ConfigProperties(const std::string& setModule, const std::string& setName, const std::string& setConfig)
				: module(setModule), name(setName), config(setConfig) {}
		ConfigProperties(const std::string& setName, const std::string& setConfig)
				: ConfigProperties("", setName, setConfig) {}
	};
}

#endif /* STRUCT_CONFIGPROPERTIES_H_ */

