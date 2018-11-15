/*
 * ConfigFile.h
 *
 * A simple one line one entry configuration file where each line consists of a key=value pair.
 *
 *  Created on: Sep 30, 2018
 *      Author: ans
 */

#ifndef CONFIGFILE_H_
#define CONFIGFILE_H_

#include <boost/algorithm/string.hpp>

#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

class ConfigFile {
public:
	ConfigFile(const std::string& name);
	virtual ~ConfigFile();

	std::string getValue(const std::string& name) const;

protected:
	struct ConfigEntry {
		std::string name;
		std::string value;
	};

	std::vector<ConfigFile::ConfigEntry> entries;
};

#endif /* CONFIGFILE_H_ */
