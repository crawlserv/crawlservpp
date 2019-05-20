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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * CurlList.hpp
 *
 * RAII wrapper for pointer to cURL list.
 *  Does NOT have ownership of the pointer!
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_CURLLIST_HPP_
#define WRAPPER_CURLLIST_HPP_

#include <curl/curl.h>

#include <stdexcept>	// std::runtime_error
#include <string>		// std::string
#include <vector>		// std::vector

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	class CurlList {
	public:
		// constructors and destructor
		CurlList() noexcept;
		CurlList(CurlList&& other) noexcept;
		~CurlList();

		// getters
		struct curl_slist * get() noexcept;
		const struct curl_slist * get() const noexcept;

		// manipulators
		void append(const CurlList& other);
		void append(const std::vector<std::string>& newElements);
		void append(const std::string& newElement);
		void reset() noexcept;

		// operators
		explicit operator bool() const noexcept;
		bool operator!() const noexcept;
		CurlList& operator=(CurlList&& other) noexcept;

		// not copyable
		CurlList(CurlList&) = delete;
		CurlList& operator=(CurlList&) = delete;

	private:
		struct curl_slist * ptr;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set pointer to nullptr
	inline CurlList::CurlList() noexcept : ptr(nullptr) {}

	// move constructor
	inline CurlList::CurlList(CurlList&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	// destructor: reset cURL list if necessary
	inline CurlList::~CurlList() {
		this->reset();
	}

	// get pointer to cURL list
	inline struct curl_slist * CurlList::get() noexcept {
		return this->ptr;
	}

	// get const pointer to cURL list
	inline const struct curl_slist * CurlList::get() const noexcept {
		return this->ptr;
	}

	// append the elements of another cURL list to the list, throws std::runtime_error
	inline void CurlList::append(const CurlList& other) {
		if(!other)
			return;

		auto item = other.ptr;

		do {
			this->append(item->data);

			item = item->next;
		}
		while(item->next);
	}

	// append the elements of a vector to the cURL list, throws std::runtime_error
	inline void CurlList::append(const std::vector<std::string>& newElements) {
		for(const auto& element : newElements)
			this->append(element);
	}

	// append element to cURL list, throws std::runtime_error
	inline void CurlList::append(const std::string& newElement) {
		const auto temp = curl_slist_append(this->ptr, newElement.c_str());

		if(!temp)
			throw std::runtime_error("curl_slist_append() failed");

		this->ptr = temp;
	}

	// reset cURL list
	inline void CurlList::reset() noexcept {
		if(this->ptr)
			curl_slist_free_all(this->ptr);

		this->ptr = nullptr;
	}

	// bool operator
	inline CurlList::operator bool() const noexcept {
		return this->ptr != nullptr;
	}

	// not operator
	inline bool CurlList::operator!() const noexcept {
		return this->ptr == nullptr;
	}

	// move assignment operator
	inline CurlList& CurlList::operator=(CurlList&& other) noexcept {
		if(&other != this) {
			this->ptr = other.ptr;
			other.ptr = nullptr;
		}

		return *this;
	}

	} /* crawlservpp::Wrapper */

#endif /* WRAPPER_CURLLIST_HPP_ */
