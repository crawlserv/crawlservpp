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
Container::QueryStruct Container::addQuery(const std::string& queryText, const std::string& queryType,
		bool queryResultBool, bool queryResultSingle, bool queryResultMulti, bool queryTextOnly) {
	Container::QueryStruct newQuery;
	newQuery.resultBool = queryResultBool;
	newQuery.resultSingle = queryResultSingle;
	newQuery.resultMulti = queryResultMulti;

	if(!queryText.empty()) {
		if(queryType == "regex") {
			std::unique_ptr<RegEx> regex = std::make_unique<RegEx>();
			regex->compile(queryText, queryResultBool || queryResultSingle, queryResultMulti);
			newQuery.index = this->queriesRegEx.size();
			newQuery.type = Container::QueryStruct::typeRegEx;
			this->queriesRegEx.push_back(std::move(regex));
		}
		else if(queryType == "xpath") {
			std::unique_ptr<XPath> xpath = std::make_unique<XPath>();
			xpath->compile(queryText, queryTextOnly);
			newQuery.index = this->queriesXPath.size();
			newQuery.type = Container::QueryStruct::typeXPath;
			this->queriesXPath.push_back(std::move(xpath));
		}
		else throw std::runtime_error("Unknown query type \'" + queryType + "\'");
	}

	return newQuery;
}

// get const pointer to RegEx query by index
const RegEx& Container::getRegExQueryPtr(unsigned long index) const {
	return *(this->queriesRegEx.at(index));
}

// get const pointer to XPath query by index
const XPath& Container::getXPathQueryPtr(unsigned long index) const {
	return *(this->queriesXPath.at(index));
}

// clear queries
void Container::clearQueries() {
	// clear queries
	this->queriesXPath.clear();
	this->queriesRegEx.clear();
}

}
