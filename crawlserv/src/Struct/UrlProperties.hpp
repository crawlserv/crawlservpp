/*
 * UrlData.hpp
 *
 * Basic URL properties (ID, URL and lock ID).
 *
 *  Created on: Mar 6, 2019
 *      Author: ans
 */

#ifndef STRUCT_URLDATA_HPP_
#define STRUCT_URLDATA_HPP_

#include <string>

namespace crawlservpp::Struct {

	struct UrlProperties {
		unsigned long id;	// ID of the URL
		std::string url;	// URL
		unsigned long lockId;	// ID of the corresponding URL lock

		UrlProperties() : id(0), lockId(0) {}
		UrlProperties(unsigned long setId) : id(setId), lockId(0) {}
		UrlProperties(const std::string& setUrl) : id(0), url(setUrl), lockId(0) {}
		UrlProperties(unsigned long setId, const std::string& setUrl) : id(setId), url(setUrl), lockId(0) {}
		UrlProperties(unsigned long setId, const std::string& setUrl, unsigned long setLockId)
				: id(setId), url(setUrl), lockId(setLockId) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_URLPROPERTIES_HPP_ */
