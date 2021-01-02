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
 * CurlList.hpp
 *
 * RAII wrapper for pointer to libcurl list.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_CURLLIST_HPP_
#define WRAPPER_CURLLIST_HPP_

#ifndef CRAWLSERVPP_TESTING

#include <curl/curl.h>

#else

#include "FakeCurl/FakeCurl.hpp"

#endif

#include <cstddef>		// std::size_t
#include <stdexcept>	// std::runtime_error
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for lists used by the libcurl API.
	/*!
	 * Sets the list to @c nullptr on construction and automatically
	 *  clears it on destruction, avoiding memory leaks.
	 *
	 * At the moment, this class is used exclusively by Network::Curl.
	 *
	 * For more information about the libcurl API, see its
	 *  <a href="https://curl.haxx.se/libcurl/c/">website</a>.
	 *
	 * \note This class does not have ownership of the underlying pointer.
	 *
	 */
	class CurlList {
	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		CurlList() = default;

		virtual ~CurlList();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] curl_slist * get() noexcept;
		[[nodiscard]] const curl_slist * getc() const noexcept;
		[[nodiscard]] bool valid() const noexcept;
		[[nodiscard]] std::size_t size() const noexcept;
		[[nodiscard]] bool empty() const noexcept;

		///@}
		///@name Manipulation and Cleanup
		///@{

		void append(const CurlList& other);
		void append(const std::vector<std::string>& newElements);
		void append(const std::string& newElement);
		void clear() noexcept;

		///@}
		/**@name Copy and Move
		 * The class is both copyable and moveable.
		 */
		///@{

		CurlList(const CurlList& other);
		CurlList& operator=(const CurlList& other);
		CurlList(CurlList&& other) noexcept;
		CurlList& operator=(CurlList&& other) noexcept;

		///@}

	private:
		curl_slist * ptr{nullptr};
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Destructor resetting the list if necessary.
	inline CurlList::~CurlList() {
		this->clear();
	}

	//! Gets a pointer to the underlying list.
	/*!
	 * \returns A pointer to the underlying libcurl list
	 *   or @c nullptr if no list has been created or
	 *   the list has already been resetted.
	 */
	inline curl_slist * CurlList::get() noexcept {
		return this->ptr;
	}

	//! Gets a const pointer to the underlying list.
	/*!
	 * \returns A const pointer to the underlying libcurl list
	 *   or @c nullptr if no list has been created or
	 *   the list has already been resetted.
	 */
	inline const curl_slist * CurlList::getc() const noexcept {
		return this->ptr;
	}

	//! Checks whether the underlying list is valid.
	/*!
	 * \returns True, if a list has been created and not been resetted.
	 *   False otherwise.
	 */
	inline bool CurlList::valid() const noexcept {
		return this->ptr != nullptr;
	}

	//! Gets the current number of elements in the list.
	/*!
	 * \returns The number of elements
	 *  in the underlying list.
	 *
	 * \note The return value is zero
	 *  if the list has not been initialized.
	 */
	inline std::size_t CurlList::size() const noexcept {
		std::size_t count{};
		const auto * element{this->ptr};

		while(element != nullptr) {
			if(element->data != nullptr) {
				++count;
			}

			element = element->next;
		}

		return count;
	}

	//! Checks whether the list is empty.
	/*!
	 * \returns True, if the list is empty or invalid. False otherwise.
	 *
	 * \sa valid
	 */
	inline bool CurlList::empty() const noexcept {
		const auto * element{this->ptr};

		while(element != nullptr) {
			if(element->data != nullptr) {
				return false;
			}
		}

		return true;
	}

	//! Appends another list to the list.
	/*!
	 * \note Calling this function with an invalid list
	 *   or on itself has no effect.
	 *
	 * \param other A const reference to the list which
	 *   elements should be appended.
	 *
	 * \throws std::runtime_error if appending to the list failed.
	 */
	inline void CurlList::append(const CurlList& other) {
		if(this == &other) {
			return;
		}

		auto * item = other.ptr;

		while(item != nullptr) {
			this->append(item->data);

			item = item->next;
		}
	}

	//! Appends the elements of a vector to the list.
	/*!
	 * \param newElements A const reference to the vector of strings
	 *   that should be appended to the list.
	 *
	 * \throws std::runtime_error if appending to the list failed.
	 */
	inline void CurlList::append(const std::vector<std::string>& newElements) {
		for(const auto& element : newElements) {
			this->append(element);
		}
	}

	//! Appends an element to the list.
	/*!
	 * A new list will be created if no list already exists.
	 *
	 * \note String views cannot be used, because the
	 *   underlying API requires a null-terminated string.
	 *
	 * \param newElement A const reference to a string
	 *   that should be appended at the end of the list.
	 *
	 * \throws std::runtime_error if the element
	 *   could not be appended by the underlying API.
	 */
	inline void CurlList::append(const std::string& newElement) {
		auto * const temp{
			curl_slist_append(this->ptr, newElement.c_str())
		};

		if(temp == nullptr) {
			throw std::runtime_error("curl_slist_append() failed");
		}

		this->ptr = temp;
	}

	//! Resets the list and frees its memory.
	/*!
	 * The list will be invalid and valid() will return false afterwards.
	 *
	 * \note Does nothing if the underlying list is not initialized.
	 */
	inline void CurlList::clear() noexcept {
		if(this->ptr != nullptr) {
			curl_slist_free_all(this->ptr);
		}

		this->ptr = nullptr;
	}

	//! Copy constructor.
	/*!
	 * Creates a new list and copies all elements
	 *  of the given list into it.
	 *
	 * \note Calling this function with an invalid
	 *  list has no effect.
	 *
	 * \param other The list to copy from.
	 *
	 * \throws std::runtime_error if the new list could
	 *   not be created or any of the elements could not
	 *   be appended to it.
	 */
	inline CurlList::CurlList(const CurlList& other) {
		this->append(other);
	}

	//! Copy assignment operator.
	/*!
	 * Clears the existing list, creates a new one and
	 *  copies all elements of the given list into it.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The buffer to copy from.
	 *
	 * \returns A reference to the class containing
	 *   the copy of the list (i.e. *this).
	 *
	 * \throws std::runtime_error if the new list could
	 *   not be created or any of the elements could not
	 *   be appended to it.
	 */
	inline CurlList& CurlList::operator=(const CurlList& other) {
		if(&other != this) {
			this->clear();

			this->append(other);
		}

		return *this;
	}

	//! Move constructor.
	/*!
	 * Moves the list from the specified location
	 *  into this instance of the class.
	 *
	 * \note The other list will be invalidated by this move.
	 *
	 * \param other The list to move from.
	 *
	 * \sa valid
	 */
	inline CurlList::CurlList(CurlList&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Move assignment operator.
	/*!
	 * Moves the list from the specified location
	 *  into this instance of the class.
	 *
	 * \note The other list will be invalidated by this move.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The list to move from.
	 *
	 * \returns A reference to the instance containing
	 *   the list after moving (i.e. *this).
	 *
	 * \sa valid
	 */
	inline CurlList& CurlList::operator=(CurlList&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

	} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_CURLLIST_HPP_ */
