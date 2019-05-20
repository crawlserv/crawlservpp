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
 * JSONPath.cpp
 *
 * Using the jsoncons library to implement a JSONPath query with boolean, single and/or multiple results.
 *
 *  Created on: Apr 26, 2019
 *      Author: ans
 */

#include "JsonPath.hpp"

namespace crawlservpp::Query {

	// constructor: check and save JSONPath string
	JsonPath::JsonPath(const std::string& pathString) : jsonPath(pathString) {
		if(pathString.empty())
			throw Exception("No JSONPath defined");
	}

	// get boolean value from parsed JSON document (at least one match?), throws JSONPointer::Exception
	bool JsonPath::getBool(const jsoncons::json& json) const {
		// check JSONPath
		if(this->jsonPath.empty())
			throw Exception("No JSONPath defined");

		try {
			// evaluate query with boolean result
			const auto result(jsoncons::jsonpath::json_query(json, this->jsonPath));

			return !(result.is_array() && result.empty());
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					std::string(e.what())
					+ " (JSONPath: \'"
					+ this->jsonPath
					+ "\')"
			);
		}
	}

	// get first match from parsed JSON document (saved to resultTo), throws JSONPointer::Exception
	void JsonPath::getFirst(const jsoncons::json& json, std::string& resultTo) const {
		// check JSONPath
		if(this->jsonPath.empty())
			throw Exception("No JSONPath defined");

		// empty target
		resultTo.clear();

		try {
			// get result
			const auto result(jsoncons::jsonpath::json_query(json, this->jsonPath));

			// check for array
			if(result.is_array()) {
				if(!result.empty())
					// stringify first element of array
					resultTo = result[0].as<std::string>();
			}
			else
				// stringify value
				resultTo = result.as<std::string>();
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					std::string(e.what())
					+ " (JSONPath: \'"
					+ this->jsonPath + "\')"
			);
		}
	}

	// get all matches from parsed JSON document (saved to resultTo), throws JSONPointer::Exception
	void JsonPath::getAll(const jsoncons::json& json, std::vector<std::string>& resultTo) const {
		// check JSONPath
		if(this->jsonPath.empty())
			throw Exception("No JSONPath defined");

		// empty target
		resultTo.clear();

		try {
			// get result
			const auto result(jsoncons::jsonpath::json_query(json, this->jsonPath));

			// check for array
			if(result.is_array()) {
				// return all array members
				resultTo.reserve(result.array_value().size());

				for(const auto& element : result.array_range())
					resultTo.emplace_back(element.as<std::string>());
			}
			else
				// return result as single object
				resultTo.emplace_back(result.as<std::string>());
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					std::string(e.what())
					+ " (JSONPath: \'"
					+ this->jsonPath + "\')"
			);
		}
	}

	// get matching sub-sets from parsed JSON document (saved to resultTo), throws JSONPointer::Exception
	void JsonPath::getSubSets(const jsoncons::json& json, std::vector<jsoncons::json>& resultTo) const {
		// check JSONPath
		if(this->jsonPath.empty())
			throw Exception("No JSONPath defined");

		// empty target
		resultTo.clear();

		try {
			// get result
			const auto result(jsoncons::jsonpath::json_query(json, this->jsonPath));

			// check for array
			if(result.is_array()) {
				// copy all array members
				for(const auto& element : result.array_range())
					resultTo.emplace_back(element);
			}
			else
				// copy JSON object
				resultTo.emplace_back(result);
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					std::string(e.what())
					+ " (JSONPath: \'"
					+ this->jsonPath + "\')"
			);
		}
	}
} /* crawlservpp::Query */
