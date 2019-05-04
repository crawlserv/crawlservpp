/*
 * ConfigFile.cpp
 *
 * A simple one line one entry configuration file where each line consists of a key=value pair.
 *
 *  Created on: Sep 30, 2018
 *      Author: ans
 */

#include "ConfigFile.hpp"

namespace crawlservpp::Main {

	// constructor: read file, throws Main::Exception
	ConfigFile::ConfigFile(const std::string& name) {
		std::ifstream fileStream(name);
		std::string line;

		if(fileStream.is_open()) {
			while(std::getline(fileStream, line)) {
				std::pair<std::string, std::string> entry;
				unsigned long nameEnd = line.find('=');

				if(nameEnd < line.length()) {
					entry.first = boost::algorithm::to_lower_copy(line.substr(0, nameEnd));
					entry.second = line.substr(nameEnd + 1);
				}
				else {
					entry.first = line;
					entry.second = "";
				}

				this->entries.emplace_back(entry);
			}

			fileStream.close();
		}
		else
			throw Exception("Could not open \"" + name + "\" for reading");
	}

	// get value of config entry (or empty string if entry does not exist)
	std::string ConfigFile::getValue(const std::string& name) const {
		for(auto i = entries.begin(); i != entries.end(); ++i) {
			if(i->first == boost::algorithm::to_lower_copy(name)) {
				return i->second;
			}
		}

		return "";
	}

} /* crawlservpp::Main */
