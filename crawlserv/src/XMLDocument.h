/*
 * XMLDocument.h
 *
 * Encapsulation of the HTML Tidy API implemented by tidy library and the pugixml parser library to parse a HTML document into clean XML.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef XMLDOCUMENT_H_
#define XMLDOCUMENT_H_

#include <tidy/tidy.h>
#include <tidy/tidybuffio.h>

#include <pugixml.hpp>

#include <sstream>
#include <string>

class XMLDocument {
	friend class XPath;

public:
	XMLDocument();
	virtual ~XMLDocument();

	bool parse(const std::string& content);

	bool getContent(std::string& resultTo) const;
	std::string getErrorMessage() const;

protected:
	pugi::xml_document * doc;

	mutable std::string errorMessage;
};

#endif /* XMLDOCUMENT_H_ */
