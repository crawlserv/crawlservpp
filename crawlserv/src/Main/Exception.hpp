/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Exception.hpp
 *
 * Base class for custom exceptions.
 *
 *  Created on: Feb 5, 2019
 *      Author: ans
 */

#ifndef MAIN_EXCEPTION_HPP_
#define MAIN_EXCEPTION_HPP_

#include <stdexcept>		// std::runtime_error
#include <string>			// std::string
#include <string_view>		// std::string_view

/*
 * MACROS FOR CLASS CREATION
 */

//! Macro used to easily define classes for general exceptions.
/*!
 * This macro will create a fully functional child class
 *  of \link crawlservpp::Main::Exception Main::Exception\endlink.
 * \warning The macro needs to be used inside another class
 *  or inside another namespace than \link crawlservpp::Main Main\endlink.
 */
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MAIN_EXCEPTION_CLASS()			class Exception : public Main::Exception { \
										public: \
											explicit Exception( \
													const std::string& description \
											) : Main::Exception(description) {} \
										}

//! Macro used to easily define classes for specific exceptions.
/*!
 * This macro will create a fully functional child class
 *  of the exception class previously defined by MAIN_EXCEPTION_CLASS().
 * \warning MAIN_EXCEPTION_CLASS() must have been used in the current context
 *  before using this macro.
 */
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage, bugprone-macro-parentheses)
#define MAIN_EXCEPTION_SUBCLASS(NAME)	class NAME : public Exception { \
										public: \
											explicit NAME( \
													const std::string& description \
											) : Exception(description) {} \
										}

namespace crawlservpp::Main {

	/*
	 * DECLARATION
	 */

	//! Base class for all exceptions thrown by the application.
	/*!
	 * Possesses a view into the string describing the exception.
	 *
	 * Use #MAIN_EXCEPTION_CLASS() for adding
	 * 	child classes of Main::Exception for general exceptions
	 * 	to another class.
	 *
	 * Use #MAIN_EXCEPTION_SUBCLASS(NAME) for adding
	 *  children for specific types of exceptions
	 *  to such a class.
	 */
	class Exception : public std::runtime_error {
	public:
		///@name Construction and Destruction
		///@{

		explicit Exception(const std::string& description);

		//! Default destructor.
		~Exception() override = default;

		///@}
		///@name Getter
		///@{

		[[nodiscard]] std::string_view view() const noexcept;

		///@}
		/**@name Copy and Move
		 * The class is both copyable and moveable.
		 */
		///@{

		//! Default copy constructor.
		Exception(const Exception& other) = default;

		//! Default copy assignment operator.
		Exception& operator=(const Exception& other) = default;

		//! Default move constructor.
		Exception(Exception&& other) = default;

		//! Default move assignment operator.
		Exception& operator=(Exception&& other) = default;

		///@}

	private:
		std::string_view stringView;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor for creating a new exception.
	/*!
	 * The string will be handled by the std::runtime_error parent class.
	 *
	 * An extra view into the string will be stored inside the class.
	 *
	 * \note An appropriate child class should be used when throwing an exception.
	 *
	 * \note Use #MAIN_EXCEPTION_CLASS() for adding
	 * 			 child classes of Main::Exception to another class.
	 *
	 * \note Use #MAIN_EXCEPTION_SUBCLASS(NAME) for adding
	 * 			 children to those classes for specific types of exceptions.
	 *
	 * \param description A const reference to a string describing the exception.
	 */
	inline Exception::Exception(const std::string& description)
			: std::runtime_error(description), stringView(std::runtime_error::what()) {}

	//! Gets the description of the exception as a view to the underlying string.
	/*!
	 * \returns A view to the string containing the description
	 *  of the exception.
	 */
	inline std::string_view Exception::view() const noexcept {
		return this->stringView;
	}

} /* namespace crawlservpp::Main */

#endif /* MAIN_EXCEPTION_HPP_ */
