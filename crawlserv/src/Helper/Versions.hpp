/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Versions.hpp
 *
 * Namespace for functions getting the versions of the different libraries used by the application.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef HELPER_VERSIONS_HPP_
#define HELPER_VERSIONS_HPP_

#include "../Helper/FileSystem.hpp"
#include "../Helper/Portability/mysqlcppconn.h"
#include "../Helper/SilentInclude/EigenRand.h"
#include "../Helper/SilentInclude/tidy.h"

#include "../_extern/asio/asio/include/asio/version.hpp"
#include "../_extern/jsoncons/include/jsoncons/config/version.hpp"
#include "../_extern/mongoose/mongoose.h"
#include "../_extern/rapidjson/include/rapidjson/rapidjson.h"
#include "../_extern/wapiti/wapiti.h"

#include <aspell.h>
#include <boost/version.hpp>
#include <cppconn/driver.h>
#include <curl/curl.h>
#include <Eigen/Core>
#include <mapbox/variant.hpp>
#include <pcre2.h>
#include <pugixml.hpp>
#include <uriparser/UriBase.h>
#include <zip.h>
#include <zlib.h>

#include <array>		// std::array
#include <fstream>		// std::ifstream
#include <string>		// std::getline, std::string, std::to_string
#include <string_view>	// std::string_view, std::string_view_literals
#include <utility>		// std::pair
#include <vector>		// std::vector

//! Namespace for functions getting the versions of the different libraries used by the application.
namespace crawlservpp::Helper::Versions {

	using std::string_view_literals::operator""sv;

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Divisor to retrieve the major version of the Asio library.
	inline constexpr auto asioMajor{100000};

	//! Divisor to retrieve the minor version of the Asio library.
	inline constexpr auto asioMinor{100};

	//! Mod divisor to retrieve the minor version of the Asio library.
	inline constexpr auto asioModMinor{1000};

	//! Mod divisor to retrieve the patch version of the Asio library.
	inline constexpr auto asioPatch{100};

	//! Divisor to retrieve the major version of the Boost library.
	inline constexpr auto boostMajor{100000};

	//! Divisor to retrieve the minor version of the Boost library.
	inline constexpr auto boostMinor{1000};

	//! Mod divisor to retrieve the patch level of the Boost library.
	inline constexpr auto boostPatch{100};

	//! Divisor to retrieve the major version of the pugixml library.
	inline constexpr auto pugixmlMajor{100};

	//! Divisor to retrieve the minor version of the pugixml library.
	inline constexpr auto pugixmlMinor{10};

	//! The path from where to read the version of the tomoto library from.
	inline constexpr std::array tomotoVersionFile{
		"src"sv,
		"_extern"sv,
		"tomotopy"sv,
		"tomotopy"sv,
		"_version.py"sv
	};

	//! The beginning of the line containing the version of the tomoto library.
	inline constexpr auto tomotoVersionBegin{"__version__ = '"sv};

	//! The end of the line containing the version of the tomoto library.
	inline constexpr auto tomotoVersionEnd{"'"sv};

	//! The version of the @c UTF8-CPP library.
	/*!
	 * \warning Hard-coded version information
	 *   might not be accurate!
	 */
	inline constexpr std::string_view utf8CppVersion{"2.1"};

	///@}

	/*
	 * DECLARATION
	 */

	//! A pair of strings.
	using StringString = std::pair<std::string, std::string>;

	///@name Getters
	///@{

	std::vector<StringString> getLibraryVersions();
	std::string getLibraryVersionsStr(const std::string& indent);
	std::string getTomotoVersion();

	///@}

	/*
	 * IMPLEMENTATION
	 */

