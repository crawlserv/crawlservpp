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

#include "../Helper/Portability/mysqlcppconn.h"

#include "../_extern/mongoose/mongoose.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../_extern/rapidjson/include/rapidjson/rapidjson.h"

#include <aspell.h>
#include <boost/version.hpp>
#include <curl/curl.h>
#include <cppconn/driver.h>
#include <tidy.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <pugixml.hpp>
#include <uriparser/UriBase.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace crawlservpp::Helper::Versions {

	/*
	 * DEFINITION
	 */

	std::vector<std::pair<std::string, std::string>> getLibraryVersions();
	std::string getLibraryVersionsStr(const std::string& indent);

	/*
	 * IMPLEMENTATION
	 */

	inline std::vector<std::pair<std::string, std::string>> getLibraryVersions() {
		std::vector<std::pair<std::string, std::string>> result;
		std::ostringstream out;

		// Boost
		out << BOOST_VERSION / 100000
			<< '.' << BOOST_VERSION / 100 % 1000
			<< '.' << BOOST_VERSION % 100;

		result.emplace_back("Boost", out.str());

		out.str("");
		out.clear();

		// cURL
		result.emplace_back("cURL", curl_version_info(CURLVERSION_NOW)->version);

		// GNU Aspell
		result.emplace_back("GNU Aspell", aspell_version_string());

		// date.h (no version information available)
		result.emplace_back("Howard E. Hinnant's date.h library", "");

		// mongoose
		result.emplace_back("mongoose", MG_VERSION);

		// MySQL Connector/C++
		sql::Driver * driver = get_driver_instance();

		if(driver) {
			out	<< driver->getMajorVersion()
				<< '.'
				<< driver->getMinorVersion()
				<< '.'
				<< driver->getPatchVersion();

			result.emplace_back(driver->getName(), out.str());

			out.str("");
			out.clear();
		}

		// PCRE2
		out	<< PCRE2_MAJOR
			<< '.'
			<< PCRE2_MINOR;

		result.emplace_back("PCRE2", out.str());

		out.str("");
		out.clear();

		// pugixml
		out	<< PUGIXML_VERSION / 100
			<< '.'
			<< PUGIXML_VERSION % 100 / 10
			<< '.'
			<< PUGIXML_VERSION % 10;

		result.emplace_back("pugixml", out.str());

		out.str("");
		out.clear();

		// RapidJSON
		result.emplace_back("RapidJSON", RAPIDJSON_VERSION_STRING);

		// rawr-gen (no version information available)
		result.emplace_back("rawr-gen by Kelly Rauchenberger", "");

		// tidy-html5
		result.emplace_back("tidy-html5", tidyLibraryVersion());

		// uriparser
		out	<< URI_VER_MAJOR
			<< '.' <<
			URI_VER_MINOR
			<< '.' <<
			URI_VER_RELEASE
			<< URI_VER_SUFFIX_ANSI;

		result.emplace_back("uriparser", out.str());

		out.str("");
		out.clear();

		// UTF8-CPP (WARNING: hard-coded version information not necessarily accurate)
		result.emplace_back("UTF8-CPP", "2.1");

		return result;
	}

	inline std::string getLibraryVersionsStr(const std::string& indent) {
		std::vector<std::pair<std::string, std::string>> versions(getLibraryVersions());
		std::string result;

		for(auto i = versions.begin(); i != versions.end(); ++i) {
			result += indent;
			result += i->first;

			if(!(i->second.empty())) {
				result += " v";
				result += i->second;
			}

			result += "\n";
		}

		return result;
	}

} /* crawlservpp::Helper::Versions */

#endif /* HELPER_VERSIONS_HPP_ */
