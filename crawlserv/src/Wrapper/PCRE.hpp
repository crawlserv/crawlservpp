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

	class PCRE {
	public:
		// constructor: set pointer to nullptr
		PCRE() noexcept : ptr(nullptr) {}

		// move constructor
		PCRE(PCRE&& other) noexcept : ptr(other.ptr) {
			other.ptr = nullptr;
		}

		// destructor: free Perl-Compatible Regular Expression if necessary
		~PCRE() noexcept {
			this->reset();
		}

		// get pointer to Perl-Compatible Regular Expression
		pcre2_code * get() noexcept {
			return this->ptr;
		}

		// get const pointer to Perl-Compatible Regular Expression
		const pcre2_code * get() const noexcept {
			return this->ptr;
		}

		// reset Perl-Compatible Regular Expression
		void reset() {
			if(this->ptr) {
				pcre2_code_free(this->ptr);

				this->ptr = nullptr;
			}
		}

		// replace Perl-Compatible Regular Expression
		void reset(pcre2_code * other) {
			this->reset();

			this->ptr = other;
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
		PCRE& operator=(PCRE&& other) noexcept {
			if(&other != this) {
				this->ptr = other.ptr;

				other.ptr = nullptr;
			}

			return *this;
		}

		// not copyable
		PCRE(PCRE&) = delete;
		PCRE& operator=(PCRE&) = delete;


	private:
		pcre2_code * ptr;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_PCRE_HPP_ */
