/*
 * IdString.h
 *
 * A simple [id,string] pair.
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCT_IDSTRING_H_
#define STRUCT_IDSTRING_H_

#include <string>

namespace crawlservpp::Struct {
	struct IdString {
		IdString(const unsigned long& newId, const std::string& newString) { this->id = newId; this->string = newString; }
		IdString() : IdString(0, "") {}
		unsigned long id;
		std::string string;

		bool operator==(unsigned long idValue) const { return this->id == idValue; }
		bool operator==(const std::string& stringValue) const { return this->string == stringValue; }
	};
}

#endif /* STRUCT_IDSTRING_H_ */
