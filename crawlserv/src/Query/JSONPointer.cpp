/*
 * JSONPointer.cpp
 *
 * Using the rapidjson library to implement a JSONPointer query with boolean, single and/or multiple results.
 *
 *  Created on: Apr 19, 2019
 *      Author: ans
 */

#include "JSONPointer.hpp"

namespace crawlservpp::Query {

	// constructor: set default values
	JSONPointer::JSONPointer(const std::string& pointerString) : pointer(pointerString) {}

	// get boolean value (at least one match?), throws JSONPointer::Exception
	bool JSONPointer::getBool(const rapidjson::Document& doc) const {
		// check document and pointer
		if(doc.HasParseError()) throw JSONPointer::Exception("JSON parsing error");
		if(!(this->pointer.IsValid())) throw JSONPointer::Exception("invalid JSONPointer");

		// evaluate query with boolean result
		return this->pointer.Get(doc) != nullptr;
	}

	// get first match only (saved to resultTo), , throws XPath::Exception
	void JSONPointer::getFirst(const rapidjson::Document& doc, std::string& resultTo) const {
		// check document and pointer
		if(doc.HasParseError()) throw JSONPointer::Exception("JSON parsing error");
		if(!(this->pointer.IsValid())) throw JSONPointer::Exception("invalid JSONPointer");

		// empty result
		resultTo.clear();

		// get result
		auto result = this->pointer.Get(doc);

		if(result != nullptr)
			// stringify result
			resultTo = Helper::Json::stringify(*result);
	}

} /* crawlservpp::Query */
