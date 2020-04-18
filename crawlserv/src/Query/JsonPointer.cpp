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
 * JSONPointer.cpp
 *
 * Using the rapidjson library to implement a JSONPointer query with boolean, single and/or multiple results.
 *
 *  NOTE:	Different from the standard, multiple results are supported when using $$ as a placeholder for 0..n,
 *  		where n is the number of matches minus 1.
 *
 *  Created on: Apr 19, 2019
 *      Author: ans
 */

#include "JsonPointer.hpp"

namespace crawlservpp::Query {

	// constructor: check for placeholder and create JSONPointer for first result, throws JSONPointer::Exception
	JsonPointer::JsonPointer(const std::string& pointerString, bool textOnlyQuery) : textOnly(textOnlyQuery) {
		// copy and trim pointer string
		std::string string(pointerString);

		Helper::Strings::trim(string);

		// check whether multiple JSONPointers need to be constructed
		if(string.find("$$") != std::string::npos)
			this->pointerStringMulti = string;

		if(this->pointerStringMulti.empty()) {
			this->pointerFirst = rapidjson::Pointer(string);

			if(!(this->pointerFirst.IsValid()))
				throw JsonPointer::Exception("Invalid JSONPointer \'" + string + "\'");
		}
		else {
			Helper::Strings::replaceAll(string, "$$", "0", true);

			this->pointerFirst = rapidjson::Pointer(string);

			if(!(this->pointerFirst.IsValid()))
				throw JsonPointer::Exception("Invalid JSONPointer \'" + string + "\'");
		}
	}

	// get boolean value from parsed JSON document (at least one match?), throws JSONPointer::Exception
	bool JsonPointer::getBool(const rapidjson::Document& doc) const {
		// check document and pointer
		if(doc.HasParseError())
			throw JsonPointer::Exception("JSON parsing error");

		if(!(this->pointerFirst.IsValid()))
			throw JsonPointer::Exception("Invalid JSONPointer");

		// evaluate query with boolean result
		return this->pointerFirst.Get(doc) != nullptr;
	}

	// get first match from parsed JSON document (saved to resultTo), throws JSONPointer::Exception
	//  NOTE: if the match is an array, only the first element will be returned, unless the query is text-only
	void JsonPointer::getFirst(const rapidjson::Document& doc, std::string& resultTo) const {
		// check document and pointer
		if(doc.HasParseError())
			throw JsonPointer::Exception("Invalid JSON");

		if(!(this->pointerFirst.IsValid()))
			throw JsonPointer::Exception("Invalid JSONPointer");

		// empty result
		resultTo.clear();

		// get result
		const auto match(this->pointerFirst.Get(doc));

		// check whether match exists
		if(match != nullptr) {
			// check type of result
			if(match->IsString())
				resultTo = std::string(match->GetString(), match->GetStringLength());
			else if(match->IsArray() && !(this->textOnly)) {
				const auto& iterator = match->GetArray().Begin();

				if(iterator->IsString())
					resultTo = std::string(iterator->GetString(), iterator->GetStringLength());
				else
					resultTo = Helper::Json::stringify(*iterator);
			}
			else
				// stringify result
				resultTo = Helper::Json::stringify(*match);
		}
	}

	// get all matches from parsed JSON document (saved to resultTo), throws JSONPointer::Exception
	//  NOTE: if there is only one match and it is an array, its members will be returned separately unless text-only is enabled
	void JsonPointer::getAll(const rapidjson::Document& doc, std::vector<std::string>& resultTo) const {
		// check document and pointer
		if(doc.HasParseError())
			throw JsonPointer::Exception("Invalid JSON");

		if(!(this->pointerFirst.IsValid()))
			throw JsonPointer::Exception("Invalid JSONPointer");

		// empty result
		resultTo.clear();

		// check whether multiple matches are possible
		if(this->pointerStringMulti.empty()) {
			// get first match only, because multiple matches are not possible
			const auto match(this->pointerFirst.Get(doc));

			// check whether match exists
			if(match != nullptr) {
				// check for array
				if(match->IsArray() && !(this->textOnly)) {
					// reserve memory for array members
					resultTo.reserve(match->GetArray().Size());

					// go through all array members
					for(const auto& member : match->GetArray())
						// check for string
						if(member.IsString())
							resultTo.emplace_back(member.GetString(), member.GetStringLength());
						else
							// stringify array member
							resultTo.emplace_back(Helper::Json::stringify(member));
				}
				// check for string
				else if(match->IsString())
					resultTo.emplace_back(match->GetString(), match->GetStringLength());
				else
					// stringify match
					resultTo.emplace_back(Helper::Json::stringify(*match));
			}
		}
		else {
			// get all matches
			std::size_t counter = 0;

			while(true) {
				std::string pointerString(this->pointerStringMulti);

				Helper::Strings::replaceAll(pointerString, "$$", std::to_string(counter), true);

				const rapidjson::Pointer pointer(pointerString);

				if(!(pointer.IsValid()))
					throw JsonPointer::Exception("Invalid JSONPointer \'" + pointerString + "\'");

				const auto match(pointer.Get(doc));

				if(match == nullptr)
					break;

				// check type of result
				if(match->IsString())
					resultTo.emplace_back(match->GetString(), match->GetStringLength());
				else
					// stringify result
					resultTo.emplace_back(Helper::Json::stringify(*match));

				++counter;
			}
		}
	}

	// get matching sub-sets from parsed JSON document (saved to resultTo), throws JSONPointer::Exception
	//  NOTE: if there is only one match possible and it is an array, its members will be returned separately unless text-only is enabled
	void JsonPointer::getSubSets(const rapidjson::Document& doc, std::vector<rapidjson::Document>& resultTo) const {
		// check document and pointer
		if(doc.HasParseError())
			throw JsonPointer::Exception("Invalid JSON");

		if(!(this->pointerFirst.IsValid()))
			throw JsonPointer::Exception("Invalid JSONPointer");

		// empty result
		resultTo.clear();

		// check whether multiple matches are possible
		if(this->pointerStringMulti.empty()) { // get first match only, because multiple matches are not possible
			// get single match
			const auto match(this->pointerFirst.Get(doc));

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
			std::size_t counter = 0;

			// loop through all possible matches
			while(true) {
				// copy JSONPointer string for placeholder replacement
				std::string pointerString(this->pointerStringMulti);

				// replace placeholders with counter value
				Helper::Strings::replaceAll(pointerString, "$$", std::to_string(counter), true);

				// create (and check) JSONPointer
				const rapidjson::Pointer pointer(pointerString);

				if(!(pointer.IsValid()))
					throw JsonPointer::Exception("Invalid JSONPointer \'" + pointerString + "\'");

				// get (and check) match
				const auto match(pointer.Get(doc));

				if(match == nullptr || match->IsNull())
					break;

				// create a new document for the match at the end of the results
				resultTo.emplace_back();

				// copy the match to the new document
				resultTo.back().CopyFrom(*match, resultTo.back().GetAllocator());

				// increment counter
				++counter;
			}
		}
	}
} /* crawlservpp::Query */
