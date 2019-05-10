/*
 * HTML.cpp
 *
 * Parse HTML, tidy it up and convert it to XML using libtidy.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#include "HTML.hpp"

namespace crawlservpp::Parsing {
	// tidy HTML and convert it to XML, throws HTML::Exception
	void HTML::tidyAndConvert(std::string& inOut, bool warnings, unsigned int numOfErrors, std::queue<std::string>& warningsTo) {
		// set options
		try {
			this->doc.setOption(TidyXmlOut, true);
			this->doc.setOption(TidyQuiet, true);
			this->doc.setOption(TidyNumEntities, true);
			this->doc.setOption(TidyMark, false);
			this->doc.setOption(TidyShowWarnings, warnings);
			this->doc.setOption(TidyForceOutput, true);
			this->doc.setOption(TidyDropEmptyElems, false);
			this->doc.setOption(TidyShowErrors, numOfErrors);
			this->doc.setOption(TidyOutCharEncoding, "utf8");

			this->doc.parseString(inOut, warningsTo);
			this->doc.cleanAndRepair(warningsTo);

			inOut = this->doc.getOutput(warningsTo);
		}
		catch(const TidyDoc::Exception& e) {
			throw HTML::Exception(e.what());
		}
	}

} /* crawlservpp::Parsing */
