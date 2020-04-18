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
 * Basic website properties (domain, namespace and name).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_WEBSITEPROPERTIES_HPP_
#define STRUCT_WEBSITEPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct WebsiteProperties {
		std::string domain;
		std::string nameSpace;
		std::string name;
		std::string dir;

		// constructors
		WebsiteProperties() {}
		WebsiteProperties(
				const std::string& setDomain,
				const std::string& setNameSpace,
				const std::string& setName,
				const std::string& setDir)
				: domain(setDomain), nameSpace(setNameSpace), name(setName), dir(setDir) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_WEBSITEPROPERTIES_HPP_ */
