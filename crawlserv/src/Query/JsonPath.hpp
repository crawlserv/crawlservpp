/*
 * JSONPath.hpp
 *
 * Using the jsoncons library to implement a JSONPath query with boolean, single and/or multiple results.
 *
 *  Created on: Apr 26, 2019
 *      Author: ans
 */

#ifndef QUERY_JSONPATH_HPP_
#define QUERY_JSONPATH_HPP_

#include "../Main/Exception.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"

#include <string>
#include <vector>

namespace crawlservpp::Query {

	class JsonPath {
	public:
		// constructor
		JsonPath(const std::string& pathString);

		// getters
		bool getBool(const jsoncons::json& json) const;
		void getFirst(const jsoncons::json& doc, std::string& resultTo) const;
		void getAll(const jsoncons::json& doc, std::vector<std::string>& resultTo) const;

		// sub-class for JSONPath exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	private:
		std::string jsonPath;
	};

} /* crawlservpp::Query */

#endif /* QUERY_JSONPATH_HPP_ */
