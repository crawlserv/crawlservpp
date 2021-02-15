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
 * TableColumn.hpp
 *
 * Structure for table columns.
 *
 *  Created on: Mar 4, 2019
 *      Author: ans
 */

#ifndef STRUCT_TABLECOLUMN_HPP_
#define STRUCT_TABLECOLUMN_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	//! Structure for table columns containing its name, type, reference, and indexing.
	struct TableColumn {
		///@name Properties
		///@{

		//! Name of the table column.
		std::string name;

		//! Type of the table column as SQL string.
		std::string type;

		//! Name of the table referenced by the table column.
		/*!
		 * This string remains empty, if no
		 *  other table column is referenced.
		 */
		std::string referenceTable;

		//! Name of the column referenced by the table column (or empty if none).
		/*!
		 * This string remains empty, if no
		 *  other table column is referenced.
		 */
		std::string referenceColumn;

		//! Indicates whether the table column is indexed.
		bool indexed;

		///@}
		///@name Construction
		///@{

		//! Constructor setting all properties of the table column.
		/*!
		 * \param setName Constant reference
		 *   to a string containing the name
		 *   of the new table column.
		 * \param setType Constant reference
		 *   to a string containing the type
		 *   of the new table column as
		 *   SQL string.
		 * \param setReferenceTable Constant
		 *   reference to a string containing
		 *   the name of the table referenced
		 *   by the new table column.
		 * \param setReferenceColumn Constant
		 *   reference to a string containing
		 *   the name of the column referenced
		 *   by the new table column.
		 * \param setIndexed Set whether the
		 *   new table column is indexed.
		 */
		TableColumn(
				const std::string& setName,
				const std::string& setType,
				const std::string& setReferenceTable,
				const std::string& setReferenceColumn,
				bool setIndexed
		) :	name(setName),
			type(setType),
			referenceTable(setReferenceTable),
			referenceColumn(setReferenceColumn),
			indexed(setIndexed) {}

		//! Constructor setting properties of an unindexed table column.
		/*!
		 * \param setName Constant reference
		 *   to a string containing the name
		 *   of the new table column.
		 * \param setType Constant reference
		 *   to a string containing the type
		 *   of the new table column as
		 *   SQL string.
		 * \param setReferenceTable Constant
		 *   reference to a string containing
		 *   the name of the table referenced
		 *   by the new table column.
		 * \param setReferenceColumn Constant
		 *   reference to a string containing
		 *   the name of the column referenced
		 *   by the new table column.
		 */
		TableColumn(
				const std::string& setName,
				const std::string& setType,
				const std::string& setReferenceTable,
				const std::string& setReferenceColumn
		) : TableColumn(setName, setType, setReferenceTable, setReferenceColumn, false) {}

		//! Constructor setting properties of an unreferenced table column.
		/*!
		 * \param setName Constant reference
		 *   to a string containing the name
		 *   of the new table column.
		 * \param setType Constant reference
		 *   to a string containing the type
		 *   of the new table column as
		 *   SQL string.
		 * \param setIndexed Set whether the
		 *   new table column is indexed.
		 */
		TableColumn(
				const std::string& setName,
				const std::string& setType,
				bool setIndexed
		) : TableColumn(setName, setType, "", "", setIndexed) {}

		//! Constructor setting properties of an unreferenced and unindexed table column.
		/*!
		 * \param setName Constant reference
		 *   to a string containing the name
		 *   of the new table column.
		 * \param setType Constant reference
		 *   to a string containing the type
		 *   of the new table column as
		 *   SQL string.
		 */
		TableColumn(
				const std::string& setName,
				const std::string& setType
		) : TableColumn(setName, setType, "", "", false) {}

		//! Constructor copying another table column and setting a new name for the copy.
		/*!
		 * \param other Constnt reference to
		 *  the table column to be copied from.
		 * \param newName Constant reference
		 *   to a string containing the name
		 *   of the new table column.
		 */
		TableColumn(
				const TableColumn& other,
				const std::string& newName
		) : TableColumn(newName, other.type, other.referenceTable, other.referenceColumn, other.indexed) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_TABLECOLUMN_HPP_ */
