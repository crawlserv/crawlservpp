/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * Container.hpp
 *
 * Abstract class for management and type-independent usage of queries.
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

#include "../Helper/Json.hpp"
#include "../Main/Exception.hpp"
#include "../Parsing/XML.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/QueryStruct.hpp"

#include "../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <algorithm>	// std::find
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint32_t, std::uint64_t
#include <iterator>		// std::make_move_iterator
#include <mutex>		// std::lock_guard, std::mutex
#include <queue>		// std::queue
#include <string>		// std::string
#include <utility>		// std::pair
#include <vector>		// std::vector

//! Namespace for classes handling queries.
namespace crawlservpp::Query {

	/*
	 * DECLARATION
	 */

	//! %Query container.
	/*!
	 * Abstract class to be inherited by module thread classes
	 *  managing their queries.
	 *
	 * Most member functions of the container are protected,
	 *  as they will only be used from inside its child classes.
	 */
	class Container {
		// for convenience
		using JsonException = Helper::Json::Exception;

		using XMLException = Parsing::XML::Exception;

		using JsonPathException = Query::JsonPath::Exception;
		using JsonPointerException = Query::JsonPointer::Exception;
		using RegExException = Query::RegEx::Exception;
		using XPathException = Query::XPath::Exception;

		using QueryProperties = Struct::QueryProperties;
		using QueryStruct = Struct::QueryStruct;

		using XPathJsonPointer = std::pair<XPath, JsonPointer>;
		using XPathJsonPath = std::pair<XPath, JsonPath>;

	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		Container() = default;

		//! Default destructor.
		virtual ~Container() = default;

		///@}

		/*!
		 * The class is not copyable, only (default) moveable.
		 */
		///@name Copy and move
		///@{

		//! Deleted copy constructor.
		Container(const Container&) = delete;

		//! Deleted copy assignment operator.
		Container& operator=(const Container&) = delete;

		//! Default move constructor.
		Container(Container&&) = default;

		//! Default move assignment operator.
		Container& operator=(Container&&) = default;

		///@}

		///@name Public Getter
		///@{

		bool isQueryUsed(std::uint64_t queryId) const;

		///@}

	protected:
		//! Class for query container exceptions.
		/*!
		 * This exception is being thrown when
		 * - a query to be added to the container
		 *    could not be created or its type is
		 *    unknown
		 * - an invalid subset had been selected
		 * - no query target has been specified
		 *    before performing a query
		 * - no or an invalid subset has been
		 *    specified before performing a query
		 *    on the subset
		 * - a query to be performed is of an
		 *    unknown type
		 */
		MAIN_EXCEPTION_CLASS();

		///@name Setters
		///@{

		void setRepairCData(bool isRepairCData);
		void setRepairComments(bool isRepairComments);
		void setRemoveXmlInstructions(bool isRemoveXmlInstructions);
		void setMinimizeMemory(bool isMinimizeMemory);
		void setTidyErrorsAndWarnings(bool warnings, std::uint32_t numOfErrors);
		void setQueryTarget(const std::string& content, const std::string& source);

		///@name Getters
		///@{

		[[nodiscard]] std::size_t getNumberOfSubSets() const;
		bool getTarget(std::string& targetTo);
		bool getXml(std::string& resultTo, std::queue<std::string>& warningsTo);

		///@}
		///@name Initialization
		///@{

		//! Pure virtual function initializing queries.
		/*!
		 * This function needs to be implemented by
		 *  the child classes of the container,
		 *  so that children need to initialize
		 *  their queries on their own.
		 */
		virtual void initQueries() = 0;

		///@}
		///@name Queries
		///@{

		QueryStruct addQuery(
				std::uint64_t id,
				const QueryProperties& properties
		);
		void clearQueries();
		void clearQueryTarget();

		///@}
		///@name Subsets
		///@{

		bool nextSubSet();

		///@}
		///@name Results
		///@{

		bool getBoolFromRegEx(
				const QueryStruct& query,
				const std::string& target,
				bool& resultTo,
				std::queue<std::string>& warningsTo
		) const;
		bool getSingleFromRegEx(
				const QueryStruct& query,
				const std::string& target,
				std::string& resultTo,
				std::queue<std::string>& warningsTo
		) const;
		bool getMultiFromRegEx(
				const QueryStruct& query,
				const std::string& target,
				std::vector<std::string>& resultTo,
				std::queue<std::string>& warningsTo
		) const;
		bool getBoolFromQuery(
				const QueryStruct& query,
				bool& resultTo,
				std::queue<std::string>& warningsTo
		);
		bool getBoolFromQueryOnSubSet(
				const QueryStruct& query,
				bool& resultTo,
				std::queue<std::string>& warningsTo
		);
		bool getSingleFromQuery(
				const QueryStruct& query,
				std::string& resultTo,
				std::queue<std::string>& warningsTo
		);
		bool getSingleFromQueryOnSubSet(
				const QueryStruct& query,
				std::string& resultTo,
				std::queue<std::string>& warningsTo
		);
		bool getMultiFromQuery(
				const QueryStruct& query,
				std::vector<std::string>& resultTo,
				std::queue<std::string>& warningsTo
		);
		bool getMultiFromQueryOnSubSet(
				const QueryStruct& query,
				std::vector<std::string>& resultTo,
				std::queue<std::string>& warningsTo
		);
		bool setSubSetsFromQuery(
				const QueryStruct& query,
				std::queue<std::string>& warningsTo
		);
		bool addSubSetsFromQueryOnSubSet(
				const QueryStruct& query,
				std::queue<std::string>& warningsTo
		);

		///@}
		///@name Memory
		///@{

		void reserveForSubSets(const QueryStruct& query, std::size_t n);

		///@}

	private:
		// queries
		std::vector<RegEx> queriesRegEx;
		std::vector<XPath> queriesXPath;
		std::vector<JsonPointer> queriesJsonPointer;
		std::vector<JsonPath> queriesJsonPath;
		std::vector<XPathJsonPointer> queriesXPathJsonPointer;
		std::vector<XPathJsonPath> queriesXPathJsonPath;

		// query IDs and their lock
		std::vector<std::uint64_t> queriesId;
		mutable std::mutex queriesIdLock;

		// options
		bool repairCData{true};						// try to repair CData when parsing HTML/XML
		bool repairComments{true};					// try to repair broken HTML/XML comments
		bool removeXmlInstructions{false};			// remove XML processing instructions
		bool minimizeMemory{false};					// minimize memory usage

		// content pointers and parsing
		const std::string * queryTargetPtr{nullptr}; // ptr to content to perform queries on
		const std::string * queryTargetSourcePtr{nullptr}; // ptr to source of content (for generating warnings)
		bool xmlParsed{false};				// content has been parsed as HTML/XML
		bool jsonParsedRapid{false};		// content has been parsed as JSON using RapidJSON
		bool jsonParsedCons{false};			// content has been parsed as JSON using jsoncons
		Parsing::XML parsedXML;				// content parsed as HTML/XML
		rapidjson::Document parsedJsonRapid; // content parsed as JSON (using RapidJSON)
		jsoncons::json parsedJsonCons;		// content parsed as JSON (using jsoncons)
		std::string xmlParsingError;		// error while parsing content as HTML/XML
		std::string jsonParsingError;		// error while parsing content as JSON

		// subset properties and parsing
		std::uint8_t subSetType{QueryStruct::typeNone}; // type of subsets
		std::size_t subSetNumber{0};		// number of subsets
		std::size_t subSetCurrent{0};		// current subset (index + 1)
		bool subSetXmlParsed{false};		// current subset has been parsed as HTML/XML
		bool subSetJsonParsedRapid{false};	// current subset has been parsed as JSON using RapidJSON
		bool subSetJsonParsedCons{false};	// current subset has been parsed as JSON using jsoncons
		Parsing::XML subSetParsedXML;		// current subset parsed as HTML/XML
		rapidjson::Document subSetParsedJsonRapid; // current subset parsed as JSON (using RapidJSON)
		jsoncons::json subSetParsedJsonCons; // current subset parsed as JSON (using jsoncons)
		std::string subSetXmlParsingError;	// error while parsing current subset as HTML/XML
		std::string subSetJsonParsingError;	// error while parsing current subset as JSON

