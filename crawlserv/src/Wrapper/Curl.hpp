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
 * Curl.hpp
 *
 * RAII wrapper for pointer to libcurl instance, also handles global instance if necessary.
 * 	Does NOT have ownership of the pointer.
 * 	The first instance has to be destructed last.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_CURL_HPP_
#define WRAPPER_CURL_HPP_

#ifndef CRAWLSERVPP_TESTING

#include <curl/curl.h>

#else

#include "FakeCurl/FakeCurl.hpp"

#endif

#include <stdexcept>	// std::runtime_error

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for handles of the @c libcurl API.
	/*!
	 * Initializes the @c libcurl API locally and
	 *  globally, if still necessary. Automatically
	 *  resets the API on destruction, avoiding memory
	 *  leaks.
	 *
	 * At the moment, this class is used exclusively by
	 *  Network::Curl.
	 *
	 * For more information about the @c libcurl API,
	 *  see its
	 *  <a href="https://curl.haxx.se/libcurl/c/">
	 *  website</a>.
	 */
	class Curl {
	public:
		///@name Construction and Destruction
		///@{

		Curl();
		virtual ~Curl();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] CURL * get() noexcept;
		[[nodiscard]] CURL ** getPtr() noexcept;
		[[nodiscard]] bool valid() const noexcept;

		///@}
		///@name Initialization and Cleanup
		///@{

		void init();
		void clear() noexcept;

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		Curl(Curl&) = delete;

		//! Deleted copy assignment operator.
		Curl& operator=(Curl&) = delete;

		Curl(Curl&& other) noexcept;
		Curl& operator=(Curl&& other) noexcept;

		///@}

	private:
		// underlying pointer to the libcurl API
		CURL * ptr{nullptr};

		// stores whether the current instance has initialized libcurl globally
		bool localInit;

		// global initialization state of the libcurl API
		static bool globalInit;
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor initializing the @c libcurl API locally and globally, if necessary.
	/*!
	 * \throws std::runtime_error if
	 *   the initialization of the API failed.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/curl_global_init.html">
	 *  curl_global_init</a>,
	 *  <a href="https://curl.haxx.se/libcurl/c/curl_easy_init.html">
	 *  curl_easy_init</a>
	 */
	inline Curl::Curl() {
		// initialize global instance of libcurl if necessary
		if(globalInit) {
			this->localInit = false;
		}
		else {
			//NOLINTNEXTLINE(hicpp-signed-bitwise)
			const auto curlCode{curl_global_init(CURL_GLOBAL_ALL)};

			if(curlCode != CURLE_OK) {
				throw std::runtime_error(curl_easy_strerror(curlCode));
			}

			globalInit = true;

			this->localInit = true;
		}

		// initialize local instance of libcurl
		try {
			this->init();
		}
		catch(...) {
			if(this->localInit) {
				if(globalInit) {
					curl_global_cleanup();

					globalInit = false;
				}

				this->localInit = false;
			}

			throw;
		}
	}

	//! Destructor cleaning up the @c libcurl API locally and globally, if necessary.
	/*!
	 * Also globally deinitializes the @c libcurl
	 *  API if it was initialized during
	 *  construction or the reponsibility for it
	 *  received during move assignment.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/curl_global_cleanup.html">
	 *   curl_global_cleanup</a>,
	 *   <a href="https://curl.haxx.se/libcurl/c/curl_easy_cleanup.html">
	 *   curl_easy_cleanup</a>
	 */
	inline Curl::~Curl() {
		// cleanup local handle if necessary
		this->clear();

		// cleanup global instance if necessary
		if(this->localInit) {
			if(globalInit) {
				curl_global_cleanup();

				globalInit = false;
			}

			this->localInit = false;
		}
	}

	/*
	 * GETTERS
	 */

	//! Gets a pointer to the underlying @c libcurl handle.
	/*!
	 * \returns A pointer to the underlying @c
	 *   libcurl handle or @c nullptr if the
	 *   initialization failed or the handle has
	 *   already been cleared.
	 */
	inline CURL * Curl::get() noexcept {
		return this->ptr;
	}

	//! Gets a pointer to the pointer containing the address of the underlying @c libcurl handle.
	/*!
	 * \returns A pointer to the pointer containing
	 *   the address of the underlying @c libcurl
	 *   handle or a pointer to a pointer containing
	 *   @c nullptr if the initialization failed or
	 *   the handle has already been cleared.
	 */
	inline CURL ** Curl::getPtr() noexcept {
		return &(this->ptr);
	}

	//! Checks whether the underlying @c libcurl handle is valid.
	/*!
	 * \returns true, if the handle is valid.
	 *   False otherwise.
	 */
	inline bool Curl::valid() const noexcept {
		return this->ptr != nullptr;
	}

	/*
	 * INITIALIZATION AND CLEANUP
	 */

	//! Initializes the underlying @c libcurl handle.
	/*!
	 * If the underlying handle is already
	 *  initialized, it will be cleared.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/curl_easy_init.html">
	 *   curl_easy_init</a>
	 */
	inline void Curl::init() {
		this->clear();

		this->ptr = curl_easy_init();

		if(this->ptr == nullptr) {
			throw std::runtime_error("curl_easy_init() failed");
		}
	}

	//! Clears the underlying @c libcurl handle.
	/*!
	 * If the handle is not initialized, the
	 *  function will have no effect.
	 *
	 * \warning Does not clear the global
	 *   initialization of the @c libcurl API,
	 *   which will only be released on
	 *   destruction or the responsibility for
	 *   it transferred away during move
	 *   assignment.
	 *
	 * \sa <a href="https://curl.haxx.se/libcurl/c/curl_easy_cleanup.html">
	 *   curl_easy_cleanup</a>
	 */
	inline void Curl::clear() noexcept {
		if(this->ptr != nullptr) {
			curl_easy_cleanup(this->ptr);

			this->ptr = nullptr;
		}
	}

	/*
	 * COPY AND MOVE
	 */

	//! Move constructor.
	/*!
	 * Moves the @c libcurl handle from the
	 *  specified location into this instance of
	 *  the class.
	 *
	 * \note The other handle will be invalidated
	 *   by this move.
	 *
	 * \param other The @c libcurl handle to move
	 *   from.
	 *
	 * \sa valid
	 */
	inline Curl::Curl(Curl&& other) noexcept {
		this->ptr = other.ptr;

		other.ptr = nullptr;

		this->localInit = other.localInit;

		other.localInit = false;
	}

	//! Move assignment operator.
	/*!
	 * Moves the @c libcurl handle from the
	 *  specified location into this instance of
	 *  the class.
	 *
	 * The responsibility to clear the @c libcurl
	 *  API globally will be kept in this instance
	 *  or transferred from the other instance if
	 *  necessary.
	 *
	 * \note The other handle will be invalidated
	 *   by this move.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The @c libcurl handle to move
	 *   from.
	 *
	 * \returns A reference to the instance
	 *   containing the @c libcurl handle after moving
	 *   (i.e. @c *this).
	 *
	 * \sa valid
	 */
	inline Curl& Curl::operator=(Curl&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;

			// take, but also keep responsibility for global cleanup
			if(other.localInit) {
				this->localInit = true;

				other.localInit = false;
			}
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_CURL_HPP_ */
