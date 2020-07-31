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
 * Configuration properties (module, name and configuration).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */


#ifndef STRUCT_CONFIGPROPERTIES_HPP_
#define STRUCT_CONFIGPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	//! Configuration properties containing its module, name, and JSON string.
	struct ConfigProperties {
		///@name Properties
		///@{

		//! The name of the module using the configuration.
		std::string module;

		//! The name of the configuration.
		std::string name;

		//! The configuration string containing JSON.
		std::string config;

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		ConfigProperties() = default;

		//! Constructor setting module, name and configuration string.
		/*!
		 * \param setModule Constant reference to a string
		 *   containing the name of the module.
		 * \param setName Constant reference to a string
		 *   containing the name of the configuration.
		 * \param setConfig Constant reference to a string
		 *   containing the configuration as JSON code.
		 */
		ConfigProperties(
				const std::string& setModule,
				const std::string& setName,
				const std::string& setConfig
		) : module(setModule), name(setName), config(setConfig) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_CONFIGPROPERTIES_HPP_ */

