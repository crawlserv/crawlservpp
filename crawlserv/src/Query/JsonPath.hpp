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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * JSONPath.hpp
 *
 * Using the jsoncons library to implement a JSONPath query with boolean, single and/or multiple results.
 *
 *  Created on: Apr 26, 2019
 *      Author: ans
 */

#ifndef QUERY_JSONPATH_HPP_
#define QUERY_JSONPATH_HPP_

#include "../Main/Exception.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"

#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Query {

	class JsonPath {
	public:
		// constructor
		JsonPath(const std::string& pathString);

		// getters
		bool getBool(const jsoncons::json& json) const;
		void getFirst(const jsoncons::json& json, std::string& resultTo) const;
		void getAll(const jsoncons::json& json, std::vector<std::string>& resultTo) const;
		void getSubSets(const jsoncons::json& json, std::vector<jsoncons::json>& resultTo) const;

		// sub-class for JSONPath exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	private:
		const std::string jsonPath;
	};

} /* crawlservpp::Query */

#endif /* QUERY_JSONPATH_HPP_ */
