/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Container.cpp
 *
 * Abstract class for management and type-independent usage of queries.
 *
 *  Created on: Jan 8, 2019
 *      Author: ans
 */

#include "Container.hpp"

namespace crawlservpp::Query {

	// constructor stub
	Container::Container()
			: repairCData(true),
			  queryTargetPtr(nullptr),
			  queryTargetSourcePtr(nullptr),
			  xmlParsed(false),
			  jsonParsedRapid(false),
			  jsonParsedCons(false),
			  subSetType(QueryStruct::typeNone),
			  subSetNumber(0),
			  subSetCurrent(0),
			  subSetXmlParsed(false),
			  subSetJsonParsedRapid(false),
			  subSetJsonParsedCons(false) {}

	// destructor stub
	Container::~Container() {}

	// set whether to try to repair CData when parsing XML
	void Container::setRepairCData(bool isRepairCData) {
		this->repairCData = isRepairCData;
	}

	// set options for tidy-html reporting errors and warnings
	void Container::setTidyErrorsAndWarnings(unsigned int errors, bool warnings) {
		this->parsedXML.setOptions(warnings, errors);
		this->subSetParsedXML.setOptions(warnings, errors);
	}

	// set content to use queries on
	void Container::setQueryTarget(const std::string& content, const std::string& source) {
		// set new target
		this->queryTargetPtr = &content;
		this->queryTargetSourcePtr = &source;

		// reset parsing state
		this->resetParsingState();

		// clear subsets
		switch(this->subSetType) {
		case QueryStruct::typeXPath:
			this->xPathSubSets.clear();

			break;

		case QueryStruct::typeJsonPointer:
			this->jsonPointerSubSets.clear();

			break;

		case QueryStruct::typeJsonPath:
			this->jsonPathSubSets.clear();

			break;
		}

		this->subSetType = QueryStruct::typeNone;
		this->subSetNumber = 0;
		this->subSetCurrent = 0;

		this->stringifiedSubSets.clear();

		// reset parsing state for subsets
		this->resetSubSetParsingState();
	}

	// get number of subsets
	unsigned long Container::getNumberOfSubSets() const {
		return this->subSetNumber;
	}

	// add query to internal vectors and return index, throws Container::Exception
	Struct::QueryStruct Container::addQuery(const QueryProperties& properties) {
		QueryStruct newQuery;

		newQuery.resultBool = properties.resultBool;
		newQuery.resultSingle = properties.resultSingle;
		newQuery.resultMulti = properties.resultMulti;

		if(!properties.text.empty()) {
			if(properties.type == "regex") {
				// add RegEx query
				newQuery.index = this->queriesRegEx.size();

				try {
					this->queriesRegEx.emplace_back(
							properties.text, properties.resultBool || properties.resultSingle,
							properties.resultMulti
					);
				}
				catch(const RegExException& e) {
					throw Exception("[RegEx] " + e.whatStr());
				}

				newQuery.type = QueryStruct::typeRegEx;

			}
			else if(properties.type == "xpath") {
				// add XPath query
				newQuery.index = this->queriesXPath.size();

				try {
					this->queriesXPath.emplace_back(properties.text, properties.textOnly);
				}
				catch(const XPathException& e) {
					throw Exception("[XPath] " + e.whatStr());
				}

				newQuery.type = QueryStruct::typeXPath;
			}
			else if(properties.type == "jsonpointer") {
				// add JSONPointer query
				newQuery.index = this->queriesJsonPointer.size();

				try {
					this->queriesJsonPointer.emplace_back(properties.text);
				}
				catch(const JsonPointerException &e) {
					throw Exception("[JSONPointer] " + e.whatStr());
				}

				newQuery.type = QueryStruct::typeJsonPointer;
			}
			else if(properties.type == "jsonpath") {
				// add JSONPath query
				newQuery.index = this->queriesJsonPath.size();

				try {
					this->queriesJsonPath.emplace_back(properties.text);
				}
				catch(const JsonPathException &e) {
					throw Exception("[JSONPath] " + e.whatStr());
				}

				newQuery.type = QueryStruct::typeJsonPath;
			}
			else throw Exception(
					"Query::Container::addQuery(): Unknown query type \'"
					+ properties.type
					+ "\'"
			);
		}

		return newQuery;
	}

