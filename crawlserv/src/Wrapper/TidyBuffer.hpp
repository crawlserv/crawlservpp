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
#include <utility>	// std::move

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for buffers used by the tidy-html5 API.
	/*!
	 * Zeroes the buffer on construction and automatically
	 *  clears it on destruction, avoiding memory leaks.
	 *
	 * At the moment, this class is used exclusively by TidyDoc.
	 */
	class TidyBuffer {
	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		TidyBuffer() = default;

		virtual ~TidyBuffer();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] ::TidyBuffer * get() noexcept;
		[[nodiscard]] const ::TidyBuffer * getc() const noexcept;
		[[nodiscard]] std::string getString() const noexcept;
		[[nodiscard]] bool valid() const noexcept;
		[[nodiscard]] std::size_t size() const noexcept;
		[[nodiscard]] bool empty() const noexcept;
		[[nodiscard]] std::size_t capacity() const noexcept;

		///@}
		///@name Cleanup
		///@{

		void clear() noexcept;

		///@}
		/**@name Copy and Move
		 * The class is both copyable and moveable.
		 */
		///@{

		TidyBuffer(const TidyBuffer& other);
		TidyBuffer& operator=(const TidyBuffer& other);
		TidyBuffer(TidyBuffer&& other) noexcept;
		TidyBuffer& operator=(TidyBuffer&& other) noexcept;

		///@}

	private:
		::TidyBuffer buffer{};
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Destructor clearing the underlying buffer if necessary.
	inline TidyBuffer::~TidyBuffer() {
		this->clear();
	}

	//! Gets a pointer to the underlying buffer.
	/*!
	 * \returns A pointer to the underlying TidyBuffer instance.
	 */
	inline ::TidyBuffer * TidyBuffer::get() noexcept {
		return &(this->buffer);
	}

	//! Gets a const pointer to the underlying buffer.
	/*!
	 * \returns A const pointer to the underlying TidyBuffer instance.
	 */
	inline const ::TidyBuffer * TidyBuffer::getc() const noexcept {
		return &(this->buffer);
	}

	//! Copies the content of the underlying buffer into a string.
	/*!
	 * \returns A string copy of the contents of the underlying buffer
	 *  or an empty string if the buffer is invalid (or empty).
	 */
	inline std::string TidyBuffer::getString() const noexcept {
		if(this->buffer.bp == nullptr || this->buffer.size == 0) {
			return "";
		}

		return std::string(
				static_cast<const char *>(static_cast<void *>(this->buffer.bp)),
				this->buffer.size
		);
	}

	//! Checks whether the underlying buffer is valid.
	/*!
	 * \returns True, if the buffer is valid. False otherwise.
	 *
	 * \note Also returns true if the underlying buffer is empty, but valid.
	 *
	 * \sa empty, clear
	 */
	inline bool TidyBuffer::valid() const noexcept {
		return this->buffer.bp != nullptr;
	}

	//! Gets the current size of the content in the underlying buffer in bytes.
	/*!
	 * \returns The size of the content
	 *  in the underlying buffer in bytes,
	 *  i.e. the bytes it currently uses.
	 *
	 * \note The return value is zero
	 *  if the buffer has not been initialized.
	 *
	 * \sa capacity
	 */
	inline std::size_t TidyBuffer::size() const noexcept {
		return this->buffer.size;
	}

	//! Gets the current capacity of the underlying buffer in bytes.
	/*!	 *
	 * \note The size() of the actual content in the buffer,
	 *   i.e. the number of used bytes, might be smaller.
	 *
	 * \note The return value is zero
	 *   if the buffer has not been initialized.
	 *
	 * \returns The number of bytes currently allocated
	 *   for the underlying buffer.
	 */
	inline std::size_t TidyBuffer::capacity() const noexcept {
		return this->buffer.allocated;
	}

	//! Checks whether the underlying buffer is empty.
	/*!
	 * \returns True, if the buffer is empty or invalid. False otherwise.
	 *
	 * \sa valid
	 */
	inline bool TidyBuffer::empty() const noexcept {
		return this->buffer.bp == nullptr || this->buffer.size == 0;
	}

	//! Frees the underlying buffer.
	/*!
	 * The buffer will be invalid and valid() will return false afterwards.
	 *
	 * \note Does nothing if the underlying buffer is not initialized.
	 */
	inline void TidyBuffer::clear() noexcept {
		if(this->buffer.bp != nullptr) {
			tidyBufFree(&(this->buffer)); // (also sets buffer structure to zeroes)
		}
	}

	//! Copy constructor.
	/*!
	 * Allocates the same amount of memory as
	 *  the capacity() of the other buffer
	 *  and copies its content.
	 *
	 * \note Uses the allocator used in other,
	 *  or the default allocator if none can be detected.
	 *
	 * \param other The buffer to copy from.
	 */
	inline TidyBuffer::TidyBuffer(const TidyBuffer& other) : buffer{} {
		if(other.valid()) {
			if(other.buffer.allocator != nullptr) {
				::tidyBufAllocWithAllocator(
						&(this->buffer),
						other.buffer.allocator,
						other.capacity()
				);
			}
			else {
				::tidyBufAlloc(
						&(this->buffer),
						other.capacity()
				);
			}

			if(!other.empty()) {
				::tidyBufAppend(
						&(this->buffer),
						other.buffer.bp,
						other.size()
				);
			}
		}
	}

	//! Copy assignment operator.
	/*!
	 * Clears the existing buffer, allocates
	 *  the same amount of memory as capacity() of
	 *  the other buffer and copies its content.
	 *
	 * \note Uses the allocator used in other,
	 *  or the default allocator if none can be detected.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The buffer to copy from.
	 *
	 * \returns A reference to the class containing
	 *   the copy of the buffer (i.e. *this).
	 */
	inline TidyBuffer& TidyBuffer::operator=(const TidyBuffer& other) {
		if(this != &other) {
			// clear old buffer if necessary
			this->clear();

			if(other.valid()) {
				// allocate memory for the copy
				if(other.buffer.allocator != nullptr) {
					::tidyBufAllocWithAllocator(
							&(this->buffer),
							other.buffer.allocator,
							other.capacity()
					);
				}
				else {
					::tidyBufAlloc(
							&(this->buffer),
							other.capacity()
					);
				}

				// copy the bytes
				if(!other.empty()) {
					::tidyBufAppend(
							&(this->buffer),
							other.buffer.bp,
							other.size()
					);
				}
			}
		}

		return *this;
	}

	//! Move constructor.
	/*!
	 * Moves the buffer from the specified location
	 *  into this instance of the class.
	 *
	 * \note The other buffer will be invalidated by this move.
	 *
	 * \param other The buffer to move from.
	 *
	 * \sa valid
	 */
	inline TidyBuffer::TidyBuffer(TidyBuffer&& other) noexcept : buffer{} {
		this->buffer = other.buffer;

		other.buffer.allocator = nullptr;
		other.buffer.bp = nullptr;
		other.buffer.size = 0;
		other.buffer.allocated = 0;
		other.buffer.next = 0;
	}

	//! Move assignment operator.
	/*!
	 * Moves the buffer from the specified location
	 *  into this instance of the class.
	 *
	 * \note The other buffer will be invalidated by this move.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The buffer to move from.
	 *
	 * \returns A reference to the instance containing
	 *   the buffer after moving (i.e. *this).
	 *
	 * \sa valid
	 */
	inline TidyBuffer& TidyBuffer::operator=(TidyBuffer&& other) noexcept {
		if(this != &other) {
			this->clear();

			this->buffer = other.buffer;

			other.buffer.allocator = nullptr;
			other.buffer.bp = nullptr;
			other.buffer.size = 0;
			other.buffer.allocated = 0;
			other.buffer.next = 0;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_TIDYBUFFER_HPP_ */
