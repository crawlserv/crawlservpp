/*
 * ConfigProperties.hpp
 *
 * Basic configuration properties (module, name and configuration).
 *
 * NOTE: module will be ignored on update!
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */


#ifndef STRUCT_CONFIGPROPERTIES_HPP_
#define STRUCT_CONFIGPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct ConfigProperties {
		std::string module;
		std::string name;
		std::string config;

		// constructors
		ConfigProperties() {}
		ConfigProperties(
				const std::string& setModule,
				const std::string& setName,
				const std::string& setConfig
		) : module(setModule), name(setName), config(setConfig) {}
		ConfigProperties(
				const std::string& setName,
				const std::string& setConfig
		) : ConfigProperties("", setName, setConfig) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_CONFIGPROPERTIES_HPP_ */

