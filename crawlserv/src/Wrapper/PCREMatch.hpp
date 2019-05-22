/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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

	class PCREMatch {
	public:
		// constructor: set pointer to nullptr
		PCREMatch(pcre2_match_data * setPtr) noexcept : ptr(setPtr) {}

		// move constructor
		PCREMatch(PCREMatch&& other) noexcept : ptr(other.ptr) {
			other.ptr = nullptr;
		}

		// destructor: free Perl-Compatible Regular Expression match if necessary
		~PCREMatch() {
			if(this->ptr)
				pcre2_match_data_free(this->ptr);
		}

		// get pointer to Perl-Compatible Regular Expression match
		pcre2_match_data * get() noexcept {
			return this->ptr;
		}

		// get const pointer to Perl-Compatible Regular Expression match
		const pcre2_match_data * get() const noexcept {
			return this->ptr;
		}

		// bool operator
		explicit operator bool() const noexcept {
			return this->ptr != nullptr;
		}

		// not operator
		bool operator!() const noexcept {
			return this->ptr == nullptr;
		}

		// move operator
		PCREMatch& operator=(PCREMatch&& other) noexcept {
			if(&other != this) {
				this->ptr = other.ptr;
				other.ptr = nullptr;
			}

			return *this;
		}

		// not copyable
		PCREMatch(PCREMatch&) = delete;
		PCREMatch& operator=(PCREMatch&) = delete;

	private:
		pcre2_match_data * ptr;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_PCREMATCH_HPP_ */
