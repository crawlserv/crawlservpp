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

// constructor: set default values
XPath::XPath() {
	this->isTextOnly = false;
	this->query = NULL;
	this->isParsed = false;
}

// destructor: delete query (if necessary) and reset values
XPath::~XPath() {
	if(this->query) {
		delete this->query;
		this->query = NULL;
		this->isTextOnly = false;
	}
	this->isParsed = false;
	this->errorMessage = "";
}

// compile a XPath query
bool XPath::compile(const std::string& xpath, bool textOnly) {
	// delete previous XPath object if necessary
	if(this->query) {
		delete this->query;
		this->query = NULL;
		this->isTextOnly = false;
	}

	// create new XPath object
	try {
		this->query = new pugi::xpath_query(xpath.c_str());
	}
	catch(const pugi::xpath_exception& e) {
		this->errorMessage = "XPath compiling error: " + std::string(e.what());
		return false;
	}

	// save XPath option
	this->isTextOnly = textOnly;
	return true;
}

// get boolean value (at least one match?, saved to resultTo), return false on error
bool XPath::getBool(const crawlservpp::Parsing::XML& doc, bool& resultTo) const {
	// check query and content
	if(!(this->query)) {
		this->errorMessage = "XPath error: No query compiled.";
		return false;
	}
	if(!(doc.doc)) {
		this->errorMessage = "XPath error: No content parsed.";
		return false;
	}

	// evaluate query with boolean result
	try {
		resultTo = this->query->evaluate_boolean(*(doc.doc));
	}
	catch(const std::exception& e) {
		this->errorMessage = "XPath error: " + std::string(e.what());
		return false;
	}

	return true;
}

// get first match only (saved to resultTo), return false on error
bool XPath::getFirst(const crawlservpp::Parsing::XML& doc, std::string& resultTo) const {
	// check query and content
	if(!(this->query)) {
		this->errorMessage = "XPath error: No query compiled.";
		return false;
	}
	if(!(doc.doc)) {
		this->errorMessage = "XPath error: No content parsed.";
		return false;
	}

	// evaluate query with string result
	try {
		if(this->query->return_type() == pugi::xpath_type_node_set) {
			pugi::xpath_node_set nodeSet = this->query->evaluate_node_set(*(doc.doc));
			if(nodeSet.size()) resultTo = XPath::nodeToString(nodeSet[0], this->isTextOnly);
			else resultTo = "";
		}
		else resultTo = this->query->evaluate_string(*(doc.doc));
	}
	catch(const std::exception& e) {
		this->errorMessage = "XPath error: " + std::string(e.what());
		return false;
	}

	return true;
}

// get all matches as vector (saved to resultTo), return false on error
bool XPath::getAll(const crawlservpp::Parsing::XML& doc, std::vector<std::string>& resultTo) const {
	std::vector<std::string> resultArray;

	// check query and content
	if(!(this->query)) {
		this->errorMessage = "XPath error: No query compiled.";
		return false;
	}
	if(!(doc.doc)) {
		this->errorMessage = "XPath error: No content parsed.";
		return false;
	}

	// evaluate query with multiple string results
	try {
		if(this->query->return_type() == pugi::xpath_type_node_set) {
			pugi::xpath_node_set nodeSet = this->query->evaluate_node_set(*(doc.doc));
			resultArray.reserve(resultArray.size() + nodeSet.size());
			for(auto i = nodeSet.begin(); i != nodeSet.end(); ++i) {
				std::string result = XPath::nodeToString(*i, this->isTextOnly);
				if(!result.empty()) resultArray.push_back(result);
			}
		}
		else {
			std::string result = this->query->evaluate_string(*(doc.doc));
			if(!result.empty()) resultArray.push_back(result);
		}
	}
	catch(const std::exception& e) {
		this->errorMessage = "XPath error: " + std::string(e.what());
		return false;
	}

	resultTo = resultArray;
	return true;
}

// get error message
std::string XPath::getErrorMessage() const {
	return this->errorMessage;
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

}
