/*
 * Json.hpp
 *
 * Namespace with global JSON helper functions.
 *
 *  Created on: Jan 9, 2019
 *      Author: ans
 */

#ifndef HELPER_JSON_HPP_
#define HELPER_JSON_HPP_

#define RAPIDJSON_HAS_STDSTRING 1
#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/stringbuffer.h"
#include "../_extern/rapidjson/include/rapidjson/writer.h"

#include <string>
#include <tuple>
#include <vector>

namespace crawlservpp::Helper::Json {

	/*
	 * DECLARATION
	 */

	// text maps are used to describe certain parts of a text defined by their positions and lengths with certain strings (words, dates etc.)
	typedef std::vector<std::tuple<std::string, unsigned long, unsigned long>> TextMap;

	// stringify different kind of data to a JSON string
	std::string stringify(const std::vector<std::string>& vectorToStringify);
	std::string stringify(const std::string& stringToStringify);
	std::string stringify(const std::vector<std::vector<std::pair<std::string, std::string>>>& vectorToStringify);
	std::string stringify(const TextMap& textMapToStringify);

	/*
	 * IMPLEMENTATION
	 */

	// stringify a vector of strings to a JSON array in one string
	inline std::string stringify(const std::vector<std::string>& vectorToStringify) {
		// create document as array and get reference to allocator
		rapidjson::Document document;
		document.SetArray();
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		// reserve memory for all array elements
		document.Reserve(vectorToStringify.size(), allocator);

		// write vector elements as string values to array
		for(auto i = vectorToStringify.begin(); i != vectorToStringify.end(); ++i) {
			rapidjson::Value stringValue;
			stringValue.SetString(*i, allocator);
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

	// stringify a single string to a JSON array with a single element in one string
	inline std::string stringify(const std::string& stringToStringify) {
		// create document as array and get reference to allocator
		rapidjson::Document document;
		document.SetArray();
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		// write string as string element to array
		rapidjson::Value stringValue;
		stringValue.SetString(stringToStringify, allocator);
		document.PushBack(stringValue, allocator);

		// create string buffer and writer
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		// write array to string buffer
		document.Accept(writer);

		// return string
		return std::string(buffer.GetString(), buffer.GetSize());
	}

	// stringify a vector of vectors of string pairs to a JSON array with corresponding objects containing [key, value] pairs
	inline std::string stringify(const std::vector<std::vector<std::pair<std::string, std::string>>>& vectorToStringify) {
		// create document as array and get refference to allocator
		rapidjson::Document document;
		document.SetArray();
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		// reserve memory for all array elements
		document.Reserve(vectorToStringify.size(), allocator);

		// go through the vector elements representing the objects in the array
		for(auto i = vectorToStringify.begin(); i != vectorToStringify.end(); ++i) {
			// create object and reserve memory for all [key, value] pairs
			rapidjson::Value objectValue;
			objectValue.SetObject();

			// go through the sub-vector elements representing the [key, value] pairs in the object
			for(auto j = i->begin(); j != i->end(); j++) {
				// create key
				rapidjson::Value key;
				key.SetString(j->first, allocator);

				// create value
				rapidjson::Value value;
				value.SetString(j->second, allocator);

				// add [key, value] pair to object
				objectValue.AddMember(key, value, allocator);
			}

			// add object to array
			document.PushBack(objectValue, allocator);
		}

		// create string buffer and writer
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		// write array to string buffer
		document.Accept(writer);

		// return string
		return std::string(buffer.GetString(), buffer.GetSize());
	}

	// stringify a text map to a JSON array with corresponding objects containing [key, value] pairs
	inline std::string stringify(const TextMap& textmapToStringify) {
		// create document as array and get refference to allocator
		rapidjson::Document document;
		document.SetArray();
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		// reserve memory for all array elements
		document.Reserve(textmapToStringify.size(), allocator);

		// go through the vector elements representing the objects in the array
		for(auto i = textmapToStringify.begin(); i != textmapToStringify.end(); ++i) {
			// create object and reserve memory for all [key, value] pairs
			rapidjson::Value objectValue;
			objectValue.SetObject();

			// create and add [key, value] pair for describing string
			rapidjson::Value keyValue;
			keyValue.SetString("v", 1, allocator);
			rapidjson::Value valueValue;
			valueValue.SetString(std::get<0>(*i), allocator);
			objectValue.AddMember(keyValue, valueValue, allocator);

			// create and add [key, value] pair for position
			rapidjson::Value keyPos;
			keyPos.SetString("p", 1, allocator);
			rapidjson::Value valuePos;
			valuePos.SetUint64(std::get<1>(*i));
			objectValue.AddMember(keyPos, valuePos, allocator);

			// create and add [key, value] pair for length
			rapidjson::Value keyLength;
			keyLength.SetString("l", 1, allocator);
			rapidjson::Value valueLength;
			valueLength.SetUint64(std::get<2>(*i));
			objectValue.AddMember(keyLength, valueLength, allocator);

			// add object to array
			document.PushBack(objectValue, allocator);
		}

		// create string buffer and writer
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		// write array to string buffer
		document.Accept(writer);

		// return string
		return std::string(buffer.GetString(), buffer.GetSize());
	}

} /* crawlservpp::Helper::Json */

#endif /* HELPER_JSON_HPP_ */