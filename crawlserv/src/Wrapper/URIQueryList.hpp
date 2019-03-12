/*
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
		operator bool() const noexcept {
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
