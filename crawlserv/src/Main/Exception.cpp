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

// constructor: set description
Exception::Exception(const std::string& description) {
	this->_description = description;
}

// copy constructor
Exception::Exception(const Exception& other) {
	this->_description = other._description;
}

// move constructor
Exception::Exception(Exception&& other) noexcept : _description(std::exchange(other._description, nullptr)) {}

// destructor stub
Exception::~Exception() {}

// get description as C string
const char * Exception::what() const noexcept {
	return this->_description.c_str();
}

// get description as std::string
const std::string& Exception::whatStr() const noexcept {
	return this->_description;
}

// copy assignment operator
Exception& Exception::operator=(const Exception& other) {
	return *this = Exception(other);
}

// move assigmnent operator
Exception& Exception::operator=(Exception&& other) noexcept {
	std::swap(this->_description, other._description);
	return *this;
}

}
