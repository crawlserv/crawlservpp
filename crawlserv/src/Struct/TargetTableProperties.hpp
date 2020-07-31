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
 * TargetTableProperties.hpp
 *
 * Target table properties (type, website, URL list, table name and full name, columns and compression).
 *
 *  Created on: Mar 4, 2019
 *      Author: ans
 */


#ifndef STRUCT_TARGETTABLEPROPERTIES_HPP_
#define STRUCT_TARGETTABLEPROPERTIES_HPP_

#include "TableColumn.hpp"

#include <cstdint>		// std::uint64_t
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Struct {

	//! Target table properties containing its type, website, URL list, table names, columns, and compression.
	struct TargetTableProperties {
		///@name Properties
		///@{

		//! The type of the data stored in the table.
		/*!
		 * Will be ignored when updating a target table.
		 */
		std::string type;

		//! The ID of the website used for retrieving data to be stored in the table.
		std::uint64_t website{0};

		//! The ID of the URL list used for retrieving data to be stored in the table.
		std::uint64_t urlList{0};

		//! View of a string containing the name of the table.
		std::string name;

		//! The full name of the table.
		std::string fullName;

		//! Vector containing the columns of the table.
		std::vector<TableColumn> columns;

		//! Indicates whether compression is active for this table.
		bool compressed{false};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		TargetTableProperties() = default;

		//! Constructor setting properties, but not the columns of the target table.
		/*!
		 * \param setType Constant reference
		 *   to a string containing the type
		 *   of the data to be stored in the
		 *   table.
		 * \param setWebsite The ID of the
		 *   website used for retrieving data
		 *   to be stored in the table.
		 * \param setUrlList The ID of the
		 *   URL list used for retrieving data
		 *   to be stored in the table.
		 * \param setName Constant reference
		 *   to a string containing the name
		 *   of the table.
		 * \param setFullName Constant
		 *   reference to a string containing
		 *   the full name of the table.
		 * \param setCompressed Set whether
		 *   compression should be used for
		 *   storing the table in the database.
		 */
		TargetTableProperties(
				const std::string& setType,
				std::uint64_t setWebsite,
				std::uint64_t setUrlList,
				const std::string& setName,
				const std::string& setFullName,
				bool setCompressed
		) : type(setType),
			website(setWebsite),
			urlList(setUrlList),
			name(setName),
			fullName(setFullName),
			compressed(setCompressed) {}

		//! Constructor setting properties and columns of the target table.
		/*!
		 * \param setType Constant reference
		 *   to a string containing the type
		 *   of the data to be stored in the
		 *   table.
		 * \param setWebsite The ID of the
		 *   website used for retrieving data
		 *   to be stored in the table.
		 * \param setUrlList The ID of the
		 *   URL list used for retrieving data
		 *   to be stored in the table.
		 * \param setName Constant reference
		 *   to a string containing the name
		 *   of the table.
		 * \param setFullName Constant
		 *   reference to a string containing
		 *   the full name of the table.
		 * \param setColumns Constant reference
		 *   to a vector containing the columns
		 *   of the table.
		 * \param setCompressed Set whether
		 *   compression should be used for
		 *   storing the table in the database.
		 */
		TargetTableProperties(
				const std::string& setType,
				std::uint64_t setWebsite,
				std::uint64_t setUrlList,
				const std::string& setName,
				const std::string& setFullName,
				const std::vector<TableColumn>& setColumns,
				bool setCompressed
		) : type(setType),
			website(setWebsite),
			urlList(setUrlList),
			name(setName),
			fullName(setFullName),
			columns(setColumns),
			compressed(setCompressed) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_TARGETTABLEPROPERTIES_HPP_ */

