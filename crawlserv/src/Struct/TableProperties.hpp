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
 * TableProperties.hpp
 *
 * Table properties (name, columns, data directory and compression).
 *
 *  Created on: Mar 4, 2019
 *      Author: ans
 */


#ifndef STRUCT_TABLEPROPERTIES_HPP_
#define STRUCT_TABLEPROPERTIES_HPP_

#include "TableColumn.hpp"

#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Struct {
	//! Table properties containing its name, columns, data directory, and compression.
	struct TableProperties {
		///@name Properties
		///@{

		//! The name of the table.
		std::string name;

		//! Vector containing the columns of the table.
		std::vector<TableColumn> columns;

		//! The data directory of the table.
		/*!
		 * Contains an empty string if
		 *  the default data directory
		 *  is used.
		 */
		std::string dataDirectory;

		//! Indicates whether compression is active for this table.
		bool compressed{false};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		TableProperties() = default;

		//! Constructor setting table properties.
		/*!
		 * \param setName Constant reference
		 *   to a string containing the name
		 *   of the table.
		 * \param setColumns Constant reference
		 *   to a vector containing the columns
		 *   of the table.
		 * \param setDataDirectory Constant
		 *   reference to a string containing
		 *   the data directory of the table.
		 *   Set to an empty string if the
		 *   default data directory should be
		 *   used.
		 * \param setCompressed Set whether
		 *   compression should be used for
		 *   storing the table in the database.
		 */
		TableProperties(
				const std::string& setName,
				const std::vector<TableColumn>& setColumns,
				const std::string& setDataDirectory,
				bool setCompressed
		) : name(setName),
			columns(setColumns),
			dataDirectory(setDataDirectory),
			compressed(setCompressed) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_TABLEPROPERTIES_HPP_ */

