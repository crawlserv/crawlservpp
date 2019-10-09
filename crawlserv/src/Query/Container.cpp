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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
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
			  minimizeMemory(false),
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

	// set whether to minimize memory usage (may affect performance)
	void Container::setMinimizeMemory(bool isMinimizeMemory) {
		this->minimizeMemory = isMinimizeMemory;
	}

	// set how tidy-html reports errors and warnings
	void Container::setTidyErrorsAndWarnings(unsigned int errors, bool warnings) {
		this->parsedXML.setOptions(warnings, errors);
		this->subSetParsedXML.setOptions(warnings, errors);
	}

	// set content to use queries on
	void Container::setQueryTarget(const std::string& content, const std::string& source) {
		// clear old target
		this->clearQueryTarget();

		// set new target
		this->queryTargetPtr = &content;
		this->queryTargetSourcePtr = &source;
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
					this->queriesJsonPointer.emplace_back(properties.text, properties.textOnly);
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
					this->queriesJsonPath.emplace_back(properties.text, properties.textOnly);
				}
				catch(const JsonPathException &e) {
					throw Exception("[JSONPath] " + e.whatStr());
				}

				newQuery.type = QueryStruct::typeJsonPath;
			}
			else if(properties.type == "xpathjsonpointer") {
				// add combined XPath and JSONPointer query
				newQuery.index = this->queriesXPathJsonPointer.size();

				// split XPath query (first line) from JSON query
				const auto splitPos = properties.text.find('\n');
				const std::string xPathQuery(
						properties.text,
						0,
						splitPos
				);
				std::string jsonQuery;

				if(splitPos != std::string::npos && properties.text.size() > splitPos + 1)
					jsonQuery = properties.text.substr(splitPos + 1);

				try {
					this->queriesXPathJsonPointer.emplace_back(
							XPath(xPathQuery, true),
							JsonPointer(jsonQuery, properties.textOnly)
					);
				}
				catch(const XPathException& e) {
					throw Exception("[XPath] " + e.whatStr());
				}
				catch(const JsonPointerException &e) {
					throw Exception("[JSONPointer] " + e.whatStr());
				}

				newQuery.type = QueryStruct::typeXPathJsonPointer;

			}
			else if(properties.type == "xpathjsonpath") {
				// add combined XPath and JSONPath query
				newQuery.index = this->queriesXPathJsonPath.size();

				// split XPath query (first line) from JSON query
				const auto splitPos = properties.text.find('\n');
				const std::string xPathQuery(
						properties.text,
						0,
						splitPos
				);
				std::string jsonQuery;

				if(splitPos != std::string::npos && properties.text.size() > splitPos + 1)
					jsonQuery = properties.text.substr(splitPos + 1);

				try {
					this->queriesXPathJsonPath.emplace_back(
							XPath(xPathQuery, true),
							JsonPath(jsonQuery, properties.textOnly)
					);
				}
				catch(const XPathException& e) {
					throw Exception("[XPath] " + e.whatStr());
				}
				catch(const JsonPathException &e) {
					throw Exception("[JSONPath] " + e.whatStr());
				}

				newQuery.type = QueryStruct::typeXPathJsonPath;
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
		std::vector<XPath>().swap(this->queriesXPath);
		std::vector<RegEx>().swap(this->queriesRegEx);
		std::vector<JsonPointer>().swap(this->queriesJsonPointer);
		std::vector<JsonPath>().swap(this->queriesJsonPath);
		std::vector<XPathJsonPointer>().swap(this->queriesXPathJsonPointer);
		std::vector<XPathJsonPath>().swap(this->queriesXPathJsonPath);
	}

	// clear query target
	void Container::clearQueryTarget() {
		// clear old subsets
		this->clearSubSets();

		// clear parsed content
		this->parsedXML.clear();

		jsoncons::json().swap(this->parsedJsonCons);
		rapidjson::Value(rapidjson::kObjectType).Swap(this->parsedJsonRapid);

		// reset parsing state
		this->xmlParsed = false;
		this->jsonParsedRapid = false;
		this->jsonParsedCons = false;

		// clear parsing errors
		std::string().swap(this->xmlParsingError);
		std::string().swap(this->jsonParsingError);

		// unset pointers
		this->queryTargetPtr = nullptr;
		this->queryTargetSourcePtr = nullptr;
	}

	// use next subset for queries, return false if no more subsets exist, throws Container::Exception
	bool Container::nextSubSet() {
		// check subsets
		if(this->subSetNumber < this->subSetCurrent)
			throw Exception("Query::Container::nextSubSet(): Invalid subset selected");

		if(this->subSetNumber == this->subSetCurrent)
			return false;

		// clear previous subset
		if(!(this->stringifiedSubSets.empty()))
			std::string().swap(this->stringifiedSubSets.at(this->subSetCurrent - 1));

		switch(this->subSetType) {
		case QueryStruct::typeXPath:
			this->xPathSubSets.at(this->subSetCurrent - 1).clear();

			break;

		case QueryStruct::typeJsonPointer:
			rapidjson::Value(rapidjson::kObjectType).Swap(this->jsonPointerSubSets.at(this->subSetCurrent - 1));

			break;

		case QueryStruct::typeJsonPath:
			jsoncons::json().swap(this->jsonPathSubSets.at(this->subSetCurrent - 1));

			break;
		}

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
	) const {
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
	) const {
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
	) const {
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
			// parse content as HTML/XML if still necessary
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
			if(this->parseJsonCons(warningsTo)) {
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

		case QueryStruct::typeXPathJsonPointer:
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get first result from the XPath query
				try {
					std::string json;

					this->queriesXPathJsonPointer.at(query.index).first.getFirst(this->parsedXML, json);

					if(json.empty())
						resultTo = false;
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get boolean result from the JSONPointer query
						resultTo = this->queriesXPathJsonPointer.at(query.index).second.getBool(parsedJson);
					}

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

		case QueryStruct::typeXPathJsonPath:
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get first result from the XPath query
				try {
					std::string json;

					this->queriesXPathJsonPath.at(query.index).first.getFirst(this->parsedXML, json);

					if(json.empty())
						resultTo = false;
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get boolean result from the JSONPath query
						resultTo = this->queriesXPathJsonPath.at(query.index).second.getBool(parsedJson);
					}

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
			// parse current subset as HTML/XML if still necessary
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

		case QueryStruct::typeXPathJsonPointer:
			// parse current subset as HTML/XML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get first result from the XPath query on the current subset
				try {
					std::string json;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					else
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);

					if(json.empty())
						resultTo = false;
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get boolean result from the JSONPointer query
						resultTo = this->queriesXPathJsonPointer.at(query.index).second.getBool(parsedJson);
					}

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

		case QueryStruct::typeXPathJsonPath:
			// parse current subset as HTML/XML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get first result from the XPath query on the current subset
				try {
					std::string json;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					else
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);

					if(json.empty())
						resultTo = false;
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get boolean result from the JSONPath query
						resultTo = this->queriesXPathJsonPath.at(query.index).second.getBool(parsedJson);
					}

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
			// parse content as HTML/XML if still necessary
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
			if(this->parseJsonCons(warningsTo)) {
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

		case QueryStruct::typeXPathJsonPointer:
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get first result from the XPath query
				try {
					std::string json;

					this->queriesXPathJsonPointer.at(query.index).first.getFirst(this->parsedXML, json);

					if(json.empty())
						resultTo = "";
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get single result from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getFirst(parsedJson, resultTo);
					}

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

		case QueryStruct::typeXPathJsonPath:
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get first result from the XPath query
				try {
					std::string json;

					this->queriesXPathJsonPath.at(query.index).first.getFirst(this->parsedXML, json);

					if(json.empty())
						resultTo = "";
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get single result from the JSONPath query
						this->queriesXPathJsonPath.at(query.index).second.getFirst(parsedJson, resultTo);
					}

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
			// parse current subset as HTML/XML if still necessary
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

		case QueryStruct::typeXPathJsonPointer:
			// parse current subset as HTML/XML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get first result from the XPath query on the current subset
				try {
					std::string json;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					else
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);

					if(json.empty())
						resultTo = "";
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get single result from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getFirst(parsedJson, resultTo);
					}

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

		case QueryStruct::typeXPathJsonPath:
			// parse current subset as HTML/XML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get first result from the XPath query on the current subset
				try {
					std::string json;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					else
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);

					if(json.empty())
						resultTo = "";
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get single result from the JSONPath query
						this->queriesXPathJsonPath.at(query.index).second.getFirst(parsedJson, resultTo);
					}

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
			// get multiple results from a RegEx query
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
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get multiple results from a XPath query
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
				// get multiple results from a JSONPointer query
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
			if(this->parseJsonCons(warningsTo)) {
				// get multiple results from a JSONPath query
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

		case QueryStruct::typeXPathJsonPointer:
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get first result from the XPath query
				try {
					std::string json;

					this->queriesXPathJsonPointer.at(query.index).first.getFirst(this->parsedXML, json);

					if(json.empty())
						resultTo.clear();
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get multiple results from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getAll(parsedJson, resultTo);
					}

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

		case QueryStruct::typeXPathJsonPath:
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get first result from the XPath query
				try {
					std::string json;

					this->queriesXPathJsonPath.at(query.index).first.getFirst(this->parsedXML, json);

					if(json.empty())
						resultTo.clear();
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get multiple results from the JSONPath query
						this->queriesXPathJsonPath.at(query.index).second.getAll(parsedJson, resultTo);
					}

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
			// get multiple result from a RegEx query on the current subset
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
			// parse current subset as HTML/XML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get multiple results from a XPath query on the current subset
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
				// get multiple results from a JSONPointer query on the current subset
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
				// get multiple results from a JSONPath query on the current subset
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

		case QueryStruct::typeXPathJsonPointer:
			// parse current subset as HTML/XML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get first result from the XPath query on the current subset
				try {
					std::string json;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					else
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);

					if(json.empty())
						resultTo.clear();
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get multiple results from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getAll(parsedJson, resultTo);
					}

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

		case QueryStruct::typeXPathJsonPath:
			// parse current subset as HTML/XML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get first result from the XPath query on the current subset
				try {
					std::string json;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					else
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);

					if(json.empty())
						resultTo.clear();
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get multiple results from the JSONPath query
						this->queriesXPathJsonPath.at(query.index).second.getAll(parsedJson, resultTo);
					}

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
		this->clearSubSets();

		// set new subset type
		switch(query.type) {
		case QueryStruct::typeXPath:
			this->subSetType = QueryStruct::typeXPath;

			break;

		case QueryStruct::typeJsonPointer:
		case QueryStruct::typeXPathJsonPointer:
			this->subSetType = QueryStruct::typeJsonPointer;

			break;

		case QueryStruct::typeJsonPath:
		case QueryStruct::typeXPathJsonPath:
			this->subSetType = QueryStruct::typeJsonPath;

			break;
		}

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
			// parse content as HTML/XML if still necessary
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
				// get subsets from a JSONPointer query
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
			if(this->parseJsonCons(warningsTo)) {
				// get subsets from a JSONPath query
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

		case QueryStruct::typeXPathJsonPointer:
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get first result from the XPath query
				try {
					std::string json;

					this->queriesXPathJsonPointer.at(query.index).first.getFirst(this->parsedXML, json);

					if(!json.empty()) {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get subsets from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getSubSets(
								parsedJson,
								this->jsonPointerSubSets
						);
					}

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

		case QueryStruct::typeXPathJsonPath:
			// parse content as HTML/XML if still necessary
			if(this->parseXml(warningsTo)) {
				// get first result from the XPath query
				try {
					std::string json;

					this->queriesXPathJsonPath.at(query.index).first.getFirst(this->parsedXML, json);

					if(!json.empty()) {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get subsets from the JSONPointer query
						this->queriesXPathJsonPath.at(query.index).second.getSubSets(
								parsedJson,
								this->jsonPathSubSets
						);
					}

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

	// add more subsets after the current one based on a query on the current subset (for recursive extracting),
	//  return whether new subsets have been added
	bool Container::addSubSetsFromQueryOnSubSet(
			const QueryStruct& query,
			std::queue<std::string>& warningsTo
	) {
		// check pointer
		if(!(this->queryTargetSourcePtr))
			throw Exception("Query::Container::addSubSetsFromQueryOnSubSet(): No content source specified");

		// check current subset
		if(!(this->subSetCurrent))
			throw Exception("Query::Container::addSubSetsFromQueryOnSubSet(): No subset specified");

		if(this->subSetCurrent > this->subSetNumber)
			throw Exception("Query::Container::addSubSetsFromQueryOnSubSet(): Invalid subset specified");

		// check result type
		if(query.type != QueryStruct::typeNone && !query.resultSubSets) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not subset."
			);

			return false;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get more subsets from a RegEx query on the current subset
			try {
				// stringify old subsets if necessary
				if(this->subSetType != QueryStruct::typeRegEx)
					this->stringifySubSets(warningsTo);

				// get new subsets from RegEx query
				std::vector<std::string> subsets;

				this->queriesRegEx.at(query.index).getAll(
						this->stringifiedSubSets.at(this->subSetCurrent - 1),
						subsets
				);

				// check number of results
				if(subsets.empty())
					return false;

				// insert new RegEx subsets
				this->insertSubSets(subsets);

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
			// parse current subset as HTML/XML (and stringify old subsets) if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get more subsets from a XPath query on the current subset
				try {
					std::vector<Parsing::XML> subsets;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPath.at(query.index).getSubSets(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								subsets
						);
					else
						this->queriesXPath.at(query.index).getSubSets(
								this->subSetParsedXML,
								subsets
						);

					// check number of results
					if(subsets.empty())
						return false;

					// insert new XPath subsets
					this->insertSubSets(subsets);

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
			// parse current subset as JSON using RapidJSON (and stringify old subsets) if still necessary
			if(this->parseSubSetJsonRapid(warningsTo)) {
				// get more subsets from a JSONPointer query on the current subset
				try {
					std::vector<rapidjson::Document> subsets;

					if(this->subSetType == QueryStruct::typeJsonPointer)
						this->queriesJsonPointer.at(query.index).getSubSets(
								this->jsonPointerSubSets.at(this->subSetCurrent - 1),
								subsets
						);
					else
						this->queriesJsonPointer.at(query.index).getSubSets(
								this->subSetParsedJsonRapid,
								subsets
						);

					// check number of results
					if(subsets.empty())
						return false;

					// insert new JSONPointer subsets
					this->insertSubSets(subsets);

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
			// parse current subset as JSON using jsoncons (and stringify old subsets) if still necessary
			if(this->parseSubSetJsonRapid(warningsTo)) {
				// get more subsets from a JSONPath query on the current subset
				try {
					std::vector<jsoncons::json> subsets;

					if(this->subSetType == QueryStruct::typeJsonPath)
						this->queriesJsonPath.at(query.index).getSubSets(
								this->jsonPathSubSets.at(this->subSetCurrent - 1),
								subsets
						);
					else
						this->queriesJsonPath.at(query.index).getSubSets(
								this->subSetParsedJsonCons,
								subsets
						);

					// check number of results
					if(subsets.empty())
						return false;

					// insert new JSONPath subsets
					this->insertSubSets(subsets);

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

		case QueryStruct::typeXPathJsonPointer:
			// parse current subset as HTML/XML (and stringify old subsets) if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get first result from the XPath query on the current subset
				try {
					std::string json;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					else
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);

					if(json.empty())
						return false;

					// temporarily parse JSON using rapidJSON
					const auto parsedJson(Helper::Json::parseRapid(json));

					// get more subsets from the JSONPointer query
					std::vector<rapidjson::Document> subsets;

					this->queriesXPathJsonPointer.at(query.index).second.getSubSets(parsedJson, subsets);

					// stringify old subsets if necessary (and if it was not already done for HTML/XML parsing)
					if(this->subSetType == QueryStruct::typeXPath)
						this->stringifySubSets(warningsTo);

					// check number of results
					if(subsets.empty())
						return false;

					// insert new JSONPointer subsets
					this->insertSubSets(subsets);

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

		case QueryStruct::typeXPathJsonPath:
			// parse current subset as HTML/XML if still necessary
			if(this->parseSubSetXml(warningsTo)) {
				// get first result from the XPath query on the current subset
				try {
					std::string json;

					if(this->subSetType == QueryStruct::typeXPath)
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					else
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);

					if(json.empty())
						return true;

					// temporarily parse JSON using jsoncons
					const auto parsedJson(Helper::Json::parseCons(json));

					// get more subsets from JSONPath query
					std::vector<jsoncons::json> subsets;

					// get multiple results from the JSONPath query
					this->queriesXPathJsonPath.at(query.index).second.getSubSets(parsedJson, subsets);

					// stringify old subsets if necessary (and if it was not already done for HTML/XML parsing)
					if(this->subSetType == QueryStruct::typeXPath)
						this->stringifySubSets(warningsTo);

					// check number of results
					if(subsets.empty())
						return false;

					// insert new JSONPath subsets
					this->insertSubSets(subsets);

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
			throw Exception("Query::Container::addSubSetsFromQueryOnSubSet(): Unknown query type");
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

	// reserve memory for a specific number of subsets
	void Container::reserveForSubSets(const QueryStruct& query, unsigned long n) {
		this->stringifiedSubSets.reserve(n);

		switch(query.type) {
		case QueryStruct::typeXPath:
			this->xPathSubSets.reserve(n);

			break;

		case QueryStruct::typeJsonPointer:
		case QueryStruct::typeXPathJsonPointer:
			this->jsonPointerSubSets.reserve(n);

			break;

		case QueryStruct::typeJsonPath:
		case QueryStruct::typeXPathJsonPath:
			this->jsonPathSubSets.reserve(n);
		}
	}

	// private helper function to parse content as HTML/XML if still necessary,
	//  return false if parsing failed, throws Container::Exception
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

	// private helper function to parse subset as HTML/XML if still necessary,
	//  return false if parsing failed, throws Container::Exception
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
		std::string().swap(this->subSetXmlParsingError);
		std::string().swap(this->subSetJsonParsingError);

		// clear parsed content
		this->subSetParsedXML.clear();

		jsoncons::json().swap(this->subSetParsedJsonCons);

		rapidjson::Value(rapidjson::kObjectType).Swap(this->subSetParsedJsonRapid);
	}

	// clear subsets
	void Container::clearSubSets() {
		if(this->minimizeMemory) {
			switch(this->subSetType) {
			case QueryStruct::typeXPath:
				std::vector<Parsing::XML>().swap(this->xPathSubSets);

				break;

			case QueryStruct::typeJsonPointer:
				std::vector<rapidjson::Document>().swap(this->jsonPointerSubSets);

				break;

			case QueryStruct::typeJsonPath:
				std::vector<jsoncons::json>().swap(this->jsonPathSubSets);

				break;
			}

			std::vector<std::string>().swap(this->stringifiedSubSets);
		}
		else {
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

			this->stringifiedSubSets.clear();
		}

		this->subSetType = QueryStruct::typeNone;
		this->subSetNumber = 0;
		this->subSetCurrent = 0;

		// reset parsing state for subset
		this->resetSubSetParsingState();
	}

	// stringify subsets if still necessary
	void Container::stringifySubSets(std::queue<std::string>& warningsTo) {
		if(!(this->stringifiedSubSets.empty()))
			return;

		switch(this->subSetType) {
		case QueryStruct::typeXPath:
			for(const auto& subset : this->xPathSubSets) {
				std::string subsetString;

				subset.getContent(subsetString);

				this->stringifiedSubSets.emplace_back(subsetString);
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
			warningsTo.emplace("WARNING: RegEx subsets cannot be stringified.");

			break;

		case QueryStruct::typeNone:
			break;

		default:
			warningsTo.emplace("WARNING: Unknown subset type in Query::Container::stringifySubSets(...).");
		}
	}

	// insert RegEx subsets after the current subset
	//  NOTE:	the new subsets will be moved away from the vector;
	//			if the subset type is different, the old subsets need to be already stringified
	void Container::insertSubSets(std::vector<std::string>& subsets) {
		// update number of subsets
		this->subSetNumber += subsets.size();

		// insert new subsets
		this->stringifiedSubSets.insert(
				this->stringifiedSubSets.begin() + this->subSetCurrent,
				std::make_move_iterator(subsets.begin()),
				std::make_move_iterator(subsets.end())
		);

		// clear non-stringified subsets if necessary
		switch(this->subSetType) {
		case QueryStruct::typeXPath:
			if(this->minimizeMemory)
				std::vector<Parsing::XML>().swap(this->xPathSubSets);
			else
				this->xPathSubSets.clear();

			break;

		case QueryStruct::typeJsonPointer:
			if(this->minimizeMemory)
				std::vector<rapidjson::Document>().swap(this->jsonPointerSubSets);
			else
				this->jsonPointerSubSets.clear();

			break;

		case QueryStruct::typeJsonPath:
			if(this->minimizeMemory)
				std::vector<jsoncons::json>().swap(this->jsonPathSubSets);
			else
				this->jsonPathSubSets.clear();

			break;
		}

		// set the subset type to RegEx (i. e. strings only)
		this->subSetType = QueryStruct::typeRegEx;
	}

	// insert XPath subsets after the current subset, stringify new subsets if needed
	//  NOTE:	the new subsets will be moved away from the vector;
	//  		if the subset type is different, the old subsets need to be already stringified
	void Container::insertSubSets(std::vector<Parsing::XML>& subsets) {
		// update number of subsets
		this->subSetNumber += subsets.size();

		// check subset type
		if(this->subSetType == QueryStruct::typeXPath)
			// insert new XPath subsets
			this->xPathSubSets.insert(
					this->xPathSubSets.begin() + this->subSetCurrent,
					std::make_move_iterator(subsets.begin()),
					std::make_move_iterator(subsets.end())
			);
		else {
			// stringify new subsets (old ones should already be stringified)
			std::vector<std::string> stringified;

			stringified.reserve(subsets.size());

			for(const auto& subset : subsets) {
				stringified.emplace_back();

				subset.getContent(stringified.back());
			}

			// insert new (stringified) XPath subsets
			this->stringifiedSubSets.insert(
				this->stringifiedSubSets.begin() + this->subSetCurrent,
				stringified.begin(),
				stringified.end()
			);

			// clear non-stringified subsets if neccesary
			switch(this->subSetType) {
			case QueryStruct::typeJsonPointer:
				if(this->minimizeMemory)
					std::vector<rapidjson::Document>().swap(this->jsonPointerSubSets);
				else
					this->jsonPointerSubSets.clear();

				break;

			case QueryStruct::typeJsonPath:
				if(this->minimizeMemory)
					std::vector<jsoncons::json>().swap(this->jsonPathSubSets);
				else
					this->jsonPathSubSets.clear();

				break;
			}

			// set the subset type to RegEx (i. e. strings only)
			this->subSetType = QueryStruct::typeRegEx;
		}
	}

	// insert JSONPointer subsets after the current subset, stringify new subsets if needed
	//  NOTE:	the new subsets will be moved away from the vector;
	//  		if the subset type is different, the old subsets need to be already stringified
	void Container::insertSubSets(std::vector<rapidjson::Document>& subsets) {
		// update number of subsets
		this->subSetNumber += subsets.size();

		// check subset type
		if(this->subSetType == QueryStruct::typeJsonPointer)
			// insert new JSONPointer subsets
			this->jsonPointerSubSets.insert(
					this->jsonPointerSubSets.begin() + this->subSetCurrent,
					std::make_move_iterator(subsets.begin()),
					std::make_move_iterator(subsets.end())
			);
		else {
			// stringify new subsets
			std::vector<std::string> stringified;

			stringified.reserve(subsets.size());

			for(const auto& subset : subsets)
				stringified.emplace_back(Helper::Json::stringify(subset));

			// insert new (stringified) JSONPointer subsets
			this->stringifiedSubSets.insert(
				this->stringifiedSubSets.begin() + this->subSetCurrent,
				stringified.begin(),
				stringified.end()
			);

			// clear non-stringified subsets if neccesary
			switch(this->subSetType) {
			case QueryStruct::typeXPath:
				if(this->minimizeMemory)
					std::vector<Parsing::XML>().swap(this->xPathSubSets);
				else
					this->xPathSubSets.clear();

				break;

			case QueryStruct::typeJsonPath:
				if(this->minimizeMemory)
					std::vector<jsoncons::json>().swap(this->jsonPathSubSets);
				else
					this->jsonPathSubSets.clear();

				break;
			}

			// set the subset type to RegEx (i. e. strings only)
			this->subSetType = QueryStruct::typeRegEx;
		}
	}

	// insert JSONPath subsets after the current subset, stringify subsets if needed
	//  NOTE:	the new subsets will be moved away from the vector;
	//  		if the subset type is different, the old subsets need to be already stringified
	void Container::insertSubSets(std::vector<jsoncons::json>& subsets) {
		// update number of subsets
		this->subSetNumber += subsets.size();

		// check subset type
		if(this->subSetType == QueryStruct::typeJsonPath)
			// insert new JSONPath subsets
			this->jsonPathSubSets.insert(
					this->jsonPathSubSets.begin() + this->subSetCurrent,
					std::make_move_iterator(subsets.begin()),
					std::make_move_iterator(subsets.end())
			);
		else {
			// stringify new subsets
			std::vector<std::string> stringified;

			stringified.reserve(subsets.size());

			for(const auto& subset : subsets)
				stringified.emplace_back(Helper::Json::stringify(subset));

			// insert new (stringified) JSONPath subsets
			this->stringifiedSubSets.insert(
				this->stringifiedSubSets.begin() + this->subSetCurrent,
				stringified.begin(),
				stringified.end()
			);

			// clear non-stringified subsets if neccesary
			switch(this->subSetType) {
			case QueryStruct::typeXPath:
				if(this->minimizeMemory)
					std::vector<Parsing::XML>().swap(this->xPathSubSets);
				else
					this->xPathSubSets.clear();

				break;

			case QueryStruct::typeJsonPointer:
				if(this->minimizeMemory)
					std::vector<rapidjson::Document>().swap(this->jsonPointerSubSets);
				else
					this->jsonPointerSubSets.clear();

				break;
			}

			// set the subset type to RegEx (i. e. strings only)
			this->subSetType = QueryStruct::typeRegEx;
		}
	}

} /* crawlservpp::Query */
