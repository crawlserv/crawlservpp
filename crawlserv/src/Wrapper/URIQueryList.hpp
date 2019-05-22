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

	class URIQueryList {
	public:
		// constructor: set pointer to nullptr
		URIQueryList() noexcept : ptr(nullptr) {}

		// move constructor
		URIQueryList(URIQueryList&& other) noexcept : ptr(other.ptr) {
			other.ptr = nullptr;
		}

		// destructor: free query list if necessary
		~URIQueryList() {
			if(this->ptr) uriFreeQueryListA(this->ptr);
		}

		// get pointer to URI query list
		UriQueryListA * get() noexcept {
			return this->ptr;
		}

		// get const pointer to URI query list
		const UriQueryListA * get() const noexcept {
			return this->ptr;
		}

		// get pointer to pointer to URI query list
		UriQueryListA ** getPtr() noexcept {
			return &(this->ptr);
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
		URIQueryList& operator=(URIQueryList&& other) noexcept {
			if(&other != this) {
				this->ptr = other.ptr;
				other.ptr = nullptr;
			}

			return *this;
		}

		// not copyable
		URIQueryList(URIQueryList&) = delete;
		URIQueryList& operator=(URIQueryList&) = delete;

	private:
		UriQueryListA * ptr;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_URIQUERYLIST_HPP_ */
