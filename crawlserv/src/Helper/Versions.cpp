/*
 * Versions.cpp
 *
 * Get the versions of the different libraries used by crawlserv.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#include "Versions.h"

namespace Helper::Versions {

std::string getLibraryVersions(const std::string& indent) {
	std::ostringstream out;

	// Boost
	out << indent << "Boost v" << BOOST_VERSION / 100000 << "." << BOOST_VERSION / 100 % 1000 << "." << BOOST_VERSION % 100
			<< "\n";

	// Curl
	out << indent << "cURL v" << curl_version_info(CURLVERSION_NOW)->version << "\n";

	// GNU Aspell
	out << indent << "GNU Aspell v" << aspell_version_string() << "\n";

	// date.h (no version information available)
	out << indent << "Howard E. Hinnant's date.h library\n";

	// mongoose
	out << indent << "mongoose v" << MG_VERSION << "\n";

	// MySQL Connector/C++
	sql::Driver * driver = get_driver_instance();
	out << indent << driver->getName() << " v" << driver->getMajorVersion() << "." << driver->getMinorVersion() << "."
			<< driver->getPatchVersion() << "\n";

	// PCRE
	out << indent << "PCRE2 v" << PCRE2_MAJOR << "." << PCRE2_MINOR << "\n";

	// pugixml
	out << indent << "pugixml v" << PUGIXML_VERSION / 100 << "." << PUGIXML_VERSION % 100 / 10 << "." << PUGIXML_VERSION % 10
			<< "\n";

	// RapidJSON
	out << indent << "RapidJSON v" << RAPIDJSON_VERSION_STRING << "\n";

	// rawr-gen (no version information available)
	out << indent << "rawr-gen by Kelly Rauchenberger\n";

	// tidy-html5
	out << indent << "tidy-html5 v" << tidyLibraryVersion() << "-" << tidyReleaseDate() << "\n";

	// uriparser
	out << indent << "uriparser v" << URI_VER_MAJOR << "." << URI_VER_MINOR << "." << URI_VER_RELEASE << URI_VER_SUFFIX_ANSI
			<< "\n";

	// UTF8-CPP
	out << indent << "UTF8-CPP v2.1\n";

	return out.str();
}

}
