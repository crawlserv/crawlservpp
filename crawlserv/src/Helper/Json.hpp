/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * Namespace for global JSON helper functions.
 *
 *  Created on: Jan 9, 2019
 *      Author: ans
 */

#ifndef HELPER_JSON_HPP_
#define HELPER_JSON_HPP_

#include "../Main/Exception.hpp"
#include "../Struct/TextMap.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"

#include <cstddef>	// std::size_t

namespace rapidjson { typedef ::std::size_t SizeType; }

#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/error/en.h"
#include "../_extern/rapidjson/include/rapidjson/error/error.h"
#include "../_extern/rapidjson/include/rapidjson/stringbuffer.h"
#include "../_extern/rapidjson/include/rapidjson/writer.h"

#include <cctype>	// std::iscntrl, std::isxdigit, std::tolower
#include <string>	// std::string
#include <utility>	// std::pair
#include <vector>	// std::vector

//! Namespace for global JSON helper functions.
namespace crawlservpp::Helper::Json {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The length of an escaped Unicode character in JSON code (including the '\\u').
	inline constexpr auto unicodeEscapeLength{6};

	//! The offset of the first Unicode character digit in JSON code (from the '\\').
	inline constexpr auto unicodeEscapeDigit1{2};

	//! The offset of the second Unicode character digit in JSON code (from the '\\').
	inline constexpr auto unicodeEscapeDigit2{3};

	//! The offset of the third Unicode character digit in JSON code (from the '\\').
	inline constexpr auto unicodeEscapeDigit3{4};

	//! The offset of the fourth Unicode character digit in JSON code (from the '\\').
	inline constexpr auto unicodeEscapeDigit4{5};

	//! The number of characters to show before and behind a JSON error.
	inline constexpr auto numDebugChars{25};

	///@}

	/*
	 * DECLARATION
	 */

	///@name Stringification
	///@{

	[[nodiscard]] std::string stringify(const std::vector<std::string>& vectorToStringify);
	[[nodiscard]] std::string stringify(const std::string& stringToStringify);
	[[nodiscard]] std::string stringify(const char * stringToStringify);
	[[nodiscard]] std::string stringify(
			const std::vector<std::vector<std::pair<std::string, std::string>>>& vectorToStringify
	);
	[[nodiscard]] std::string stringify(const Struct::TextMap& textMapToStringify);
	[[nodiscard]] std::string stringify(const rapidjson::Value& value);
	[[nodiscard]] std::string stringify(const jsoncons::json& json);

	///@}
	///@name Parsing
	///@{

