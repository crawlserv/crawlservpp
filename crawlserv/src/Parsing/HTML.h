/*
 * HTML.h
 *
 * Parse HTML, tidy it up and convert it to XML using libtidy.
 *
 *  Created on: Feb 1, 2019
 *      Author: ans
 */

#ifndef PARSING_HTML_H_
#define PARSING_HTML_H_

#include "tidy.h"
#include "tidybuffio.h"

#include <stdexcept>
#include <string>

namespace crawlservpp::Parsing {

class HTML final {
public:
	HTML();
	virtual ~HTML();

	void tidyAndConvert(std::string& content);

private:
	TidyDoc doc;
	TidyBuffer buffer;
};

}

#endif /* PARSING_HTML_H_ */
