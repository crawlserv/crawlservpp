/*
 * URLListProperties.hpp
 *
 * Basic URL list properties (namespace and name).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_URLLISTPROPERTIES_HPP_
#define STRUCT_URLLISTPROPERTIES_HPP_

#include <string>

namespace crawlservpp::Struct {

	struct UrlListProperties {
		std::string nameSpace;
		std::string name;

		// constructors
		UrlListProperties() {}
		UrlListProperties(const std::string& setNameSpace, const std::string& setName) : nameSpace(setNameSpace), name(setName) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_URLLISTPROPERTIES_HPP_ */
