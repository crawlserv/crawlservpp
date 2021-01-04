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
 * PCRE.hpp
 *
 * Wrapper for pointer to Perl-Compatible Regular Expression.
 *  Does NOT have ownership of the pointer, but takes care of its deletion!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_PCRE_HPP_
#define WRAPPER_PCRE_HPP_

#include <pcre2.h>

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for Perl-compatible regular expressions.
	/*!
	 * Sets an empty pointer on construction and clears the RegEx
	 *  on destruction if necessary, avoiding memory leaks.
	 *
	 * At the moment, this class is used exclusively by the
	 *  Query::Regex class.
	 *
	 * For more information about the %PCRE library used, visit its
	 *  <a href="https://www.pcre.org/">website</a>.
	 *
	 * \note The class does not own the underlying pointer,
	 *   but takes care of its deletion via API call.
	 */
	class PCRE {
	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor
		PCRE() = default;

		explicit PCRE(pcre2_code * regExPtr) noexcept;
		virtual ~PCRE();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] pcre2_code * get() noexcept;
		[[nodiscard]] const pcre2_code * getc() const noexcept;
		[[nodiscard]] bool valid() const noexcept;

		///@}
		///@name Setter
		///@{

		void set(pcre2_code * regExPtr);

		///@}
		///@name Cleanup
		///@{

		void clear() noexcept;

		///@}
		/**@name Copy and Move
		 * The class is both copyable and moveable.
		 */
		///@{

		PCRE(const PCRE& other);
		PCRE& operator=(const PCRE& other);
		PCRE(PCRE&& other) noexcept;
		PCRE& operator=(PCRE&& other) noexcept;

		///@}

	private:
		// underlying pointer to regular expression
		pcre2_code * ptr{nullptr};
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor setting the underlying pointer.
	/*!
	 * \param regExPtr The address to which the underlying pointer should be set.
	 *   Can either be @c nullptr or a valid pointer to a compiled regular expression.
	 *
	 * \note The underlying pointer will be cleared in-class via API call.
	 *
	 * \sa set
	 */
	inline PCRE::PCRE(pcre2_code * regExPtr) noexcept : ptr(regExPtr) {}

	//! Destructor freeing the underlying regular expression if necessary.
	inline PCRE::~PCRE() {
		this->clear();
	}

	//! Gets a pointer to the underlying regular expression.
	/*!
	 * \returns A pointer to the underlying regular expression
	 *   or @c nullptr if none is set.
	 */
	inline pcre2_code * PCRE::get() noexcept {
		return this->ptr;
	}

	//! Gets a const pointer to the underlying regular expression.
	/*!
	 * \returns A const pointer to the underlying regular expression
	 *   or @c nullptr if none is set.
	 */
	inline const pcre2_code * PCRE::getc() const noexcept {
		return this->ptr;
	}

	//! Checks whether the underlying regular expression is valid.
	/*!
	 * \returns True, if the regular expression is valid,
	 *   i.e. a pointer has been set. False otherwise.
	 *
	 * \sa clear
	 */
	inline bool PCRE::valid() const noexcept {
		return this->ptr != nullptr;
	}

	//! Sets a PERL-compatibe regular expression.
	/*!
	 * If another expression is already managed by the class,
	 *  it will be cleared.
	 *
	 * \param regExPtr A pointer to the new regular expression to be
	 *  stored or @c nullptr to just clear the old expression if necessary.
	 *
	 *  \note The underlying pointer will be cleared in-class via API call.
	 */
	inline void PCRE::set(pcre2_code * regExPtr) {
		this->clear();

		this->ptr = regExPtr;
	}

	//! Clears the underlying regular expression if necessary.
	/*!
	 * The regular expression will be invalid and valid()
	 *  will return false afterwards.
	 *
	 * \note Does nothing if the underlying regular expression
	 *   is not valid.
	 *
	 * \sa valid
	 */
	inline void PCRE::clear() noexcept {
		if(this->ptr != nullptr) {
			pcre2_code_free(this->ptr);

			this->ptr = nullptr;
		}
	}

	//! Copy constructor.
	/*!
	 * Creates a copy of the underlying regular expression
	 *  in the given instance, saving it in this instance.
	 *
	 * Both pattern code points and character tables are copied.
	 *
	 * If the other regular expression is invalid, the
	 *  current instance will also be invalid.
	 *
	 * \note Uses the same allocator used in other.
	 *
	 * \warning JIT (jut-in-time) information cannot be copied
	 *   and needs to be re-compiled if needed.
	 *
	 * \param other The regular expression to copy from.
	 */
	inline PCRE::PCRE(const PCRE& other) {
		this->ptr = pcre2_code_copy_with_tables(other.getc());
	}

	//! Copy assignment operator.
	/*!
	 * Clears the existing regular expression if necessary
	 *  and creates a copy of the underlying regular expression
	 *  in the given instance, saving it in this instance.
	 *
	 * \note Uses the same allocator used in other.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \warning JIT (jut-in-time) information cannot be copied
	 *   and needs to be re-compiled if needed.
	 *
	 * \param other The regular expression to copy from.
	 *
	 * \returns A reference to the class containing
	 *   the copy of the regular expression (i.e. *this).
	 */
	inline PCRE& PCRE::operator=(const PCRE& other) {
		if(&other != this) {
			this->clear();

			this->ptr = pcre2_code_copy_with_tables(other.getc());
		}

		return *this;
	}

	//! Move constructor.
	/*!
	 * Moves the regular expression from the specified
	 *  location into this instance of the class.
	 *
	 * \note The other expression will be invalidated by this move.
	 *
	 * \param other The regular expression to move from.
	 *
	 * \sa valid
	 */
	inline PCRE::PCRE(PCRE&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Move assignment operator.
	/*!
	 * Moves the regular expression from the specified
	 *  location into this instance of the class.
	 *
	 * \note The other expression will be invalidated by this move.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The regular expression to move from.
	 *
	 * \returns A reference to the instance containing
	 *   the regular expression after moving (i.e. *this).
	 *
	 * \sa valid
	 */
	inline PCRE& PCRE::operator=(PCRE&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_PCRE_HPP_ */
