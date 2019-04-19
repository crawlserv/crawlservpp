/*
 * JSONPointer.hpp
 *
 * Using the rapidjson library to implement a JSON Pointer query with boolean, single and/or multiple results.
 *
 *  Created on: Apr 19, 2019
 *      Author: ans
 */

#ifndef QUERY_JSONPOINTER_HPP_
#define QUERY_JSONPOINTER_HPP_

#include "../Helper/Json.hpp"
#include "../Main/Exception.hpp"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../_extern/rapidjson/include/rapidjson/document.h"
#include "../_extern/rapidjson/include/rapidjson/pointer.h"

#include <string>

namespace crawlservpp::Query {

	class JSONPointer {
	public:
		// constructor
		JSONPointer(const std::string& pointer);

		// getters
		bool getBool(const rapidjson::Document& doc) const;
		void getFirst(const rapidjson::Document& doc, std::string& resultTo) const;

		// sub-class for XPath exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	private:
		const rapidjson::Pointer pointer;
	};

} /* crawlservpp::Query */

#endif /* QUERY_JSONPOINTER_HPP_ */
