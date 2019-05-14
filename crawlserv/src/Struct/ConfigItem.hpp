/*
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
