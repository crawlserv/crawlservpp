/*
 * XML.cpp
 *
 * Encapsulation of the HTML Tidy API implemented by tidy library and the pugixml parser library to parse a HTML document into clean XML.
 *
 *  Created on: Oct 18, 2018
 *      Author: ans
 */

#include "XML.h"

namespace crawlservpp::Parsing {

// constructor: set document pointer to NULL
XML::XML() {
	this->doc = NULL;
}

// destructor: delete document (if necessary)
XML::~XML() {
	if(this->doc) {
		delete this->doc;
		this->doc = NULL;
	}
}

// parse XML content, return false on error
bool XML::parse(const std::string& content) {
	// delete previous document if necessary
	if(this->doc) {
		delete this->doc;
		this->doc = NULL;
	}

	std::string tidyResult;
	unsigned long begin = 0;

	while(content.size() > begin && isspace(content.at(begin))) begin++;
	if(content.size() <= begin + 6 || content.substr(begin, 6) != "<?xml ") {
		// tidy HTML code (and convert it to XML)
		TidyDoc tidyDoc = tidyCreate();
		TidyBuffer tidyBuffer = {0};
		bool tidyError = false;

		if(!tidyOptSetBool(tidyDoc, TidyXmlOut, yes)
				|| !tidyOptSetBool(tidyDoc, TidyQuiet, yes)
				|| !tidyOptSetBool(tidyDoc, TidyNumEntities, yes)
				|| !tidyOptSetBool(tidyDoc, TidyMark, no)
				|| !tidyOptSetBool(tidyDoc, TidyShowWarnings, no)
				|| !tidyOptSetInt(tidyDoc, TidyShowErrors, 0)
				|| !tidyOptSetValue(tidyDoc, TidyOutCharEncoding, "utf8")
				|| !tidyOptSetBool(tidyDoc, TidyForceOutput, yes)) {
			this->errorMessage = "TidyLib error: Could not set options.";
			tidyError = true;
		}
		if(!tidyError && tidyParseString(tidyDoc, content.c_str()) < 0) {
			this->errorMessage = "TidyLib error: Could not parse HTML content.";
			tidyError = true;
		}
		if(!tidyError) {
			int tidyResult = tidyCleanAndRepair(tidyDoc);
			if(tidyResult < 0) {
				this->errorMessage = "TidyLib error: Could not clean and repair HTML content.";
				tidyError = true;
			}
		}
		if(!tidyError && tidySaveBuffer(tidyDoc, &tidyBuffer) < 0) {
			this->errorMessage = "TidyLib error: Could not save output buffer.";
			tidyError = true;
		}

		if(!tidyError &&tidyBuffer.bp) {
			tidyResult = (char*) tidyBuffer.bp;
			tidyBufFree(&tidyBuffer);
		}
		else tidyResult = content;
		tidyRelease(tidyDoc);
		if(tidyError) return false;
	}
	else tidyResult = content;

	// create XML document
	this->doc = new pugi::xml_document();

	// parse XHTML with pugixml
	std::istringstream in(tidyResult);
	pugi::xml_parse_result result = this->doc->load(in, pugi::parse_full);

	if(!result) {
		// parsing error
		std::ostringstream errStrStr;
		errStrStr << "XML parsing error: " << result.description() << " at #" << result.offset << " (";
		if((long) content.size() > result.offset + 100) errStrStr << "\'[...]" << content.substr(result.offset, 100) << "[...]\'";
		else if((long) content.size() > result.offset) errStrStr << "\'[...]" << content.substr(result.offset);
		else errStrStr << "[end]";
		errStrStr << ").";
		this->errorMessage = errStrStr.str();

		// delete XML document
		if(this->doc) {
			delete this->doc;
			this->doc = NULL;
		}
		return false;
	}

	return true;
}

// get content of XML document (saved to resultTo), return false on error
bool XML::getContent(std::string& resultTo) const {
	if(!(this->doc)) {
		this->errorMessage = "XML error: No content parsed.";
		return false;
	}

	std::ostringstream out;
	resultTo = "";
	this->doc->print(out, "\t", 0);
	resultTo += out.str();
	return true;
}

// get error string
std::string XML::getErrorMessage() const {
	return this->errorMessage;
}

}
