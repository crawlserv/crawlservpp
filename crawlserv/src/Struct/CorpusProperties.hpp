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
 * CorpusProperties.hpp
 *
 * Basic corpus properties (source type, table and field).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_CORPUSPROPERTIES_HPP_
#define STRUCT_CORPUSPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct CorpusProperties {
		unsigned short sourceType;
		std::string sourceTable;
		std::string sourceField;

		// constructors
		CorpusProperties() : sourceType(0) {}
		CorpusProperties(
				unsigned short setSourceType,
				const std::string& setSourceTable,
				const std::string& setSourceField
		) : sourceType(setSourceType),
			sourceTable(setSourceTable),
			sourceField(setSourceField) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_CORPUSPROPERTIES_HPP_ */
