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
	void XML::parse(const std::string& content, std::queue<std::string>& warningsTo, bool repairCData) {
		// remove whitespaces
		unsigned long begin = 0;

		while(content.size() > begin && isspace(content.at(begin)))
			++begin;

		std::string xml(content, begin, content.length() - begin);

		// if necessary, try to tidy HTML and convert it to XML
		if(xml.size() <= 6 || xml.substr(0, 6) != "<?xml ") {
			HTML tidy;

			try {
				tidy.tidyAndConvert(xml, warningsTo); // TODO: MEMORY LEAK !!!
			}
			catch(const HTML::Exception& e) {
				throw XML::Exception("TidyLib error: " + e.whatStr());
			}
		}

		// try to repair CData
		if(repairCData)
			cDataRepair(xml);

		// create XML document
		this->doc = std::make_unique<pugi::xml_document>();

		// parse XHTML with pugixml
		std::istringstream in(xml);

		const pugi::xml_parse_result result(this->doc->load(in, pugi::parse_full));

		if(!result) {
			// parsing error
			std::ostringstream errStrStr;

			errStrStr << "XML parsing error: " << result.description() << " at #" << result.offset << " (";

			if(result.offset > 0) {
				errStrStr << "\'[...]";

				if(result.offset > 50)
					errStrStr << xml.substr(result.offset - 50, 50);
				else
					errStrStr << xml.substr(0, result.offset);

				errStrStr << "[!!!]";

				if(xml.size() > static_cast<size_t>(result.offset + 50))
					errStrStr << "\'[...]" << xml.substr(result.offset, 50) << "[...]";
				else
					errStrStr << "\'[...]" << xml.substr(result.offset);

				errStrStr << "\').";
			}

			throw XML::Exception(errStrStr.str());
		}
	}

	// get content of XML document (saved to resultTo), throws XML::Exception
	void XML::getContent(std::string& resultTo) const {
		if(!(this->doc))
			throw XML::Exception("No content parsed.");

		std::ostringstream out;

		resultTo = "";

		this->doc->print(out, "\t", 0);

		resultTo += out.str();
	}

	// internal static helper function: try to fix CData error (illegal ']]>' inside CData tag)
	void XML::cDataRepair(std::string& content) {
		auto pos = content.find("<![CDATA[");

		if(pos == std::string::npos)
			return;
		else
			pos += 9;

		while(pos < content.size()) {
			const auto next = content.find("<![CDATA[", pos);

			if(next == std::string::npos)
				break;
			else {
				auto last = content.rfind("]]>", next - 3);

				if(last != std::string::npos && last > pos)
					while(true) {
						pos = content.find("]]>", pos);

						if(pos < last) {
							content.insert(pos + 2, 1, ' ');

							pos += 4;
						}
						else
							break;
					}

				pos = next + 9;
			}
		}
	}

} /* crawlservpp::Parsing */
