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

#define PCRE2_CODE_UNIT_WIDTH 8

#include "../_extern/mongoose/mongoose.h"
#include "../_extern/rapidjson/include/rapidjson/rapidjson.h"

#include "tidy.h"

#include <boost/version.hpp>
#include <curl/curl.h>
#include <cppconn/driver.h>

#include <aspell.h>
#include <pcre2.h>
#include <pugixml.hpp>
#include <uriparser/UriBase.h>

#include <sstream>
#include <string>

namespace Helper::Versions {
	std::string getLibraryVersions(const std::string& indent);
}

#endif /* HELPER_VERSIONS_H_ */
