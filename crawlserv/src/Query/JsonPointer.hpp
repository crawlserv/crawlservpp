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
 * Using the rapidJSON library to implement a JSONPointer query
 *  with boolean, single and/or multiple results.
 *
 *  NOTE:	Different from the standard, multiple results are
 *  		 supported when using $$ as a placeholder for 0..n,
 *  		 where n is the number of matches minus 1.
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

	/*
	 * DECLARATION
	 */

	//! Implements an extended JSONPointer query using the @c rapidJSON library.
	/*!
	 * For more information about the @c rapidJSON library,
	 *  see its <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * Different from the JSONPointer standard (and the
	 *  library), multiple results are possible by using
	 *  @c $$ as a placeholder for @c 0..n, where @c n is
	 *  the number of matches minus @c 1.
	 */
	class JsonPointer {
	public:
		///@name Construction
		///@{

		JsonPointer(const std::string& pointerString, bool textOnlyQuery);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] bool getBool(const rapidjson::Document& doc) const;
		void getFirst(const rapidjson::Document& doc, std::string& resultTo) const;
		void getAll(const rapidjson::Document& doc, std::vector<std::string>& resultTo) const;
		void getSubSets(const rapidjson::Document& doc, std::vector<rapidjson::Document>& resultTo) const;

		///@}

		//! Class for JSONPointer exceptions.
		/*!
		 * Throws an exception when
		 * - the given JSONPointer expression is empty
		 * - the given string contains an invalid
		 *    JSONPointer query
		 * - parsing errors occured in a given JSON
		 *    document
		 * - no valid JSONPointer query has been
		 *    set prior to performing it
		 */
		MAIN_EXCEPTION_CLASS();

	private:
		rapidjson::Pointer pointerFirst;
		std::string pointerStringMulti;
		bool textOnly;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor setting a JSONPointer string and whether the result should be text-only.
	/*!
	 * \param pointerString Const reference to
	 *   a string containing the JSONPointer
	 *   expression. Will be trimmed before
	 *   use.
	 * \param textOnlyQuery Set whether the query
	 *   should result in raw text only. In case
	 *   of an array, the full array string will
	 *   be returned by getFirst when the query
	 *   is text-only. The same is true for
	 *   getAll if there is only one match.
	 *
	 * \throws JsonPointer::Exception if the
	 *   given string is empty after trimming,
	 *   or it contains an invalid JSONPointer
	 *   query.
	 */
	inline JsonPointer::JsonPointer(const std::string& pointerString, bool textOnlyQuery) : textOnly(textOnlyQuery) {
		// copy and trim pointer string
		std::string string{pointerString};

		Helper::Strings::trim(string);

		if(string.empty()) {
			throw Exception("No JSONPointer string given");
		}

		// check whether multiple JSONPointers need to be constructed
		if(string.find("$$") != std::string::npos) {
			this->pointerStringMulti = string;
		}

		if(this->pointerStringMulti.empty()) {
			this->pointerFirst = rapidjson::Pointer(string);

			if(!(this->pointerFirst.IsValid())) {
				throw JsonPointer::Exception("Invalid JSONPointer '" + string + "'");
			}
		}
		else {
			Helper::Strings::replaceAll(string, "$$", "0");

			this->pointerFirst = rapidjson::Pointer(string);

			if(!(this->pointerFirst.IsValid())) {
				throw JsonPointer::Exception("Invalid JSONPointer '" + string + "'");
			}
		}
	}

	//! Gets a boolean result from performing the query on a parsed JSON document.
	/*!
	 * \param doc Const reference to a JSON
	 *   document parsed by the @c rapidJSON
	 *   library.
	 *
	 * \returns True, if there is at least one
	 *   match after performing the query on the
	 *   document. False otherwise.
	 *
	 * \throws JsonPointer::Exception if parsing
	 *   errors occured in the given JSON document
	 *   or no valid JSONPointer query has been set.
	 */
	inline bool JsonPointer::getBool(const rapidjson::Document& doc) const {
		// check document and pointer
		if(doc.HasParseError()) {
			throw JsonPointer::Exception("JSON parsing error");
		}

		if(!(this->pointerFirst.IsValid())) {
			throw JsonPointer::Exception("Invalid JSONPointer");
		}

		// evaluate query with boolean result
		return this->pointerFirst.Get(doc) != nullptr;
	}

	//! Gets the first match from performing the query on a parsed JSON document.
	/*!
	 * \note If the first match is an array,
	 *   only the first element of it will
	 *   be returned, unless the query has
	 *   been set to text-only on construction.
	 *
	 * \param doc Const reference to a JSON
	 *   document parsed by the @c rapidJSON
	 *   library.
	 * \param resultTo Reference to a string
	 *   to which the result will be written.
	 *   The string will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws JsonPointer::Exception if parsing
	 *   errors occured in the given JSON document
	 *   or no valid JSONPointer query has been set.
	 *
	 * \sa JsonPointer::JsonPointer
	 */
	//  NOTE: if the match is an array, only the first element will be returned, unless the query is text-only
	inline void JsonPointer::getFirst(const rapidjson::Document& doc, std::string& resultTo) const {
		// empty result
		resultTo.clear();

		// check document and pointer
		if(doc.HasParseError()) {
			throw JsonPointer::Exception("Invalid JSON");
		}

		if(!(this->pointerFirst.IsValid())) {
			throw JsonPointer::Exception("Invalid JSONPointer");
		}

		// get result
		const auto * match{this->pointerFirst.Get(doc)};

		// check whether match exists
		if(match != nullptr) {
			// check type of result
			if(match->IsString()) {
				resultTo = std::string(match->GetString(), match->GetStringLength());
			}
			else if(match->IsArray() && !(this->textOnly)) {
				const auto& iterator{match->GetArray().Begin()};

				if(iterator != nullptr) {
					if(iterator->IsString()) {
						resultTo = std::string(iterator->GetString(), iterator->GetStringLength());
					}
					else {
						resultTo = Helper::Json::stringify(*iterator);
					}
				}
			}
			else {
				// stringify result
				resultTo = Helper::Json::stringify(*match);
			}
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
	 * \param doc Const reference to a JSON
	 *   document parsed by the @c rapidJSON
	 *   library.
	 * \param resultTo Reference to a vector
	 *   to which the results will be written.
	 *   The vector will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws JsonPointer::Exception if parsing
	 *   errors occured in the given JSON document
	 *   or no valid JSONPointer query has been set.
	 *
	 * \sa JsonPointer::JsonPointer
	 */
	inline void JsonPointer::getAll(const rapidjson::Document& doc, std::vector<std::string>& resultTo) const {
		// empty result
		resultTo.clear();

		// check document and pointer
		if(doc.HasParseError()) {
			throw JsonPointer::Exception("Invalid JSON");
		}

		if(!(this->pointerFirst.IsValid())) {
			throw JsonPointer::Exception("Invalid JSONPointer");
		}

		// check whether multiple matches are possible
		if(this->pointerStringMulti.empty()) {
			// get first match only, because multiple matches are not possible
			const auto * match{this->pointerFirst.Get(doc)};

			// check whether match exists
			if(match != nullptr) {
				// check for array
				if(match->IsArray() && !(this->textOnly)) {
					// reserve memory for array members
					resultTo.reserve(match->GetArray().Size());

					// go through all array members
					for(const auto& member : match->GetArray()) {
						// check for string
						if(member.IsString()) {
							resultTo.emplace_back(member.GetString(), member.GetStringLength());
						}
						else {
							// stringify array member
							resultTo.emplace_back(Helper::Json::stringify(member));
						}
					}
				}
				// check for string
				else if(match->IsString()) {
					resultTo.emplace_back(match->GetString(), match->GetStringLength());
				}
				else {
					// stringify match
					resultTo.emplace_back(Helper::Json::stringify(*match));
				}
			}
		}
		else {
			// get all matches
			std::size_t counter{0};

			while(true) {
				std::string pointerString(this->pointerStringMulti);

				Helper::Strings::replaceAll(pointerString, "$$", std::to_string(counter));

				const rapidjson::Pointer pointer{pointerString};

				if(!(pointer.IsValid())) {
					throw JsonPointer::Exception("Invalid JSONPointer '" + pointerString + "'");
				}

				const auto * match{pointer.Get(doc)};

				if(match == nullptr) {
					break;
				}

				// check type of result
				if(match->IsString()) {
					resultTo.emplace_back(match->GetString(), match->GetStringLength());
				}
				else {
					// stringify result
					resultTo.emplace_back(Helper::Json::stringify(*match));
				}

				++counter;
			}
		}
	}

	//! Gets all matching subsets from performing the query on a parsed JSON document.
	/*!
	 * The subsets will be saved as JSON documents
	 *  as defined by the @c rapidJSON library.
	 *
	 * \note If there is only one match and
	 *   it is an array, its members will be
	 *   returned separately, unless the
	 *   query has been set to text-only on
	 *   construction.
	 *
	 * \param doc Const reference to a JSON
	 *   document parsed by the jsoncons library.
	 * \param resultTo Reference to a vector
	 *   to which the results will be written.
	 *   The vector will be cleared even if
	 *   an error occurs during execution of
	 *   the query.
	 *
	 * \throws JsonPointer::Exception if parsing
	 *   errors occured in the given JSON document
	 *   or no valid JSONPointer query has been set.
	 *
	 * \sa JsonPointer::JsonPointer
	 */
	inline void JsonPointer::getSubSets(const rapidjson::Document& doc, std::vector<rapidjson::Document>& resultTo) const {
		// empty result
		resultTo.clear();

		// check document and pointer
		if(doc.HasParseError()) {
			throw JsonPointer::Exception("Invalid JSON");
		}

		if(!(this->pointerFirst.IsValid())) {
			throw JsonPointer::Exception("Invalid JSONPointer");
		}

		// check whether multiple matches are possible
		if(this->pointerStringMulti.empty()) { // get first match only, because multiple matches are not possible
			// get single match
			const auto * match{this->pointerFirst.Get(doc)};

			// check whether match exists
			if(match != nullptr) {
				// check whether match is an array (and query is not text-only)
				if(match->IsArray() && !(this->textOnly)) {
					// reserve memory for results
					resultTo.reserve(match->GetArray().Size());

					// go through all array members
					for(const auto& member : match->GetArray()) {
						// create new document for the array member at the end of the results
						resultTo.emplace_back();

						// copy the array member to the new document
						resultTo.back().CopyFrom(member, resultTo.back().GetAllocator());
					}
				}
				else {
					// create a new document for the match at the end of the results
					resultTo.emplace_back();

					// copy the match to the new document
					resultTo.back().CopyFrom(*match, resultTo.back().GetAllocator());
				}
			}
		}
		else {
			// get all matches
			std::size_t counter{0};

			// loop through all possible matches
			while(true) {
				// copy JSONPointer string for placeholder replacement
				std::string pointerString{this->pointerStringMulti};

				// replace placeholders with counter value
				Helper::Strings::replaceAll(pointerString, "$$", std::to_string(counter));

				// create (and check) JSONPointer
				const rapidjson::Pointer pointer{pointerString};

				if(!(pointer.IsValid())) {
					throw JsonPointer::Exception(
							"Invalid JSONPointer '"
							+ pointerString + "'"
					);
				}

				// get (and check) match
				const auto * match{pointer.Get(doc)};

				if(match == nullptr || match->IsNull()) {
					break;
				}

				// create a new document for the match at the end of the results
				resultTo.emplace_back();

				// copy the match to the new document
				resultTo.back().CopyFrom(*match, resultTo.back().GetAllocator());

				// increment counter
				++counter;
			}
		}
	}

} /* namespace crawlservpp::Query */

#endif /* QUERY_JSONPOINTER_HPP_ */
