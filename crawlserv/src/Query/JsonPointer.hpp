/*
 * JSONPointer.hpp
 *
 * Using the rapidjson library to implement a JSONPointer query with boolean, single and/or multiple results.
 *
 *  NOTE:	Different from the standard, multiple results are supported when using $$ as a placeholder for 0..n,
 *  		where n is the number of matches minus 1.
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

#include <string>	// std::string, std::to_string
#include <vector>	// std::vector

namespace crawlservpp::Query {

	class JsonPointer {
	public:
		// constructor and destructor
		JsonPointer(const std::string& pointerString);
		virtual ~JsonPointer();

		// getters
		bool getBool(const rapidjson::Document& doc) const;
		void getFirst(const rapidjson::Document& doc, std::string& resultTo) const;
		void getAll(const rapidjson::Document& doc, std::vector<std::string>& resultTo) const;
		void getSubSets(const rapidjson::Document& doc, std::vector<rapidjson::Document>& resultTo) const;

		// sub-class for JSONPointer exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// only moveable (using default), not copyable
		JsonPointer(JsonPointer&) = delete;
		JsonPointer(JsonPointer&&) = default;
		JsonPointer& operator=(JsonPointer) = delete;
		JsonPointer& operator=(JsonPointer&&) = default;

	private:
		rapidjson::Pointer pointerFirst;
		std::string pointerStringMulti;
	};

} /* crawlservpp::Query */

#endif /* QUERY_JSONPOINTER_HPP_ */