		// subset data
		std::vector<Parsing::XML> xPathSubSets;
		std::vector<rapidjson::Document> jsonPointerSubSets;
		std::vector<jsoncons::json> jsonPathSubSets;

		std::vector<std::string> stringifiedSubSets;

		// internal helper functions
		bool parseXml(std::queue<std::string>& warningsTo);
		bool parseJsonRapid(std::queue<std::string>& warningsTo);
		bool parseJsonCons(std::queue<std::string>& warningsTo);
		bool parseSubSetXml(std::queue<std::string>& warningsTo);
		bool parseSubSetJsonRapid(std::queue<std::string>& warningsTo);
		bool parseSubSetJsonCons(std::queue<std::string>& warningsTo);
		void resetSubSetParsingState();
		void clearSubSets();

		void stringifySubSets(std::queue<std::string>& warningsTo);
		void insertSubSets(std::vector<std::string>& subsets);
		void insertSubSets(std::vector<Parsing::XML>& subsets);
		void insertSubSets(std::vector<jsoncons::json>& subsets);
		void insertSubSets(std::vector<rapidjson::Document>& subsets);
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * PUBLIC GETTER
	 */

	//! Gets whether a certain query is used by the container.
	/*!
	 * \b Thread-safe. This function can be used by any thread.
	 *
	 * \param queryId ID of the query to be checked.
	 */
	inline bool Container::isQueryUsed(std::uint64_t queryId) const {
		std::lock_guard<std::mutex> idLock(this->queriesIdLock);

		return std::find(
				this->queriesId.cbegin(),
				this->queriesId.cend(),
				queryId
		) != this->queriesId.cend();
	}

	/*
	 * SETTERS (proctected)
	 */

	//! Sets whether to try to repair CData when parsing XML.
	/*!
	 * \param isRepairCData Set whether to try
	 *   to repair CData when parsing XML.
	 */
	inline void Container::setRepairCData(bool isRepairCData) {
		this->repairCData = isRepairCData;
	}

	//! Sets whether to try to repair broken HTML/XML comments.
	/*!
	 * \param isRepairComments Set whether to try
	 *   to repair broken HTML/XML comments.
	 */
	inline void Container::setRepairComments(bool isRepairComments) {
		this->repairComments = isRepairComments;
	}

	//! Sets whether to remove XML processing instructions (@c <?xml:...>) before parsing HTML/XML content.
	/*!
	 * \param isRemoveXmlInstructions Sets whether
	 *   to remove XML processing instructions.
	 */
	inline void Container::setRemoveXmlInstructions(bool isRemoveXmlInstructions) {
		this->removeXmlInstructions = isRemoveXmlInstructions;
	}

	//! Sets whether to minimize memory usage.
	/*!
	 * \note Setting memory minimization to true
	 *   might negatively affect performance.
	 *
	 * \param isMinimizeMemory Set whether to
	 *   minimize memory usage, prioritizing
	 *   memory usage over performance.
	 */
	inline void Container::setMinimizeMemory(bool isMinimizeMemory) {
		this->minimizeMemory = isMinimizeMemory;
	}

	//! Sets how @c tidy-html5 reports errors and warnings.
	/*!
	 * The reporting of both errors and
	 *  warnings is deactivated by default.
	 *
	 * For more information about tidy-html5,
	 *  see its
	 *  <a href="https://github.com/htacg/tidy-html5">
	 *  GitHub repository</a>.
	 *
	 * \param warnings Specify whether to
	 *   report simple warnings.
	 *
	 * \param numOfErrors Set the number of
	 *   errors to be reported. Set to zero
	 *   to deactivate error reporting.
	 */
	inline void Container::setTidyErrorsAndWarnings(bool warnings, std::uint32_t numOfErrors) {
		this->parsedXML.setOptions(warnings, numOfErrors);
		this->subSetParsedXML.setOptions(warnings, numOfErrors);
	}

	//! Sets the content to use the managed queries on.
	/*!
	 * The old query target referencing
	 *  the old content will be cleared.
	 *
	 * \warning Pointers to the strings
	 *   will be saved in-class. Make
	 *   sure the strings remain valid
	 *   as long as they are used!
	 *
	 * \param content Constant reference
	 *   to a string containing the content
	 *   to use the managed queries on.
	 * \param source Constant reference
	 *   to a string containing the source
	 *   (URL) of the content. It will be
	 *   used for logging and error reporting
	 *   purposes only.
	 *
	 */
	inline void Container::setQueryTarget(
			const std::string& content,
			const std::string& source
	) {
		// clear old target
		this->clearQueryTarget();

		// set new target
		this->queryTargetPtr = &content;
		this->queryTargetSourcePtr = &source;
	}

	/*
	 * GETTERS (protected)
	 */

	//! Gets the number of subsets currently acquired.
	/*!
	 * \returns The number of subsets generated
	 *   by the last query that generated subsets
	 *   as its result.
	 */
	inline std::size_t Container::getNumberOfSubSets() const {
		return this->subSetNumber;
	}

	//! Gets the current query target, if available, and writes it to the given string.
	/*!
	 * \param targetTo Reference to a string
	 *   the query target will be written to,
	 *   if one is available. Its content will
	 *   not be changed if no query target is
	 *   available.
	 *
	 * \returns True, if a query target was
	 *   available and has been written to
	 *   the referenced string. Returns false,
	 *   if no query target was available and
	 *   the referenced string has not been
	 *   changed.
	 */
	inline bool Container::getTarget(std::string& targetTo) {
		if(this->queryTargetPtr != nullptr) {
			targetTo = *(this->queryTargetPtr);

			return true;
		}

		return true;
	}

	//! Parses the current query target as tidied XML and writes it to the given string.
	/*!
	 * \param resultTo Reference to a
	 *   string the parsed query target
	 *   will be written to.
	 * \param warningsTo Reference to a
	 *   vector of strings to which
	 *   warnings that occured during
	 *   parsing will be appended.
	 *
	 * \returns True, if the parsing was
	 *   successful and the tidied XML was
	 *   written to the given string.
	 *   False, if the parsing was not
	 *   successful and the given string
	 *   has not been changed.
	 */
	inline bool Container::getXml(std::string& resultTo, std::queue<std::string>& warningsTo) {
		if(this->parseXml(warningsTo)) {
			this->parsedXML.getContent(resultTo);

			return true;
		}

		if(warningsTo.empty()) {
			warningsTo.emplace(
					"WARNING: [XML] "
					+ this->xmlParsingError
					+ " ["
					+ *(this->queryTargetSourcePtr)
					+ "]"
			);
		}

		return false;
	}

	/*
	 * QUERIES (protected)
	 */

