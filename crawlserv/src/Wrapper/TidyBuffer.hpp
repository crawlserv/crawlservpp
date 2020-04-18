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
 * TidyBuffer.hpp
 *
 * RAII wrapper for tidyhtml buffers.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_TIDYBUFFER_HPP_
#define WRAPPER_TIDYBUFFER_HPP_

#include <tidybuffio.h>

#include <string>	// std::string

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	class TidyBuffer {
	public:
		// constructor and destructor
		TidyBuffer() noexcept;
		~TidyBuffer();

		// getters
		::TidyBuffer * get();
		const ::TidyBuffer * get() const;
		std::string getString() const;

		// clearer
		void clear();

		// operators
		explicit operator bool() const noexcept;
		bool operator!() const noexcept;

		// not moveable, not copyable
		TidyBuffer(TidyBuffer&) = delete;
		TidyBuffer(TidyBuffer&&) = delete;
		TidyBuffer& operator=(TidyBuffer&) = delete;
		TidyBuffer& operator=(TidyBuffer&&) = delete;

	private:
		::TidyBuffer buffer;
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: fill buffer with zeros
	inline TidyBuffer::TidyBuffer() noexcept : buffer(::TidyBuffer()) {}

	// destructor: free buffer if necessary
	inline TidyBuffer::~TidyBuffer() {
		this->clear();
	}

	// get pointer to buffer
	inline ::TidyBuffer * TidyBuffer::get() {
		return &(this->buffer);
	}

	// get const pointer to buffer
	inline const ::TidyBuffer * TidyBuffer::get() const {
		return &(this->buffer);
	}

	// create string from buffer
	inline std::string TidyBuffer::getString() const {
		return std::string(
				reinterpret_cast<const char *>(this->buffer.bp),
				this->buffer.size
		);
	}

	// free buffer
	inline void TidyBuffer::clear() {
		if(this->buffer.bp)
			tidyBufFree(&(this->buffer));
	}

	// bool operator
	inline TidyBuffer::operator bool() const noexcept {
		return this->buffer.bp != nullptr;
	}

	// not operator
	inline bool TidyBuffer::operator!() const noexcept {
		return this->buffer.bp == nullptr;
	}

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_TIDYBUFFER_HPP_ */
