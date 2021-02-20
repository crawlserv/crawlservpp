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
 * Version.hpp
 *
 * General version information for crawlserv++.
 *
 *  Created on: May 20, 2019
 *      Author: ans
 */

#ifndef MAIN_VERSION_HPP_
#define MAIN_VERSION_HPP_

#define CRAWLSERVPP_VERSION_MAJOR 0 //NOLINT(cppcoreguidelines-macro-usage)
#define CRAWLSERVPP_VERSION_MINOR 0 //NOLINT(cppcoreguidelines-macro-usage)
#define CRAWLSERVPP_VERSION_RELEASE 1 //NOLINT(cppcoreguidelines-macro-usage)
#define CRAWLSERVPP_VERSION_SUFFIX "alpha" //NOLINT(cppcoreguidelines-macro-usage)

#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view_literals

//! Namespace for version information.
namespace crawlservpp::Main::Version {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! Major version of the application.
	inline constexpr auto crawlservVersionMajor{CRAWLSERVPP_VERSION_MAJOR};

	//! Minor version of the application
	inline constexpr auto crawlservVersionMinor{CRAWLSERVPP_VERSION_MINOR};

	//! Current release (i.e. patch) version of the application.
	inline constexpr auto crawlservVersionRelease{CRAWLSERVPP_VERSION_RELEASE};

	//! Version suffix of the application.
	inline constexpr std::string_view crawlservVersionSuffix{CRAWLSERVPP_VERSION_SUFFIX};

	///@}

	/*
	 * DECLARATION
	 */

	///@name Version Information
	///@{

	[[nodiscard]] std::string getString();

	///@}

	/*
	 * IMPLEMENTATION
	 */

	//! Gets the current version of crawlserv++.
	/*!
	 * \returns The copy of a string containing the current version
	 *   of crawlserv++.
	 */
	inline std::string getString() {
		std::string result;

		result += std::to_string(crawlservVersionMajor);
		result += ".";
		result += std::to_string(crawlservVersionMinor);
		result += ".";
		result += std::to_string(crawlservVersionRelease);
		result += crawlservVersionSuffix;

		return result;
	}

} /* namespace crawlservpp::Main::Version */

#endif /* MAIN_VERSION_HPP_ */
