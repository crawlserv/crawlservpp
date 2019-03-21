/*
 * XML.cpp
 *
 * Encapsulation of the HTML Tidy API implemented by tidy library and the pugixml parser library to parse a HTML document into clean XML.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#include "XML.hpp"

namespace crawlservpp::Parsing {

	// constructor stub
	XML::XML() {}

	// destructor stub
	XML::~XML() {}

	// parse XML content, throws XML::Exception
	void XML::parse(const std::string& content) {
		// remove whitespaces
		unsigned long begin = 0;
		while(content.size() > begin && isspace(content.at(begin))) begi++n;
		std::string xml(content, begin, content.length() - begin);

		// if necessary, try to tidy HTML and convert it to XML
		if(xml.size() <= 6 || xml.substr(0, 6) != "<?xml ") {
			HTML tidy;
			try {
				tidy.tidyAndConvert(xml);
			}
			catch(const HTML::Exception& e) {
				throw XML::Exception("TidyLib error: " + e.whatStr());
			}
		}

		// create XML document
		this->doc = std::make_unique<pugi::xml_document>();

		// parse XHTML with pugixml
		std::istringstream in(xml);
		pugi::xml_parse_result result = this->doc->load(in, pugi::parse_full);

		if(!result) {
			// parsing error
			std::ostringstream errStrStr;
			errStrStr << "XML parsing error: " << result.description() << " at #" << result.offset << " (";
			if((long) content.size() > result.offset + 100) errStrStr << "\'[...]" << content.substr(result.offset, 100) << "[...]\'";
			else if((long) content.size() > result.offset) errStrStr << "\'[...]" << content.substr(result.offset);
			else errStrStr << "[end]";
			errStrStr << ").";
			throw XML::Exception(errStrStr.str());
		}
	}

	// get content of XML document (saved to resultTo), throws XML::Exception
	void XML::getContent(std::string& resultTo) const {
		if(!(this->doc)) throw XML::Exception("No content parsed.");

		std::ostringstream out;
		resultTo = "";
		this->doc->print(out, "\t", 0);
		resultTo += out.str();
	}

} /* crawlservpp::Parsing */
