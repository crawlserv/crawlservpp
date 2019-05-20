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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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
	//  NOTE: Does not change input if output of tidying was empty
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

			const std::string output(this->doc.getOutput(warningsTo));

			if(output.empty())
				return;

			inOut = output;
		}
		catch(const TidyDoc::Exception& e) {
			throw HTML::Exception(e.what());
		}
	}

} /* crawlservpp::Parsing */
