/*
 * ConfigFile.cpp
 *
 * A simple one line one entry configuration file where each line consists of a key=value pair.
 *
 *  Created on: Sep 30, 2018
 *      Author: ans
 */

#include "../Main/ConfigFile.h"

namespace crawlservpp::Main {

// constructor: read file
ConfigFile::ConfigFile(const std::string& name) {
	std::ifstream fileStream(name);
	std::string line;

	if(fileStream.is_open()) {
		while(std::getline(fileStream, line)) {
			ConfigFile::NameValueEntry entry;
			unsigned long nameEnd = line.find('=');
			if(nameEnd < line.length()) {
				entry.name = boost::algorithm::to_lower_copy(line.substr(0, nameEnd));
				entry.value = line.substr(nameEnd + 1);
			}
			else {
				entry.name = line;
				entry.value = "";
			}
			this->entries.push_back(entry);
		}
		fileStream.close();
	}
	else {
		throw std::runtime_error("Could not open \"" + name + "\" for reading");
	}
}

// destructor: stub
ConfigFile::~ConfigFile() {}

// get value of config entry (or empty string if entry does not exist)
std::string ConfigFile::getValue(const std::string& name) const {
	for(auto i = entries.begin(); i != entries.end(); ++i) {
		if(i->name == boost::algorithm::to_lower_copy(name)) {
			return i->value;
		}
	}
	return "";
}

}