	//! Gets the versions of the used libraries.
	/*!
	 * \returns The versions of the libraries
	 *   as @c [name, @c version] pairs.
	 */
	inline std::vector<StringString> getLibraryVersions() {
		std::vector<StringString> result;

		// asio
		result.emplace_back(
				"Asio",
				std::to_string(ASIO_VERSION / asioMajor)
				+ '.'
				+ std::to_string(ASIO_VERSION / asioMinor % asioModMinor)
				+ '.'
				+ std::to_string(ASIO_VERSION % asioPatch)
		);

		// Boost
		result.emplace_back(
				"Boost",
				std::to_string(BOOST_VERSION / boostMajor)
				+ '.'
				+ std::to_string(BOOST_VERSION / boostPatch % boostMinor)
				+ '.'
				+ std::to_string(BOOST_VERSION % boostPatch)
		);

		// Eigen
		result.emplace_back(
				"Eigen",
				std::to_string(EIGEN_WORLD_VERSION)
				+ '.'
				+ std::to_string(EIGEN_MAJOR_VERSION)
				+ '.'
				+ std::to_string(EIGEN_MINOR_VERSION)
		);

		// EigenRand
		result.emplace_back(
				"EigenRand",
				std::to_string(EIGENRAND_WORLD_VERSION)
					+ '.'
					+ std::to_string(EIGENRAND_MAJOR_VERSION)
					+ '.'
					+ std::to_string(EIGENRAND_MINOR_VERSION)
		);

		// GNU Aspell
		result.emplace_back("GNU Aspell", aspell_version_string());

		// Howard E. Hinnant's date.h library (no version information available)
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

		// libcurl
		result.emplace_back("libcurl", curl_version_info(CURLVERSION_NOW)->version);

		// libzip
		result.emplace_back("libzip", zip_libzip_version());

		// Mapbox Variant
		result.emplace_back(
				"Mapbox Variant",
				std::to_string(VARIANT_MAJOR_VERSION)
				+ '.'
				+ std::to_string(VARIANT_MINOR_VERSION)
				+ '.'
				+ std::to_string(VARIANT_PATCH_VERSION)
		);

		// mongoose
		result.emplace_back("Mongoose", MG_VERSION);

		// MySQL Connector/C++
		sql::Driver * driver{get_driver_instance()};

		if(driver != nullptr) {
			result.emplace_back(
					driver->getName(),
					std::to_string(driver->getMajorVersion())
					+ '.'
					+ std::to_string(driver->getMinorVersion())
					+ '.'
					+ std::to_string(driver->getPatchVersion())
			);
		}

		// PCRE2
		result.emplace_back(
				"PCRE2",
				std::to_string(PCRE2_MAJOR)
				+ '.'
				+ std::to_string(PCRE2_MINOR)
		);

		// porter2_stemmer (no version information available)
		result.emplace_back("porter2_stemmer", "");

		// pugixml
		result.emplace_back(
				"pugixml",
				std::to_string(PUGIXML_VERSION / pugixmlMajor)
				+ '.'
				+ std::to_string(PUGIXML_VERSION % pugixmlMajor / pugixmlMinor)
				+ '.'
				+ std::to_string(PUGIXML_VERSION % pugixmlMinor)
		);

		// RapidJSON
		result.emplace_back("RapidJSON", RAPIDJSON_VERSION_STRING);

		// tomoto
		result.emplace_back("tomoto", getTomotoVersion());

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
		result.emplace_back("UTF8-CPP", utf8CppVersion);

		// Wapiti
		result.emplace_back("Wapiti", Data::wapiti::version);

		// zlib
		result.emplace_back("zlib", ZLIB_VERSION);

		return result;
	}

	//! Gets the versions of the used libraries as one indented string.
	/*!
	 * \param indent Constant reference to a
	 *   string containing the indent to be
	 *   added in front of each line.
	 * \returns A copy of a string containing
	 *   the versions of the libraries with
	 *   one (indented) line for each library.
	 */
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

		if(!versions.empty()) {
			result.pop_back();
		}

		return result;
	}

	//! Gets the version of the \c tomoto library if available.
	/*!
	 * \returns The version of the \c tomoto library
	 *   as string, or \c <unknown> if not available.
	 */
	inline std::string getTomotoVersion() {
		const char pathSeparator{Helper::FileSystem::getPathSeparator()};
		std::string pathToFile;
		std::string version;

		for(const auto& pathElement : tomotoVersionFile) {
			pathToFile += pathElement;
			pathToFile += pathSeparator;
		}

		if(!pathToFile.empty()) {
			pathToFile.pop_back();
		}

		std::ifstream in(pathToFile);

		if(in.is_open()) {
			std::string line;

			while(std::getline(in, line)) {
				if(line.substr(0, tomotoVersionBegin.size()) == tomotoVersionBegin) {
					const auto end{
						line.find(tomotoVersionEnd, tomotoVersionBegin.size())
					};

					if(end != std::string::npos) {
						version = line.substr(
								tomotoVersionBegin.size(),
								end - tomotoVersionBegin.size()
						);
					}
				}
			}

			in.close();
		}

		if(version.empty()) {
			return "<unknown>";
		}

		return version;
	}

} /* namespace crawlservpp::Helper::Versions */

#endif /* HELPER_VERSIONS_HPP_ */
