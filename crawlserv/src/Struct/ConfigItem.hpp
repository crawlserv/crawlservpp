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

#define RAPIDJSON_HAS_STDSTRING 1
#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <string>

namespace crawlservpp::Struct {

	/*
	 * DECLARATION
	 */

	struct ConfigItem {
		std::string category;
		std::string name;
		const rapidjson::Value * value;

		std::string str(); // stringify category and name
	};

	/*
	 * IMPLEMENTATION
	 */

	inline std::string ConfigItem::str() {
		return std::string(category + "." + name);
	}

} /* crawlservpp::Struct */

#endif /* SRC_STRUCT_CONFIGITEM_HPP_ */
