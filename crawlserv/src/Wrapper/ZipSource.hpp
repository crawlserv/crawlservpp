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
 * ZipSource.hpp
 *
 * Wrapper for pointer to a libzip source.
 *  Does NOT have ownership of the pointer, but takes care of its deletion!
 *
 *  Created on: Jan 4, 2021
 *      Author: ans
 */

#ifndef WRAPPER_ZIPSOURCE_HPP_
#define WRAPPER_ZIPSOURCE_HPP_

#include <zip.h>

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for sources used by libzip.
	/*!
	 * Creates the source on construction
	 *  and clears it on destruction,
	 *  avoiding memory leaks.
	 *
	 * This class is used exclusively by
	 *  functions in the
	 *  Data::Compression::Zip namespace.
	 *
	 * For more information about the
	 *  libzip library used, visit its
	 *  <a href="https://libzip.org/">website</a>.
	 *
	 * \note The class does not own the
	 *   underlying pointer, but takes care
	 *   of its deletion via API call.
	 */
	class ZipSource {
	public:
		///@name Construction and Destruction
		///@{

		ZipSource();
		ZipSource(const void * data, zip_uint64_t size);
		virtual ~ZipSource();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] zip_source_t * get() noexcept;
		[[nodiscard]] const zip_source_t * getc() const noexcept;
		[[nodiscard]] bool valid() const noexcept;
		[[nodiscard]] zip_error_t getError() const;

		///@}
		///@name Cleanup
		///@{

		void clear() noexcept;

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		ZipSource(ZipSource&) = delete;

		//! Deleted copy assignment operator.
		ZipSource& operator=(ZipSource&) = delete;

		ZipSource(ZipSource&& other) noexcept;
		ZipSource& operator=(ZipSource&& other) noexcept;

		///@}

	private:
		// underlying pointer to ZIP source
		zip_source_t * ptr{nullptr};

		// last occured error
		zip_error_t error{};
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor creating an empty source.
	inline ZipSource::ZipSource() {
		this->ptr = zip_source_buffer_create(nullptr, 0, 0, &(this->error));
	}

	//! Constructor creating a source from the given data.
	/*!
	 * \param data Pointer to the data from
	 *   which to create the source.
	 * \param size Size of the data in bytes.
	 *
	 * \warning The data must remain valid
	 *   for the lifetime of the created
	 *   source!
	 */
	inline ZipSource::ZipSource(const void * data, zip_uint64_t size) {
		this->ptr = zip_source_buffer_create(data, size, 0, &(this->error));
	}

	//! Destructor clearing the source if necessary
	inline ZipSource::~ZipSource() {
		this->clear();
	}

	//! Gets a pointer to the underlying source.
	/*!
	 * \returns A pointer to the underlying
	 *   source or @c nullptr if none is set.
	 */
	inline zip_source_t * ZipSource::get() noexcept {
		return this->ptr;
	}

	//! Gets a const pointer to the underlying source.
	/*!
	 * \returns A constant pointer to the
	 *   underlying source or @c nullptr
	 *   if none is set.
	 */
	inline const zip_source_t * ZipSource::getc() const noexcept {
		return this->ptr;
	}

	//! Checks whether the underlying source is valid.
	/*!
	 * \returns True, if the source is
	 *   valid, i.e. a pointer has been
	 *   set. False otherwise.
	 *
	 * \sa clear
	 */
	inline bool ZipSource::valid() const noexcept {
		return this->ptr != nullptr;
	}

	//! Get the last occurred error.
	/*!
	 * \returns A copy of the structure
	 *   containing the last error that
	 *   occurred while calling the
	 *   libzip API.
	 */
	inline zip_error_t ZipSource::getError() const {
		return this->error;
	}

	//! Clears the underlying source if necessary.
	/*!
	 * The source will be invalid and valid()
	 *  will return false afterwards.
	 *
	 * \note Does nothing if the underlying
	 *   source is not valid.
	 *
	 * \sa valid
	 */
	inline void ZipSource::clear() noexcept {
		if(this->ptr != nullptr) {
			zip_source_free(this->ptr);

			this->ptr = nullptr;
		}

		zip_error_fini(&(this->error));
	}

	//! Move constructor.
	/*!
	 * Moves the source from the specified
	 *  location into this instance of the
	 *  class.
	 *
	 * \note The other source will be
	 *   invalidated by this move.
	 *
	 * \param other The source to move from.
	 *
	 * \sa valid
	 */
	inline ZipSource::ZipSource(ZipSource&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Move assignment operator.
	/*!
	 * Moves the source from the specified
	 *  location into this instance of the
	 *  class.
	 *
	 * \note The other source will be
	 *   invalidated by this move.
	 *
	 * \note Nothing will be done if used
	 *   on itself.
	 *
	 * \param other The source to move from.
	 *
	 * \returns A reference to the instance
	 *   containing the source after moving
	 *   (i.e. @c *this).
	 *
	 * \sa valid
	 */
	inline ZipSource& ZipSource::operator=(ZipSource&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_ZIPSOURCE_HPP_ */
