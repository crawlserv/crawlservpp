/*
 * Versions.h
 *
 * Get the versions of the different libraries used by crawlserv.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef HELPER_VERSIONS_H_
#define HELPER_VERSIONS_H_

#include "../_extern/mongoose.h"
#include "../_extern/rapidjson/rapidjson.h"

#include <boost/version.hpp>
#include <curl/curl.h>
#include <cppconn/driver.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <pugixml.hpp>
#include <tidy/tidy.h>
#include <uriparser/UriBase.h>

#include <sstream>
#include <string>

namespace crawlservpp::Helper::Versions {
	std::string getLibraryVersions(const std::string& indent);
}

#endif /* HELPER_VERSIONS_H_ */
