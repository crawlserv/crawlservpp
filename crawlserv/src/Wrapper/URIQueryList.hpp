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
 * URIQueryList.hpp
 *
 * RAII wrapper for pointer to URI query list.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_URIQUERYLIST_HPP_
#define WRAPPER_URIQUERYLIST_HPP_

#include <uriparser/Uri.h>

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for the %URI query list used by uriparser.
	/*!
	 * Accepts an externally created %URI query list and automatically
	 *  frees it on destruction, avoiding memory leaks.
	 *
	 * At the moment, this class is used exclusively by
	 *  Parsing::URI::getSubUrl().
	 *
	 * For more information about the uriparser API, see its
	 *  <a href="https://github.com/uriparser/uriparser">GitHub repository</a>.
	 *
	 * \note This class does not have ownership of the underlying pointer.
	 */
	class URIQueryList {
	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		URIQueryList() = default;

		virtual ~URIQueryList();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] UriQueryListA * get() noexcept;
		[[nodiscard]] const UriQueryListA * getc() const noexcept;
		[[nodiscard]] UriQueryListA ** getPtr() noexcept;
		[[nodiscard]] bool valid() const noexcept;

		///@}
		///@name Cleanup
		///@{

		void clear() noexcept;

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		URIQueryList(URIQueryList&) = delete;

		//! Deleted copy assignment operator.
		URIQueryList& operator=(URIQueryList&) = delete;

		URIQueryList(URIQueryList&& other) noexcept;
		URIQueryList& operator=(URIQueryList&& other) noexcept;

		///@}

	private:
		// underlying pointer, not owned by this class
		UriQueryListA * ptr{nullptr};
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Destructor clearing the underlying query list if necessary.
	inline URIQueryList::~URIQueryList() {
		this->clear();
	}

	//! Gets a pointer to the underlying query list.
	/*!
	 * \returns A pointer to the underlying query list
	 *   or @c nullptr if no query list has been assigned or
	 *   the query list has already been freed.
	 */
	inline UriQueryListA * URIQueryList::get() noexcept {
		return this->ptr;
	}

	//! Gets a const pointer to the underlying query list.
	/*!
	 * \returns A const pointer to the underlying query list
	 *   or @c nullptr if no query list has been assigned or
	 *   the query list has already been freed.
	 */
	inline const UriQueryListA * URIQueryList::getc() const noexcept {
		return this->ptr;
	}

	//! Gets a pointer to the pointer containing the address of the underlying query list.
	/*!
	 * \returns A pointer to the pointer containing the address
	 *   of the underlying query list or a pointer to a pointer
	 *   containing @c nullptr if no query list has been assigned
	 *   or the query list has already been freed.
	 */
	inline UriQueryListA ** URIQueryList::getPtr() noexcept {
		return &(this->ptr);
	}

	//! Checks whether the underlying query list is valid.
	/*!
	 * \returns true, if the query list is valid. False otherwise.
	 */
	inline bool URIQueryList::valid() const noexcept {
		return this->ptr != nullptr;
	}

	//! Clears the underlying query list if necessary.
	inline void URIQueryList::clear() noexcept {
		if(this->ptr != nullptr) {
			uriFreeQueryListA(this->ptr);

			this->ptr = nullptr;
		}
	}

	//! Move constructor.
	/*!
	 * Moves the query list from the specified location
	 *  into this instance of the class.
	 *
	 * \note The other query list will be invalidated by this move.
	 *
	 * \param other The query list to move from.
	 *
	 * \sa valid
	 */
	inline URIQueryList::URIQueryList(URIQueryList&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Move assignment operator.
	/*!
	 * Moves the query list from the specified location
	 *  into this instance of the class.
	 *
	 * \note The other query list will be invalidated by this move.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The query list to move from.
	 *
	 * \returns A reference to the instance containing
	 *   the query list after moving (i.e. *this).
	 *
	 * \sa valid
	 */
	inline URIQueryList& URIQueryList::operator=(URIQueryList&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_URIQUERYLIST_HPP_ */
