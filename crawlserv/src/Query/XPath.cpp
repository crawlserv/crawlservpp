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
 * XPath.cpp
 *
 * Using the pugixml parser library to implement a XPath query with boolean, single and/or multiple results.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "XPath.hpp"

namespace crawlservpp::Query {

	// constructor: set default values
	XPath::XPath(const std::string& xpath, bool textOnly) : compiled(false), isTextOnly(false) {
		// create new XPath object
		try {
			this->query = pugi::xpath_query(xpath.c_str());
			this->compiled = true;
		}
		catch(const pugi::xpath_exception& e) {
			throw XPath::Exception(e.what());
		}

		// save XPath option
		this->isTextOnly = textOnly;
	}

	// destructor stub
	XPath::~XPath() {}

	// get boolean value (at least one match?), throws XPath::Exception
	bool XPath::getBool(const Parsing::XML& doc) const {
		// check query and content
		if(!(this->compiled))
			throw XPath::Exception("No query compiled");

		if(!(doc.doc))
			throw XPath::Exception("No content parsed");

		// evaluate query with boolean result
		try {
			return this->query.evaluate_boolean(*(doc.doc));
		}
		catch(const std::exception& e) {
			throw XPath::Exception(e.what());
		}
	}

	// get first match only (saved to resultTo), throws XPath::Exception
	void XPath::getFirst(const Parsing::XML& doc, std::string& resultTo) const {
		// check query and content
		if(!(this->compiled))
			throw XPath::Exception("No query compiled");

		if(!(doc.doc))
			throw XPath::Exception("No content parsed");

		// evaluate query with string result
		try {
			if(this->query.return_type() == pugi::xpath_type_node_set) {
				const pugi::xpath_node_set nodeSet(this->query.evaluate_node_set(*(doc.doc)));

				if(nodeSet.empty())
					resultTo = "";
				else
					resultTo = XPath::nodeToString(nodeSet[0], this->isTextOnly);
			}
			else
				resultTo = this->query.evaluate_string(*(doc.doc));
		}
		catch(const std::exception& e) {
			throw XPath::Exception(e.what());
		}
	}

	// get all matches as vector (saved to resultTo), throws XPath::Exception
	void XPath::getAll(const Parsing::XML& doc, std::vector<std::string>& resultTo) const {
		// check query and content
		if(!(this->compiled))
			throw XPath::Exception("No query compiled");

		if(!(doc.doc))
			throw XPath::Exception("No content parsed");

		// empty result
		resultTo.clear();

		// evaluate query with multiple string results
		try {
			if(this->query.return_type() == pugi::xpath_type_node_set) {
				const pugi::xpath_node_set nodeSet(
						this->query.evaluate_node_set(*(doc.doc))
				);

				resultTo.reserve(nodeSet.size());

				for(const auto& node : nodeSet) {
					const std::string result(XPath::nodeToString(node, this->isTextOnly));

					if(!result.empty())
						resultTo.emplace_back(result);
				}
			}
			else {
				const std::string result(this->query.evaluate_string(*(doc.doc)));

				if(!result.empty())
					resultTo.emplace_back(result);
			}
		}
		catch(const std::exception& e) {
			throw XPath::Exception(e.what());
		}
	}

	// get matching sub-sets from parsed XML document (saved to resultTo), throws XPath::Exception
	void XPath::getSubSets(const Parsing::XML& doc, std::vector<Parsing::XML>& resultTo) const {
		// check query and content
		if(!(this->compiled))
			throw XPath::Exception("No query compiled");

		if(!(doc.doc))
			throw XPath::Exception("No content parsed");

		// empty result
		resultTo.clear();

		// evaluate query with multiple results
		try {
			if(this->query.return_type() == pugi::xpath_type_node_set) {
				const pugi::xpath_node_set nodeSet(
						this->query.evaluate_node_set(*(doc.doc))
				);

				resultTo.reserve(nodeSet.size());

				for(const auto& node : nodeSet)
					if(node)
						resultTo.emplace_back(node.node());
			}
			else
				throw XPath::Exception("Could not create subset, because the result of the query is no node set");
		}
		catch(const std::exception& e) {
			throw XPath::Exception(e.what());
		}
	}

	// static helper function: convert node to string
	std::string XPath::nodeToString(const pugi::xpath_node& node, bool textOnly) {
		std::string result;

		if(node.attribute())
			result = node.attribute().as_string();
		else if(node.node()) {
			if(textOnly) {
				XPath::TextOnlyWalker walker;

				node.node().traverse(walker);

				result = walker.getResult();

				if(!result.empty())
					result.pop_back();
			}
			else {
				for(const auto& child : node.node().children()) {
					std::ostringstream outStrStr;
					std::string out;

					child.print(outStrStr, "", 0);

					out = outStrStr.str();

					// parse CDATA
					if(
							out.length() > 12
							&& out.substr(0, 9) == "<![CDATA["
							&& out.substr(out.length() - 3) == "]]>"
					)
						out = out.substr(9, out.length() - 12);

					result += out;
				}
			}
		}

		return result;
	}

	// XML walker for text-only conversion helper functions
	bool XPath::TextOnlyWalker::for_each(pugi::xml_node& node) {
		if(node.type() == pugi::node_pcdata) {
			std::string nodeText(node.text().as_string());

			Helper::Strings::trim(nodeText);

			this->result += nodeText;

			this->result.push_back(' ');
		}

		return true;
	}

	// get result from XML walker
	std::string XPath::TextOnlyWalker::getResult() const {
		return this->result;
	}

} /* crawlservpp::Query */
