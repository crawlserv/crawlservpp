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
 * Gzip.hpp
 *
 * Namespace for functions for gzip compression.
 *
 *  Created on: May 2, 2019
 *      Author: ans
 */

#ifndef DATA_GZIP_HPP_
#define DATA_GZIP_HPP_

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>

#include <sstream>		// std::stringstream
#include <string>		// std::string

//! Namespace for compressing and decompressing gzip.
namespace crawlservpp::Data::Compression::Gzip {

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

	//! The compression level used when compressing with gzip.
	inline constexpr auto compressionLevel{9};

	///@}

	/*
	 * IMPLEMENTATION
	 */

	//! Compresses content using gzip.
	/*!
	 * \note The string will be compressed via copying
	 *   it into a string stream, therefore a string
	 *   view is not feasible.
	 *
	 * \param content A const reference to the content
	 *   to be compressed.
	 *
	 * \returns The gzip-compressed content as copied
	 *   string or an empty string if the given content
	 *   is empty.
	 */
	inline std::string compress(const std::string& content) {
		if(content.empty()) {
			return "";
		}

		std::stringstream compressed;
		std::stringstream origin(content);

		boost::iostreams::filtering_streambuf<boost::iostreams::input> out;

		out.push(boost::iostreams::gzip_compressor(boost::iostreams::gzip_params(compressionLevel)));
		out.push(origin);

		boost::iostreams::copy(out, compressed);

		// return compressed content
		return compressed.str();
	}

	//! Decompresses gzip-compressed content.
	/*!
	 * \note The string will be decompressed via copying
	 *   it into a string stream, therefore a string view
	 *   is not feasible.
	 *
	 * \param compressedContent A const reference to the
	 *   gzip-compressed content to be decompressed.
	 *
	 * \returns The decompressed content as copied string
	 *   or an empty string if the given content is empty.
	 */
	inline std::string decompress(const std::string& compressedContent) {
		if(compressedContent.empty()) {
			return "";
		}

		std::stringstream compressed(compressedContent);
		std::stringstream decompressed;

		boost::iostreams::filtering_streambuf<boost::iostreams::input> out;

		out.push(boost::iostreams::gzip_decompressor());
		out.push(compressed);

		boost::iostreams::copy(out, decompressed);

		// return decompressed content
		return decompressed.str();
	}

} /* namespace crawlservpp::Data::Compression::Gzip */

#endif /* DATA_GZIP_HPP_ */
