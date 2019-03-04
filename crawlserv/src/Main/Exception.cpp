/*
 * Exception.cpp
 *
 * Class for custom exceptions.
 *
 *  Created on: Feb 8, 2019
 *      Author: ans
 */

#include "Exception.h"

namespace crawlservpp::Main {

// get description as C string
const char * Exception::what() const noexcept {
	return this->_description.c_str();
}

// get description as std::string
const std::string& Exception::whatStr() const noexcept {
	return this->_description;
}

}
