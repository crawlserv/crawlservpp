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
				const size_t nameEnd = line.find('=');

				if(nameEnd < line.length()) {
					std::string nameInLine(line, 0, nameEnd);

					std::transform(nameInLine.begin(), nameInLine.end(), nameInLine.begin(), ::tolower);

					this->entries.emplace_back(
							nameInLine,
							line.substr(nameEnd + 1)
					);
				}
				else
					this->entries.emplace_back(
							line,
							""
					);
			}

			fileStream.close();
		}
		else
			throw Exception("Could not open \"" + name + "\" for reading");
	}

	// get value of config entry (or empty string if entry does not exist)
	std::string ConfigFile::getValue(const std::string& name) const {
		std::string nameCopy(name);

		std::transform(nameCopy.begin(), nameCopy.end(), nameCopy.begin(), ::tolower);

		const auto valueIt = std::find_if(
				this->entries.begin(),
				this->entries.end(),
				[&nameCopy](const auto& entry) {
					return entry.first == nameCopy;
				}
		);

		if(valueIt != this->entries.end())
			return valueIt->second;

		return "";
	}

} /* crawlservpp::Main */
