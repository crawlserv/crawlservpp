/*
 * XML.h
 *
 * Encapsulation of the HTML Tidy API implemented by tidy library and the pugixml parser library to parse a HTML document into clean XML.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef PARSING_XML_H_
#define PARSING_XML_H_

#include <tidy/tidy.h>
#include <tidy/tidybuffio.h>

#include <pugixml.hpp>

#include <memory>
#include <sstream>
#include <string>

namespace crawlservpp::Query {
	class XPath;
}

namespace crawlservpp::Parsing {
	class XML {
		friend class crawlservpp::Query::XPath;

	public:
		XML();
		virtual ~XML();

		bool parse(const std::string& content);

		bool getContent(std::string& resultTo) const;
		std::string getErrorMessage() const;

	protected:
		std::unique_ptr<pugi::xml_document> doc;

		mutable std::string errorMessage;
	};
}

#endif /* PARSING_XML_H_ */
