/*
 * ParsingEntry.h
 *
 * Parsed data (content ID, parsed ID, parsed date/time, parsed fields).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_PARSINGENTRY_H_
#define STRUCT_PARSINGENTRY_H_

#include <string>
#include <vector>

namespace crawlservpp::Struct {
	struct ParsingEntry {
		unsigned long contentId;
		std::string parsedId;
		std::string dateTime;
		std::vector<std::string> fields;

		// constructor
		ParsingEntry(unsigned long setContentId) : contentId(setContentId) {}
	};
}

#endif /* STRUCT_PARSINGENTRY_H_ */
