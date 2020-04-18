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
 * JSONPointer.hpp
 *
 * Using the rapidjson library to implement a JSONPointer query with boolean, single and/or multiple results.
 *
 *  NOTE:	Different from the standard, multiple results are supported when using $$ as a placeholder for 0..n,
 *  		where n is the number of matches minus 1.
 *
 *  Created on: Apr 19, 2019
 *      Author: ans
 */

#ifndef QUERY_JSONPOINTER_HPP_
#define QUERY_JSONPOINTER_HPP_

#include "../Helper/Json.hpp"
#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"

#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/pointer.h"

#include <cstddef>	// std::size_t
#include <limits>	// std::numeric_limits
#include <string>	// std::string, std::to_string
#include <vector>	// std::vector

namespace crawlservpp::Query {

	class JsonPointer {
	public:
		// constructor and destructor
		JsonPointer(const std::string& pointerString, bool textOnlyQuery);

		// getters
		bool getBool(const rapidjson::Document& doc) const;
		void getFirst(const rapidjson::Document& doc, std::string& resultTo) const;
		void getAll(const rapidjson::Document& doc, std::vector<std::string>& resultTo) const;
		void getSubSets(const rapidjson::Document& doc, std::vector<rapidjson::Document>& resultTo) const;

		// class for JSONPointer exceptions
		MAIN_EXCEPTION_CLASS();

	private:
		rapidjson::Pointer pointerFirst;
		std::string pointerStringMulti;
		bool textOnly;
	};

} /* crawlservpp::Query */

#endif /* QUERY_JSONPOINTER_HPP_ */
