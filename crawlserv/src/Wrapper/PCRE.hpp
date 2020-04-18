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
 *  Does NOT have ownership of the pointer!
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

	class PCRE {
	public:
		// constructors and destructor
		PCRE() noexcept;
		PCRE(PCRE&& other) noexcept;
		~PCRE() noexcept;

		// getters
		pcre2_code * get() noexcept;
		const pcre2_code * get() const noexcept;

		// resetters
		void reset();
		void reset(pcre2_code * other);

		// operators
		explicit operator bool() const noexcept;
		bool operator!() const noexcept;

		// move operator
		PCRE& operator=(PCRE&& other) noexcept;

		// not copyable
		PCRE(PCRE&) = delete;
		PCRE& operator=(PCRE&) = delete;

	private:
		pcre2_code * ptr;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set pointer to nullptr
	inline PCRE::PCRE() noexcept : ptr(nullptr) {}

	// move constructor
	inline PCRE::PCRE(PCRE&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	// destructor: free Perl-Compatible Regular Expression if necessary
	inline PCRE::~PCRE() noexcept {
		this->reset();
	}

	// get pointer to Perl-Compatible Regular Expression
	inline pcre2_code * PCRE::get() noexcept {
		return this->ptr;
	}

	// get const pointer to Perl-Compatible Regular Expression
	inline const pcre2_code * PCRE::get() const noexcept {
		return this->ptr;
	}

	// reset Perl-Compatible Regular Expression
	inline void PCRE::reset() {
		if(this->ptr) {
			pcre2_code_free(this->ptr);

			this->ptr = nullptr;
		}
	}

	// replace Perl-Compatible Regular Expression
	inline void PCRE::reset(pcre2_code * other) {
		this->reset();

		this->ptr = other;
	}

	// boolean operator
	inline PCRE::operator bool() const noexcept {
		return this->ptr != nullptr;
	}

	// inverse boolean (i.e. not) operator
	inline bool PCRE::operator!() const noexcept {
		return this->ptr == nullptr;
	}

	// move operator
	inline PCRE& PCRE::operator=(PCRE&& other) noexcept {
		if(&other != this) {
			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_PCRE_HPP_ */
