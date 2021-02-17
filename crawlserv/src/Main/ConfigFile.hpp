/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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

#include <boost/core/typeinfo.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>	// std::find_if, std::transform
#include <cctype>		// std::tolower
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
		///@name Getters
		///@{

		bool getValue(const std::string& name, std::string& to) const;

		//! Gets the converted value of a configuration entry.
		/*!
		 * \param name Constant reference to a string
		 *   containing the name of the configuration
		 *   entry to be retrieved.
		 * \param to Reference to a variable to which
		 *   the converted value of the entry should
		 *   be written. Will not be changed, if the
		 *   given configuration entry does not
		 *   exist or its value is empty.
		 *
		 * \return True, if the given configuration
		 *   entry exists and is not empty.
		 *   False otherwise.
		 *
		 * \throws ConfigFile::Exception, if the
		 *   conversion of the configuration entry
		 *   value failed.
		 */
		template<typename T> bool getValue(const std::string& name, T& to) const {
			std::string result;

			if(this->getValue(name, result) && !result.empty()) {
				try {
					to = boost::lexical_cast<T>(result);

					return true;
				}
				catch(const boost::bad_lexical_cast& e) {
					throw Exception(
							this->fileName + ":"
							" Could not convert config file entry \"" + name + "\""
							" (=\"" + result + "\")"
							" to " + boost::core::demangled_name(typeid(T))
					);
				}
			}

			return false;
		}

		///@}

		//! Class for configuration file exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		// configuration entries
		std::vector<std::pair<std::string, std::string>> entries;

	private:
		// file name
		std::string fileName;
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
	inline ConfigFile::ConfigFile(const std::string& name) : fileName(name) {
		std::ifstream fileStream(name);
		std::string line;

		if(fileStream.is_open()) {
			while(std::getline(fileStream, line)) {
				const auto nameEnd{line.find('=')};

				if(nameEnd < line.length()) {
					std::string nameInLine(line, 0, nameEnd);

					std::transform(
							nameInLine.begin(),
							nameInLine.end(),
							nameInLine.begin(),
							[](const auto c) {
								return std::tolower(c);
							}
					);

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

	//! Gets the string value of a configuration entry.
	/*!
	 * \param name Constant reference to a string
	 *   containing the name of the configuration
	 *   entry to be retrieved.
	 * \param to Reference to a string to which
	 *   the value of the entry should be
	 *   written. Will not be changed, if the
	 *   given configuration entry does not
	 *   exist.
	 *
	 * \return True, if the given configuration
	 *   entry exists. False otherwise.
	 */
	inline bool ConfigFile::getValue(const std::string& name, std::string& to) const {
		std::string nameCopy(name);

		std::transform(
				nameCopy.begin(),
				nameCopy.end(),
				nameCopy.begin(),
				[](const auto c) {
					return std::tolower(c);
				}
		);

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
			to = valueIt->second;

			return true;
		}

		return false;
	}

} /* namespace crawlservpp::Main */

#endif /* MAIN_CONFIGFILE_HPP_ */
