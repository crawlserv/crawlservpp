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
 * JSONPath.hpp
 *
 * Using the jsoncons library to implement a JSONPath query
 *  with boolean, single and/or multiple results.
 *
 *  Created on: Apr 26, 2019
 *      Author: ans
 */

#ifndef QUERY_JSONPATH_HPP_
#define QUERY_JSONPATH_HPP_

#include "../Helper/Strings.hpp"
#include "../Main/Exception.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"

#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Query {

	/*
	 * DECLARATION
	 */

	//! Implements a JSONPath query using the jsoncons library.
	/*!
	 * For more information about the jsoncons library, see
	 *  its <a href="https://github.com/danielaparker/jsoncons>
	 *  GitHub repository</a>.
	 */
	class JsonPath {
	public:
		///@name Construction
		///@{

		JsonPath(const std::string& pathString, bool textOnlyQuery);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] bool getBool(const jsoncons::json& json) const;
		void getFirst(const jsoncons::json& json, std::string& resultTo) const;
		void getAll(const jsoncons::json& json, std::vector<std::string>& resultTo) const;
		void getSubSets(const jsoncons::json& json, std::vector<jsoncons::json>& resultTo) const;

		///@}

		//! Class for JSONPath exceptions.
		/*!
		 * Throws an exception when
		 * - the given JSONPath expression is empty
		 * - no JSONPath query has been defined
		 *    prior to performing it
		 * - an error occurs during execution
		 *    of the query
		 * - a query that should wield results
		 *    does not return an array
		 */
		MAIN_EXCEPTION_CLASS();

	private:
		std::string jsonPath;
		const bool textOnly;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor setting a JSONPath string and whether the result should be text-only.
	/*!
	 * \param pathString Const reference to
	 *   a string containing the JSONPath
	 *   expression. Will be trimmed before
	 *   use.
	 * \param textOnlyQuery Set whether the
	 *   query should result in raw text only.
	 *   In case  of an array, the full array
	 *   will be returned by getFirst when the
	 *   query is text-only. The same is true
	 *   for getAll if there is only one match.
	 *
	 * \throws JsonPath::Exception if the given
	 *   string is empty after trimming.
	 */
	inline JsonPath::JsonPath(const std::string& pathString, bool textOnlyQuery)
			: jsonPath(pathString),
			  textOnly(textOnlyQuery) {
		Helper::Strings::trim(this->jsonPath);

		if(this->jsonPath.empty()) {
			throw Exception("No JSONPath string given");
		}
	}

	//! Gets a boolean result from performing the query on a parsed JSON document.
	/*!
	 * \param json Const reference to a JSON
	 *   document parsed by the jsoncons library.
	 *
	 * \returns True, if there is at least one
	 *   match after performing the query on the
	 *   document. False otherwise.
	 *
	 * \throws JsonPath::Exception if no JSONPath
	 *   query has been defined or an error
	 *   occurs during execution of the query.
	 */
	inline bool JsonPath::getBool(const jsoncons::json& json) const {
		// check JSONPath query
		if(this->jsonPath.empty()) {
			throw Exception("No JSONPath query defined");
		}

		try {
			// evaluate query with boolean result
			const auto result{jsoncons::jsonpath::json_query(json, this->jsonPath)};

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

	//! Gets the first match from performing the query on a parsed JSON document.
	/*!
	 * \note If the first match is an array,
	 *   only the first element of it will
	 *   be returned, unless the query has
	 *   been set to text-only on construction.
	 *
	 * \param json Const reference to a JSON
	 *   document parsed by the jsoncons library.
	 * \param resultTo Reference to a string
	 *   to which the result will be written.
	 *   The string will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws JsonPath::Exception if no JSONPath
	 *   query has been defined, an error occurs
	 *   during execution of the query, or the
	 *   query did not return an array.
	 *
	 * \sa JsonPath::JsonPath
	 */
	inline void JsonPath::getFirst(const jsoncons::json& json, std::string& resultTo) const {
		// empty target
		resultTo.clear();

		// check JSONPath
		if(this->jsonPath.empty()) {
			throw Exception("No JSONPath defined");
		}

		try {
			// get result
			const auto result{jsoncons::jsonpath::json_query(json, this->jsonPath)};

			// check validity of result
			if(!result.is_array()) {
				throw Exception("jsoncons::jsonpath::json_query() did not return an array");
			}

			// check whether there are matches
			if(!result.array_value().empty()) {
				if(result[0].is_array() && !(this->textOnly)) {
					// return first array member of first match
					resultTo = result[0][0].as<std::string>();
				}
				else {
					// return first match only
					resultTo = result[0].as<std::string>();
				}
			}
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					std::string(e.what())
					+ " (JSONPath: \'"
					+ this->jsonPath + "\')"
			);
		}
	}

	//! Gets all matches from performing the query on a parsed JSON document.
	/*!
	 * \note If there is only one match and
	 *   it is an array, its members will be
	 *   returned separately, unless the
	 *   query has been set to text-only on
	 *   construction.
	 *
	 * \param json Const reference to a JSON
	 *   document parsed by the jsoncons library.
	 * \param resultTo Reference to a vector
	 *   to which the results will be written.
	 *   The vector will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws JsonPath::Exception if no JSONPath
	 *   query has been defined, an error occurs
	 *   during execution of the query, or the
	 *   query did not return an array.
	 *
	 * \sa JsonPath::JsonPath
	 */
	inline void JsonPath::getAll(const jsoncons::json& json, std::vector<std::string>& resultTo) const {
		// empty target
		resultTo.clear();

		// check JSONPath
		if(this->jsonPath.empty()) {
			throw Exception("No JSONPath defined");
		}

		try {
			// get result
			const auto result{jsoncons::jsonpath::json_query(json, this->jsonPath)};

			// check validity of result
			if(!result.is_array()) {
				throw Exception("jsoncons::jsonpath::json_query() did not return an array");
			}

			// check number of matches
			switch(result.array_value().size()) {
			case 0:
				break;

			case 1:
				if(result[0].is_array() && !(this->textOnly)) {
					// return all array members of first match
					resultTo.reserve(result[0].array_value().size());

					for(const auto& element : result[0].array_range()) {
						resultTo.emplace_back(element.as<std::string>());
					}
				}
				else {
					resultTo.emplace_back(result[0].as<std::string>());
				}

				break;

			default:
				// return all matches
				resultTo.reserve(result.array_value().size());

				for(const auto& element : result.array_range()) {
					resultTo.emplace_back(element.as<std::string>());
				}
			}
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					std::string(e.what())
					+ " (JSONPath: \'"
					+ this->jsonPath + "\')"
			);
		}
	}

	//! Gets all matching subsets from performing the query on a parsed JSON document.
	/*!
	 * The subsets will be saved as JSON documents
	 *  as defined by the jsoncons library.
	 *
	 * \note If there is only one match and
	 *   it is an array, its members will be
	 *   returned separately, unless the
	 *   query has been set to text-only on
	 *   construction.
	 *
	 * \param json Const reference to a JSON
	 *   document parsed by the jsoncons library.
	 * \param resultTo Reference to a vector
	 *   to which the results will be written.
	 *   The vector will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws JsonPath::Exception if no JSONPath
	 *   query has been defined, an error occurs
	 *   during execution of the query, or the
	 *   query did not return an array.
	 *
	 * \sa JsonPath::JsonPath
	 */
	inline void JsonPath::getSubSets(const jsoncons::json& json, std::vector<jsoncons::json>& resultTo) const {
		// empty target
		resultTo.clear();

		// check JSONPath
		if(this->jsonPath.empty()) {
			throw Exception("No JSONPath defined");
		}

		try {
			// get result
			const auto result{jsoncons::jsonpath::json_query(json, this->jsonPath)};

			// check validity of result
			if(!result.is_array()) {
				throw Exception("jsoncons::jsonpath::json_query() did not return an array");
			}

			// check number of matches
			switch(result.array_value().size()) {
			case 0:
				break;

			case 1:
				if(result[0].is_array() && !(this->textOnly)) {
					// return all array members of first match
					resultTo.reserve(result[0].array_value().size());

					for(const auto& element : result[0].array_range()) {
						resultTo.emplace_back(element);
					}
				}
				else {
					resultTo.emplace_back(result[0]);
				}

				break;

			default:
				// return all matches
				resultTo.reserve(result.array_value().size());

				for(const auto& element : result.array_range()) {
					resultTo.emplace_back(element);
				}
			}
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					std::string(e.what())
					+ " (JSONPath: \'"
					+ this->jsonPath + "\')"
			);
		}
	}

} /* namespace crawlservpp::Query */

#endif /* QUERY_JSONPATH_HPP_ */
