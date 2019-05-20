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
 * Curl.hpp
 *
 * RAII wrapper for pointer to cURL instance, also handles global instance if necessary.
 * 	Does NOT have ownership of the pointer.
 * 	The first instance has to be destructed last.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_CURL_HPP_
#define WRAPPER_CURL_HPP_

#include <curl/curl.h>

#include <stdexcept>	// std::runtime_error

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	class Curl {
	public:
		// constructors and destructor
		Curl();
		Curl(Curl&& other) noexcept;
		~Curl();

		// getters
		const CURL * get() const noexcept;
		CURL * get() noexcept;
		CURL ** getPtr() noexcept;

		// controllers
		void init();
		void reset();

		// operators
		explicit operator bool() const noexcept;
		bool operator!() const noexcept;
		Curl& operator=(Curl&& other) noexcept;

		// not copyable
		Curl(Curl&) = delete;
		Curl& operator=(Curl&) = delete;

	private:
		CURL * ptr;
		bool localInit;

		static bool globalInit;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: set pointer to nullptr, throws std::runtime_error
	inline Curl::Curl() : ptr(nullptr) {
		// initialize global instance if necessary
		if(globalInit)
			this->localInit = false;
		else {
			globalInit = true;

			this->localInit = true;

			const auto curlCode = curl_global_init(CURL_GLOBAL_ALL);

			if(curlCode != CURLE_OK)
				throw std::runtime_error(curl_easy_strerror(curlCode));
		}

		// initialize cURL
		this->init();
	}

	// move constructor
	inline Curl::Curl(Curl&& other) noexcept {
		this->ptr = other.ptr;
		other.ptr = nullptr;

		this->localInit = other.localInit;
		other.localInit = false;
	}

	// destructor: cleanup cURL if necessary
	inline Curl::~Curl() {
		this->reset();

		// cleanup global instance if necessary
		if(globalInit && this->localInit) {
			curl_global_cleanup();

			globalInit = false;

			this->localInit = false;
		}
	}

	// get const pointer to query list
	inline const CURL * Curl::get() const noexcept {
		return this->ptr;
	}

	// get non-const pointer to query list
	inline CURL * Curl::get() noexcept {
		return this->ptr;
	}

	// get non-const pointer to pointer to query list
	inline CURL ** Curl::getPtr() noexcept {
		return &(this->ptr);
	}

	// initialize cURL pointer, throws std::runtime_error
	inline void Curl::init() {
		this->ptr = curl_easy_init();

		if(!this->ptr)
			throw std::runtime_error("curl_easy_init() failed");
	}

	// reset cURL pointer
	inline void Curl::reset() {
		if(this->ptr) {
			curl_easy_cleanup(this->ptr);

			this->ptr = nullptr;
		}
	}

	// bool operator
	inline Curl::operator bool() const noexcept {
		return this->ptr != nullptr;
	}

	// not operator
	inline bool Curl::operator!() const noexcept {
		return this->ptr == nullptr;
	}

	// move assignment operator
	inline Curl& Curl::operator=(Curl&& other) noexcept {
		if(&other != this) {
			this->ptr = other.ptr;
			other.ptr = nullptr;

			this->localInit = other.localInit;
			other.localInit = false;
		}

		return *this;
	}

} /* crawlservpp::Wrapper */

#endif /* SRC_WRAPPER_CURL_HPP_ */
