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
 * DataEntry.hpp
 *
 * Parsed or extracted data (content ID, parsed ID, parsed date/time, parsed or extracted fields).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_DATAENTRY_HPP_
#define STRUCT_DATAENTRY_HPP_

#include <cstdint>	// std::uint64_t
#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Struct {

	//! A data entry containing either parsed or extracted data.
	/*!
	 * Contains content and data ID, parsed date/time and
	 *  the fields that have been either parsed or extracted.
	 */
	struct DataEntry {
		///@name Properties
		///@{

		//! The ID of the content from which the data has been either parsed or extracted.
		std::uint64_t contentId;

		//! The ID of the data that has been either parsed or extracted.
		std::string dataId;

		//! The date/time of the data that has been either parsed or extracted.
		std::string dateTime;

		//! A vector containing the data fields that have been either parsed or extracted.
		std::vector<std::string> fields;

		///@}
		///@name Construction
		///@{

		// Constructor setting the content ID.
		/*!
		 * \param setContentId The ID of the content
		 *   from which the data has been either
		 *   parsed or extracted.
		 */
		explicit DataEntry(std::uint64_t setContentId) : contentId(setContentId) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_DATAENTRY_HPP_ */
