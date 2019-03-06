/*
 * UrlData.h
 *
 * Structure for URL data.
 *
 *  Created on: Mar 6, 2019
 *      Author: ans
 */

#ifndef STRUCT_URLDATA_H_
#define STRUCT_URLDATA_H_

#include <string>

namespace crawlservpp::Struct {
	struct UrlData {
		unsigned long id;	// ID of the URL
		std::string url;	// URL
		unsigned long lockId;	// ID of the corresponding URL lock

		UrlData() : id(0), lockId(0) {}
		UrlData(const std::string& setUrl) : id(0), url(setUrl), lockId(0) {}
		UrlData(unsigned long setId, const std::string& setUrl) : id(setId), url(setUrl), lockId(0) {}
		UrlData(unsigned long setId, const std::string& setUrl, unsigned long setLockId) : id(setId), url(setUrl), lockId(setLockId) {}
	};
}

#endif /* STRUCT_URLDATA_H_ */
