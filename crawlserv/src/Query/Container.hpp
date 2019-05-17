/*
 * Container.hpp
 *
 * Abstract class for managing queries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#ifndef QUERY_CONTAINER_HPP_
#define QUERY_CONTAINER_HPP_

#include "JsonPath.hpp"
#include "JsonPointer.hpp"
#include "RegEx.hpp"
#include "XPath.hpp"

#include "../Main/Exception.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/QueryStruct.hpp"

#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Query {

	// for convenience
	typedef Struct::QueryProperties QueryProperties;
	typedef Struct::QueryStruct QueryStruct;

	class Container {
	public:
		Container();
		virtual ~Container();

		// sub-class for query container exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	protected:
		// query functions
		virtual void initQueries() = 0;
		QueryStruct addQuery(const QueryProperties& properties);
		const RegEx& getRegExQuery(unsigned long index) const;
		const XPath& getXPathQuery(unsigned long index) const;
		const JsonPointer& getJsonPointerQuery(unsigned long index) const;
		const JsonPath& getJsonPathQuery(unsigned long index) const;
		void clearQueries();

		// not moveable, not copyable
		Container(Container&) = delete;
		Container(Container&&) = delete;
		Container& operator=(Container&) = delete;
		Container& operator=(Container&&) = delete;

	private:
		// queries
		std::vector<RegEx> queriesRegEx;
		std::vector<XPath> queriesXPath;
		std::vector<JsonPointer> queriesJSONPointer;
		std::vector<JsonPath> queriesJSONPath;
	};

} /* crawlservpp::Query */

#endif /* QUERY_CONTAINER_HPP_ */
