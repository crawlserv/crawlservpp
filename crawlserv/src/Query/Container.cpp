/*
 * Container.cpp
 *
 * Abstract class for managing queries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#include "Container.h"

namespace crawlservpp::Query {

// constructor stub
Container::Container() {}

// destructor stub
Container::~Container() {}

// reserve memory for additional queries (optimized for speed, not memory)
void Container::reserveForQueries(unsigned long numOfAdditionalQueries) {
	this->queriesRegEx.reserve(this->queriesRegEx.size() + numOfAdditionalQueries);
	this->queriesXPath.reserve(this->queriesXPath.size() + numOfAdditionalQueries);
}

// add query to internal vectors and return index, throws std::runtime_error
Container::QueryStruct Container::addQuery(const crawlservpp::Struct::QueryProperties& properties) {
	Container::QueryStruct newQuery;
	newQuery.resultBool = properties.resultBool;
	newQuery.resultSingle = properties.resultSingle;
	newQuery.resultMulti = properties.resultMulti;

	if(!properties.text.empty()) {
		if(properties.type == "regex") {
			RegEx regex;
			regex.compile(properties.text, properties.resultBool || properties.resultSingle, properties.resultMulti);
			newQuery.index = this->queriesRegEx.size();
			newQuery.type = Container::QueryStruct::typeRegEx;
			this->queriesRegEx.push_back(std::move(regex));
		}
		else if(properties.type == "xpath") {
			XPath xpath;
			xpath.compile(properties.text, properties.textOnly);
			newQuery.index = this->queriesXPath.size();
			newQuery.type = Container::QueryStruct::typeXPath;
			this->queriesXPath.push_back(std::move(xpath));
		}
		else throw std::runtime_error("Unknown query type \'" + properties.type + "\'");
	}

	return newQuery;
}

// get const pointer to RegEx query by index
const RegEx& Container::getRegExQueryPtr(unsigned long index) const {
	return this->queriesRegEx.at(index);
}

// get const pointer to XPath query by index
const XPath& Container::getXPathQueryPtr(unsigned long index) const {
	return this->queriesXPath.at(index);
}

// clear queries
void Container::clearQueries() {
	// clear queries
	this->queriesXPath.clear();
	this->queriesRegEx.clear();
}

}
