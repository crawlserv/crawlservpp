/*
 * Exception.h
 *
 * Class for custom exceptions.
 *
 *  Created on: Feb 5, 2019
 *      Author: ans
 */

#ifndef MAIN_EXCEPTION_H_
#define MAIN_EXCEPTION_H_

#include <exception>
#include <string>

namespace crawlservpp::Main {

class Exception : public std::exception {
public:
	Exception(const std::string& description) { this->_description = description; }
	virtual ~Exception() {}
	const char * what() const throw() { return this->_description.c_str(); }
	const std::string& whatStr() const throw() { return this->_description; }
private:
	std::string _description;
};

}

#endif /* MAIN_EXCEPTION_H_ */
