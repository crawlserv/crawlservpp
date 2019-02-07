/*
 * XPath.cpp
 *
 * Using the pugixml parser library to implement a XPath query with boolean, single and/or multiple results.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "XPath.h"

namespace crawlservpp::Query {

// constructor: set default values
XPath::XPath(const std::string& xpath, bool textOnly) {
	this->compiled = false;
	this->isTextOnly = false;

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

// move constructor
XPath::XPath(XPath&& other) {
	this->query = std::move(other.query);
	this->compiled = std::move(other.compiled);
	this->isTextOnly = std::move(other.isTextOnly);
}

// destructor stub
XPath::~XPath() {}

// get boolean value (at least one match?), throws XPath::Exception
bool XPath::getBool(const crawlservpp::Parsing::XML& doc) const {
	// check query and content
	if(!(this->compiled)) throw XPath::Exception("No query compiled");
	if(!(doc.doc)) throw XPath::Exception("No content parsed");

	// evaluate query with boolean result
	try {
		return this->query.evaluate_boolean(*(doc.doc));
	}
	catch(const std::exception& e) {
		throw XPath::Exception(e.what());
	}
}

// get first match only (saved to resultTo), , throws XPath::Exception
void XPath::getFirst(const crawlservpp::Parsing::XML& doc, std::string& resultTo) const {
	// check query and content
	if(!(this->compiled)) throw XPath::Exception("No query compiled");
	if(!(doc.doc)) throw XPath::Exception("No content parsed");

	// evaluate query with string result
	try {
		if(this->query.return_type() == pugi::xpath_type_node_set) {
			pugi::xpath_node_set nodeSet = this->query.evaluate_node_set(*(doc.doc));
			if(!nodeSet.empty()) resultTo = XPath::nodeToString(nodeSet[0], this->isTextOnly);
			else resultTo = "";
		}
		else resultTo = this->query.evaluate_string(*(doc.doc));
	}
	catch(const std::exception& e) {
		throw XPath::Exception(e.what());
	}
}

// get all matches as vector (saved to resultTo), , throws XPath::Exception
void XPath::getAll(const crawlservpp::Parsing::XML& doc, std::vector<std::string>& resultTo) const {
	std::vector<std::string> resultArray;

	// check query and content
	if(!(this->compiled)) throw XPath::Exception("No query compiled");
	if(!(doc.doc)) throw XPath::Exception("No content parsed");

	// evaluate query with multiple string results
	try {
		if(this->query.return_type() == pugi::xpath_type_node_set) {
			pugi::xpath_node_set nodeSet = this->query.evaluate_node_set(*(doc.doc));
			resultArray.reserve(resultArray.size() + nodeSet.size());
			for(auto i = nodeSet.begin(); i != nodeSet.end(); ++i) {
				std::string result = XPath::nodeToString(*i, this->isTextOnly);
				if(!result.empty()) resultArray.push_back(result);
			}
		}
		else {
			std::string result = this->query.evaluate_string(*(doc.doc));
			if(!result.empty()) resultArray.push_back(result);
		}
	}
	catch(const std::exception& e) {
		throw XPath::Exception(e.what());
	}

	resultTo = resultArray;
}

// move operator
XPath& XPath::operator=(XPath&& other) {
	if(&other != this) {
		this->query = std::move(other.query);
		this->compiled = std::move(other.compiled);
		this->isTextOnly = std::move(other.isTextOnly);
	}
	return *this;
}

// static helper function: convert node to string
std::string XPath::nodeToString(const pugi::xpath_node& node, bool textOnly) {
	std::string result;

	if(node.attribute()) result = node.attribute().as_string();
	else if(node.node()) {
		if(textOnly) {
			XPath::TextOnlyWalker walker;
			node.node().traverse(walker);
			result = walker.getResult();
			if(!result.empty()) result.pop_back();
		}
		else {
			for(auto i = node.node().children().begin(); i != node.node().children().end(); ++i) {
				std::ostringstream outStrStr;
				std::string out;
				i->print(outStrStr, "", 0);
				out = outStrStr.str();

				// parse CDATA
				if(out.length() > 12 && out.substr(0, 9) == "<![CDATA[" && out.substr(out.length() - 3) == "]]>")
					out = out.substr(9, out.length() - 12);
				result += out;
			}
		}
	}
	return result;
}

// XML walker helper classes
bool XPath::TextOnlyWalker::for_each(pugi::xml_node& node) {
	if(node.type() == pugi::node_pcdata) {
		std::string nodeText = node.text().as_string();
		crawlservpp::Helper::Strings::trim(nodeText);
		this->result += nodeText;
		this->result.push_back(' ');
	}
	return true;
}
std::string XPath::TextOnlyWalker::getResult() const {
	return this->result;
}

}
