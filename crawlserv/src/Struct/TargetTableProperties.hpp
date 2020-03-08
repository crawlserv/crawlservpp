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
 * TargetTableProperties.hpp
 *
 * Target table properties (type, website, URL list, table name and full name, columns, data directory, compression).
 *
 * NOTE: type will be ignored on update!
 *
 *  Created on: Mar 4, 2019
 *      Author: ans
 */


#ifndef STRUCT_TARGETTABLEPROPERTIES_HPP_
#define STRUCT_TARGETTABLEPROPERTIES_HPP_

#include "TableColumn.hpp"

#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Struct {

	struct TargetTableProperties {
		std::string type;
		size_t website;
		size_t urlList;
		std::string name;
		std::string fullName;
		std::vector<TableColumn> columns;
		bool compressed;

		// constructors
		TargetTableProperties() : website(0), urlList(0), compressed(false) {}

		TargetTableProperties(
				const std::string& setType,
				size_t setWebsite,
				size_t setUrlList,
				const std::string& setName,
				const std::string& setFullName,
				bool setCompressed
		) : type(setType),
			website(setWebsite),
			urlList(setUrlList),
			name(setName),
			fullName(setFullName),
			compressed(setCompressed) {}

		TargetTableProperties(
				const std::string& setType,
				size_t setWebsite,
				size_t setUrlList,
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
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_TARGETTABLEPROPERTIES_HPP_ */

