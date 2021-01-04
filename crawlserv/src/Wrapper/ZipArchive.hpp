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
 * ZipArchive.hpp
 *
 * Wrapper for pointer to a libzip archive.
 *  Does NOT have ownership of the pointer, but takes care of its deletion!
 *
 *  Created on: Jan 4, 2021
 *      Author: ans
 */

#ifndef WRAPPER_ZIPARCHIVE_HPP_
#define WRAPPER_ZIPARCHIVE_HPP_

#include "ZipSource.hpp"

#include <zip.h>

#include <string>	// std::string

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for ZIP archives used by libzip.
	/*!
	 * Creates the archive on construction
	 *  and closes it on destruction,
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
	class ZipArchive {
	public:
		///@name Construction and Destruction
		///@{

		ZipArchive();
		virtual ~ZipArchive();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] struct zip * get() noexcept;
		[[nodiscard]] const struct zip * getc() const noexcept;
		[[nodiscard]] bool valid() const noexcept;
		[[nodiscard]] std::string getError();

		///@}
		///@name Archive Functionality
		///@{

		bool addEmptyDirectory(const std::string& name);

		bool addFile(
				const std::string& fileName,
				const std::string& content,
				bool overwrite
		);

		///@}
		///@name Cleanup
		///@{

		void close() noexcept;
		void close(std::string& dumpTo);

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		ZipArchive(ZipArchive&) = delete;

		//! Deleted copy assignment operator.
		ZipArchive& operator=(ZipArchive&) = delete;

		ZipArchive(ZipArchive&& other) noexcept;
		ZipArchive& operator=(ZipArchive&& other) noexcept;

		///@}

	private:
		// underlying source
		ZipSource source;

		// underlying pointer to ZIP archive
		struct zip * ptr{nullptr};

		// last occured error
		zip_error_t error{};

		// internal static helper function
		static void copyError(const zip_error_t * from, zip_error_t& to);
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor creating an archive from an empty source.
	inline ZipArchive::ZipArchive() {
		// check whether empty source has been created
		if(!(this->source.valid())) {
			this->error = this->source.getError();

			return;
		}

		// create archive from empty source
		this->ptr = zip_open_from_source(this->source.get(), ZIP_TRUNCATE, &(this->error));

		if(this->ptr != nullptr) {
			// keep the source (to be cleared by its wrapper)
			zip_source_keep(this->source.get());
		}
	}

	//! Destructor clearing the archive if necessary
	inline ZipArchive::~ZipArchive() {
		this->close();
	}

	/*
	 * GETTERS
	 */

	//! Gets a pointer to the underlying archive.
	/*!
	 * \returns A pointer to the underlying
	 *   archive or @c nullptr if none is set.
	 */
	inline struct zip * ZipArchive::get() noexcept {
		return this->ptr;
	}

	//! Gets a const pointer to the underlying archive.
	/*!
	 * \returns A constant pointer to the
	 *   underlying archive or @c nullptr
	 *   if none is set.
	 */
	inline const struct zip * ZipArchive::getc() const noexcept {
		return this->ptr;
	}

	//! Checks whether the underlying archive is valid.
	/*!
	 * \returns True, if the archive is
	 *   valid, i.e. a pointer has been
	 *   set. False otherwise.
	 *
	 * \sa clear
	 */
	inline bool ZipArchive::valid() const noexcept {
		return this->ptr != nullptr;
	}

	//! Get the last occurred error as string.
	/*!
	 * \returns A string containing a
	 *   description of the last error
	 *   that occurred while calling the
	 *   libzip API.
	 */
	inline std::string ZipArchive::getError() {
		return zip_error_strerror(&(this->error));
	}

	/*
	 * ARCHIVE FUNCTIONALITY
	 */

	//! Adds an empty directory to the archive.
	/*!
	 * \param name The name of the
	 *   directory to add to the archive.
	 *
	 * \returns True on success. False,
	 *   if the archive is invalid or
	 *   the adding failed. If it failed,
	 *   use getError() to find out why.
	 *
	 * \note It is not necessary to add
	 *   directories that contain files
	 *   before adding these files.
	 *
	 * \sa addFile
	 */
	inline bool ZipArchive::addEmptyDirectory(const std::string& name) {
		if(this->ptr == nullptr) {
			return false;
		}

		const auto result{
			zip_add_dir(this->ptr, name.c_str())
		};

		if(result < 0) {
			// copy error from archive
			ZipArchive::copyError(
					zip_get_error(this->ptr),
					this->error
			);

			return false;
		}

		return true;
	}

	//! Adds a file to the archive.
	/*!
	 * \param name The name of the file
	 *   to add to the archive.
	 * \param content The content of the
	 *   file to add to the archive.
	 * \param overwrite Specifies whether
	 *   to overwrite an already existing
	 *   file with the same name.
	 *
	 * \returns True on success. False,
	 *   if the archive is invalid or
	 *   the adding failed. If it failed,
	 *   use getError() to find out why.
	 *
	 * \note Automatically creates the
	 *   directories contained in the
	 *   file names.
	 */
	inline bool ZipArchive::addFile(
			const std::string& name,
			const std::string& content,
			bool overwrite
	) {
		if(this->ptr == nullptr) {
			return false;
		}

		ZipSource fileSource(content.data(), content.size());

		// keep the source (to be cleared by its wrapper)
		zip_source_keep(fileSource.get());

		const auto result{
			zip_file_add(
					this->ptr,
					name.c_str(),
					fileSource.get(),
					ZIP_FL_ENC_UTF_8 | overwrite ? ZIP_FL_OVERWRITE : 0
			)
		};

		if(result < 0) {
			// decrement reference count as it has not been used on error
			zip_source_free(fileSource.get());

			ZipArchive::copyError(zip_get_error(this->ptr), this->error);

			return false;
		}

		return true;
	}

	/*
	 * CLEANUP
	 */

	//! Closes the underlying archive if necessary.
	/*!
	 * The archive will be invalid and valid()
	 *  will return false afterwards.
	 *
	 * \note Does nothing if the underlying
	 *   archive is not valid.
	 *
	 * \sa valid
	 */
	inline void ZipArchive::close() noexcept {
		if(this->ptr != nullptr) {
			zip_close(this->ptr);

			this->ptr = nullptr;
		}

		zip_error_fini(&(this->error));
	}

	//! Closes the underlying archive and dumps its contents.
	/*!
	 * \param dumpTo Reference to a string
	 *   into which the contents of the
	 *   archive will be written.
	 *
	 * \note Does nothing if the underlying
	 *   archive is not valid.
	 *
	 * \sa valid
	 */
	inline void ZipArchive::close(std::string& dumpTo) {
		if(this->ptr == nullptr) {
			return;
		}

		this->close();

		if(this->source.valid()) {
			zip_source_open(this->source.get());

			zip_source_seek(this->source.get(), 0, SEEK_END);

			const zip_int64_t size{
				zip_source_tell(this->source.get())
			};

			zip_source_seek(this->source.get(), 0, SEEK_SET);

			dumpTo.resize(size);

			zip_source_read(
					this->source.get(),
					static_cast<void*>(dumpTo.data()),
					size
			);
		}
	}

	/*
	 * COPY AND MOVE
	 */

	//! Move constructor.
	/*!
	 * Moves the archive from the specified
	 *  location into this instance of the
	 *  class.
	 *
	 * \note The other archive will be
	 *   invalidated by this move.
	 *
	 * \param other The archive to move from.
	 *
	 * \sa valid
	 */
	inline ZipArchive::ZipArchive(ZipArchive&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Move assignment operator.
	/*!
	 * Moves the archive from the specified
	 *  location into this instance of the
	 *  class.
	 *
	 * \note The other archive will be
	 *   invalidated by this move.
	 *
	 * \note Nothing will be done if used
	 *   on itself.
	 *
	 * \param other The archive to move from.
	 *
	 * \returns A reference to the instance
	 *   containing the archive after moving
	 *   (i.e. @c *this).
	 *
	 * \sa valid
	 */
	inline ZipArchive& ZipArchive::operator=(ZipArchive&& other) noexcept {
		if(&other != this) {
			this->close();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTION (private)
	 */

	// copy error
	inline void ZipArchive::copyError(const zip_error_t * from, zip_error_t& to) {
		zip_error_fini(&to);
		zip_error_init(&to);

		to.sys_err = from->sys_err;
		to.zip_err = from->zip_err;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_ZIPARCHIVE_HPP_ */
