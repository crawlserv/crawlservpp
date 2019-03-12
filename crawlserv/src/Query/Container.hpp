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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../Struct/QueryProperties.hpp"
#include "RegEx.hpp"
#include "XPath.hpp"

namespace crawlservpp::Query {

	// for convenience
	typedef Struct::QueryProperties QueryProperties;

	class Container {
	public:
		Container();
		virtual ~Container();

	protected:
		// structure used by child classes to identify queries
		struct QueryStruct {
			static const unsigned char typeNone = 0;
			static const unsigned char typeRegEx = 1;
			static const unsigned char typeXPath = 2;
			unsigned char type;
			unsigned long index;
			bool resultBool;
			bool resultSingle;
			bool resultMulti;

			// constructor: set default values
			QueryStruct() {
				this->type = QueryStruct::typeNone;
				this->index = 0;
				this->resultBool = false;
				this->resultSingle = false;
				this->resultMulti = false;
			}
		};

		// query functions
		virtual void initQueries() = 0;
		void reserveForQueries(unsigned long numOfAdditionalQueries);
		QueryStruct addQuery(const QueryProperties& properties);
		const RegEx& getRegExQueryPtr(unsigned long index) const;
		const XPath& getXPathQueryPtr(unsigned long index) const;
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
	};

} /* crawlservpp::Query */

#endif /* QUERY_CONTAINER_HPP_ */
