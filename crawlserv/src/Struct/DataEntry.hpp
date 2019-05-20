/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * DataEntry.hpp
 *
 * Parsed or extracted data (content ID, parsed ID, parsed date/time, parsed or extracted fields).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_DATAENTRY_HPP_
#define STRUCT_DATAENTRY_HPP_

#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Struct {

	struct DataEntry {
		unsigned long contentId;
		std::string dataId;
		std::string dateTime;
		std::vector<std::string> fields;

		// constructor
		DataEntry(unsigned long setContentId) : contentId(setContentId) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_DATAENTRY_HPP_ */