	//! Adds a query with the given query properties to the container.
	/*!
	 * \param properties Constant reference to
	 *   the properties of the query to add to
	 *   the container.
	 *
	 * \returns A structure to be used to identify
	 *   the added query, including the index of
	 *   the query inside the container.
	 *
	 * \throws Container::Exception if an error
	 *   occured while creating a query with
	 *   the given properties or the specified
	 *   type of the query is unknown.
	 *
	 */
	inline Struct::QueryStruct Container::addQuery(
			std::uint64_t id,
			const QueryProperties& properties
	) {
		QueryStruct newQuery;

		newQuery.resultBool = properties.resultBool;
		newQuery.resultSingle = properties.resultSingle;
		newQuery.resultMulti = properties.resultMulti;
		newQuery.resultSubSets = properties.resultSubSets;

		if(!properties.text.empty()) {
			if(properties.type == "regex") {
				// add RegEx query
				newQuery.index = this->queriesRegEx.size();

				try {
					this->queriesRegEx.emplace_back(
							properties.text,
							properties.resultBool
							|| properties.resultSingle,
							properties.resultMulti
					);
				}
				catch(const RegExException& e) {
					throw Exception(
							"[RegEx] "
							+ std::string(e.view())
					);
				}

				newQuery.type = QueryStruct::typeRegEx;

			}
			else if(properties.type == "xpath") {
				// add XPath query
				newQuery.index = this->queriesXPath.size();

				try {
					this->queriesXPath.emplace_back(
							properties.text,
							properties.textOnly
					);
				}
				catch(const XPathException& e) {
					throw Exception(
							"[XPath] "
							+ std::string(e.view())
					);
				}

				newQuery.type = QueryStruct::typeXPath;
			}
			else if(properties.type == "jsonpointer") {
				// add JSONPointer query
				newQuery.index = this->queriesJsonPointer.size();

				try {
					this->queriesJsonPointer.emplace_back(
							properties.text,
							properties.textOnly
					);
				}
				catch(const JsonPointerException &e) {
					throw Exception(
							"[JSONPointer] "
							+ std::string(e.view())
					);
				}

				newQuery.type = QueryStruct::typeJsonPointer;
			}
			else if(properties.type == "jsonpath") {
				// add JSONPath query
				newQuery.index = this->queriesJsonPath.size();

				try {
					this->queriesJsonPath.emplace_back(
							properties.text,
							properties.textOnly
					);
				}
				catch(const JsonPathException &e) {
					throw Exception(
							"[JSONPath] "
							+ std::string(e.view())
					);
				}

				newQuery.type = QueryStruct::typeJsonPath;
			}
			else if(properties.type == "xpathjsonpointer") {
				// add combined XPath and JSONPointer query
				newQuery.index = this->queriesXPathJsonPointer.size();

				// split XPath query (first line) from JSON query
				const auto splitPos{properties.text.find('\n')};
				const std::string xPathQuery(
						properties.text,
						0,
						splitPos
				);
				std::string jsonQuery;

				if(
						splitPos != std::string::npos
						&& properties.text.size() > splitPos + 1
				) {
					jsonQuery = properties.text.substr(splitPos + 1);
				}

				try {
					this->queriesXPathJsonPointer.emplace_back(
							XPath(xPathQuery, true),
							JsonPointer(jsonQuery, properties.textOnly)
					);
				}
				catch(const XPathException& e) {
					throw Exception(
							"[XPath] "
							+ std::string(e.view())
					);
				}
				catch(const JsonPointerException &e) {
					throw Exception(
							"[JSONPointer] "
							+ std::string(e.view())
					);
				}

				newQuery.type = QueryStruct::typeXPathJsonPointer;

			}
			else if(properties.type == "xpathjsonpath") {
				// add combined XPath and JSONPath query
				newQuery.index = this->queriesXPathJsonPath.size();

				// split XPath query (first line) from JSON query
				const auto splitPos{properties.text.find('\n')};
				const std::string xPathQuery(
						properties.text,
						0,
						splitPos
				);
				std::string jsonQuery;

				if(
						splitPos != std::string::npos
						&& properties.text.size() > splitPos + 1
				) {
					jsonQuery = properties.text.substr(splitPos + 1);
				}

				try {
					this->queriesXPathJsonPath.emplace_back(
							XPath(xPathQuery, true),
							JsonPath(jsonQuery, properties.textOnly)
					);
				}
				catch(const XPathException& e) {
					throw Exception(
							"[XPath] "
							+ std::string(e.view())
					);
				}
				catch(const JsonPathException &e) {
					throw Exception(
							"[JSONPath] "
							+ std::string(e.view())
					);
				}

				newQuery.type = QueryStruct::typeXPathJsonPath;
			}
			else {
				throw Exception(
					"Query::Container::addQuery(): Unknown query type '"
					+ properties.type
					+ "'"
				);
			}
		}

		// thread-safely add ID
		if(id > 0) {
			std::lock_guard<std::mutex> lockIds(this->queriesIdLock);

			this->queriesId.emplace_back(id);
		}

		return newQuery;
	}

	//! Clears all queries currently managed by the container and frees the associated memory.
	inline void Container::clearQueries() {
		std::vector<XPath>().swap(this->queriesXPath);
		std::vector<RegEx>().swap(this->queriesRegEx);
		std::vector<JsonPointer>().swap(this->queriesJsonPointer);
		std::vector<JsonPath>().swap(this->queriesJsonPath);
		std::vector<XPathJsonPointer>().swap(this->queriesXPathJsonPointer);
		std::vector<XPathJsonPath>().swap(this->queriesXPathJsonPath);
	}

