/*
 * XPath.h
 *
 * Using the pugixml parser library to implement a XPath query with boolean, single and/or multiple results.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef XPATH_H_
#define XPATH_H_

#include "XMLDocument.h"

#include <pugixml.hpp>

#include <exception>
#include <sstream>
#include <string>
#include <vector>

class XPath {
	class TextOnlyWalker : public pugi::xml_tree_walker {
	public:
		virtual bool for_each(pugi::xml_node& node);
		std::string getResult() const;

	protected:
		std::string result;
	};

public:
	XPath();
	virtual ~XPath();

	bool compile(const std::string& xpath, bool textOnly);
	bool getBool(const XMLDocument& doc, bool& resultTo) const;
	bool getFirst(const XMLDocument& doc, std::string& resultTo) const;
	bool getAll(const XMLDocument& doc, std::vector<std::string>& resultTo) const;

	std::string getErrorMessage() const;

private:
	pugi::xpath_query * query;
	bool isTextOnly;

	static std::string nodeToString(const pugi::xpath_node& node, bool textOnly);

	bool isParsed;
	mutable std::string errorMessage;
};

#endif /* XPATH_H_ */
