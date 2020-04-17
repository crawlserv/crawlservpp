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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
#include "../Struct/TextMap.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"

#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/error/en.h"
#include "../_extern/rapidjson/include/rapidjson/error/error.h"
#include "../_extern/rapidjson/include/rapidjson/stringbuffer.h"
#include "../_extern/rapidjson/include/rapidjson/writer.h"

#include <cctype>	// ::iscntrl, ::isxdigit, ::tolower
#include <string>	// std::string
#include <utility>	// std::pair
#include <vector>	// std::vector

namespace crawlservpp::Helper::Json {

	/*
	 * DECLARATION
	 */

	// stringify different kind of data to a JSON string
	std::string stringify(const std::vector<std::string>& vectorToStringify);
	std::string stringify(const std::string& stringToStringify);
	std::string stringify(const std::vector<std::vector<std::pair<std::string, std::string>>>& vectorToStringify);
	std::string stringify(const Struct::TextMap& textMapToStringify);
	std::string stringify(const rapidjson::Value& value);
	std::string stringify(const jsoncons::json& json);

	// parsing functions
	std::string cleanCopy(const std::string& json);
	rapidjson::Document parseRapid(const std::string& json);
	jsoncons::json parseCons(const std::string& json);
	Struct::TextMap parseTextMapJson(const std::string& json);

	/*
	 * CLASS FOR JSON EXCEPTIONS
	 */

	MAIN_EXCEPTION_CLASS();

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
	inline std::string stringify(const Struct::TextMap& textMapToStringify) {
		// create document as array and get refference to allocator
		rapidjson::Document document;
		
		document.SetArray();
		
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

		// reserve memory for all array elements
		document.Reserve(textMapToStringify.size(), allocator);

		// go through the vector elements representing the objects in the array
		for(const auto& textMapEntry : textMapToStringify) {
			// create object and reserve memory for all [key, value] pairs
			rapidjson::Value objectValue;
			
			objectValue.SetObject();

			// create and add [key, value] pair for position
			rapidjson::Value keyPos;
			
			keyPos.SetString("p", 1, allocator);
			
			rapidjson::Value valuePos;
			
			valuePos.SetUint64(textMapEntry.pos);
			
			objectValue.AddMember(keyPos, valuePos, allocator);

			// create and add [key, value] pair for length
			rapidjson::Value keyLength;
			
			keyLength.SetString("l", 1, allocator);
			
			rapidjson::Value valueLength;
			
			valueLength.SetUint64(textMapEntry.length);
			
			objectValue.AddMember(keyLength, valueLength, allocator);

			// create and add [key, value] pair for describing string
			rapidjson::Value keyValue;

			keyValue.SetString("v", 1, allocator);

			rapidjson::Value valueValue;

			valueValue.SetString(textMapEntry.value, allocator);

			objectValue.AddMember(keyValue, valueValue, allocator);

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

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	// stringify JSON using jsoncons
	inline std::string stringify(const jsoncons::json& json) {
		std::string result;

		json.dump(result);

		return result;
	}

	// copy and clean JSON before parsing (remove control characters, correct escape sequences)
	// 	NOTE: in standard JSON, allowed escape sequence names are: " \ / b f n r t as well as u + 4 hex digits
	inline std::string cleanCopy(const std::string &json) {
		if(json.empty())
			return "";

		std::string result;

		for(std::string::size_type n = 0; n < json.length(); ++n) {
			// ignore control characters
			if(::iscntrl(json[n]))
				continue;

			// check escape sequences
			if(json[n] == '\\') {
				bool validEscape = false;

				if(n < json.length() - 1)
					switch(::tolower(json[n + 1])) {
					// check for escaped backslash
					case '\\':
						++n;					// do not check the following (escaped) backslash...

						result.push_back('\\');	// ...but add the ignored backslash to the result

						validEscape = true;

						break;

					// check for single-digit escape sequence names
					case 'b':
					case 'f':
					case 'n':
					case 'r':
					case 't':
					case '\"':
					case '\'':	// non-standard lenience
					case '/':
						validEscape = true;

						break;

					// check for Unicode character references
					case 'u':
						if(n < json.length() - 5)
							validEscape =
									::isxdigit(json[n + 2])
									&& ::isxdigit(json[n + 3])
									&& ::isxdigit(json[n + 4])
									&& ::isxdigit(json[n + 5]);

						break;

				}

				if(!validEscape)
					result.push_back('\\');	// simply escape the backslash of an invalid escape sequences

				result.push_back('\\');
			}
			else
				result.push_back(json[n]);
		}

		return result;
	}

	// parse JSON using RapidJSON, throws Json::Exception
	inline rapidjson::Document parseRapid(const std::string& json) {
		// clean input
		std::string cleanJson(cleanCopy(json));

		rapidjson::Document doc;

		doc.Parse(cleanJson);

		if(doc.HasParseError()) {
			std::string exceptionStr("Json::parseRapid(): ");

			exceptionStr += rapidjson::GetParseError_En(doc.GetParseError());
			exceptionStr += " at \'";

			if(doc.GetErrorOffset() > 25)
				exceptionStr += cleanJson.substr(doc.GetErrorOffset() - 25, 25);
			else if(doc.GetErrorOffset() > 0)
				exceptionStr += cleanJson.substr(0, doc.GetErrorOffset());

			exceptionStr += "[!]";

			if(cleanJson.size() > doc.GetErrorOffset() + 25)
				exceptionStr += cleanJson.substr(doc.GetErrorOffset(), 25);
			else if(cleanJson.size() > doc.GetErrorOffset())
				exceptionStr += cleanJson.substr(doc.GetErrorOffset());

			exceptionStr += "\'";

			throw Exception(exceptionStr);
		}

		return doc;
	}

	// parse JSON using jsoncons, throws Json::Exception
	inline jsoncons::json parseCons(const std::string& json) {
		// clean input
		std::string cleanJson(cleanCopy(json));

		try {
			jsoncons::json result = jsoncons::json::parse(cleanJson);

			return result;
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					"Json::parseCons(): "
					+ std::string(e.what())
			);
		}
	}

	// parse JSON using rapidjson and convert it into a text map, throws Json::Exception
	inline Struct::TextMap parseTextMapJson(const std::string& json) {
		Struct::TextMap result;

		if(json.empty())
			return result;

		// parse JSON
		rapidjson::Document document = parseRapid(json);

		if(!document.IsArray())
			throw Exception("Json::parseTextMapJson(): Invalid text map (is not an array)");

		for(const auto& element : document.GetArray()) {
			if(!element.IsObject())
				throw Exception(
						"Json::parseTextMapJson(): Invalid text map (an array element is not an object"
				);

			auto p = element.FindMember("p");
			auto l = element.FindMember("l");
			auto v = element.FindMember("v");

			if(p == element.MemberEnd() || !(p->value.IsUint64()))
				throw Exception(
						"Json::parseTextMapJson(): Invalid text map (could not find valid position)"
				);

			if(l == element.MemberEnd() || !(l->value.IsUint64()))
				throw Exception(
						"Json::parseTextMapJson(): Invalid text map (could not find valid length)"
				);

			if(v == element.MemberEnd() || !(v->value.IsString()))
				throw Exception(
						"Json::parseTextMapJson(): Invalid text map (could not find valid value)"
				);

			result.emplace_back(
					p->value.GetUint64(),
					l->value.GetUint64(),
					std::string(v->value.GetString(), v->value.GetStringLength())
			);
		}

		return result;
	}

} /* crawlservpp::Helper::Json */

#endif /* HELPER_JSON_HPP_ */
