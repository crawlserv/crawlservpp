/*
 * DataEntry.hpp
 *
 * Parsed or extracted data (content ID, parsed ID, parsed date/time, parsed or extracted fields).
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef STRUCT_DATAENTRY_HPP_
#define STRUCT_DATAENTRY_HPP_

#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Struct {

	struct DataEntry {
		unsigned long contentId;
		std::string dataId;
		std::string dateTime;
		std::vector<std::string> fields;

		// constructor
		DataEntry(unsigned long setContentId) : contentId(setContentId) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_DATAENTRY_HPP_ */
