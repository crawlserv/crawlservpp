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
#include <utility>

namespace crawlservpp::Main {

class Exception : public std::exception {
public:
	// constructor
	Exception(const std::string& description);

	// getters
	const char * what() const noexcept;
	const std::string& whatStr() const noexcept;

private:
	std::string _description;
};

}

#endif /* MAIN_EXCEPTION_H_ */
