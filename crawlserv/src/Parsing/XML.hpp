/*
 * XML.hpp
 *
 * Encapsulation of the HTML Tidy API implemented by tidy library and the pugixml parser library to parse a HTML document into clean XML.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#ifndef PARSING_XML_HPP_
#define PARSING_XML_HPP_

#include "../Main/Exception.hpp"

#include <pugixml.hpp>

#include <memory>
#include <sstream>
#include <string>
#include "HTML.hpp"

namespace crawlservpp::Query {

	class XPath;

} /* crawlservpp::Query */

namespace crawlservpp::Parsing {

	class XML {
		friend class Query::XPath;

	public:
		XML();
		virtual ~XML();

		// getter
		void getContent(std::string& resultTo) const;

		// parse functionURI
		void parse(const std::string& content);

		// sub-class for XML exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

		// not moveable, not copyable
		XML(XML&) = delete;
		XML(XML&&) = delete;
		XML& operator=(XML) = delete;
		XML& operator=(XML&&) = delete;

	protected:
		// unique pointer to (pugi)XML document
		std::unique_ptr<pugi::xml_document> doc;
	};

} /* crawlservpp::Parsing */

#endif /* PARSING_XML_HPP_ */
