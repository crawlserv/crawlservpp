/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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

