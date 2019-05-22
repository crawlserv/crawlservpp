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
 * Table properties (name, columns, data directory, compression).
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
		) : name(setName),
			columns(setColumns),
			dataDirectory(setDataDirectory),
			compressed(setCompressed) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_TABLEPROPERTIES_HPP_ */

