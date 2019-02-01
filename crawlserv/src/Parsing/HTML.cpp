/*
 * HTML.cpp
 *
 * Parse HTML, tidy it up and convert it to XML using libtidy.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#include "HTML.h"

namespace crawlservpp::Parsing {

// constructor: create tidy object and fill buffer struct with zeros
HTML::HTML() {
	this->doc = tidyCreate();
	this->buffer = { 0 };
}

// destructor: free buffer if necessary and release tidy object
HTML::~HTML() {
	if(this->buffer.allocated) tidyBufFree(&(this->buffer));
	tidyRelease(this->doc);
}

// tidy HTML and convert it to XML, throws std::runtime_error
void HTML::tidyAndConvert(std::string& content) {
	if(!tidyOptSetBool(this->doc, TidyXmlOut, yes)
			|| !tidyOptSetBool(this->doc, TidyQuiet, yes)
			|| !tidyOptSetBool(this->doc, TidyNumEntities, yes)
			|| !tidyOptSetBool(this->doc, TidyMark, no)
			|| !tidyOptSetBool(this->doc, TidyShowWarnings, no)
			|| !tidyOptSetInt(this->doc, TidyShowErrors, 0)
			|| !tidyOptSetValue(this->doc, TidyOutCharEncoding, "utf8")
			|| !tidyOptSetBool(this->doc, TidyForceOutput, yes))
		throw std::runtime_error("TidyLib error: Could not set options.");

	// parse content
	if(tidyParseString(this->doc, content.c_str()) < 0)
		throw std::runtime_error("TidyLib error: Could not parse HTML content.");

	// clean and repair
	if(tidyCleanAndRepair(this->doc) < 0)
		throw std::runtime_error("TidyLib error: Could not clean and repair HTML content.");

	// save buffer
	if(tidySaveBuffer(this->doc, &(this->buffer)) < 0)
		throw std::runtime_error("TidyLib error: Could not save output buffer.");

	// save output
	if(this->buffer.bp) content = std::string((char *) this->buffer.bp);
}

}
