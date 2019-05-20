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
 * Json.hpp
 *
 * Namespace with global JSON helper functions.
 *
 *  Created on: Jan 9, 2019
 *      Author: ans
 */

#ifndef HELPER_JSON_HPP_
#define HELPER_JSON_HPP_

#include "../Main/Exception.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"

#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/error/en.h"
#include "../_extern/rapidjson/include/rapidjson/error/error.h"
#include "../_extern/rapidjson/include/rapidjson/stringbuffer.h"
#include "../_extern/rapidjson/include/rapidjson/writer.h"

#include <string>	// std::string
#include <tuple>	// std::get(std::tuple), std::tuple
#include <utility>	// std::pair
#include <vector>	// std::vector

namespace crawlservpp::Helper::Json {

	/*
	 * DECLARATION
	 */

	// text maps are used to describe certain parts of a text defined by their positions and lengths with certain strings
	//  (words, dates etc.)
	typedef std::vector<std::tuple<std::string, unsigned long, unsigned long>> TextMap;

	// stringify different kind of data to a JSON string
	std::string stringify(const std::vector<std::string>& vectorToStringify);
	std::string stringify(const std::string& stringToStringify);
	std::string stringify(const std::vector<std::vector<std::pair<std::string, std::string>>>& vectorToStringify);
	std::string stringify(const TextMap& textMapToStringify);
	std::string stringify(const rapidjson::Value& value);
	std::string stringify(const jsoncons::json& json);

	// parsing functions
	rapidjson::Document parseRapid(const std::string& json);
	jsoncons::json parseCons(const std::string& json);

	/*
	 * CLASS FOR JSON EXCEPTIONS
	 */

	class Exception : public Main::Exception {
	public:
		Exception(const std::string& description) : Main::Exception(description) {}
		virtual ~Exception() {}
	};

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
		for(const auto& element : vectorToStringify) {
			rapidjson::Value stringValue;
			
			stringValue.SetString(element, allocator);
			
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
		// create document as array and get reference to allocator
		rapidjson::Document document;
		
		document.SetArray();
		
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		// reserve memory for all array elements
		document.Reserve(vectorToStringify.size(), allocator);

		// go through the vector elements representing the objects in the array
		for(const auto& element : vectorToStringify) {
			// create object and reserve memory for all [key, value] pairs
			rapidjson::Value objectValue;
			
			objectValue.SetObject();

			// go through the sub-vector elements representing the [key, value] pairs in the object
			for(const auto& pair : element) {
				// create key
				rapidjson::Value key;
				
				key.SetString(pair.first, allocator);

				// create value
				rapidjson::Value value;
				
				value.SetString(pair.second, allocator);

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
		for(const auto& element : textmapToStringify) {
			// create object and reserve memory for all [key, value] pairs
			rapidjson::Value objectValue;
			
			objectValue.SetObject();

			// create and add [key, value] pair for describing string
			rapidjson::Value keyValue;
			
			keyValue.SetString("v", 1, allocator);
			
			rapidjson::Value valueValue;
			
			valueValue.SetString(std::get<0>(element), allocator);
			
			objectValue.AddMember(keyValue, valueValue, allocator);

			// create and add [key, value] pair for position
			rapidjson::Value keyPos;
			
			keyPos.SetString("p", 1, allocator);
			
			rapidjson::Value valuePos;
			
			valuePos.SetUint64(std::get<1>(element));
			
			objectValue.AddMember(keyPos, valuePos, allocator);

			// create and add [key, value] pair for length
			rapidjson::Value keyLength;
			
			keyLength.SetString("l", 1, allocator);
			
			rapidjson::Value valueLength;
			
			valueLength.SetUint64(std::get<2>(element));
			
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

	// stringify a JSON value using RapidJSON
	inline std::string stringify(const rapidjson::Value& value) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		value.Accept(writer);

		return buffer.GetString();
	}

	// stringify JSON using jsoncons
	inline std::string stringify(const jsoncons::json& json) {
		std::string result;

		json.dump(result);

		return result;
	}

	// parse JSON using RapidJSON, throws Json::Exception
	inline rapidjson::Document parseRapid(const std::string& json) {
		rapidjson::Document doc;

		doc.Parse(json);

		if(doc.HasParseError()) {
			std::string exceptionStr(rapidjson::GetParseError_En(doc.GetParseError()));

			exceptionStr += " at \'";

			if(doc.GetErrorOffset() > 25)
				exceptionStr += json.substr(doc.GetErrorOffset() - 25, 25);
			else if(doc.GetErrorOffset() > 0)
				exceptionStr += json.substr(0, doc.GetErrorOffset());

			exceptionStr += "[!]";

			if(json.size() > doc.GetErrorOffset() + 25)
				exceptionStr += json.substr(doc.GetErrorOffset(), 25);
			else if(json.size() > doc.GetErrorOffset())
				exceptionStr += json.substr(doc.GetErrorOffset());

			exceptionStr += "\'";

			throw Exception(exceptionStr);
		}

		return doc;
	}

	// parse JSON using jsoncons, throws Json::Exception
	inline jsoncons::json parseCons(const std::string& json) {
		try {
			jsoncons::json result = jsoncons::json::parse(json);

			return result;
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(e.what());
		}
	}

} /* crawlservpp::Helper::Json */

#endif /* HELPER_JSON_HPP_ */
