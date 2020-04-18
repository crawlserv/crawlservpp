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
 * ConfigFile.hpp
 *
 * A simple one line one entry configuration file where each line consists of a key=value pair.
 *
 *  Created on: Sep 30, 2018
 *      Author: ans
 */

#ifndef MAIN_CONFIGFILE_HPP_
#define MAIN_CONFIGFILE_HPP_

#include "Exception.hpp"

#include <algorithm>	// std::find_if, std::transform
#include <cctype>		// ::tolower
#include <fstream>		// std::ifstream
#include <string>		// std::string, std::getline
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Main {

	class ConfigFile {

	public:
		// constructor
		ConfigFile(const std::string& name);

		// getter
		std::string getValue(const std::string& name) const;

	protected:
		std::vector<std::pair<std::string, std::string>> entries;
	};

} /* crawlservpp::Main */

#endif /* MAIN_CONFIGFILE_HPP_ */
