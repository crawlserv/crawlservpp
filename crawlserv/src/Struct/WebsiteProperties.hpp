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
 * WebsiteProperties.hpp
 *
 * Website properties (domain, namespace, name and data directory).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_WEBSITEPROPERTIES_HPP_
#define STRUCT_WEBSITEPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	//! Website properties containing its domain, namespace, name, and data directory.
	struct WebsiteProperties {
		///@name Properties
		///@{

		//! The domain of the website.
		/*!
		 * If the domain is empty, the
		 *  website is cross-domain.
		 */
		std::string domain;

		//! The namespace of the website.
		std::string nameSpace;

		//! The name of the website.
		std::string name;

		//! The data directory to be used by the website.
		/*!
		 * If the directory is empty,
		 *  the default data directory
		 *  will be used.
		 */
		std::string dir;

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		WebsiteProperties() = default;

		//! Constructor setting website properties.
		/*!
		 * \param setDomain Constant reference
		 *   to a string containing the domain
		 *   of the website. If the string is
		 *   empty, the website is cross-domain.
		 * \param setNameSpace Constant reference
		 *   to a string containing the namespace
		 *   of the website.
		 * \param setName Constant reference to
		 *   a string containing the name of the
		 *   website.
		 * \param setDir Constant reference to a
		 *   string containing the data directory
		 *   to be used by the website. If the
		 *   string is empty, the default data
		 *   directory will be used.
		 */
		WebsiteProperties(
				const std::string& setDomain,
				const std::string& setNameSpace,
				const std::string& setName,
				const std::string& setDir)
				: domain(setDomain), nameSpace(setNameSpace), name(setName), dir(setDir) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_WEBSITEPROPERTIES_HPP_ */
