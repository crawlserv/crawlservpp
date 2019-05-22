/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * Version information.
 *
 *  Created on: May 20, 2019
 *      Author: ans
 */

#ifndef MAIN_VERSION_HPP_
#define MAIN_VERSION_HPP_

#define CRAWLSERV_VERSION_MAJOR 0
#define CRAWLSERV_VERSION_MINOR 0
#define CRAWLSERV_VERSION_RELEASE 0
#define CRAWLSERV_VERSION_SUFFIX "alpha"

#include <string>	// std::string, std::to_string

namespace crawlservpp::Main::Version {

	inline std::string getString() {
		return
				std::to_string(CRAWLSERV_VERSION_MAJOR)
				+ "."
				+ std::to_string(CRAWLSERV_VERSION_MINOR)
				+ "."
				+ std::to_string(CRAWLSERV_VERSION_RELEASE)
				+ CRAWLSERV_VERSION_SUFFIX;
	}

} /* crawlservpp::Main::Version */

#endif /* SRC_MAIN_VERSION_HPP_ */
