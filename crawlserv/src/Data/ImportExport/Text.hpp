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
 * Text.hpp
 *
 * Namespace with functions to import/export data from/to text file.
 *
 *  Created on: May 4, 2019
 *      Author: ans
 */

#ifndef DATA_IMPORTEXPORT_TEXT_H_
#define DATA_IMPORTEXPORT_TEXT_H_

#include "../../Helper/Strings.hpp"

#include <queue>	// std::queue
#include <string>	// std::string

namespace crawlservpp::Data::ImportExport::Text {

	/*
	 * DECLARATION
	 */

	std::queue<std::string> importList(
			const std::string& content,
			bool skipFirstLine,
			bool removeEmpty
	);

	std::string exportList(
			std::queue<std::string>& list,
			bool writeHeader,
			const std::string& header
	);

	/*
	 * IMPLEMENTATION
	 */

	// import list from text file content, ignoring empty lines
	inline std::queue<std::string> importList(
			const std::string& content,
			bool skipFirstLine,
			bool removeEmpty
	) {
		// split content into entries
		std::queue<std::string> result(Helper::Strings::splitToQueue(content, '\n', removeEmpty));

		// delete header if necessary
		if(skipFirstLine && result.size())
			result.pop();

		// return list
		return result;
	}

	// export list to text file content, ignoring empty entries
	inline std::string exportList(
			std::queue<std::string>& list,
			bool writeHeader,
			const std::string& header
	) {
		std::string result;

		// write header to string if necessary
		if(writeHeader) {
			result.reserve(header.size() + 1);

			result = header + "\n";
		}

		// write list entries to string
		Helper::Strings::join(list, '\n', true, result);

		// return string
		return result;
	}

} /* crawlservpp::Data::ImportExport::Text */

#endif /* DATA_IMPORTEXPORT_TEXT_H_ */
