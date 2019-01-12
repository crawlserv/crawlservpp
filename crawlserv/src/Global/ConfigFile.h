/*
 * ConfigFile.h
 *
 * A simple one line one entry configuration file where each line consists of a key=value pair.
 *
 *  Created on: Sep 30, 2018
 *      Author: ans
 */

#ifndef GLOBAL_CONFIGFILE_H_
#define GLOBAL_CONFIGFILE_H_

#include <boost/algorithm/string.hpp>

#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace crawlservpp::Global {
	class ConfigFile {
	public:
		ConfigFile(const std::string& name);
		virtual ~ConfigFile();

		std::string getValue(const std::string& name) const;

	protected:
		struct NameValueEntry {
			std::string name;
			std::string value;
		};

		std::vector<crawlservpp::Global::ConfigFile::NameValueEntry> entries;
	};
}

#endif /* GLOBAL_CONFIGFILE_H_ */