	[[nodiscard]] std::string cleanCopy(std::string_view json);
	[[nodiscard]] rapidjson::Document parseRapid(std::string_view json);
	[[nodiscard]] jsoncons::json parseCons(std::string_view json);
	[[nodiscard]] Struct::TextMap parseTextMapJson(std::string_view json);
	[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> parsePosLenPairsJson(
			std::string_view json
	);

	///@}

	/*
	 * CLASS FOR JSON EXCEPTIONS
	 */

	//! Class for JSON exceptions.
	/*!
	 * This exception is being thrown when
	 * - an error occurs while parsing JSON
	 * - an error occurs while parsing a JSON
	 *    text map
	 * - an error occurs while parsing a JSON
	 *    array of @c [pos;length] pairs
	 */
	MAIN_EXCEPTION_CLASS();

	/*
	 * IMPLEMENTATION
	 */

	//! Stringifies a vector of strings into one string containing a JSON array.
	/*!
	 * Uses @c RapidJSON for conversion into JSON.
	 *  For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \param vectorToStringify A const reference
	 *   to the vector of strings to be combined
	 *   and converted into valid JSON code.
	 *
	 * \returns The copy of a string containing
	 *   valid JSON code representing an array
	 *   contining all strings in the given vector.
	 */
	inline std::string stringify(const std::vector<std::string>& vectorToStringify) {
		// create document as array and get reference to allocator
		rapidjson::Document document;
		
		document.SetArray();
		
		rapidjson::Document::AllocatorType& allocator{document.GetAllocator()};

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

	//! Converts a string into a JSON array with the string as the only element inside it.
	/*!
	 * Uses @c RapidJSON for conversion into JSON.
	 *  For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \note A string view cannot be used, because
	 *   the underlying API requires the constant
	 *   reference to a string.
	 *
	 * \param stringToStringify A const reference
	 *   to the string to be converted into a JSON
	 *   array.
	 *
	 * \returns The copy of a string containing
	 *   valid JSON code representing an array
	 *   containing the given string as its only
	 *   element.
	 */
	inline std::string stringify(const std::string& stringToStringify) {
		// create document as array and get reference to allocator
		rapidjson::Document document;
		
		document.SetArray();
		
		rapidjson::Document::AllocatorType& allocator{document.GetAllocator()};

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

	//! Converts a string into a JSON array with the string as the only element inside it.
	/*!
	 * Uses @c RapidJSON for conversion into JSON.
	 *  For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \note A string view cannot be used, because
	 *   the underlying API requires the constant
	 *   reference to a string.
	 *
	 * \param stringToStringify A const pointer to
	 *   a null-terminated string to be converted
	 *   into a JSON array.
	 *
	 * \returns The copy of a string containing
	 *   valid JSON code representing an array
	 *   containing the given string as its only
	 *   element.
	 */
	inline std::string stringify(const char * stringToStringify) {
		return stringify(std::string(stringToStringify));
	}

	//! Converts a vector of vectors of string pairs into a JSON array with corresponding objects containing [key, value] pairs
	/*!
	 * Uses @c RapidJSON for conversion into JSON.
	 *  For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \param vectorToStringify A const reference
	 *   to the vector containing vectors of string
	 *   pairs, each of which represents a
	 *   [key, value] pair.
	 *
	 * \returns The copy of a string containing
	 *   valid JSON code representing an array of
	 *   objects containing the given [key, value]
	 *   pairs.
	 */
	inline std::string stringify(const std::vector<std::vector<std::pair<std::string, std::string>>>& vectorToStringify) {
		// create document as array and get reference to allocator
		rapidjson::Document document;
		
		document.SetArray();
		
		rapidjson::Document::AllocatorType& allocator{document.GetAllocator()};

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

	//! Converts a text map into a JSON array with corresponding objects containing [key, value] pairs
	/*!
	 * Uses @c RapidJSON for conversion into JSON.
	 *  For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \param textMapToStringify A const reference
	 *   to the text map to be represented as
	 *   [key, value] pairs.
	 *
	 * \returns The copy of a string containing
	 *   valid JSON code representing an array of
	 *   objects containing the [key, value] pairs
	 *   of the text map.
	 *
	 * \sa Struct::TextMap, Module::Analyzer::Database::createCorpus
	 */
	inline std::string stringify(const Struct::TextMap& textMapToStringify) {
		// create document as array and get refference to allocator
		rapidjson::Document document;
		
		document.SetArray();
		
		rapidjson::Document::AllocatorType& allocator{document.GetAllocator()};

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

	//! Stringifies a JSON value using the @c RapidJSON.
	/*!
	 * For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \param value The RapidJSON value to be
	 *  stringified.
	 *
	 * \returns A copy of the string containing
	 *   the representation of the given JSON
	 *   value.
	 */
	inline std::string stringify(const rapidjson::Value& value) {
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

		value.Accept(writer);

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	//! Stringifies a JSON value using @c jsoncons.
	/*!
	 * For more information about @c jsoncons,
	 *  see its
	 *  <a href="https://github.com/danielaparker/jsoncons">
	 *  GitHub repository</a>.
	 *
	 * \param json The @c jsoncons value to be
	 *   stringified.
	 *
	 * \returns A copy of the string containing the
	 *   representation of the given JSON value.
	 */
	inline std::string stringify(const jsoncons::json& json) {
		std::string result;

		json.dump(result);

		return result;
	}

	//! Copies and cleans the given JSON code to prepare it for parsing.
	/*!
	 * Removes control characters and corrects
	 *  escape sequences in the given JSON code.
	 *
	 * If the given JSON code is empty, an empty
	 *  string will be returned.
	 *
	 * \note In standard JSON, allowed escape
	 *  sequence names are:
	 *  @c " @c \ @c / @c b @c f @c n @c r @c t,
	 *   as well as @c u + 4 hex digits.
	 *
	 * \param json A string view containing the
	 *  JSON code to be copied and cleaned.
	 *
	 * \returns A copy of the string containing
	 *   the cleaned JSON code.
	 */
	inline std::string cleanCopy(std::string_view json) {
		if(json.empty()) {
			return std::string();
		}

		std::string result;

		for(std::size_t n{}; n < json.length(); ++n) {
			// ignore control characters
			if(std::iscntrl(json[n]) != 0) {
				continue;
			}

			// check escape sequences
			if(json[n] == '\\') {
				bool validEscapeSequence{false};

				if(n < json.length() - 1) {
					switch(std::tolower(json[n + 1])) {
					// check for escaped backslash
					case '\\':
						// do not check the following (escaped) backslash...
						++n;

						// ...but add the ignored backslash to the result
						result.push_back('\\');

						validEscapeSequence = true;

						break;

					// check for single-digit escape sequence names
					case 'b':
					case 'f':
					case 'n':
					case 'r':
					case 't':
					case '\"':
					case '\'': /* non-standard lenience */
					case '/':
						validEscapeSequence = true;

						break;

					// check for escaped Unicode character sequence
					case 'u':
						if(n < json.length() - unicodeEscapeLength) {
							validEscapeSequence =
									::isxdigit(json[n + unicodeEscapeDigit1]) != 0
									&& ::isxdigit(json[n + unicodeEscapeDigit2]) != 0
									&& ::isxdigit(json[n + unicodeEscapeDigit3]) != 0
									&& ::isxdigit(json[n + unicodeEscapeDigit4]) != 0;
						}

						break;

					default:
						break;
					}
				}

				if(!validEscapeSequence) {
					// simply escape the backslash of an invalid escape sequence
					result.push_back('\\');
				}

				result.push_back('\\');
			}
			else {
				result.push_back(json[n]);
			}
		}

		return result;
	}

	//! Parses JSON code using @c RapidJSON.
	/*!
	 * For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \param json A string view containing the
	 *   JSON code to parse.
	 *
	 * \returns A @c RapidJSON document containing
	 *   the parsed JSON.
	 *
	 * \throws Json::Exception if an error occurs
	 *   while parsing the given JSON code.
	 */
	inline rapidjson::Document parseRapid(std::string_view json) {
		// clean input
		std::string cleanJson(cleanCopy(json));

		rapidjson::Document doc;

		doc.Parse(cleanJson);

		if(doc.HasParseError()) {
			std::string exceptionStr{
				"Json::parseRapid(): "
			};

			exceptionStr += rapidjson::GetParseError_En(doc.GetParseError());
			exceptionStr += " at '";

			if(doc.GetErrorOffset() > numDebugChars) {
				exceptionStr += cleanJson.substr(doc.GetErrorOffset() - numDebugChars, numDebugChars);
			}
			else if(doc.GetErrorOffset() > 0) {
				exceptionStr += cleanJson.substr(0, doc.GetErrorOffset());
			}

			exceptionStr += "[!]";

			if(cleanJson.size() > doc.GetErrorOffset() + numDebugChars) {
				exceptionStr += cleanJson.substr(doc.GetErrorOffset(), numDebugChars);
			}
			else if(cleanJson.size() > doc.GetErrorOffset()) {
				exceptionStr += cleanJson.substr(doc.GetErrorOffset());
			}

			exceptionStr += "'";

			throw Exception(exceptionStr);
		}

		return doc;
	}

	//! Parses JSON code using @c jsoncons.
	/*!
	 * For more information about @c jsoncons,
	 *  see its
	 *  <a href="https://github.com/danielaparker/jsoncons">
	 *  GitHub repository</a>.
	 *
	 * \param json A string view containing the
	 *   JSON code to parse.
	 *
	 * \returns A jsoncons object containing the
	 *   parsed JSON.
	 *
	 * \throws Json::Exception if an error occurs
	 *   while parsing the given JSON code.
	 */
	inline jsoncons::json parseCons(std::string_view json) {
		// clean input
		std::string cleanJson(cleanCopy(json));

		try {
			jsoncons::json result{jsoncons::json::parse(cleanJson)};

			return result;
		}
		catch(const jsoncons::json_exception& e) {
			throw Exception(
					"Json::parseCons(): "
					+ std::string(e.what())
			);
		}
	}

	//! Parses JSON code using @c RapidJSON and converts it into a text map.
	/*!
	 * If the given JSON code is empty, an empty
	 *  text map will be returned.
	 *
	 * For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \param json A string view containing the
	 *   JSON code to be parsed and converted
	 *   into a text map.
	 *
	 * \returns The text map parsed from the
	 *   given JSON code.
	 *
	 * \throws Json::Exception if the given string
	 *   view does not contain valid JSON code or
	 *   the contained JSON code does not describe
	 *   a valid text map.
	 *
	 * \sa parseRapid, Struct::TextMap
	 */
	inline Struct::TextMap parseTextMapJson(std::string_view json) {
		if(json.empty()) {
			return {};
		}

		// parse JSON
		rapidjson::Document document{parseRapid(json)};

		if(!document.IsArray()) {
			throw Exception(
					"Json::parseTextMapJson():"
					" Invalid text map"
					" (is not an array)"
			);
		}

		Struct::TextMap result;

		for(const auto& element : document.GetArray()) {
			if(!element.IsObject()) {
				throw Exception(
						"Json::parseTextMapJson():"
						" Invalid text map"
						" (an array element is not an object"
				);
			}

			const auto p{element.FindMember("p")};
			const auto l{element.FindMember("l")};
			const auto v{element.FindMember("v")};

			if(p == element.MemberEnd() || !(p->value.IsUint64())) {
				throw Exception(
						"Json::parseTextMapJson():"
						" Invalid text map"
						" (could not find valid position)"
				);
			}

			if(l == element.MemberEnd() || !(l->value.IsUint64())) {
				throw Exception(
						"Json::parseTextMapJson():"
						" Invalid text map"
						" (could not find valid length)"
				);
			}

			if(v == element.MemberEnd() || !(v->value.IsString())) {
				throw Exception(
						"Json::parseTextMapJson():"
						" Invalid text map"
						" (could not find valid value)"
				);
			}

			result.emplace_back(
					p->value.GetUint64(),
					l->value.GetUint64(),
					std::string(
							v->value.GetString(),
							v->value.GetStringLength()
					)
			);
		}

		return result;
	}

	//! Parses JSON code using @c RapidJSON and converts it into @c [pos;length] pairs.
	/*!
	 * If the given JSON code is empty, an
	 *  empty array will be returned.
	 *
	 * For more information about @c RapidJSON,
	 *  see its
	 *  <a href="https://github.com/Tencent/rapidjson">
	 *  GitHub repository</a>.
	 *
	 * \param json A string view containing the
	 *   JSON code to be parsed and converted
	 *   into pairs of numbers.
	 *
	 * \returns The pairs parsed from the
	 *   given JSON code.
	 *
	 * \throws Json::Exception if the given
	 *   string view does not contain valid JSON
	 *   code or the contained JSON code does
	 *   not describe a valid array of @c
	 *   [pos;length] pairs.
	 *
	 * \sa parseRapid
	 */
	inline std::vector<std::pair<std::size_t, std::size_t>> parsePosLenPairsJson(
			std::string_view json
	) {
		if(json.empty()) {
			return {};
		}

		// parse JSON
		rapidjson::Document document{parseRapid(json)};

		if(!document.IsArray()) {
			throw Exception(
					"Json::parsePosLenPairsJson():"
					" Invalid array of [pos;length] pairs"
					" (is not an array)"
			);
		}

		std::vector<std::pair<std::size_t, std::size_t>> result;

		for(const auto& element : document.GetArray()) {
			if(!element.IsArray()) {
				throw Exception(
						"Json::parsePosLenPairsJson():"
						" Invalid array of [pos;length] pairs"
						" (an array element is not an array)"
				);
			}

			if(element.Size() != 2) {
				throw Exception(
						"Json::parsePosLenPairsJson():"
						" Invalid array of [pos;length] pairs"
						" (a pair is not of size 2)"
				);
			}

			const auto a{element.GetArray()};

			if(!(a[0].IsUint64())) {
				throw Exception(
						"Json::parsePosLenPairsJson():"
						" Invalid array of [pos;length] pairs"
						" (could not find valid position)"
				);
			}

			if(!(a[1].IsUint64())) {
				throw Exception(
						"Json::parsePosLenPairsJson():"
						" Invalid array of [pos;length] pairs"
						" (could not find valid length)"
				);
			}

			result.emplace_back(
					a[0].GetUint64(),
					a[1].GetUint64()
			);
		}

		return result;
	}

} /* namespace crawlservpp::Helper::Json */

#endif /* HELPER_JSON_HPP_ */
