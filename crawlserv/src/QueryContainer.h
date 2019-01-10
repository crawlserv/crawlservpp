/*
 * QueryContainer.h
 *
 * Abstract class for managing queries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#ifndef SRC_QUERYCONTAINER_H_
#define SRC_QUERYCONTAINER_H_

#include "RegEx.h"
#include "XPath.h"

#include <string>
#include <vector>

class QueryContainer {
public:
	QueryContainer();
	virtual ~QueryContainer();

protected:
	// structure used by child classes to identify queries
	struct Query {
		static const unsigned char typeNone = 0;
		static const unsigned char typeRegEx = 1;
		static const unsigned char typeXPath = 2;
		unsigned char type;
		unsigned long index;
		bool resultBool;
		bool resultSingle;
		bool resultMulti;

		// constructor: set default values
		Query() {
			this->type = QueryContainer::Query::typeNone;
			this->index = 0;
			this->resultBool = false;
			this->resultSingle = false;
			this->resultMulti = false;
		}
	};

	// query functions
	virtual void initQueries() = 0;
	QueryContainer::Query addQuery(const std::string& queryText, const std::string& queryType, bool queryResultBool,
			bool queryResultSingle, bool queryResultMulti, bool queryTextOnly);
	const RegEx * getRegExQueryPtr(unsigned long index) const;
	const XPath * getXPathQueryPtr(unsigned long index) const;
	void clearQueries();

private:
	// queries
	std::vector<RegEx*> queriesRegEx;
	std::vector<XPath*> queriesXPath;
};

#endif /* SRC_QUERYCONTAINER_H_ */
