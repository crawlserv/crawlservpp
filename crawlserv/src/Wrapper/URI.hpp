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
 * URI.hpp
 *
 * Wrapper for pointer to URI structure.
 *  Structure is only created when needed.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_URI_HPP_
#define WRAPPER_URI_HPP_

#include <uriparser/Uri.h>

#include <memory>	// std::make_unique, std::unique_ptr
#include <utility>	// std::move

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for the RFC 3986 %URI structure used by uriparser.
	/*!
	 * Creates a uriparser structure on request and automatically
	 *  frees it on destruction, avoiding memory leaks.
	 *
	 * At the moment, this class is used exclusively by
	 *  Parsing::URI.
	 *
	 * For more information about the uriparser API, see its
	 *  <a href="https://github.com/uriparser/uriparser">GitHub repository</a>.
	 */
	class URI {
	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		URI() = default;

		virtual ~URI();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] UriUriA * get() noexcept;
		[[nodiscard]] const UriUriA * getc() const noexcept;
		[[nodiscard]] bool valid() const noexcept;

		///@}
		///@name Creation and Cleanup
		///@{

		void create();
		void clear();

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		URI(URI&) = delete;

		//! Deleted copy assignment operator.
		URI& operator=(URI&) = delete;

		//! Default move constructor.
		URI(URI&& other) = default;

		//! Default move assignment operator.
		URI& operator=(URI&& other) = default;

		///@}

	private:
		// underlying smart pointer
		std::unique_ptr<UriUriA> ptr;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Destructor freeing the %URI if necessary.
	/*!
	 * Has no effect if no %URI has been created.
	 */
	inline URI::~URI() {
		this->clear();
	}

	//! Gets a pointer to the underlying %URI structure.
	/*!
	 * \returns Pointer to the underlying %URI structure.
	 */
	inline UriUriA * URI::get() noexcept {
		return this->ptr.get();
	}

	//! Gets a const pointer to the underlying %URI structure.
	/*!
	 * \returns Pointer to the underlying %URI structure.
	 */
	inline const UriUriA * URI::getc() const noexcept {
		return this->ptr.get();
	}

	//! Checks whether the %URI is valid.
	/*!
	 * \returns True, if the underlying %URI structure is valid.
	 *   False otherwise.
	 */
	inline bool URI::valid() const noexcept {
		return this->ptr.operator bool();
	}

	//! Creates a new and empty %URI.
	/*!
	 * Frees the underlying %URI structure beforehand, if necessary.
	 */
	inline void URI::create() {
		this->clear();

		this->ptr = std::make_unique<UriUriA>();
	}

	//! Frees the current %URI.
	/*!
	 * Frees and resets the underlying %URI structure.
	 *
	 * Has no effect if no %URI has been created.
	 */
	inline void URI::clear() {
		if(this->ptr) {
			uriFreeUriMembersA(this->ptr.get());

			this->ptr.reset();
		}
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_URI_HPP_ */