	//! Clears the current query target and frees the associated memory.
	inline void Container::clearQueryTarget() {
		// clear old subsets
		this->clearSubSets();

		// clear parsed content
		this->parsedXML.clear();

		jsoncons::json().swap(this->parsedJsonCons);

		rapidjson::Value(
				rapidjson::kObjectType
		).Swap(this->parsedJsonRapid);

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

	/*
	 * SUBSETS (protected)
	 */

	//! Requests the next subset for all subsequent queries.
	/*!
	 * \returns True, if another subset
	 *   existed that will be used by
	 *   subsequent queries. False, if no
	 *   more subsets exist.
	 *
	 * \throws Container::Exception if an
	 *   invalid subset had previously
	 *   been selected.
	 */
	inline bool Container::nextSubSet() {
		// check subsets
		if(this->subSetNumber < this->subSetCurrent) {
			throw Exception(
					"Query::Container::nextSubSet():"
					" Invalid subset selected"
			);
		}

		if(this->subSetNumber == this->subSetCurrent) {
			return false;
		}

		// clear previous subset
		if(this->subSetCurrent > 0) {
			if(!(this->stringifiedSubSets.empty())) {
				std::string().swap(
						this->stringifiedSubSets.at(
								this->subSetCurrent - 1
						)
				);
			}

			switch(this->subSetType) {
			case QueryStruct::typeXPath:
				this->xPathSubSets.at(
						this->subSetCurrent - 1
				).clear();

				break;

			case QueryStruct::typeJsonPointer:
				rapidjson::Value(rapidjson::kObjectType).Swap(
						this->jsonPointerSubSets.at(
								this->subSetCurrent - 1
						)
				);

				break;

			case QueryStruct::typeJsonPath:
				jsoncons::json().swap(
						this->jsonPathSubSets.at(
								this->subSetCurrent - 1
						)
				);

				break;

			default:
				break;
			}
		}

		// increment index (+ 1) of current subset
		++(this->subSetCurrent);

		return true;
	}

	/*
	 * RESULTS (protected)
	 */

	//! Gets a boolean result from a %RegEx query on a separate string.
	/*!
	 * \param query A constant reference to a
	 *   structure identifying the %RegEx query
	 *   that will be performed.
	 * \param target A constant reference to
	 *   a string containing the target on which
	 *   the query will be performed.
	 * \param resultTo A reference to a boolean
	 *   variable which will be set according
	 *   to the result of the query.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the query is of a different type
	 *   or its execution failed.
	 */
	inline bool Container::getBoolFromRegEx(
			const QueryStruct& query,
			const std::string& target,
			bool& resultTo,
			std::queue<std::string>& warningsTo
	) const {
		// check query type
		if(query.type != QueryStruct::typeRegEx) {
			if(query.type != QueryStruct::typeNone) {
				warningsTo.emplace(
					"WARNING: RegEx query is of invalid type - not RegEx."
				);
			}
		}
		// check result type
		else if(query.type != QueryStruct::typeNone && !query.resultBool) {
			warningsTo.emplace(
				"WARNING: RegEx query has invalid result type - not boolean."
			);
		}
		// check query target
		else if(target.empty()) {
			resultTo = false;

			return true;
		}
		else {
			// get boolean result from a RegEx query
			try {
				resultTo = this->queriesRegEx.at(query.index).getBool(
						target
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
						+ " ["
						+ target
						+ "]."
				);
			}
		}

		return false;
	}

	//! Gets a single result from a %RegEx query on a separate string.
	/*!
	 * \param query A constant reference to
	 *   a structure identifying the %RegEx query
	 *   that will be performed.
	 * \param target A constant reference to
	 *   a string containing the target on which
	 *   the query will be performed.
	 * \param resultTo A reference to a string
	 *   to which the result of the query
	 *   will be written.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the query is of a different type
	 *   or its execution failed.
	 */
	inline bool Container::getSingleFromRegEx(
			const QueryStruct& query,
			const std::string& target,
			std::string& resultTo,
			std::queue<std::string>& warningsTo
	) const {
		// check query type
		if(query.type != QueryStruct::typeRegEx) {
			if(query.type != QueryStruct::typeNone) {
				warningsTo.emplace(
					"WARNING: RegEx query is of invalid type - not RegEx."
				);
			}
		}
		// check result type
		else if(query.type != QueryStruct::typeNone && !query.resultSingle) {
			warningsTo.emplace(
				"WARNING: RegEx query has invalid result type - not single."
			);
		}
		// check query target
		else if(target.empty()) {
			resultTo.clear();

			return true;
		}
		else {
			// get single result from a RegEx query
			try {
				this->queriesRegEx.at(query.index).getFirst(
						target,
						resultTo
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
						+ " ["
						+ target
						+ "]."
				);
			}
		}

		return false;
	}

	//! Gets multiple results from a %RegEx query on a separate string.
	/*!
	 * \param query A constant reference to a
	 *   structure identifying the %RegEx query
	 *   that will be performed.
	 * \param target A constant reference to
	 *   a string containing the target on which
	 *   the query will be performed.
	 * \param resultTo A reference to a vector
	 *   to which the results of the query
	 *   will be appended.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the query is of a different type
	 *   or its execution failed.
	 */
	inline bool Container::getMultiFromRegEx(
			const QueryStruct& query,
			const std::string& target,
			std::vector<std::string>& resultTo,
			std::queue<std::string>& warningsTo
	) const {
		// check query type
		if(query.type != QueryStruct::typeRegEx) {
			if(query.type != QueryStruct::typeNone) {
				warningsTo.emplace(
					"WARNING: RegEx query is of invalid type - not RegEx."
				);
			}
		}
		// check result type
		else if(query.type != QueryStruct::typeNone && !query.resultMulti) {
			warningsTo.emplace(
				"WARNING: RegEx query has invalid result type - not multi."
			);
		}
		// check query target
		else if(target.empty()) {
			resultTo.clear();

			return true;
		}
		else {
			// get multiple results from a RegEx query
			try {
				this->queriesRegEx.at(query.index).getAll(
						target,
						resultTo
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
						+ " ["
						+ target
						+ "]."
				);
			}
		}

		return false;
	}

	//! Gets a boolean result from a query of any type on the current query target.
	/*!
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   that will be performed.
	 * \param resultTo A reference to a boolean
	 *   variable which will be set according
	 *   to the result of the query.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the execution of the query failed.
	 *
	 * \throws Container::Exception if no query
	 *   target has been specified or the query
	 *   is of an unknown type.
	 */
	inline bool Container::getBoolFromQuery(
			const QueryStruct& query,
			bool& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointers
		if(this->queryTargetPtr == nullptr) {
			throw Exception(
					"Query::Container::getBoolFromQuery():"
					" No content specified"
			);
		}

		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::getBoolFromQuery():"
					" No content source specified"
			);
		}

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
				resultTo = this->queriesRegEx.at(query.index).getBool(
						*(this->queryTargetPtr)
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
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
					resultTo = this->queriesXPath.at(query.index).getBool(
							this->parsedXML
					);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
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
					resultTo = this->queriesJsonPointer.at(query.index).getBool(
							this->parsedJsonRapid
					);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
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
					resultTo = this->queriesJsonPath.at(query.index).getBool(
							this->parsedJsonCons
					);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					this->queriesXPathJsonPointer.at(query.index).first.getFirst(
							this->parsedXML,
							json
					);

					if(json.empty()) {
						resultTo = false;
					}
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get boolean result from the JSONPointer query
						resultTo = this->queriesXPathJsonPointer.at(query.index).second.getBool(
								parsedJson
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					this->queriesXPathJsonPath.at(query.index).first.getFirst(
							this->parsedXML,
							json
					);

					if(json.empty()) {
						resultTo = false;
					}
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get boolean result from the JSONPath query
						resultTo = this->queriesXPathJsonPath.at(query.index).second.getBool(
								parsedJson
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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
			throw Exception(
					"Query::Container::getBoolFromQuery():"
					" Unknown query type"
			);
		}

		return false;
	}

	//! Gets a boolean result from a query of any type on the current subset.
	/*!
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   that will be performed.
	 * \param resultTo A reference to a boolean
	 *   variable which will be set according
	 *   to the result of the query.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the execution of the query failed.
	 *
	 * \throws Container::Exception if no query
	 *   target or no subset has been specified,
	 *   the current subset is invalid, or the
	 *   given query is of an unknown type.
	 */
	inline bool Container::getBoolFromQueryOnSubSet(
			const QueryStruct& query,
			bool& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointer
		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::getBoolFromQueryOnSubSet():"
					" No content source has been specified"
			);
		}

		// check current subset
		if(this->subSetCurrent == 0) {
			throw Exception(
					"Query::Container::getBoolFromQueryOnSubSet():"
					" No subset has been specified"
			);
		}

		if(this->subSetCurrent > this->subSetNumber) {
			throw Exception(
					"Query::Container::getBoolFromQueryOnSubSet():"
					" Invalid subset has been specified"
			);
		}

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
				if(this->subSetType != QueryStruct::typeRegEx) {
					this->stringifySubSets(warningsTo);
				}

