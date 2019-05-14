/*
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
