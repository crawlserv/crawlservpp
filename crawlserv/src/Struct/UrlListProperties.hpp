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
 * URLListProperties.hpp
 *
 * Basic URL list properties (namespace and name).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_URLLISTPROPERTIES_HPP_
#define STRUCT_URLLISTPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct UrlListProperties {
		std::string nameSpace;
		std::string name;

		// constructors
		UrlListProperties() {}
		UrlListProperties(
				const std::string& setNameSpace,
				const std::string& setName
		) : nameSpace(setNameSpace), name(setName) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_URLLISTPROPERTIES_HPP_ */
