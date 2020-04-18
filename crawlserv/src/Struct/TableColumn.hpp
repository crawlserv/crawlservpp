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

	struct TableColumn {
		std::string name;				// name of the column
		std::string type;				// type of the column
		std::string referenceTable;		// name of the referenced table
		std::string referenceColumn;	// name of the referenced column
		bool indexed;					// column is indexed

		TableColumn(
				const std::string& setName,
				const std::string& setType,
				const std::string& setReferenceTable,
				const std::string& setReferenceColumn,
				bool setIndexed)
		: name(setName),
		  type(setType),
		  referenceTable(setReferenceTable),
		  referenceColumn(setReferenceColumn),
		  indexed(setIndexed) {}

		TableColumn(
				const std::string& setName,
				const std::string& setType,
				const std::string& setReferenceTable,
				const std::string& setReferenceColumn
		) : TableColumn(setName, setType, setReferenceTable, setReferenceColumn, false) {}

		TableColumn(
				const std::string& setName,
				const std::string& setType,
				bool setIndexed
		) : TableColumn(setName, setType, "", "", setIndexed) {}

		TableColumn(
				const std::string& setName,
				const std::string& setType
		) : TableColumn(setName, setType, "", "", false) {}

		TableColumn(
				const std::string& newName,
				const TableColumn& c
		) : TableColumn(newName, c.type, c.referenceTable, c.referenceColumn, c.indexed) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_TABLECOLUMN_HPP_ */
