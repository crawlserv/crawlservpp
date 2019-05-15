/*
 * Versions.hpp
 *
 * Get the versions of the different libraries used by crawlserv.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef HELPER_VERSIONS_HPP_
#define HELPER_VERSIONS_HPP_

#include "../Helper/Portability/compiler.h"
#include "../Helper/Portability/mysqlcppconn.h"

#include "../_extern/jsoncons/include/jsoncons/config/version.hpp"
#include "../_extern/mongoose/mongoose.h"
#include "../_extern/rapidjson/include/rapidjson/rapidjson.h"

#include <aspell.h>
#include <boost/version.hpp>
#include <curl/curl.h>
#include <cppconn/driver.h>
#include <tidy.h>
#include <pcre2.h>
#include <pugixml.hpp>
#include <uriparser/UriBase.h>
#include <zlib.h>

#include <string>	// std::string, std::to_string
#include <utility>	// std::pair
#include <vector>	// std::vector

namespace crawlservpp::Helper::Versions {
	// for convenience
	typedef std::pair<std::string, std::string> StringString;

	/*
	 * DEFINITION
	 */

	std::vector<StringString> getLibraryVersions();
	std::string getLibraryVersionsStr(const std::string& indent);

	/*
	 * IMPLEMENTATION
	 */

	// Get the versions of the libraries as vector of [name, version] string pairs
	inline std::vector<StringString> getLibraryVersions() {
		std::vector<StringString> result;

		// Boost
		result.emplace_back(
				"Boost",
				std::to_string(BOOST_VERSION / 100000)
				+ '.'
				+ std::to_string(BOOST_VERSION / 100 % 1000)
				+ '.'
				+ std::to_string(BOOST_VERSION % 100)
		);

		// cURL
		result.emplace_back("cURL", curl_version_info(CURLVERSION_NOW)->version);

		// GNU Aspell
		result.emplace_back("GNU Aspell", aspell_version_string());

		// date.h (no version information available)
		result.emplace_back("Howard E. Hinnant's date.h", "");

		// jsoncons
		result.emplace_back(
				"jsoncons",
				std::to_string(JSONCONS_VERSION_MAJOR)
				+ '.'
				+ std::to_string(JSONCONS_VERSION_MINOR)
				+ '.'
				+ std::to_string(JSONCONS_VERSION_PATCH)
		);

		// mongoose
		result.emplace_back("mongoose", MG_VERSION);

		// MySQL Connector/C++
		sql::Driver * driver = get_driver_instance();

		if(driver)
			result.emplace_back(
					driver->getName(),
					std::to_string(driver->getMajorVersion())
					+ '.'
					+ std::to_string(driver->getMinorVersion())
					+ '.'
					+ std::to_string(driver->getPatchVersion())
			);

		// PCRE2
		result.emplace_back(
				"PCRE2",
				std::to_string(PCRE2_MAJOR)
				+ '.'
				+ std::to_string(PCRE2_MINOR)
		);

		// pugixml
		result.emplace_back(
				"pugixml",
				std::to_string(PUGIXML_VERSION / 100)
				+ '.'
				+ std::to_string(PUGIXML_VERSION % 100 / 10)
				+ '.'
				+ std::to_string(PUGIXML_VERSION % 10)
		);

		// RapidJSON
		result.emplace_back("RapidJSON", RAPIDJSON_VERSION_STRING);

		// rawr-gen (no version information available)
		result.emplace_back("rawr-gen by Kelly Rauchenberger", "");

		// tidy-html5
		result.emplace_back("tidy-html5", tidyLibraryVersion());

		// uriparser
		result.emplace_back(
				"uriparser",
				std::to_string(URI_VER_MAJOR)
				+ '.'
				+ std::to_string(URI_VER_MINOR)
				+ '.'
				+ std::to_string(URI_VER_RELEASE)
				+ URI_VER_SUFFIX_ANSI
		);

		// UTF8-CPP (WARNING: hard-coded version information not necessarily accurate)
		result.emplace_back("UTF8-CPP", "2.1");

		// zlib
		result.emplace_back("zlib", ZLIB_VERSION);

		return result;
	}

	// get the versions of the libraries as one indented string
	inline std::string getLibraryVersionsStr(const std::string& indent) {
		// get versions
		const std::vector<StringString> versions(getLibraryVersions());

		// create resulting string
		std::string result;

		for(const auto& version : versions) {
			result += indent;
			result += version.first;

			if(!(version.second.empty())) {
				result += " v";
				result += version.second;
			}

			result += "\n";
		}

		return result;
	}

} /* crawlservpp::Helper::Versions */

#endif /* HELPER_VERSIONS_HPP_ */
