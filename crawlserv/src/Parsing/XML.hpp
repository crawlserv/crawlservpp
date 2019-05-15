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

#include "HTML.hpp"

#include "../Main/Exception.hpp"

#include <pugixml.hpp>

#include <memory>	// std::make_unique, std::unique_ptr
#include <queue>	// std::queue
#include <sstream>	// std::istringstream, std::ostringstream
#include <string>	// std::string, std::to_string

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

		// setter
		void setOptions(bool showWarnings, unsigned int numOfErrors);

		// parse function
		void parse(const std::string& content, std::queue<std::string>& warningsTo, bool repairCData);

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

		// internal static helper function
		static void cDataRepair(std::string& content);

	private:
		// options
		bool warnings;
		unsigned int errors;
	};

} /* crawlservpp::Parsing */

#endif /* PARSING_XML_HPP_ */
