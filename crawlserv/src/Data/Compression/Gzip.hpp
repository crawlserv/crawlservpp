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
 * Namespace with functions for gzip compression.
 *
 *  Created on: May 2, 2019
 *      Author: ans
 */

#ifndef DATA_GZIP_HPP_
#define DATA_GZIP_HPP_

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include <sstream>	// std::stringstream
#include <string>	// std::string

namespace crawlservpp::Data::Compression::Gzip {

	/*
	 * DECLARATION
	 */

	std::string compress(const std::string& content);
	std::string decompress(const std::string& compressedContent);

	/*
	 * IMPLEMENTATION
	 */

	// compress content
	inline std::string compress(const std::string& content) {
		std::stringstream compressed;
		std::stringstream origin(content);

		boost::iostreams::filtering_streambuf<boost::iostreams::input> out;

		out.push(boost::iostreams::gzip_compressor(boost::iostreams::gzip_params(9)));
		out.push(origin);

		boost::iostreams::copy(out, compressed);

		// return compressed content
		return compressed.str();
	}

	// decompress content
	inline std::string decompress(const std::string& compressedContent) {
		std::stringstream compressed(compressedContent);
		std::stringstream decompressed;

		boost::iostreams::filtering_streambuf<boost::iostreams::input> out;

		out.push(boost::iostreams::gzip_decompressor());
		out.push(compressed);

		boost::iostreams::copy(out, decompressed);

		// return decompressed content
		return decompressed.str();
	}

} /* crawlservpp::Data::Compression::Gzip */

#endif /* DATA_GZIP_HPP_ */
