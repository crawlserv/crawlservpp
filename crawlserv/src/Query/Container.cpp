/*
 * Container.cpp
 *
 * Abstract class for managing queries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#include "Container.hpp"

namespace crawlservpp::Query {

	// constructor stub
	Container::Container() {}

	// destructor stub
	Container::~Container() {}

	// add query to internal vectors and return index, throws Container::Exception
	Container::QueryStruct Container::addQuery(const QueryProperties& properties) {
		Container::QueryStruct newQuery;

		newQuery.resultBool = properties.resultBool;
		newQuery.resultSingle = properties.resultSingle;
		newQuery.resultMulti = properties.resultMulti;

		if(!properties.text.empty()) {
			if(properties.type == "regex") {
				newQuery.index = this->queriesRegEx.size();

				this->queriesRegEx.emplace_back(
						properties.text, properties.resultBool || properties.resultSingle, properties.resultMulti);

				newQuery.type = QueryStruct::typeRegEx;

			}
			else if(properties.type == "xpath") {
				newQuery.index = this->queriesXPath.size();

				this->queriesXPath.emplace_back(properties.text, properties.textOnly);

				newQuery.type = QueryStruct::typeXPath;
			}
			else if(properties.type == "jsonpointer") {
				newQuery.index = this->queriesJSONPointer.size();

				this->queriesJSONPointer.emplace_back(properties.text);

				newQuery.type = QueryStruct::typeJsonPointer;
			}
			else if(properties.type == "jsonpath") {
				newQuery.index = this->queriesJSONPath.size();

				this->queriesJSONPath.emplace_back(properties.text);

				newQuery.type = QueryStruct::typeJsonPath;
			}
			else throw Exception("Unknown query type \'" + properties.type + "\'");
		}

		return newQuery;
	}

	// get RegEx query by index
	const RegEx& Container::getRegExQuery(unsigned long index) const {
		return this->queriesRegEx.at(index);
	}

	// get XPath query by index
	const XPath& Container::getXPathQuery(unsigned long index) const {
		return this->queriesXPath.at(index);
	}

	// get JSONPointer query by index
	const JsonPointer& Container::getJsonPointerQuery(unsigned long index) const {
		return this->queriesJSONPointer.at(index);
	}

	// get JSONPath query by index
	const JsonPath& Container::getJsonPathQuery(unsigned long index) const {
		return this->queriesJSONPath.at(index);
	}

	// clear queries
	void Container::clearQueries() {
		// clear queries
		this->queriesXPath.clear();
		this->queriesRegEx.clear();
		this->queriesJSONPointer.clear();
		this->queriesJSONPath.clear();
	}

} /* crawlservpp::Query */
