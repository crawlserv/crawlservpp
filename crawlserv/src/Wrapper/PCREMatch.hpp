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
 *  Does NOT have ownership of the pointer!
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

	class PCREMatch {
	public:
		// constructors and destructor
		PCREMatch(pcre2_match_data * setPtr) noexcept;
		PCREMatch(PCREMatch&& other) noexcept;
		~PCREMatch();

		// getters
		pcre2_match_data * get() noexcept;
		const pcre2_match_data * get() const noexcept;

		// operators
		explicit operator bool() const noexcept;
		bool operator!() const noexcept;
		PCREMatch& operator=(PCREMatch&& other) noexcept;

		// not copyable
		PCREMatch(PCREMatch&) = delete;
		PCREMatch& operator=(PCREMatch&) = delete;

	private:
		pcre2_match_data * ptr;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set pointer to nullptr
	inline PCREMatch::PCREMatch(pcre2_match_data * setPtr) noexcept : ptr(setPtr) {}

	// move constructor
	inline PCREMatch::PCREMatch(PCREMatch&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	// destructor: free Perl-Compatible Regular Expression match if necessary
	inline PCREMatch::~PCREMatch() {
		if(this->ptr)
			pcre2_match_data_free(this->ptr);
	}

	// get pointer to Perl-Compatible Regular Expression match
	inline pcre2_match_data * PCREMatch::get() noexcept {
		return this->ptr;
	}

	// get const pointer to Perl-Compatible Regular Expression match
	inline const pcre2_match_data * PCREMatch::get() const noexcept {
		return this->ptr;
	}

	// boolean operator
	inline PCREMatch::operator bool() const noexcept {
		return this->ptr != nullptr;
	}

	// inverse boolean (i.e. not) operator
	inline bool PCREMatch::operator!() const noexcept {
		return this->ptr == nullptr;
	}

	// move operator
	inline PCREMatch& PCREMatch::operator=(PCREMatch&& other) noexcept {
		if(&other != this) {
			this->ptr = other.ptr;
			other.ptr = nullptr;
		}

		return *this;
	}

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_PCREMATCH_HPP_ */
