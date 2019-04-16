/*
 * TableProperties.hpp
 *
 * Table properties (name, columns, data directory, compression).
 *
 *  Created on: Mar 4, 2019
 *      Author: ans
 */


#ifndef STRUCT_TABLEPROPERTIES_HPP_
#define STRUCT_TABLEPROPERTIES_HPP_

#include "TableColumn.hpp"

#include <string>
#include <vector>

namespace crawlservpp::Struct {

	struct TableProperties {
		std::string name;
		std::vector<TableColumn> columns;
		std::string dataDirectory;
		bool compressed;

		// constructors
		TableProperties() : compressed(false) {}

		TableProperties(
				const std::string& setName,
				const std::vector<TableColumn>& setColumns,
				const std::string& setDataDirectory,
				bool setCompressed
		)		:	name(setName),
					columns(setColumns),
					dataDirectory(setDataDirectory),
					compressed(setCompressed) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_TABLEPROPERTIES_HPP_ */

