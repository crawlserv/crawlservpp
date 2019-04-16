/*
 * WebsiteProperties.hpp
 *
 * Basic website properties (domain, namespace and name).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_WEBSITEPROPERTIES_HPP_
#define STRUCT_WEBSITEPROPERTIES_HPP_

#include <string>

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
