/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * AspellList.hpp
 *
 * RAII wrapper for pointer to aspell word lists.
 *  Does NOT have ownership of the pointer, but takes care of its deletion!
 *
 *  Created on: Feb 27, 2021
 *      Author: ans
 */

#ifndef WRAPPER_ASPELLLIST_HPP_
#define WRAPPER_ASPELLLIST_HPP_

#include <aspell.h>

#include <string>	// std::string

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for @c aspell word lists.
	/*!
	 * Creates the word list on
	 *  construction and deletes it on
	 *  destruction, if still necessary,
	 *  avoiding memory leaks.
	 *
	 * \note The class does not own the
	 *   underlying pointer, but takes care
	 *   of its deletion via API call.
	 */
	class AspellList {
	public:
		///@name Construction and Destruction
		///@{

		explicit AspellList(const AspellWordList * source);
		virtual ~AspellList();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] bool valid() const;

		///@}
		///@name List Iteration
		///@{

		bool next(std::string& nextTo);

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}
		/**@name Copy and Move
		 * The class is both copyable and
		 *  moveable.
		 */
		///@{

		AspellList(const AspellList& other);
		AspellList(AspellList&& other) noexcept;
		AspellList& operator=(const AspellList& other);
		AspellList& operator=(AspellList&& other) noexcept;

		///@}

		//! Class for @c aspell word list-specific exceptions.
		MAIN_EXCEPTION_CLASS();

	private:
		// underlying pointer to the word list
		AspellStringEnumeration * ptr{nullptr};
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor creating a new word list.
	/*!
	 * \param source The source of the word list,
	 *   e.g. as returned by
	 *   @c aspell_speller_suggest .
	 */
	inline AspellList::AspellList(const AspellWordList * source) {
		if(source != nullptr) {
			this->ptr = aspell_word_list_elements(source);
		}
	}

	//! Destructor deleting the word list, if necessary.
	/*!
	 * \sa clear
	 */
	inline AspellList::~AspellList() {
		this->clear();
	}

	/*
	 * GETTERS
	 */

	//! Gets whether the word list is valid.
	/*!
	 * \returns True, if the word list
	 *   is valid. False otherwise.
	 */
	inline bool AspellList::valid() const {
		return this->ptr != nullptr;
	}

	/*
	 * LIST ITERATION
	 */

	//! Checks for the next list element.
	/*!
	 * \param nextTo Reference to a
	 *   string to which the next list
	 *   element will be written, if
	 *   available.
	 *
	 * \returns True, if another list
	 *   element was available and has
	 *   been written to \p nextTo.
	 *   False otherwise.
	 */
	inline bool AspellList::next(std::string& nextTo) {
		const auto * next{aspell_string_enumeration_next(this->ptr)};

		if(next == nullptr) {
			return false;
		}

		nextTo = std::string(next);

		return true;
	}

	/*
	 * CLEANUP
	 */

	//! Deletes the word list, if necessary.
	inline void AspellList::clear() {
		if(this->ptr != nullptr) {
			delete_aspell_string_enumeration(this->ptr);

			this->ptr = nullptr;
		}
	}

	/*
	 * COPY AND MOVE
	 */

	//! Copy constructor.
	/*!
	 * Creates a copy of the underlying
	 *  word list in the given instance,
	 *  saving it in this instance.
	 *
	 * If the other word list is invalid,
	 *  the current instance will also be
	 *  invalid.
	 *
	 * \param other The word list to copy
	 *   from.
	 */
	inline AspellList::AspellList(const AspellList& other) {
		if(other.ptr != nullptr) {
			aspell_string_enumeration_assign(this->ptr, other.ptr);
		}
	}

	//! Move constructor.
	/*!
	 * Moves the word list from the
	 *  specified location into this
	 *  instance of the class.
	 *
	 * \note The other word list will be
	 *   invalidated by this move.
	 *
	 * \param other The word list to move
	 *   from.
	 *
	 * \sa valid
	 */
	inline AspellList::AspellList(AspellList&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Copy assignment operator.
	/*!
	 * Clears the existing word list if
	 *  necessary and creates a copy of
	 *  the underlying word list in the
	 *  given instance, saving it in this
	 *  instance.
	 *
	 * \note Nothing will be done if used
	 *   on itself.
	 *
	 * \param other The word list to copy
	 *   from.
	 *
	 * \returns A reference to the class
	 *   containing the copy of the word
	 *   list (i.e. @c *this).
	 */
	inline AspellList& AspellList::operator=(const AspellList& other) {
		if(&other != this) {
			this->clear();

			aspell_string_enumeration_assign(this->ptr, other.ptr);
		}

		return *this;
	}

	//! Move assignment operator.
	/*!
	 * Moves the word list from the
	 *  specified location into this
	 *  instance of the class.
	 *
	 * \note The other word list will be
	 *   invalidated by this move.
	 *
	 * \note Nothing will be done if used on
	 *   itself.
	 *
	 * \param other The word list to move
	 *   from.
	 *
	 * \returns A reference to the instance
	 *   containing the word list after
	 *   moving (i.e. @c *this).
	 *
	 * \sa valid
	 */
	inline AspellList& AspellList::operator=(AspellList&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_ASPELLLIST_HPP_ */
