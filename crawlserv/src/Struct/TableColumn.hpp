/*
 * TableColumn.h
 *
 * Structure for table columns.
 *
 *  Created on: Mar 4, 2019
 *      Author: ans
 */

#ifndef STRUCT_TABLECOLUMN_HPP_
#define STRUCT_TABLECOLUMN_HPP_

#include <string>

namespace crawlservpp::Struct {

	struct TableColumn {
		std::string name;				// name of the column
		std::string type;				// type of the column
		std::string referenceTable;		// name of the referenced table
		std::string referenceColumn;	// name of the referenced column
		bool indexed;					// is column indexed

		TableColumn(const std::string& setName, const std::string& setType, const std::string& setReferenceTable,
				const std::string& setReferenceColumn, bool setIndexed) : name(setName), type(setType),
						referenceTable(setReferenceTable), referenceColumn(setReferenceColumn), indexed(setIndexed) {}
		TableColumn(const std::string& setName, const std::string& setType, const std::string& setReferenceTable,
				const std::string& setReferenceColumn) : TableColumn(setName, setType, setReferenceTable, setReferenceColumn, false) {}
		TableColumn(const std::string& setName, const std::string& setType, bool setIndexed)
				: TableColumn(setName, setType, "", "", setIndexed) {}
		TableColumn(const std::string& setName, const std::string& setType) : TableColumn(setName, setType, "", "", false) {}
		TableColumn(const std::string& newName, const TableColumn& c)
				: TableColumn(newName, c.type, c.referenceTable, c.referenceColumn, c.indexed) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_TABLECOLUMN_HPP_ */
