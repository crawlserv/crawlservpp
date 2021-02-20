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
 * Namespace for functions to import/export data from/to text file.
 *
 *  Created on: May 4, 2019
 *      Author: ans
 */

#ifndef DATA_IMPORTEXPORT_TEXT_H_
#define DATA_IMPORTEXPORT_TEXT_H_

#include "../../Helper/Strings.hpp"

#include <optional>	// std::optional
#include <queue>	// std::queue
#include <string>	// std::string

//! Namespace for importing and exporting raw text.
namespace crawlservpp::Data::ImportExport::Text {

	/*
	 * DECLARATION
	 */

	///@name Import and Export
	///@{

	std::queue<std::string> importList(
			const std::string& content,
			bool skipFirstLine,
			bool ignoreEmpty
	);

	std::string exportList(
			std::queue<std::string>& list,
			const std::optional<std::string>& header,
			bool ignoreEmpty
	);

	///@}

	/*
	 * IMPLEMENTATION
	 */

	//! Imports a list from raw text content, with each line representing a list entry.
	/*!
	 * \param content A constant reference to the content to be
	 *   parsed as a list, each line representing a list entry.
	 * \param skipFirstLine If true, the first line in the content
	 *   will be ignored, e.g. when it contains a header for the list.
	 * \param ignoreEmpty If true, empty lines will be ignored.
	 *
	 * \returns A queue containing the list entries extracted from
	 *   the given content.
	 */
	inline std::queue<std::string> importList(
			const std::string& content,
			bool skipFirstLine,
			bool ignoreEmpty
	) {
		// split content into entries
		std::queue<std::string> result{Helper::Strings::splitToQueue(content, '\n', ignoreEmpty)};

		// delete header if necessary
		if(skipFirstLine && !result.empty()) {
			result.pop();
		}

		// return list
		return result;
	}

	//! Exports a list to raw text content, with each line representing a list entry.
	/*!
	 * \param list Reference to a queue containing the list entries.
	 *   It will be emptied in the process, even if entries will be
	 *   ignored, because they are empty.
	 * \param header Constant reference to an optional string
	 *   containing a header for the list. If given, it will be
	 *   added to the beginning of the resulting content, in a
	 *   separate line.
	 * \param ignoreEmpty If true, empty list entries will not be
	 *   written to the resulting raw text content, although they
	 *   will still be removed from the given queue.
	 *
	 * \returns A new string containing the resulting content.
	 */
	inline std::string exportList(
			std::queue<std::string>& list,
			const std::optional<std::string>& header,
			bool ignoreEmpty
	) {
		std::string result;

		// write header to string if necessary
		if(header.has_value()) {
			result.reserve(header.value().size() + 1);

			result = header.value() + "\n";
		}

		// write list entries to string
		Helper::Strings::join(list, '\n', ignoreEmpty, result);

		// return string
		return result;
	}

} /* namespace crawlservpp::Data::ImportExport::Text */

#endif /* DATA_IMPORTEXPORT_TEXT_H_ */
