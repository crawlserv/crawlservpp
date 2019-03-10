/*
 * Exception.h
 *
 * Class for custom exceptions.
 *
 *  Created on: Feb 5, 2019
 *      Author: ans
 */

#ifndef MAIN_EXCEPTION_HPP_
#define MAIN_EXCEPTION_HPP_

#include <exception>
#include <string>
#include <utility>

namespace crawlservpp::Main {

	/*
	 * DECLARATION
	 */

	class Exception : public std::exception {
	public:
		// constructor
		Exception(const std::string& description) : _description(description) {}

		// getters
		const char * what() const noexcept;
		const std::string& whatStr() const noexcept;

	private:
		std::string _description;
	};

	/*
	 * IMPLEMENTATION
	 */

	// get description as C string
	inline const char * Exception::what() const noexcept {
		return this->_description.c_str();
	}

	// get description as std::string
	inline const std::string& Exception::whatStr() const noexcept {
		return this->_description;
	}

} /* crawlservpp::Main */

#endif /* MAIN_EXCEPTION_HPP_ */