	// clear queries
	void Container::clearQueries() {
		// clear queries
		this->queriesXPath.clear();
		this->queriesRegEx.clear();
		this->queriesJsonPointer.clear();
		this->queriesJsonPath.clear();
	}

	// use next subset for queries, return false if no more subsets exist, throws Container::Exception
	bool Container::nextSubSet() {
		// check subsets
		if(this->subSetNumber < this->subSetCurrent)
			throw Exception("Query::Container::nextSubSet(): Invalid subset selected");

		if(this->subSetNumber == this->subSetCurrent)
			return false;

		// increment index (+ 1) of current subset
		++(this->subSetCurrent);

		return true;
	}

	// get a boolean result from a RegEx query on a separate string,
	//	return false if query is of different type or failed
	bool Container::getBoolFromRegEx(
			const QueryStruct& query,
			const std::string& target,
			bool& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check query type
		if(query.type != QueryStruct::typeRegEx) {
			if(query.type != QueryStruct::typeNone)
				warningsTo.emplace(
					"WARNING: RegEx query is of invalid type - not RegEx."
				);
		}
		// check result type
		else if(query.type != QueryStruct::typeNone && !query.resultBool)
			warningsTo.emplace(
				"WARNING: RegEx query has invalid result type - not boolean."
			);
		// check query target
		else if(target.empty()) {
			resultTo = false;

			return true;
		}
		else {
			// get boolean result from a RegEx query
			try {
				resultTo = this->queriesRegEx.at(query.index).getBool(target);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ target
						+ "]."
				);
			}
		}

