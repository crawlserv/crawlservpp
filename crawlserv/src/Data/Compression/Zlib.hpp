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
 * Zlib.hpp
 *
 * Namespace for functions for zlib compression.
 *
 *  Created on: May 2, 2019
 *      Author: ans
 */

#ifndef DATA_ZLIB_HPP_
#define DATA_ZLIB_HPP_

#include "../../Main/Exception.hpp"

#include <zlib.h>

#include <array>	// std::array
#include <string>	// std::string

//! Namespace for compressing and decompressing zlib.
namespace crawlservpp::Data::Compression::Zlib {

	/*
	 * DECLARATION
	 */

	///@name Compression and Decompression
	///@{

	std::string compress(const std::string& content);
	std::string decompress(const std::string& compressedContent);

	///@}
	///@name Constant
	///@{

	//! The maximum buffer size for zlib compression and decompression.
	inline constexpr auto bufferSize{65536};

	///@}

	/*
	 * CLASS FOR ZLIB EXCEPTIONS
	 */

	//! Class for zlib exceptions.
	/*!
	 * This exception is being thrown when
	 * - zlib compression could not be initialized
	 * - zlib decompression could not be initialized
	 * - compressing content using zlib failed
	 * - decompressing zlib-compressed content failed
	 *
	 * \sa compress, decompress
	 */
	MAIN_EXCEPTION_CLASS();

	/*
	 * IMPLEMENTATION
	 */

	//! Compresses content using zlib.
	/*!
	 * \note The string will be compressed via copying
	 *   it into a string stream, therefore a string
	 *   view is not feasible.
	 *
	 * \param content A const reference to the content
	 *   to be compressed.
	 *
	 * \returns The zlib-compressed content as copied
	 *   string or an empty string if the given content
	 *   is empty.
	 *
	 * \throws Zlib::Exception if zlib compression could
	 *   not be initialized or an error occured while
	 *   compressing the given content using zlib.
	 */
	inline std::string compress(const std::string& content) {
		if(content.empty()) {
			return "";
		}

		z_stream control{z_stream()};
		int returnValue{};
		std::array<char, bufferSize> buffer{};
		std::string result;

		result.reserve(content.size());

		if(deflateInit(&control, Z_BEST_COMPRESSION) != Z_OK) {
			throw Exception(
					"Data::Compression::Zlib::compress():"
					" Could not initialize zlib compression"
			);
		}

		control.next_in = reinterpret_cast<const Bytef *>(
				content.data()
		);
		control.avail_in = content.size();

		do {
			control.next_out = reinterpret_cast<Bytef *>(buffer.data());
			control.avail_out = buffer.size();

			returnValue = deflate(&control, Z_FINISH);

			if(result.size() < control.total_out) {
				result.append(buffer.data(), control.total_out - result.size());
			}
		}
		while(returnValue == Z_OK);

		deflateEnd(&control);

		if(returnValue != Z_STREAM_END) {
			throw Exception(
					"Data::Compression::Zlib::compress():"
					" zlib error: "
					+ std::string(control.msg)
			);
		}

		return result;
	}

	//! Decompresses zlib-compressed content.
	/*!
	 * \note The string will be decompressed via copying
	 *   it into a string stream, therefore a string view
	 *   is not feasible.
	 *
	 * \param compressedContent A const reference to the
	 *   zlib-compressed content to be decompressed.
	 *
	 * \returns The decompressed content as copied string
	 *   or an empty string if the given content is empty.
	 *
	 * \throws Zlib::Exception if zlib decompression could
	 *   not be initialized or an error occured while
	 *   decompressing the given zlib-compressed content.
	 */
	inline std::string decompress(const std::string& compressedContent) {
		if(compressedContent.empty()) {
			return "";
		}

		z_stream control{z_stream()};
		int returnValue{};
		std::array<char, bufferSize> buffer{};
		std::string result;

		if(inflateInit(&control) != Z_OK) {
			throw Exception(
					"Data::Compression::Zlib::decompress():"
					" Could not initialize zlib decompression"
			);
		}

		control.next_in = reinterpret_cast<const Bytef *>(
				compressedContent.data()
		);
		control.avail_in = compressedContent.size();

		do {
			control.next_out = reinterpret_cast<Bytef *>(buffer.data());
			control.avail_out = buffer.size();

			returnValue = inflate(&control, 0);

			if(result.size() < control.total_out) {
				result.append(buffer.data(), control.total_out - result.size());
			}
		}
		while(returnValue == Z_OK);

		inflateEnd(&control);

		if(returnValue != Z_STREAM_END) {
			throw Exception(
					"Data::Compression::Zlib::decompress():"
					" zlib error: "
					+ std::string(control.msg)
			);
		}

		return result;
	}

} /* namespace crawlservpp::Data::Compression::Zlib */

#endif /* DATA_ZLIB_HPP_ */
