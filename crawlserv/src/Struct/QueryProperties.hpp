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
 * QueryProperties.hpp
 *
 * Basic query properties (name, text, type and result type).
 *
 *  Created on: Feb 2, 2019
 *      Author: ans
 */

#ifndef STRUCT_QUERYPROPERTIES_HPP_
#define STRUCT_QUERYPROPERTIES_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct QueryProperties {
		std::string name;
		std::string text;
		std::string type;
		bool resultBool;
		bool resultSingle;
		bool resultMulti;
		bool resultSubSets;
		bool textOnly;

		// constructors
		QueryProperties() :	resultBool(false),
							resultSingle(false),
							resultMulti(false),
							resultSubSets(false),
							textOnly(false) {}
		QueryProperties(
				const std::string& setName,
				const std::string& setText,
				const std::string& setType,
				bool setResultBool,
				bool setResultSingle,
				bool setResultMulti,
				bool setResultSubSets,
				bool setTextOnly
		) : name(setName),
			text(setText),
			type(setType),
			resultBool(setResultBool),
			resultSingle(setResultSingle),
			resultMulti(setResultMulti),
			resultSubSets(setResultSubSets),
			textOnly(setTextOnly) {}

		QueryProperties(
				const std::string& setText,
				const std::string& setType,
				bool setResultBool,
				bool setResultSingle,
				bool setResultMulti,
				bool setResultSubSets,
				bool setTextOnly
		) : QueryProperties(
				"",
				setText,
				setType,
				setResultBool,
				setResultSingle,
				setResultMulti,
				setResultSubSets,
				setTextOnly
			) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_QUERYPROPERTIES_HPP_ */
