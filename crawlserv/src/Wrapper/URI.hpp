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
 * URI.hpp
 *
 * Wrapper for pointer to URI structure.
 *  Structure is only created when needed.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_URI_HPP_
#define WRAPPER_URI_HPP_

#include <uriparser/Uri.h>

#include <memory>	// std::make_unique, std::unique_ptr
#include <utility>	// std::move

namespace crawlservpp::Wrapper {

	class URI {
	public:
		// constructor stub
		URI() noexcept {}

		// move constructor
		URI(URI&& other) noexcept : ptr(std::move(other.ptr)) {}

		// destructor: free and reset URI structure if necessary
		~URI() {
			this->reset();
		}

		// get pointer to URI structure
		UriUriA * get() noexcept {
			return this->ptr.get();
		}

		// get const pointer to URI structure
		const UriUriA * get() const noexcept {
			return this->ptr.get();
		}

		// create URI structure, free old structure if necessary
		void create() {
			this->reset();
			this->ptr = std::make_unique<UriUriA>();
		}

		// free and reset URI structure if necessary
		void reset() {
			if(this->ptr) {
				uriFreeUriMembersA(this->ptr.get());
				this->ptr.reset();
			}
		}

		// bool operator
		explicit operator bool() const noexcept {
			return this->ptr.operator bool();
		}

		// not operator
		bool operator!() const noexcept {
			return !(this->ptr);
		}

		// move assignment operator
		URI& operator=(URI&& other) noexcept {
			if(&other != this)
				this->ptr = std::move(other.ptr);

			return *this;
		}

		// not copyable
		URI(URI&) = delete;
		URI& operator=(URI&) = delete;

	private:
		std::unique_ptr<UriUriA> ptr;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_URI_HPP_ */
