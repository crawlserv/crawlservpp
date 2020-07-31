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
 * PCREMatch.hpp
 *
 * RAII wrapper for pointer to Perl-Compatible Regular Expression match.
 *  Does NOT have ownership of the pointer, but takes care of its deletion!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_PCREMATCH_HPP_
#define WRAPPER_PCREMATCH_HPP_

#include <pcre2.h>

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for Perl-compatible regular expression matches.
	/*!
	 * Sets the RegEx match on construction and clears it
	 *  on destruction, avoiding memory leaks.
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
	class PCREMatch {
	public:
		///@name Construction and Destruction
		///@{

		explicit PCREMatch(pcre2_match_data * setPtr) noexcept;
		virtual ~PCREMatch();

		///@}
		///@name Getters
		///@{

		// getters
		[[nodiscard]] pcre2_match_data * get() noexcept;
		[[nodiscard]] const pcre2_match_data * getc() const noexcept;
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
		PCREMatch(PCREMatch&) = delete;

		//! Deleted copy assignment operator.
		PCREMatch& operator=(PCREMatch&) = delete;

		PCREMatch(PCREMatch&& other) noexcept;
		PCREMatch& operator=(PCREMatch&& other) noexcept;

		///@}

	private:
		// underlying pointer to regular expression match
		pcre2_match_data * ptr;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor setting the underlying regular expression.
	/*!
	 * \param setPtr The pointer to a regular expression match to be used
	 *   or @c nullptr to create an invalid match.
	 *
	 * \note The underlying pointer will be cleared in-class via API call.
	 */
	inline PCREMatch::PCREMatch(pcre2_match_data * setPtr) noexcept : ptr(setPtr) {}

	//! Destructor clearing the underlying regular expression if necessary.
	inline PCREMatch::~PCREMatch() {
		this->clear();
	}

	//! Gets a pointer to the underlying regular expression match.
	/*!
	 * \returns A pointer to the underlying regular expression match.
	 */
	inline pcre2_match_data * PCREMatch::get() noexcept {
		return this->ptr;
	}

	//! Gets a const pointer to the underlying regular expression match.
	/*!
	 * \returns A const pointer to the underlying regular expression match.
	 */
	inline const pcre2_match_data * PCREMatch::getc() const noexcept {
		return this->ptr;
	}

	//! Checks whether the underlying regular expression match is valid.
	/*!
	 * \returns True, if the regular expression match is valid.
	 *   False otherwise.
	 */
	inline bool PCREMatch::valid() const noexcept {
		return this->ptr != nullptr;
	}

	//! Clears the underlying regular expression match if necessary.
	inline void PCREMatch::clear() noexcept {
		if(this->ptr != nullptr) {
			pcre2_match_data_free(this->ptr);

			this->ptr = nullptr;
		}
	}

	//! Move constructor.
	/*!
	 * Moves the regular expression match from the
	 *  specified location into this instance of the class.
	 *
	 * \note The other match will be invalidated by this move.
	 *
	 * \param other The regular expression match to move from.
	 *
	 * \sa valid
	 */
	inline PCREMatch::PCREMatch(PCREMatch&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Move assignment operator.
	/*!
	 * Moves the regular expression match from the
	 *  specified location into this instance of the class.
	 *
	 * \note The other match will be invalidated by this move.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The regular expression match to move from.
	 *
	 * \returns A reference to the instance containing the
	 *   regular expression match after moving (i.e. *this).
	 *
	 * \sa valid
	 */
	inline PCREMatch& PCREMatch::operator=(PCREMatch&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_PCREMATCH_HPP_ */