				resultTo = this->queriesRegEx.at(query.index).getBool(
						this->stringifiedSubSets.at(this->subSetCurrent - 1)
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeXPath) {
						resultTo = this->queriesXPath.at(query.index).getBool(
								this->xPathSubSets.at(this->subSetCurrent - 1)
						);
					}
					else {
						resultTo = this->queriesXPath.at(query.index).getBool(
								this->subSetParsedXML
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeJsonPointer) {
						resultTo = this->queriesJsonPointer.at(query.index).getBool(
								this->jsonPointerSubSets.at(this->subSetCurrent - 1)
						);
					}
					else {
						resultTo = this->queriesJsonPointer.at(query.index).getBool(
								this->subSetParsedJsonRapid
						);
					}

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeJsonPath) {
						resultTo = this->queriesJsonPath.at(query.index).getBool(
								this->jsonPathSubSets.at(this->subSetCurrent - 1)
						);
					}
					else {
						resultTo = this->queriesJsonPath.at(query.index).getBool(
								this->subSetParsedJsonCons
						);
					}

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					}
					else {
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);
					}

					if(json.empty()) {
						resultTo = false;
					}
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get boolean result from the JSONPointer query
						resultTo = this->queriesXPathJsonPointer.at(query.index).second.getBool(
								parsedJson
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					}
					else {
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);
					}

					if(json.empty()) {
						resultTo = false;
					}
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get boolean result from the JSONPath query
						resultTo = this->queriesXPathJsonPath.at(query.index).second.getBool(
								parsedJson
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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
			throw Exception(
					"Query::Container::getBoolFromQueryOnSubSet():"
					" Unknown query type"
			);
		}

		return false;
	}

	//! Gets a single result from a query of any type on the current query target.
	/*!
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   that will be performed.
	 * \param resultTo A reference to a string
	 *   to which the result of the query
	 *   will be written.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the execution of the query failed.
	 *
	 * \throws Container::Exception if no query
	 *   target has been specified or the query
	 *   is of an unknown type.
	 */
	inline bool Container::getSingleFromQuery(
			const QueryStruct& query,
			std::string& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointers
		if(this->queryTargetPtr == nullptr) {
			throw Exception(
					"Query::Container::getSingleFromQuery():"
					" No content has been specified"
			);
		}

		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::getSingleFromQuery():"
					" No content source has been specified"
			);
		}

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
				this->queriesRegEx.at(query.index).getFirst(
						*(this->queryTargetPtr),
						resultTo
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
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
					this->queriesXPath.at(query.index).getFirst(
							this->parsedXML,
							resultTo
					);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
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
					this->queriesJsonPointer.at(query.index).getFirst(
							this->parsedJsonRapid,
							resultTo
					);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
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
					this->queriesJsonPath.at(query.index).getFirst(
							this->parsedJsonCons,
							resultTo
					);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					this->queriesXPathJsonPointer.at(query.index).first.getFirst(
							this->parsedXML,
							json
					);

					if(json.empty()) {
						resultTo = "";
					}
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get single result from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getFirst(
								parsedJson,
								resultTo
						);
					}
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					this->queriesXPathJsonPath.at(query.index).first.getFirst(
							this->parsedXML,
							json
					);

					if(json.empty()) {
						resultTo = "";

						return true;
					}

					try {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get single result from the JSONPath query
						this->queriesXPathJsonPath.at(query.index).second.getFirst(
								parsedJson,
								resultTo
						);

						return true;
					}
					catch(const JsonException& e) {
						warningsTo.emplace(
								"WARNING: JSONPath error - "
								+ std::string(e.view())
								+ " ["
								+ *(this->queryTargetSourcePtr)
								+ "]."
						);
					}
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

	//! Gets a single result from a query of any type on the current subset.
	/*!
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   that will be performed.
	 * \param resultTo A reference to a string
	 *   to which the result of the query
	 *   will be written.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the execution of the query failed.
	 *
	 * \throws Container::Exception if no query
	 *   target or no subset has been specified,
	 *   the current subset is invalid, or the
	 *   given query is of an unknown type.
	 */
	inline bool Container::getSingleFromQueryOnSubSet(
			const QueryStruct& query,
			std::string& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointer
		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::getSingleFromQueryOnSubSet():"
					" No content source has been specified"
			);
		}

		// check current subset
		if(this->subSetCurrent == 0) {
			throw Exception(
					"Query::Container::getSingleFromQueryOnSubSet():"
					" No subset has been specified"
			);
		}

		if(this->subSetCurrent > this->subSetNumber) {
			throw Exception(
					"Query::Container::getSingleFromQueryOnSubSet():"
					" Invalid subset has been specified"
			);
		}

		// check result type
		if(
				query.type != QueryStruct::typeNone
				&& !query.resultSingle
		) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not single."
			);

			return false;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get single result from a RegEx query on the current subset
			try {
				if(this->subSetType != QueryStruct::typeRegEx) {
					this->stringifySubSets(warningsTo);
				}

				this->queriesRegEx.at(query.index).getFirst(
						this->stringifiedSubSets.at(this->subSetCurrent - 1),
						resultTo
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPath.at(query.index).getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					}
					else {
						this->queriesXPath.at(query.index).getFirst(
								this->subSetParsedXML,
								resultTo
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeJsonPointer) {
						this->queriesJsonPointer.at(query.index).getFirst(
								this->jsonPointerSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					}
					else {
						this->queriesJsonPointer.at(query.index).getFirst(
								this->subSetParsedJsonRapid,
								resultTo
						);
					}

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeJsonPath) {
						this->queriesJsonPath.at(query.index).getFirst(
								this->jsonPathSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					}
					else {
						this->queriesJsonPath.at(query.index).getFirst(
								this->subSetParsedJsonCons,
								resultTo
						);
					}

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					}
					else {
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);
					}

					if(json.empty()) {
						resultTo = "";
					}
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get single result from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getFirst(
								parsedJson,
								resultTo
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					}
					else {
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);
					}

					if(json.empty()) {
						resultTo = "";
					}
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get single result from the JSONPath query
						this->queriesXPathJsonPath.at(query.index).second.getFirst(
								parsedJson,
								resultTo
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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
			throw Exception(
					"Query::Container::getSingleFromQueryOnSubSet():"
					" Unknown query type"
			);
		}

		return false;
	}

	//! Gets multiple results from a query of any type on the current query target.
	/*!
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   that will be performed.
	 * \param resultTo A reference to a vector
	 *   to which the results of the query
	 *   will be appended.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the execution of the query failed.
	 *
	 * \throws Container::Exception if no query
	 *   target has been specified or the query
	 *   is of an unknown type.
	 */
	inline bool Container::getMultiFromQuery(
			const QueryStruct& query,
			std::vector<std::string>& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointers
		if(this->queryTargetPtr == nullptr) {
			throw Exception(
					"Query::Container::getMultiFromQuery():"
					" No content has been specified"
			);
		}

		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::getMultiFromQuery():"
					" No content source has been specified"
			);
		}

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
				this->queriesRegEx.at(query.index).getAll(
						*(this->queryTargetPtr),
						resultTo
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
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
							+ std::string(e.view())
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
							+ std::string(e.view())
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
					this->queriesJsonPath.at(query.index).getAll(
							this->parsedJsonCons,
							resultTo
					);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					this->queriesXPathJsonPointer.at(query.index).first.getFirst(
							this->parsedXML,
							json
					);

					if(json.empty()) {
						resultTo.clear();
					}
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get multiple results from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getAll(
								parsedJson,
								resultTo
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					this->queriesXPathJsonPath.at(query.index).first.getFirst(
							this->parsedXML,
							json
					);

					if(json.empty()) {
						resultTo.clear();
					}
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get multiple results from the JSONPath query
						this->queriesXPathJsonPath.at(query.index).second.getAll(
								parsedJson,
								resultTo
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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
			throw Exception(
					"Query::Container::getMultiFromQuery():"
					" Unknown query type"
			);
		}

		return false;
	}

	//! Gets multiple results from a query of any type on the current subset.
	/*!
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   that will be performed.
	 * \param resultTo A reference to a vector
	 *   to which the results of the query
	 *   will be appended.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the execution of the query failed.
	 *
	 * \throws Container::Exception if no query
	 *   target or no subset has been specified,
	 *   the current subset is invalid, or the
	 *   given query is of an unknown type.
	 */
	inline bool Container::getMultiFromQueryOnSubSet(
			const QueryStruct& query,
			std::vector<std::string>& resultTo,
			std::queue<std::string>& warningsTo
	) {
		// check pointer
		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::getMultiFromQueryOnSubSet():"
					" No content source has been specified"
			);
		}

		// check current subset
		if(this->subSetCurrent == 0) {
			throw Exception(
					"Query::Container::getMultiFromQueryOnSubSet():"
					" No subset has been specified"
			);
		}

		if(this->subSetCurrent > this->subSetNumber) {
			throw Exception(
					"Query::Container::getMultiFromQueryOnSubSet():"
					" Invalid subset has been specified"
			);
		}

		// check result type
		if(
				query.type != QueryStruct::typeNone
				&& !query.resultMulti
		) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not multi."
			);

			return false;
		}

		switch(query.type) {
		case QueryStruct::typeRegEx:
			// get multiple result from a RegEx query on the current subset
			try {
				if(this->subSetType != QueryStruct::typeRegEx) {
					this->stringifySubSets(warningsTo);
				}

				this->queriesRegEx.at(query.index).getAll(
						this->stringifiedSubSets.at(this->subSetCurrent - 1),
						resultTo
				);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPath.at(query.index).getAll(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					}
					else {
						this->queriesXPath.at(query.index).getAll(
								this->subSetParsedXML,
								resultTo
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeJsonPointer) {
						this->queriesJsonPointer.at(query.index).getAll(
								this->jsonPointerSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					}
					else {
						this->queriesJsonPointer.at(query.index).getAll(
								this->subSetParsedJsonRapid,
								resultTo
						);
					}

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
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
					if(this->subSetType == QueryStruct::typeJsonPath) {
						this->queriesJsonPath.at(query.index).getAll(
								this->jsonPathSubSets.at(this->subSetCurrent - 1),
								resultTo
						);
					}
					else {
						this->queriesJsonPath.at(query.index).getAll(
								this->subSetParsedJsonCons,
								resultTo
						);
					}

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					}
					else {
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);
					}

					if(json.empty()) {
						resultTo.clear();
					}
					else {
						// temporarily parse JSON using rapidJSON
						const auto parsedJson(Helper::Json::parseRapid(json));

						// get multiple results from the JSONPointer query
						this->queriesXPathJsonPointer.at(query.index).second.getAll(
								parsedJson,
								resultTo
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					}
					else {
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);
					}

					if(json.empty()) {
						resultTo.clear();
					}
					else {
						// temporarily parse JSON using jsoncons
						const auto parsedJson(Helper::Json::parseCons(json));

						// get multiple results from the JSONPath query
						this->queriesXPathJsonPath.at(query.index).second.getAll(
								parsedJson,
								resultTo
						);
					}

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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
			throw Exception(
					"Query::Container::getMultiFromQueryOnSubSet():"
					" Unknown query type"
			);
		}

		return false;
	}

	//! Sets subsets for subsequent queries using a query of any type.
	/*!
	 * The subsets resulting from the query
	 *  will be saved in-class. Previous
	 *  subsets will be overwritten.
	 *
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   that will be performed to acquire
	 *   the subset.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if the query was successful.
	 *   False, if the execution of the query failed.
	 *
	 * \throws Container::Exception if no query
	 *   target has been specified or the query
	 *   is of an unknown type.
	 */
	inline bool Container::setSubSetsFromQuery(
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

		default:
			break;
		}

		// check pointers
		if(this->queryTargetPtr == nullptr) {
			throw Exception(
					"Query::Container::setSubSetsFromQuery():"
					" No content has been specified"
			);
		}

		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::setSubSetsFromQuery():"
					" No content source has been specified"
			);
		}

		// check result type
		if(
				query.type != QueryStruct::typeNone
				&& !query.resultSubSets
		) {
			warningsTo.emplace(
				"WARNING: Query has invalid result type - not subsets."
			);

			return false;
		}

		// check query target
		if(this->queryTargetPtr->empty()) {
			return true;
		}

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
						+ std::string(e.view())
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
							+ std::string(e.view())
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
							+ std::string(e.view())
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
							+ std::string(e.view())
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

					this->queriesXPathJsonPointer.at(query.index).first.getFirst(
							this->parsedXML,
							json
					);

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
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					this->queriesXPathJsonPath.at(query.index).first.getFirst(
							this->parsedXML,
							json
					);

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
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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
			throw Exception(
					"Query::Container::setSubSetsFromQuery():"
					" Unknown query type"
			);
		}

		return false;
	}

	//! Inserts more subsets after the current one based on a query on the current subset.
	/*!
	 * This function is used for recursive extracting.
	 *
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   that will be performed to acquire
	 *   the subset.
	 * \param warningsTo A reference to a vector
	 *   of strings to which all warnings will
	 *   be appended that occur during the
	 *   execution of the query.
	 *
	 * \returns True, if new subsets have been
	 *   added.  False, if the execution of the
	 *   query failed or did not see any results.
	 *
	 * \throws Container::Exception if no query
	 *   target or no subset has been specified,
	 *   the current subset is invalid, or the
	 *   given query is of an unknown type.
	 */
	inline bool Container::addSubSetsFromQueryOnSubSet(
			const QueryStruct& query,
			std::queue<std::string>& warningsTo
	) {
		// check pointer
		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::addSubSetsFromQueryOnSubSet():"
					" No content source has been specified"
			);
		}

		// check current subset
		if(this->subSetCurrent == 0) {
			throw Exception(
					"Query::Container::addSubSetsFromQueryOnSubSet():"
					" No subset has been specified"
			);
		}

		if(this->subSetCurrent > this->subSetNumber) {
			throw Exception(
					"Query::Container::addSubSetsFromQueryOnSubSet():"
					" Invalid subset has been specified"
			);
		}

		// check result type
		if(
				query.type != QueryStruct::typeNone
				&& !query.resultSubSets
		) {
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
				if(this->subSetType != QueryStruct::typeRegEx) {
					this->stringifySubSets(warningsTo);
				}

				// get new subsets from RegEx query
				std::vector<std::string> subsets;

				this->queriesRegEx.at(query.index).getAll(
						this->stringifiedSubSets.at(this->subSetCurrent - 1),
						subsets
				);

				// check number of results
				if(subsets.empty()) {
					return false;
				}

				// insert new RegEx subsets
				this->insertSubSets(subsets);

				return true;
			}
			catch(const RegExException& e) {
				warningsTo.emplace(
						"WARNING: RegEx error - "
						+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPath.at(query.index).getSubSets(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								subsets
						);
					}
					else {
						this->queriesXPath.at(query.index).getSubSets(
								this->subSetParsedXML,
								subsets
						);
					}

					// check number of results
					if(subsets.empty()) {
						return false;
					}

					// insert new XPath subsets
					this->insertSubSets(subsets);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeJsonPointer) {
						this->queriesJsonPointer.at(query.index).getSubSets(
								this->jsonPointerSubSets.at(this->subSetCurrent - 1),
								subsets
						);
					}
					else {
						this->queriesJsonPointer.at(query.index).getSubSets(
								this->subSetParsedJsonRapid,
								subsets
						);
					}

					// check number of results
					if(subsets.empty()) {
						return false;
					}

					// insert new JSONPointer subsets
					this->insertSubSets(subsets);

					return true;
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeJsonPath) {
						this->queriesJsonPath.at(query.index).getSubSets(
								this->jsonPathSubSets.at(this->subSetCurrent - 1),
								subsets
						);
					}
					else {
						this->queriesJsonPath.at(query.index).getSubSets(
								this->subSetParsedJsonCons,
								subsets
						);
					}

					// check number of results
					if(subsets.empty()) {
						return false;
					}

					// insert new JSONPath subsets
					this->insertSubSets(subsets);

					return true;
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					}
					else {
						this->queriesXPathJsonPointer.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);
					}

					if(json.empty()) {
						return false;
					}

					// temporarily parse JSON using rapidJSON
					const auto parsedJson(Helper::Json::parseRapid(json));

					// get more subsets from the JSONPointer query
					std::vector<rapidjson::Document> subsets;

					this->queriesXPathJsonPointer.at(query.index).second.getSubSets(parsedJson, subsets);

					// stringify old subsets if necessary (and if it was not already done for HTML/XML parsing)
					if(this->subSetType == QueryStruct::typeXPath) {
						this->stringifySubSets(warningsTo);
					}

					// check number of results
					if(subsets.empty()) {
						return false;
					}

					// insert new JSONPointer subsets
					this->insertSubSets(subsets);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPointerException& e) {
					warningsTo.emplace(
							"WARNING: JSONPointer error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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

					if(this->subSetType == QueryStruct::typeXPath) {
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->xPathSubSets.at(this->subSetCurrent - 1),
								json
						);
					}
					else {
						this->queriesXPathJsonPath.at(query.index).first.getFirst(
								this->subSetParsedXML,
								json
						);
					}

					if(json.empty()) {
						return true;
					}

					// temporarily parse JSON using jsoncons
					const auto parsedJson(Helper::Json::parseCons(json));

					// get more subsets from JSONPath query
					std::vector<jsoncons::json> subsets;

					// get multiple results from the JSONPath query
					this->queriesXPathJsonPath.at(query.index).second.getSubSets(parsedJson, subsets);

					// stringify old subsets if necessary (and if it was not already done for HTML/XML parsing)
					if(this->subSetType == QueryStruct::typeXPath) {
						this->stringifySubSets(warningsTo);
					}

					// check number of results
					if(subsets.empty()) {
						return false;
					}

					// insert new JSONPath subsets
					this->insertSubSets(subsets);

					return true;
				}
				catch(const XPathException& e) {
					warningsTo.emplace(
							"WARNING: XPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonPathException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
							+ " ["
							+ *(this->queryTargetSourcePtr)
							+ "]."
					);
				}
				catch(const JsonException& e) {
					warningsTo.emplace(
							"WARNING: JSONPath error - "
							+ std::string(e.view())
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
			throw Exception(
					"Query::Container::addSubSetsFromQueryOnSubSet():"
					" Unknown query type"
			);
		}

		return false;
	}

	/*
	 * MEMORY (protected)
	 */

	//! Reserves memory for a specific number of subsets.
	/*!
	 * \param query A constant reference to
	 *   a structure identifying the query
	 *   for whose type memory will be
	 *   specifically reserved.
	 * \param n The number of subsets for
	 *   which memory will be reserved.
	 */
	inline void Container::reserveForSubSets(const QueryStruct& query, std::size_t n) {
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

			break;

		default:
			break;
		}
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// parse content as HTML/XML if still necessary,
	//  return false if parsing failed, throws Container::Exception
	inline bool Container::parseXml(std::queue<std::string>& warningsTo) {
		// check pointers
		if(this->queryTargetPtr == nullptr) {
			throw Exception(
					"Query::Container::parseXml():"
					" No content has been specified"
			);
		}

		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::parseXml():"
					" No content source has been specified"
			);
		}

		if(
				!(this->xmlParsed)
				&& this->xmlParsingError.empty()
		) {
			try {
				this->parsedXML.parse(
						*(this->queryTargetPtr),
						this->repairCData,
						this->repairComments,
						this->removeXmlInstructions,
						warningsTo
				);

				this->xmlParsed = true;
			}
			catch(const XMLException& e) {
				this->xmlParsingError = e.view();

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

	// parse content as JSON using RapidJSON if still necessary,
	//  return false if parsing failed, throws Container::Exception
	inline bool Container::parseJsonRapid(std::queue<std::string>& warningsTo) {
		// check pointers
		if(this->queryTargetPtr == nullptr) {
			throw Exception(
					"Query::Container::parseJsonRapid():"
					" No content has been specified"
			);
		}

		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::parseJsonRapid():"
					" No content source has been specified"
			);
		}

		if(
				!(this->jsonParsedRapid)
				&& this->jsonParsingError.empty()
		) {
			try {
				this->parsedJsonRapid = Helper::Json::parseRapid(
						*(this->queryTargetPtr)
				);

				this->jsonParsedRapid = true;
			}
			catch(const JsonException& e) {
				this->jsonParsingError = e.view();

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

	// parse content as JSON using jsoncons if still necessary,
	//  return false if parsing failed, throws Container::Exception
	inline bool Container::parseJsonCons(std::queue<std::string>& warningsTo) {
		// check pointers
		if(this->queryTargetPtr == nullptr) {
			throw Exception(
					"Query::Container::parseJsonCons():"
					" No content has been specified"
			);
		}

		if(this->queryTargetSourcePtr == nullptr) {
			throw Exception(
					"Query::Container::parseJsonCons():"
					" No content source has been specified"
			);
		}

		if(
				!(this->jsonParsedCons)
				&& this->jsonParsingError.empty()
		) {
			try {
				this->parsedJsonCons = Helper::Json::parseCons(
						*(this->queryTargetPtr)
				);

				this->jsonParsedCons = true;
			}
			catch(const JsonException& e) {
				this->jsonParsingError = e.view();

				warningsTo.emplace("WARNING: [JSON] " + this->jsonParsingError);
			}
		}

		return this->jsonParsedCons;
	}

	// parse subset as HTML/XML if still necessary,
	//  return false if parsing failed, throws Container::Exception
	inline bool Container::parseSubSetXml(std::queue<std::string>& warningsTo) {
		// check current subset
		if(this->subSetCurrent == 0) {
			throw Exception(
					"Query::Container::parseSubSetXml():"
					" No subset has been specified"
			);
		}

		if(this->subSetCurrent > this->subSetNumber) {
			throw Exception(
					"Query::Container::parseSubSetXml():"
					" Invalid subset has been specified"
			);
		}

		// if the subset is of type XPath, no further parsing is required
		if(this->subSetType == QueryStruct::typeXPath) {
			return this->xPathSubSets.at(this->subSetCurrent - 1).valid();
		}

		if(
				!(this->subSetXmlParsed)
				&& this->subSetXmlParsingError.empty()
		) {
			// stringify the subsets if still necessary
			this->stringifySubSets(warningsTo);

			try {
				this->subSetParsedXML.parse(
						this->stringifiedSubSets.at(this->subSetCurrent - 1),
						this->repairCData,
						this->repairComments,
						this->removeXmlInstructions,
						warningsTo
				);

				this->subSetXmlParsed = true;
			}
			catch(const XMLException& e) {
				this->subSetXmlParsingError = e.view();

				warningsTo.emplace("WARNING: [XML] " + this->subSetXmlParsingError);
			}
		}

		return this->subSetXmlParsed;
	}

	// parse subset as JSON using RapidJSON if still necessary,
	//  return false if parsing failed, throws Container::Exception
	inline bool Container::parseSubSetJsonRapid(std::queue<std::string>& warningsTo) {
		// check current subset
		if(this->subSetCurrent == 0) {
			throw Exception(
					"Query::Container::parseSubSetJsonRapid():"
					" No subset has been specified"
			);
		}

		if(this->subSetCurrent > this->subSetNumber) {
			throw Exception(
					"Query::Container::parseSubSetJsonRapid():"
					" Invalid subset has been specified"
			);
		}

		// if the subset is of type JSONPointer, no further parsing is required
		if(this->subSetType == QueryStruct::typeJsonPointer) {
			return true;
		}

		if(
				!(this->subSetJsonParsedRapid)
				&& this->subSetJsonParsingError.empty()
		) {
			// stringify the subsets if still necessary
			this->stringifySubSets(warningsTo);

			try {
				this->subSetParsedJsonRapid = Helper::Json::parseRapid(
						this->stringifiedSubSets.at(this->subSetCurrent - 1)
				);

				this->subSetJsonParsedRapid = true;
			}
			catch(const JsonException& e) {
				this->subSetJsonParsingError = e.view();

				warningsTo.emplace("WARNING: [JSON] " + this->subSetJsonParsingError);
			}
		}

		return this->subSetJsonParsedRapid;
	}

	// parse subset as JSON using jsoncons if still necessary,
	//  return false if parsing failed, throws Container::Exception
	inline bool Container::parseSubSetJsonCons(std::queue<std::string>& warningsTo) {
		// check current subset
		if(this->subSetCurrent == 0) {
			throw Exception(
					"Query::Container::parseSubSetJsonCons():"
					" No subset has been specified"
			);
		}

		if(this->subSetCurrent > this->subSetNumber) {
			throw Exception(
					"Query::Container::parseSubSetJsonCons():"
					" Invalid subset has been specified"
			);
		}

		// if the subset is of type JSONPath, no further parsing is required
		if(this->subSetType == QueryStruct::typeJsonPath) {
			return true;
		}

		if(
				!(this->subSetJsonParsedCons)
				&& this->subSetJsonParsingError.empty()
		) {
			// stringify the subsets if still necessary
			this->stringifySubSets(warningsTo);

			try {
				this->subSetParsedJsonCons = Helper::Json::parseCons(
						this->stringifiedSubSets.at(this->subSetCurrent - 1)
				);

				this->subSetJsonParsedCons = true;
			}
			catch(const JsonException& e) {
				this->subSetJsonParsingError = e.view();

				warningsTo.emplace("WARNING: [JSON] " + this->subSetJsonParsingError);
			}
		}

		return this->subSetJsonParsedCons;
	}

	// reset parsing state for subset
	inline void Container::resetSubSetParsingState() {
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
	inline void Container::clearSubSets() {
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

			default:
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

			default:
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
	inline void Container::stringifySubSets(std::queue<std::string>& warningsTo) {
		if(!(this->stringifiedSubSets.empty())) {
			return;
		}

		switch(this->subSetType) {
		case QueryStruct::typeXPath:
			for(const auto& subset : this->xPathSubSets) {
				std::string subsetString;

				subset.getContent(subsetString);

				this->stringifiedSubSets.emplace_back(subsetString);
			}

			break;

		case QueryStruct::typeJsonPath:
			for(const auto& subset : this->jsonPathSubSets) {
				this->stringifiedSubSets.emplace_back(
						Helper::Json::stringify(subset)
				);
			}

			break;

		case QueryStruct::typeJsonPointer:
			for(const auto& subset : this->jsonPointerSubSets) {
				this->stringifiedSubSets.emplace_back(
						Helper::Json::stringify(subset)
				);
			}

			break;

		case QueryStruct::typeRegEx:
			warningsTo.emplace(
					"WARNING: RegEx subsets cannot be stringified."
			);

			break;

		case QueryStruct::typeNone:
			break;

		default:
			warningsTo.emplace(
					"WARNING: Unknown subset type"
					" in Query::Container::stringifySubSets(...)."
			);
		}
	}

	// insert RegEx subsets after the current subset
	//  NOTE:	the new subsets will be moved away from the vector;
	//			if the subset type is different, the old subsets need to be already stringified
	inline void Container::insertSubSets(std::vector<std::string>& subsets) {
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
			if(this->minimizeMemory) {
				std::vector<Parsing::XML>().swap(this->xPathSubSets);
			}
			else {
				this->xPathSubSets.clear();
			}

			break;

		case QueryStruct::typeJsonPointer:
			if(this->minimizeMemory) {
				std::vector<rapidjson::Document>().swap(this->jsonPointerSubSets);
			}
			else {
				this->jsonPointerSubSets.clear();
			}

			break;

		case QueryStruct::typeJsonPath:
			if(this->minimizeMemory) {
				std::vector<jsoncons::json>().swap(this->jsonPathSubSets);
			}
			else {
				this->jsonPathSubSets.clear();
			}

			break;

		default:
			break;
		}

		// set the subset type to RegEx (i. e. strings only)
		this->subSetType = QueryStruct::typeRegEx;
	}

	// insert XPath subsets after the current subset, stringify new subsets if needed
	//  NOTE:	the new subsets will be moved away from the vector;
	//  		if the subset type is different, the old subsets need to be already stringified
	inline void Container::insertSubSets(std::vector<Parsing::XML>& subsets) {
		// update number of subsets
		this->subSetNumber += subsets.size();

		// check subset type
		if(this->subSetType == QueryStruct::typeXPath) {
			// insert new XPath subsets
			this->xPathSubSets.insert(
					this->xPathSubSets.begin() + this->subSetCurrent,
					std::make_move_iterator(subsets.begin()),
					std::make_move_iterator(subsets.end())
			);

			// stringify new subsets if the others are also stringified
			if(!(this->stringifiedSubSets.empty())) {
				std::vector<std::string> stringified;

				stringified.reserve(subsets.size());

				for(const auto& subset : subsets) {
					std::string subsetString;

					subset.getContent(subsetString);

					stringified.emplace_back(subsetString);
				}

				this->stringifiedSubSets.insert(
						this->stringifiedSubSets.begin() + this->subSetCurrent,
						std::make_move_iterator(stringified.begin()),
						std::make_move_iterator(stringified.end())
				);
			}
		}
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
				if(this->minimizeMemory) {
					std::vector<rapidjson::Document>().swap(
							this->jsonPointerSubSets
					);
				}
				else {
					this->jsonPointerSubSets.clear();
				}

				break;

			case QueryStruct::typeJsonPath:
				if(this->minimizeMemory) {
					std::vector<jsoncons::json>().swap(
							this->jsonPathSubSets
					);
				}
				else {
					this->jsonPathSubSets.clear();
				}

				break;

			default:
				break;
			}

			// set the subset type to RegEx (i. e. strings only)
			this->subSetType = QueryStruct::typeRegEx;
		}
	}

	// insert JSONPath subsets after the current subset, stringify subsets if needed
	//  NOTE:	the new subsets will be moved away from the vector;
	//  		if the subset type is different, the old subsets need to be already stringified
	inline void Container::insertSubSets(std::vector<jsoncons::json>& subsets) {
		// update number of subsets
		this->subSetNumber += subsets.size();

		// check subset type
		if(this->subSetType == QueryStruct::typeJsonPath) {
			// insert new JSONPath subsets
			this->jsonPathSubSets.insert(
					this->jsonPathSubSets.begin() + this->subSetCurrent,
					std::make_move_iterator(subsets.begin()),
					std::make_move_iterator(subsets.end())
			);

			// stringify new subsets if the others are also stringified
			if(!(this->stringifiedSubSets.empty())) {
				std::vector<std::string> stringified;

				stringified.reserve(subsets.size());

				for(const auto& subset : subsets) {
					stringified.emplace_back(
							Helper::Json::stringify(subset)
					);
				}

				this->stringifiedSubSets.insert(
						this->stringifiedSubSets.begin() + this->subSetCurrent,
						std::make_move_iterator(stringified.begin()),
						std::make_move_iterator(stringified.end())
				);
			}
		}
		else {
			// stringify new subsets
			std::vector<std::string> stringified;

			stringified.reserve(subsets.size());

			for(const auto& subset : subsets) {
				stringified.emplace_back(
						Helper::Json::stringify(subset)
				);
			}

			// insert new (stringified) JSONPath subsets
			this->stringifiedSubSets.insert(
				this->stringifiedSubSets.begin()
				+ this->subSetCurrent,
				stringified.begin(),
				stringified.end()
			);

			// clear non-stringified subsets if neccesary
			switch(this->subSetType) {
			case QueryStruct::typeXPath:
				if(this->minimizeMemory) {
					std::vector<Parsing::XML>().swap(
							this->xPathSubSets
					);
				}
				else {
					this->xPathSubSets.clear();
				}

				break;

			case QueryStruct::typeJsonPointer:
				if(this->minimizeMemory) {
					std::vector<rapidjson::Document>().swap(
							this->jsonPointerSubSets
					);
				}
				else {
					this->jsonPointerSubSets.clear();
				}

				break;

			default:
				break;
			}

			// set the subset type to RegEx (i. e. strings only)
			this->subSetType = QueryStruct::typeRegEx;
		}
	}

	// insert JSONPointer subsets after the current subset, stringify new subsets if needed
	//  NOTE:	the new subsets will be moved away from the vector;
	//  		if the subset type is different, the old subsets need to be already stringified
	inline void Container::insertSubSets(std::vector<rapidjson::Document>& subsets) {
		// update number of subsets
		this->subSetNumber += subsets.size();

		// check subset type
		if(this->subSetType == QueryStruct::typeJsonPointer) {
			// insert new JSONPointer subsets
			this->jsonPointerSubSets.insert(
					this->jsonPointerSubSets.begin() + this->subSetCurrent,
					std::make_move_iterator(subsets.begin()),
					std::make_move_iterator(subsets.end())
			);

			// stringify new subsets if the others are also stringified
			if(!(this->stringifiedSubSets.empty())) {
				std::vector<std::string> stringified;

				stringified.reserve(subsets.size());

				for(const auto& subset : subsets) {
					stringified.emplace_back(
							Helper::Json::stringify(subset)
					);
				}

				this->stringifiedSubSets.insert(
						this->stringifiedSubSets.begin() + this->subSetCurrent,
						std::make_move_iterator(stringified.begin()),
						std::make_move_iterator(stringified.end())
				);
			}
		}
		else {
			// stringify new subsets
			std::vector<std::string> stringified;

			stringified.reserve(subsets.size());

			for(const auto& subset : subsets) {
				stringified.emplace_back(
						Helper::Json::stringify(subset)
				);
			}

			// insert new (stringified) JSONPointer subsets
			this->stringifiedSubSets.insert(
				this->stringifiedSubSets.begin()
				+ this->subSetCurrent,
				stringified.begin(),
				stringified.end()
			);

			// clear non-stringified subsets if neccesary
			switch(this->subSetType) {
			case QueryStruct::typeXPath:
				if(this->minimizeMemory) {
					std::vector<Parsing::XML>().swap(
							this->xPathSubSets
					);
				}
				else {
					this->xPathSubSets.clear();
				}

				break;

			case QueryStruct::typeJsonPath:
				if(this->minimizeMemory) {
					std::vector<jsoncons::json>().swap(
							this->jsonPathSubSets
					);
				}
				else {
					this->jsonPathSubSets.clear();
				}

				break;

			default:
				break;
			}

			// set the subset type to RegEx (i. e. strings only)
			this->subSetType = QueryStruct::typeRegEx;
		}
	}

} /* namespace crawlservpp::Query */

#endif /* QUERY_CONTAINER_HPP_ */
