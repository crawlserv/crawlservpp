/*
 * Json.cpp
 *
 * Namespace with global JSON helper functions.
 *
 *  Created on: Jan 9, 2019
 *      Author: ans
 */

#include "Json.h"

// stringify a vector of strings to JSON code in one string
std::string crawlservpp::Helper::Json::stringify(const std::vector<std::string>& vectorToStringify) {
	// create document as array and get reference to allocator
	rapidjson::Document document;
	document.SetArray();
	rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

	// write vector elements as string values to array
	for(auto i = vectorToStringify.begin(); i != vectorToStringify.end(); ++i) {
		rapidjson::Value stringValue;
		stringValue.SetString(i->c_str(), i->length(), allocator);
		document.PushBack(stringValue, allocator);
	}

	// create string buffer and writer
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

	// write array to string buffer
	document.Accept(writer);

	// return string
	return std::string(buffer.GetString(), buffer.GetSize());
}

// stringify a string to an array in JSON code
std::string crawlservpp::Helper::Json::stringify(const std::string& stringToStringify) {
	// create document as array and get reference to allocator
	rapidjson::Document document;
	document.SetArray();
	rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

	// write string as string element to array
	rapidjson::Value stringValue;
	stringValue.SetString(stringToStringify.c_str(), stringToStringify.length(), allocator);
	document.PushBack(stringValue, allocator);

	// create string buffer and writer
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

	// write array to string buffer
	document.Accept(writer);

	// return string
	return std::string(buffer.GetString(), buffer.GetSize());
}
