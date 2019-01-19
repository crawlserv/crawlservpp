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

// destructor: clear queries
Container::~Container() {
	this->clearQueries();
}

// reserve memory for queries
void Container::reserveQueries(unsigned long numQueries) {
	this->queriesRegEx.reserve(this->queriesRegEx.size() + numQueries);
	this->queriesXPath.reserve(this->queriesXPath.size() + numQueries);
}

// add query to internal vectors and return index
Container::QueryStruct Container::addQuery(const std::string& queryText, const std::string& queryType,
		bool queryResultBool, bool queryResultSingle, bool queryResultMulti, bool queryTextOnly) {
	Container::QueryStruct newQuery;
	newQuery.resultBool = queryResultBool;
	newQuery.resultSingle = queryResultSingle;
	newQuery.resultMulti = queryResultMulti;

	if(!queryText.empty()) {
		if(queryType == "regex") {
			RegEx * regex = new RegEx;
			regex->compile(queryText, queryResultBool || queryResultSingle, queryResultMulti);
			newQuery.index = this->queriesRegEx.size();
			newQuery.type = Container::QueryStruct::typeRegEx;
			this->queriesRegEx.push_back(regex);
		}
		else if(queryType == "xpath") {
			XPath * xpath = new XPath;
			xpath->compile(queryText, queryTextOnly);
			newQuery.index = this->queriesXPath.size();
			newQuery.type = Container::QueryStruct::typeXPath;
			this->queriesXPath.push_back(xpath);
		}
		else throw std::runtime_error("Unknown query type \'" + queryType + "\'");
	}

	return newQuery;
}

// get const pointer to RegEx query by index
const RegEx * Container::getRegExQueryPtr(unsigned long index) const {
	return this->queriesRegEx.at(index);
}

// get const pointer to XPath query by index
const XPath * Container::getXPathQueryPtr(unsigned long index) const {
	return this->queriesXPath.at(index);
}

// clear queries
void Container::clearQueries() {
	// clear XPath queries
	for(auto i = this->queriesXPath.begin(); i != this->queriesXPath.end(); ++i) {
		if(*i) {
			delete *i;
			*i = NULL;
		}
	}
	this->queriesXPath.clear();

	// clear RegEx queries
	for(auto i = this->queriesRegEx.begin(); i != this->queriesRegEx.end(); ++i) {
		if(*i) {
			delete *i;
			*i = NULL;
		}
	}
	this->queriesRegEx.clear();
}

}
