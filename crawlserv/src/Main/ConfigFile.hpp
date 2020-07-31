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

	/*
	 * DECLARATION
	 */

	//! Configuration file.
	/*!
	 * In this text file, each line represents
	 *  one entry and consists of a @c key=value
	 *  pair.
	 */
	class ConfigFile {

	public:
		///@name Construction
		///@{

		explicit ConfigFile(const std::string& name);

		///@}
		///@name Getter
		///@{

		[[nodiscard]] std::string getValue(const std::string& name) const;

		///@}

	protected:
		//! Vector containing the configuration entries as pairs of strings.
		std::vector<std::pair<std::string, std::string>> entries;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor reading the file.
	/*!
	 * \param name Const reference to a string
	 *   containing the name of the configuration
	 *   file to read.
	 *
	 * \throws Main::Exception if the configuration
	 *   file could not be opened for reading.
	 */
	inline ConfigFile::ConfigFile(const std::string& name) {
		std::ifstream fileStream(name);
		std::string line;

		if(fileStream.is_open()) {
			while(std::getline(fileStream, line)) {
				const auto nameEnd{line.find('=')};

				if(nameEnd < line.length()) {
					std::string nameInLine(line, 0, nameEnd);

					std::transform(nameInLine.begin(), nameInLine.end(), nameInLine.begin(), ::tolower);

					this->entries.emplace_back(
							nameInLine,
							line.substr(nameEnd + 1)
					);
				}
				else {
					this->entries.emplace_back(
							line,
							""
					);
				}
			}

			fileStream.close();
		}
		else {
			throw Exception("Could not open \"" + name + "\" for reading");
		}
	}

	//! Gets the value of a configuration entry.
	/*!
	 * \param name Constant reference to a string
	 *   containing the name of the configuration
	 *   entry to be retrieved.
	 *
	 * \returns The value of the given configuration
	 *   entry or an empty string, if the entry
	 *   does not exist.
	 */
	inline std::string ConfigFile::getValue(const std::string& name) const {
		std::string nameCopy(name);

		std::transform(nameCopy.begin(), nameCopy.end(), nameCopy.begin(), ::tolower);

		const auto valueIt{
			std::find_if(
					this->entries.cbegin(),
					this->entries.cend(),
					[&nameCopy](const auto& entry) {
						return entry.first == nameCopy;
					}
			)
		};

		if(valueIt != this->entries.cend()) {
			return valueIt->second;
		}

		return "";
	}

} /* namespace crawlservpp::Main */

#endif /* MAIN_CONFIGFILE_HPP_ */
