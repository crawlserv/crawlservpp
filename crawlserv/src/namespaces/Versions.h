/*
 * Versions.h
 *
 * Get the versions of the different libraries used by crawlserv.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef NAMESPACES_VERSIONS_H_
#define NAMESPACES_VERSIONS_H_

#include "../external/mongoose.h"
#include "../external/rapidjson/rapidjson.h"

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

namespace Versions {
	std::string getLibraryVersions(const std::string& indent);
}

#endif /* NAMESPACES_VERSIONS_H_ */
