/*
 * ConfigEntry.h
 *
 * Name, value pair from configuration file.
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCTS_CONFIGENTRY_H_
#define STRUCTS_CONFIGENTRY_H_

#include <algorithm>
#include <string>

struct ConfigEntry {
	std::string name;
	std::string value;

	// constructor with entry name
	ConfigEntry(const std::string& entryName) {
		this->name = entryName;
	}

	// does the name of the config entry equal a specific string?
	bool operator==(const std::string& str) const {
		return this->name == str;
	}
};

#endif /* STRUCTS_CONFIGENTRY_H_ */
