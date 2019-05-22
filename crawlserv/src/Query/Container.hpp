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

#include <iterator>	// std::make_move_iterator
#include <string>	// std::string
#include <queue>	// std::queue
#include <utility>	// std::pair
#include <vector>	// std::vector

namespace crawlservpp::Query {

	class Container {
		// for convenience
		typedef Helper::Json::Exception JsonException;
		typedef Parsing::XML::Exception XMLException;
		typedef Query::JsonPath::Exception JsonPathException;
		typedef Query::JsonPointer::Exception JsonPointerException;
		typedef Query::RegEx::Exception RegExException;
		typedef Query::XPath::Exception XPathException;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::QueryStruct QueryStruct;

		typedef std::pair<XPath, JsonPointer> XPathJsonPointer;
		typedef std::pair<XPath, JsonPath> XPathJsonPath;

	public:
		// constructor and destructor
		Container();
		virtual ~Container();

		// class for Query::Container exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// protected setters
		void setRepairCData(bool isRepairCData);
		void setTidyErrorsAndWarnings(unsigned int errors, bool warnings);
		void setQueryTarget(const std::string& content, const std::string& source);

		// protected getter
		unsigned long getNumberOfSubSets() const;

		// query functions
		virtual void initQueries() = 0; // children have to initialize their queries on their own (makes the class abstract)
		QueryStruct addQuery(const QueryProperties& properties);
		void clearQueries();

		// subset function
		bool nextSubSet();

		// not moveable, not copyable
		Container(Container&) = delete;
		Container(Container&&) = delete;
		Container& operator=(Container&) = delete;
		Container& operator=(Container&&) = delete;

		// query functions
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
				std::queue<std::string>&
		);

		// helper functions
		bool getXml(std::string& resultTo, std::queue<std::string>& warningsTo);
		void reserveForSubSets(const QueryStruct& query, unsigned long n);

	private:
		// queries
		std::vector<RegEx> queriesRegEx;
		std::vector<XPath> queriesXPath;
		std::vector<JsonPointer> queriesJsonPointer;
		std::vector<JsonPath> queriesJsonPath;
		std::vector<XPathJsonPointer> queriesXPathJsonPointer;
		std::vector<XPathJsonPath> queriesXPathJsonPath;

		// parsing option
		bool repairCData;							// try to repair CData when parsing HTML/XML

		// content pointers and parsing
		const std::string * queryTargetPtr;			// pointer to content to perform queries on
		const std::string * queryTargetSourcePtr;	// pointer to source of content (used for generating warnings)
		bool xmlParsed;								// content has been parsed as HTML/XML
		bool jsonParsedRapid;						// content has been parsed as JSON using RapidJSON
		bool jsonParsedCons;						// content has been parsed as JSON using jsoncons
		Parsing::XML parsedXML;						// content parsed as HTML/XML
		rapidjson::Document parsedJsonRapid;		// content parsed as JSON (using RapidJSON)
		jsoncons::json parsedJsonCons;				// content parsed as JSON (using jsoncons)
		std::string xmlParsingError;				// error while parsing content as HTML/XML
		std::string jsonParsingError;				// error while parsing content as JSON

		// subset properties and parsing
		unsigned char subSetType;					// type of subsets
		unsigned long subSetNumber;					// number of subsets
		unsigned long subSetCurrent;				// current subset (index + 1)
		bool subSetXmlParsed;						// current subset has been parsed as HTML/XML
		bool subSetJsonParsedRapid;					// current subset has been parsed as JSON using RapidJSON
		bool subSetJsonParsedCons;					// current subset has been parsed as JSON using jsoncons
		Parsing::XML subSetParsedXML;				// current subset parsed as HTML/XML
		rapidjson::Document subSetParsedJsonRapid;	// current subset parsed as JSON (using RapidJSON)
		jsoncons::json subSetParsedJsonCons;		// current subset parsed as JSON (using jsoncons)
		std::string subSetXmlParsingError;			// error while parsing current subset as HTML/XML
		std::string subSetJsonParsingError;			// error while parsing current subset as JSON

		// subset data
		std::vector<Parsing::XML> xPathSubSets;
		std::vector<rapidjson::Document> jsonPointerSubSets;
		std::vector<jsoncons::json> jsonPathSubSets;

		std::vector<std::string> stringifiedSubSets;

		// private helper functions
		bool parseXml(std::queue<std::string>& warningsTo);
		bool parseJsonRapid(std::queue<std::string>& warningsTo);
		bool parseJsonCons(std::queue<std::string>& warningsTo);
		void resetParsingState();
		bool parseSubSetXml(std::queue<std::string>& warningsTo);
		bool parseSubSetJsonRapid(std::queue<std::string>& warningsTo);
		bool parseSubSetJsonCons(std::queue<std::string>& warningsTo);
		void resetSubSetParsingState();

		void stringifySubSets(std::queue<std::string>& warningsTo);
		void insertSubSets(std::vector<std::string>& subsets);
		void insertSubSets(std::vector<Parsing::XML>& subsets);
		void insertSubSets(std::vector<rapidjson::Document>& subsets);
		void insertSubSets(std::vector<jsoncons::json>& subsets);
	};

} /* crawlservpp::Query */

#endif /* QUERY_CONTAINER_HPP_ */
