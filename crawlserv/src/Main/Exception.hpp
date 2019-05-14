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

#include <stdexcept>	// std::runtime_error
#include <string>		// std::string

namespace crawlservpp::Main {

	/*
	 * DECLARATION
	 */

	class Exception : public std::runtime_error {
	public:
		// constructor
		Exception(const std::string& description) : std::runtime_error(description), _description(description) {}

		// getters
		const char * what() const noexcept;
		const std::string& whatStr() const noexcept;
		void append(const std::string& str);

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

	// append description of exception
	inline void Exception::append(const std::string& str) {
		this->_description += " " + str;

	}

} /* crawlservpp::Main */

#endif /* MAIN_EXCEPTION_HPP_ */
