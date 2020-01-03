/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * XML.cpp
 *
 * Encapsulation of the HTML Tidy API implemented by tidy library and the pugixml parser library to parse a HTML document into clean XML.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#include "XML.hpp"

namespace crawlservpp::Parsing {

	// constructor for new XML document
	XML::XML() : warnings(false), errors(0) {}

	// constructor to create new XML document from existing node
	XML::XML(const pugi::xml_node& node) : warnings(false), errors(0) {
		// create XML document
		this->doc = std::make_unique<pugi::xml_document>();

		this->doc->append_copy(node);
	}

	// destructor stub
	XML::~XML() {
		this->clear();
	}

	// set options for logging
	void XML::setOptions(bool showWarnings, unsigned int numOfErrors) {
		this->warnings = showWarnings;
		this->errors = numOfErrors;
	}

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
				tidy.tidyAndConvert(xml, this->warnings, this->errors, warningsTo); // TODO: MEMORY LEAK !!!
			}
			catch(const HTML::Exception& e) {
				throw XML::Exception("TidyLib error: " + e.whatStr());
			}
		}

		// try to repair CData
		if(repairCData)
			cDataRepair(xml);

		// remove invalid conditional comments
		replaceInvalidConditionalComments(xml);

		// create XML document
		this->doc = std::make_unique<pugi::xml_document>();

		// parse XHTML with pugixml
		std::istringstream in(xml);

		const pugi::xml_parse_result result(this->doc->load(in, pugi::parse_full));

		if(!result) {
			// parsing error
			std::string errString(
					"XML parsing error: "
					+ std::string(result.description())
					+ " at #"
					+ std::to_string(result.offset)
					+ " ("
			);

			if(result.offset > 0) {
				errString += "\'[...]";

				if(result.offset > 50)
					errString += xml.substr(result.offset - 50, 50);
				else
					errString += xml.substr(0, result.offset);

				errString += "[!!!]";

				if(xml.size() > static_cast<size_t>(result.offset + 50))
					errString += "\'[...]" + xml.substr(result.offset, 50) + "[...]";
				else
					errString += "\'[...]" + xml.substr(result.offset);

				errString += "\').";
			}

			throw XML::Exception(errString);
		}
	}

	// clear parsed content
	void XML::clear() {
		if(this->doc)
			this->doc.reset();
	}

	// boolean operator to check document
	XML::operator bool() const noexcept {
		return this->doc != nullptr;
	}

	// not operator to check document
	bool XML::operator!() const noexcept {
		return this->doc == nullptr;
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

	// internal static helper function: try to fix CData error (invalid ']]>' inside CData tag)
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

	// internal static helper function: replace invalid conditional comments (e.g. created by MS Excel)
	void XML::replaceInvalidConditionalComments(std::string& content) {
		size_t pos = 0;

		while(pos < content.length()) {
			// find next invalid conditional comment
			pos = content.find("<![if ", pos);

			if(pos == std::string::npos)
				break;

			// find end of invalid conditional comment
			const auto end = content.find("<![endif]>", pos + 6);

			if(end == std::string::npos)
				break;

			// insert commenting to make conditional comment valid (X)HTML
			content.insert(pos + 2, "--");
			content.insert(end + 11, "--"); // (consider that "--" has been added)

			// jump to the end of the changed conditional comment
			pos = end + 4; // (consider that 2x "--" have been added)
		}
	}

} /* crawlservpp::Parsing */
