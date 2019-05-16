/*
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
	JsonPointer::JsonPointer(const std::string& pointerString) {
		if(pointerString.find("$$") != std::string::npos)
			this->pointerStringMulti = pointerString;

		if(this->pointerStringMulti.empty()) {
			this->pointerFirst = rapidjson::Pointer(pointerString);

			if(!(this->pointerFirst.IsValid()))
				throw JsonPointer::Exception("Invalid JSONPointer \'" + pointerString + "\'");
		}
		else {
			std::string pointerFirstString(pointerString);

			Helper::Strings::replaceAll(pointerFirstString, "$$", "0", true);

			this->pointerFirst = rapidjson::Pointer(pointerFirstString);

			if(!(this->pointerFirst.IsValid()))
				throw JsonPointer::Exception("Invalid JSONPointer \'" + pointerFirstString + "\'");
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
	void JsonPointer::getFirst(const rapidjson::Document& doc, std::string& resultTo) const {
		// check document and pointer
		if(doc.HasParseError())
			throw JsonPointer::Exception("Invalid JSON");

		if(!(this->pointerFirst.IsValid()))
			throw JsonPointer::Exception("Invalid JSONPointer");

		// empty result
		resultTo.clear();

		// get result
		const auto result(this->pointerFirst.Get(doc));

		if(result != nullptr) {
			// check type of result
			if(result->IsString())
				resultTo = std::string(result->GetString(), result->GetStringLength());
			else
				// stringify result
				resultTo = Helper::Json::stringify(*result);
		}
	}

	// get all matches from parsed JSON document (saved to resultTo), throws JSONPointer::Exception
	void JsonPointer::getAll(const rapidjson::Document& doc, std::vector<std::string>& resultTo) const {
		// check document and pointer
		if(doc.HasParseError())
			throw JsonPointer::Exception("Invalid JSON");

		// empty result
		resultTo.clear();

		// check whether multiple matches are possible
		if(this->pointerStringMulti.empty()) {
			// get first match only, because multiple matches are not possible
			std::string result;

			this->getFirst(doc, result);

			if(result.size())
				resultTo.emplace_back(result);
		}
		else {
			// get all matches
			unsigned long counter = 0;

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
			// create a new document for the match at the end of the results
			resultTo.emplace_back();

			// copy the match to the new document
			resultTo.back().CopyFrom(*this->pointerFirst.Get(doc), resultTo.back().GetAllocator());
		}
		else {
			// get all matches
			unsigned long counter = 0;

			while(true) {
				std::string pointerString(this->pointerStringMulti);

				Helper::Strings::replaceAll(pointerString, "$$", std::to_string(counter), true);

				const rapidjson::Pointer pointer(pointerString);

				if(!(pointer.IsValid()))
					throw JsonPointer::Exception("Invalid JSONPointer \'" + pointerString + "\'");

				const auto match(pointer.Get(doc));

				if(match == nullptr)
					break;

				// create a new document for the match at the end of the results
				resultTo.emplace_back();

				// copy the match to the new document
				resultTo.back().CopyFrom(*match, resultTo.back().GetAllocator());

				++counter;
			}
		}
	}
} /* crawlservpp::Query */
