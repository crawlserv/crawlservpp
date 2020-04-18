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
 * ConfigItem.hpp
 *
 * Configuration item (category, name, value).
 *
 *  Created on: Mar 9, 2019
 *      Author: ans
 */

#ifndef STRUCT_CONFIGITEM_HPP_
#define STRUCT_CONFIGITEM_HPP_

#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <string>	// std::string

namespace crawlservpp::Struct {

	/*
	 * DECLARATION
	 */

	struct ConfigItem {
		std::string category;
		std::string name;
		const rapidjson::Value * value;

		std::string str();
	};

	/*
	 * IMPLEMENTATION
	 */

	// stringify category and name
	inline std::string ConfigItem::str() {
		return std::string(category + "." + name);
	}

} /* crawlservpp::Struct */

#endif /* SRC_STRUCT_CONFIGITEM_HPP_ */