		return false;
	}

	// get a single result from a RegEx query on a separate string,
	//	return false if query is of different type or failed
	bool Container::getSingleFromRegEx(
			const QueryStruct& query,
			const std::string& target,
			std::string& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check query type
		if(query.type != QueryStruct::typeRegEx) {
			if(query.type != QueryStruct::typeNone)
				warningsTo.emplace(
					"WARNING: RegEx query is of invalid type - not RegEx."
				);
		}
		// check result type
		else if(query.type != QueryStruct::typeNone && !query.resultSingle)
			warningsTo.emplace(
				"WARNING: RegEx query has invalid result type - not single."
			);
		// check query target
		else if(target.empty()) {
			resultTo = false;

			return true;
		}
		else {
			// get boolean result from a RegEx query
			try {
				this->queriesRegEx.at(query.index).getFirst(target, resultTo);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ target
						+ "]."
				);
			}
		}

		return false;
	}

	// get multiple results from a RegEx query on a separate string,
	//	return false if query is of different type or failed
	bool Container::getMultiFromRegEx(
			const QueryStruct& query,
			const std::string& target,
			std::vector<std::string>& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check query type
		if(query.type != QueryStruct::typeRegEx) {
			if(query.type != QueryStruct::typeNone)
				warningsTo.emplace(
					"WARNING: RegEx query is of invalid type - not RegEx."
				);
		}
		// check result type
		else if(query.type != QueryStruct::typeNone && !query.resultMulti)
			warningsTo.emplace(
				"WARNING: RegEx query has invalid result type - not multi."
			);
		// check query target
		else if(target.empty()) {
			resultTo.clear();

			return true;
		}
		else {
			// get boolean result from a RegEx query
			try {
				this->queriesRegEx.at(query.index).getAll(target, resultTo);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ target
						+ "]."
				);
			}
		}

		return false;
	}

	// get a boolean result from a query of any type,
	//  return false if query failed, throws Container::Exception
	bool Container::getBoolFromQuery(
			const QueryStruct& query,
			bool& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointers
		if(!(this->queryTargetPtr))
			throw Exception("Query::Container::getBoolFromQuery(): No content specified");

		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::getBoolFromQuery(): No content source specified");

		// check result type
		if(query.type != QueryStruct::typeNone && !query.resultBool) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not boolean."
			);

			return false;
		}

		// check query target
		if(this->queryTargetPtr->empty()) {
			resultTo = false;

			return true;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get boolean result from a RegEx query
			try {
				resultTo = this->queriesRegEx.at(query.index).getBool(*(this->queryTargetPtr));

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]."
				);
			}

			break;

		case QueryStruct::typeXPath:
			// parse content as XML/HTML if still necessary
			if(this->parseXml(warningsTo)) {
				// get boolean result from a XPath query
				try {
					resultTo = this->queriesXPath.at(query.index).getBool(this->parsedXML);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPointer:
			// parse content as JSON using RapidJSON if still necessary
			if(this->parseJsonRapid(warningsTo)) {
				// get boolean result from a JSONPointer query
				try {
					resultTo = this->queriesJsonPointer.at(query.index).getBool(this->parsedJsonRapid);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPath:
			// parse content as JSON using jsoncons if still necessary
			if(this->parseJsonRapid(warningsTo)) {
				// get boolean result from a JSONPath query
				try {
					resultTo = this->queriesJsonPath.at(query.index).getBool(this->parsedJsonCons);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeNone:
			break;

		default:
			throw Exception("Query::Container::getBoolFromQuery(): Unknown query type");
		}

		return false;
	}

	// get boolean result from a query of any type on the current subset,
	//  return false if query failed, throws Container::Exception
	bool Container::getBoolFromQueryOnSubSet(
			const QueryStruct& query,
			bool& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointer
		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::getBoolFromQueryOnSubSet(): No content source specified");

		// check current subset
		if(!(this->subSetCurrent))
			throw Exception("Query::Container::getBoolFromQueryOnSubSet(): No subset specified");

		if(this->subSetCurrent > this->subSetNumber)
			throw Exception("Query::Container::getBoolFromQueryOnSubSet(): Invalid subset specified");

		// check result type
		if(query.type != QueryStruct::typeNone && !query.resultBool) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not boolean."
			);

			return false;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get boolean result from a RegEx query on the current subset
			try {
				if(this->subSetType != QueryStruct::typeRegEx)
					this->stringifySubSets(warningsTo);

				resultTo = this->queriesRegEx.at(query.index).getBool(
						this->stringifiedSubSets.at(this->subSetCurrent - 1)
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]."
				);
			}

			break;

		case QueryStruct::typeXPath:
			// parse current subset as XML/HTML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get boolean result from a XPath query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeXPath)
						resultTo = this->queriesXPath.at(query.index).getBool(
								this->xPathSubSets.at(this->subSetCurrent - 1)
						);
					else
						resultTo = this->queriesXPath.at(query.index).getBool(
								this->subSetParsedXML
						);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPointer:
			// parse current subset as JSON using RapidJSON if still necessary
			if(this->parseSubSetJsonRapid(warningsTo)) {
				// get boolean result from a JSONPointer query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeJsonPointer)
						resultTo = this->queriesJsonPointer.at(query.index).getBool(
								this->jsonPointerSubSets.at(this->subSetCurrent - 1)
						);
					else
						resultTo = this->queriesJsonPointer.at(query.index).getBool(
								this->subSetParsedJsonRapid
						);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPath:
			// parse current subset as JSON using jsoncons if still necessary
			if(this->parseSubSetJsonRapid(warningsTo)) {
				// get boolean result from a JSONPath query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeJsonPath)
						resultTo = this->queriesJsonPath.at(query.index).getBool(
								this->jsonPathSubSets.at(this->subSetCurrent - 1)
						);
					else
						resultTo = this->queriesJsonPath.at(query.index).getBool(
								this->subSetParsedJsonCons
						);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeNone:
			break;

		default:
			throw Exception("Query::Container::getBoolFromQueryOnSubSet(): Unknown query type");
		}

		return false;
	}

	// get a single result from a query of any type,
	//  return false if query failed, throws Container::Exception
	bool Container::getSingleFromQuery(
			const QueryStruct& query,
			std::string& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointers
		if(!(this->queryTargetPtr))
			throw Exception("Query::Container::getSingleFromQuery(): No content specified");

		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::getSingleFromQuery(): No content source specified");

		// check result type
		if(query.type != QueryStruct::typeNone && !query.resultSingle) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not single."
			);

			return false;
		}

		// check query target
		if(this->queryTargetPtr->empty()) {
			resultTo = "";

			return true;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get single result from a RegEx query
			try {
				this->queriesRegEx.at(query.index).getFirst(*(this->queryTargetPtr), resultTo);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]."
				);
			}

			break;

		case QueryStruct::typeXPath:
			// parse content as XML/HTML if still necessary
			if(this->parseXml(warningsTo)) {
				// get single result from a XPath query
				try {
					this->queriesXPath.at(query.index).getFirst(this->parsedXML, resultTo);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPointer:
			// parse content as JSON using RapidJSON if still necessary
			if(this->parseJsonRapid(warningsTo)) {
				// get single result from a JSONPointer query
				try {
					this->queriesJsonPointer.at(query.index).getFirst(this->parsedJsonRapid, resultTo);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPath:
			// parse content as JSON using jsoncons if still necessary
			if(this->parseJsonRapid(warningsTo)) {
				// get single result from a JSONPath query
				try {
					this->queriesJsonPath.at(query.index).getFirst(this->parsedJsonCons, resultTo);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeNone:
			break;

		default:
			throw Exception("Query::Container::getSingleFromQuery(): Unknown query type");
		}

		return false;
	}

	// get single result from a query of any type on the current subset,
	//  return false if query failed, throws Container::Exception
	bool Container::getSingleFromQueryOnSubSet(
			const QueryStruct& query,
			std::string& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointer
		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::getSingleFromQueryOnSubSet(): No content source specified");

		// check current subset
		if(!(this->subSetCurrent))
			throw Exception("Query::Container::getSingleFromQueryOnSubSet(): No subset specified");

		if(this->subSetCurrent > this->subSetNumber)
			throw Exception("Query::Container::getSingleFromQueryOnSubSet(): Invalid subset specified");

		// check result type
		if(query.type != QueryStruct::typeNone && !query.resultSingle) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not single."
			);

			return false;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get single result from a RegEx query on the current subset
			try {
				if(this->subSetType != QueryStruct::typeRegEx)
					this->stringifySubSets(warningsTo);

				this->queriesRegEx.at(query.index).getFirst(
						this->stringifiedSubSets.at(this->subSetCurrent - 1),
						resultTo
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]."
				);
			}

			break;

		case QueryStruct::typeXPath:
			// parse current subset as XML/HTML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get single result from a XPath query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPath.at(query.index).getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					else
						this->queriesXPath.at(query.index).getFirst(
								this->subSetParsedXML,
								resultTo
						);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPointer:
			// parse current subset as JSON using RapidJSON if still necessary
			if(this->parseSubSetJsonRapid(warningsTo)) {
				// get single result from a JSONPointer query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeJsonPointer)
						this->queriesJsonPointer.at(query.index).getFirst(
								this->jsonPointerSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					else
						this->queriesJsonPointer.at(query.index).getFirst(
								this->subSetParsedJsonRapid,
								resultTo
						);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPath:
			// parse current subset as JSON using jsoncons if still necessary
			if(this->parseSubSetJsonRapid(warningsTo)) {
				// get single result from a JSONPath query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeJsonPath)
						this->queriesJsonPath.at(query.index).getFirst(
								this->jsonPathSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					else
						this->queriesJsonPath.at(query.index).getFirst(
								this->subSetParsedJsonCons,
								resultTo
						);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeNone:
			break;

		default:
			throw Exception("Query::Container::getSingleFromQueryOnSubSet(): Unknown query type");
		}

		return false;
	}

	// get multiple results from a query of any type,
	//  return false if query failed, throws Container::Exception
	bool Container::getMultiFromQuery(
			const QueryStruct& query,
			std::vector<std::string>& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointers
		if(!(this->queryTargetPtr))
			throw Exception("Query::Container::getMultiFromQuery(): No content specified");

		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::getMultiFromQuery(): No content source specified");

		// check result type
		if(query.type != QueryStruct::typeNone && !query.resultMulti) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not multi."
			);

			return false;
		}

		// check query target
		if(this->queryTargetPtr->empty()) {
			resultTo.clear();

			return true;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get single result from a RegEx query
			try {
				this->queriesRegEx.at(query.index).getAll(*(this->queryTargetPtr), resultTo);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]."
				);
			}

			break;

		case QueryStruct::typeXPath:
			// parse content as XML/HTML if still necessary
			if(this->parseXml(warningsTo)) {
				// get single result from a XPath query
				try {
					this->queriesXPath.at(query.index).getAll(this->parsedXML, resultTo);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPointer:
			// parse content as JSON using RapidJSON if still necessary
			if(this->parseJsonRapid(warningsTo)) {
				// get single result from a JSONPointer query
				try {
					this->queriesJsonPointer.at(query.index).getAll(this->parsedJsonRapid, resultTo);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPath:
			// parse content as JSON using jsoncons if still necessary
			if(this->parseJsonRapid(warningsTo)) {
				// get single result from a JSONPath query
				try {
					this->queriesJsonPath.at(query.index).getAll(this->parsedJsonCons, resultTo);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeNone:
			break;

		default:
			throw Exception("Query::Container::getMultiFromQuery(): Unknown query type");
		}

		return false;
	}

	// get multiple results from a query of any type on the current subset,
	//  return false if query failed, throws Container::Exception
	bool Container::getMultiFromQueryOnSubSet(
			const QueryStruct& query,
			std::vector<std::string>& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointer
		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::getMultiFromQueryOnSubSet(): No content source specified");

		// check current subset
		if(!(this->subSetCurrent))
			throw Exception("Query::Container::getMultiFromQueryOnSubSet(): No subset specified");

		if(this->subSetCurrent > this->subSetNumber)
			throw Exception("Query::Container::getMultiFromQueryOnSubSet(): Invalid subset specified");

		// check result type
		if(query.type != QueryStruct::typeNone && !query.resultMulti) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not multi."
			);

			return false;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get single result from a RegEx query on the current subset
			try {
				if(this->subSetType != QueryStruct::typeRegEx)
					this->stringifySubSets(warningsTo);

				this->queriesRegEx.at(query.index).getAll(
						this->stringifiedSubSets.at(this->subSetCurrent - 1),
						resultTo
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]."
				);
			}

			break;

		case QueryStruct::typeXPath:
			// parse current subset as XML/HTML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get single result from a XPath query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPath.at(query.index).getAll(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					else
						this->queriesXPath.at(query.index).getAll(
								this->subSetParsedXML,
								resultTo
						);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPointer:
			// parse current subset as JSON using RapidJSON if still necessary
			if(this->parseSubSetJsonRapid(warningsTo)) {
				// get single result from a JSONPointer query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeJsonPointer)
						this->queriesJsonPointer.at(query.index).getAll(
								this->jsonPointerSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					else
						this->queriesJsonPointer.at(query.index).getAll(
								this->subSetParsedJsonRapid,
								resultTo
						);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPath:
			// parse current subset as JSON using jsoncons if still necessary
			if(this->parseSubSetJsonRapid(warningsTo)) {
				// get single result from a JSONPath query on the current subset
				try {
					if(this->subSetType == QueryStruct::typeJsonPath)
						this->queriesJsonPath.at(query.index).getAll(
								this->jsonPathSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					else
						this->queriesJsonPath.at(query.index).getAll(
								this->subSetParsedJsonCons,
								resultTo
						);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeNone:
			break;

		default:
			throw Exception("Query::Container::getMultiFromQueryOnSubSet(): Unknown query type");
		}

		return false;
	}

	// set subsets using a query of any type,
	//  return false if query failed, throws Container::Exception
	bool Container::setSubSetsFromQuery(
			const QueryStruct& query,
			std::queue<std::string>& warningsTo
	) {
		// clear old subsets
		switch(this->subSetType) {
		case QueryStruct::typeXPath:
			this->xPathSubSets.clear();

			break;

		case QueryStruct::typeJsonPointer:
			this->jsonPointerSubSets.clear();

			break;

		case QueryStruct::typeJsonPath:
			this->jsonPathSubSets.clear();

			break;
		}

		this->subSetType = query.type;
		this->subSetNumber = 0;
		this->subSetCurrent = 0;

		this->stringifiedSubSets.clear();

		this->resetSubSetParsingState();

		// check pointers
		if(!(this->queryTargetPtr))
			throw Exception("Query::Container::setSubSetsFromQuery(): No content specified");

		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::setSubSetsFromQuery(): No content source specified");

		// check result type
		if(query.type != QueryStruct::typeNone && !query.resultSubSets) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not subsets."
			);

			return false;
		}

		// check query target
		if(this->queryTargetPtr->empty())
			return true;

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get subsets (i. e. all matches) from a RegEx query
			try {
				this->queriesRegEx.at(query.index).getAll(
						*(this->queryTargetPtr),
						this->stringifiedSubSets
				);

				this->subSetNumber = this->stringifiedSubSets.size();

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ e.whatStr()
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]."
				);
			}

			break;

		case QueryStruct::typeXPath:
			// parse content as XML/HTML if still necessary
			if(this->parseXml(warningsTo)) {
				// get subsets from a XPath query
				try {
					this->queriesXPath.at(query.index).getSubSets(
							this->parsedXML,
							this->xPathSubSets
					);

					this->subSetNumber = this->xPathSubSets.size();

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPointer:
			// parse content as JSON using RapidJSON if still necessary
			if(this->parseJsonRapid(warningsTo)) {
				// get single result from a JSONPointer query
				try {
					this->queriesJsonPointer.at(query.index).getSubSets(
							this->parsedJsonRapid,
							this->jsonPointerSubSets
					);

					this->subSetNumber = this->jsonPointerSubSets.size();

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeJsonPath:
			// parse content as JSON using jsoncons if still necessary
			if(this->parseJsonRapid(warningsTo)) {
				// get single result from a JSONPath query
				try {
					this->queriesJsonPath.at(query.index).getSubSets(
							this->parsedJsonCons,
							this->jsonPathSubSets
					);

					this->subSetNumber = this->jsonPathSubSets.size();

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ e.whatStr()
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
			}

			break;

		case QueryStruct::typeNone:
			break;

		default:
			throw Exception("Query::Container::setSubSetsFromQuery(): Unknown query type");
		}

		return false;
	}

	// get tidied XML code as string
	bool Container::getXml(std::string& resultTo, std::queue<std::string>& warningsTo) {
		if(this->parseXml(warningsTo)) {
			this->parsedXML.getContent(resultTo);

			return true;
		}
		else if(warningsTo.empty())
			warningsTo.emplace(
					"WARNING: [XML] "
					+ this->xmlParsingError
					+ " ["
					+ *(this->queryTargetSourcePtr)
					+ "]"
			);

		return false;
	}

	// private helper function to parse content as XML/HTML if still necessary,
	//  return false if parsing failed,  throws Container::Exception
	bool Container::parseXml(std::queue<std::string>& warningsTo) {
		// check pointers
		if(!(this->queryTargetPtr))
			throw Exception("Query::Container::parseXml(): No content specified");

		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::parseXml(): No content source specified");

		if(!(this->xmlParsed) && this->xmlParsingError.empty()) {
			try {
				this->parsedXML.parse(*(this->queryTargetPtr), warningsTo, this->repairCData);

				this->xmlParsed = true;
			}
			catch(const XMLException& e) {
				this->xmlParsingError = e.whatStr();

				warningsTo.emplace(
						"WARNING: [XML] "
						+ this->xmlParsingError
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]"
				);
			}
		}

		return this->xmlParsed;
	}

	// private helper function to parse content as JSON using RapidJSON if still necessary,
	//  return false if parsing failed, throws Container::Exception
	bool Container::parseJsonRapid(std::queue<std::string>& warningsTo) {
		// check pointers
		if(!(this->queryTargetPtr))
			throw Exception("Query::Container::parseJsonRapid(): No content specified");

		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::parseJsonRapid(): No content source specified");

		if(!(this->jsonParsedRapid) && this->jsonParsingError.empty()) {
			try {
				this->parsedJsonRapid = Helper::Json::parseRapid(*(this->queryTargetPtr));

				this->jsonParsedRapid = true;
			}
			catch(const JsonException& e) {
				this->jsonParsingError = e.whatStr();

				warningsTo.emplace(
						"WARNING: [JSON] "
						+ this->jsonParsingError
						+ " ["
						+ *(this->queryTargetSourcePtr)
						+ "]"
				);
			}
		}

		return this->jsonParsedRapid;
	}

	// private helper function to parse content as JSON using jsoncons if still necessary,
	//  return false if parsing failed, throws Container::Exception
	bool Container::parseJsonCons(std::queue<std::string>& warningsTo) {
		// check pointers
		if(!(this->queryTargetPtr))
			throw Exception("Query::Container::parseJsonCons(): No content specified");

		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::parseJsonCons(): No content source specified");

		if(!(this->jsonParsedCons) && this->jsonParsingError.empty()) {
			try {
				this->parsedJsonCons = Helper::Json::parseCons(*(this->queryTargetPtr));

				this->jsonParsedCons = true;
			}
			catch(const JsonException& e) {
				this->jsonParsingError = e.whatStr();

				warningsTo.emplace("WARNING: [JSON] " + this->jsonParsingError);
			}
		}

		return this->jsonParsedCons;
	}

	// reset parsing state for content
	void Container::resetParsingState() {
		// unset parsing state
		this->xmlParsed = false;
		this->jsonParsedRapid = false;
		this->jsonParsedCons = false;

		// clear parsing errors
		this->xmlParsingError.clear();
		this->jsonParsingError.clear();

		// clear parsed content
		this->parsedXML.clear();
		this->parsedJsonCons.clear();

		rapidjson::Value(rapidjson::kObjectType).Swap(this->parsedJsonRapid);
	}

	// private helper function to parse subset as XML/HTML if still necessary,
	//  return false if parsing failed,  throws Container::Exception
	bool Container::parseSubSetXml(std::queue<std::string>& warningsTo) {
		// check current subset
		if(!(this->subSetCurrent))
			throw Exception("Query::Container::parseSubSetXml(): No subset specified");

		if(this->subSetCurrent > this->subSetNumber)
			throw Exception("Query::Container::parseSubSetXml(): Invalid subset specified");

		// if the subset is of type XPath, no further parsing is required
		if(this->subSetType == QueryStruct::typeXPath) {
			if(this->xPathSubSets.at(this->subSetCurrent - 1))
				return true;

			return false;
		}

		if(!(this->subSetXmlParsed) && this->subSetXmlParsingError.empty()) {
			// stringify the subsets if still necessary
			this->stringifySubSets(warningsTo);

			try {
				this->subSetParsedXML.parse(
						this->stringifiedSubSets.at(this->subSetCurrent - 1),
						warningsTo,
						this->repairCData
				);

				this->subSetXmlParsed = true;
			}
			catch(const XMLException& e) {
				this->subSetXmlParsingError = e.whatStr();

				warningsTo.emplace("WARNING: [XML] " + this->subSetXmlParsingError);
			}
		}

		return this->subSetXmlParsed;
	}

	// private helper function to parse subset as JSON using RapidJSON if still necessary,
	//  return false if parsing failed, throws Container::Exception
	bool Container::parseSubSetJsonRapid(std::queue<std::string>& warningsTo) {
		// check current subset
		if(!(this->subSetCurrent))
			throw Exception("Query::Container::parseSubSetJsonRapid(): No subset specified");

		if(this->subSetCurrent > this->subSetNumber)
			throw Exception("Query::Container::parseSubSetJsonRapid(): Invalid subset specified");

		// if the subset is of type JSONPointer, no further parsing is required
		if(this->subSetType == QueryStruct::typeJsonPointer)
			return true;

		if(!(this->subSetJsonParsedRapid) && this->subSetJsonParsingError.empty()) {
			// stringify the subsets if still necessary
			this->stringifySubSets(warningsTo);

			try {
				this->subSetParsedJsonRapid = Helper::Json::parseRapid(
						this->stringifiedSubSets.at(this->subSetCurrent - 1)
				);

				this->subSetJsonParsedRapid = true;
			}
			catch(const JsonException& e) {
				this->subSetJsonParsingError = e.whatStr();

				warningsTo.emplace("WARNING: [JSON] " + this->subSetJsonParsingError);
			}
		}

		return this->subSetJsonParsedRapid;
	}

	// private helper function to parse subset as JSON using jsoncons if still necessary,
	//  return false if parsing failed, throws Container::Exception
	bool Container::parseSubSetJsonCons(std::queue<std::string>& warningsTo) {
		// check current subset
		if(!(this->subSetCurrent))
			throw Exception("Query::Container::parseSubSetJsonCons(): No subset specified");

		if(this->subSetCurrent > this->subSetNumber)
			throw Exception("Query::Container::parseSubSetJsonCons(): Invalid subset specified");

		// if the subset is of type JSONPath, no further parsing is required
		if(this->subSetType == QueryStruct::typeJsonPath)
			return true;

		if(!(this->subSetJsonParsedCons) && this->subSetJsonParsingError.empty()) {
			// stringify the subsets if still necessary
			this->stringifySubSets(warningsTo);

			try {
				this->subSetParsedJsonCons = Helper::Json::parseCons(
						this->stringifiedSubSets.at(this->subSetCurrent - 1)
				);

				this->subSetJsonParsedCons = true;
			}
			catch(const JsonException& e) {
				this->subSetJsonParsingError = e.whatStr();

				warningsTo.emplace("WARNING: [JSON] " + this->subSetJsonParsingError);
			}
		}

		return this->subSetJsonParsedCons;
	}

	// reset parsing state for subset
	void Container::resetSubSetParsingState() {
		// unset parsing state
		this->subSetXmlParsed = false;
		this->subSetJsonParsedRapid = false;
		this->subSetJsonParsedCons = false;

		// clear parsing errors
		this->subSetXmlParsingError.clear();
		this->subSetJsonParsingError.clear();

		// clear parsed content
		this->subSetParsedXML.clear();
		this->subSetParsedJsonCons.clear();

		rapidjson::Value(rapidjson::kObjectType).Swap(this->subSetParsedJsonRapid);
	}

	// private helper function to stringify subsets if still necessary
	void Container::stringifySubSets(std::queue<std::string>& warningsTo) {
		if(!(this->stringifiedSubSets.empty()))
			return;

		switch(this->subSetType) {
		case QueryStruct::typeXPath:
			for(const auto& subset : this->xPathSubSets) {
				std::string subsetString;

				subset.getContent(subsetString);

				this->stringifiedSubSets.emplace_back(std::move(subsetString));
			}

			break;

		case QueryStruct::typeJsonPath:
			for(const auto& subset : this->jsonPathSubSets)
				this->stringifiedSubSets.emplace_back(Helper::Json::stringify(subset));

			break;

		case QueryStruct::typeJsonPointer:
			for(const auto& subset : this->jsonPointerSubSets)
				this->stringifiedSubSets.emplace_back(Helper::Json::stringify(subset));

			break;

		case QueryStruct::typeRegEx:
			warningsTo.emplace(
					"WARNING: RegEx subsets cannot be stringified."
			);

			break;

		case QueryStruct::typeNone:
			break;

		default:
			warningsTo.emplace("WARNING: Unknown subset type in Query::Container::stringifySubSets(...).");
		}
	}

} /* crawlservpp::Query */
