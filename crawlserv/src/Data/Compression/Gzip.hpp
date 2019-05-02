/*
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

#include <sstream>
#include <string>

namespace crawlservpp::Data::Gzip {
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

} /* crawlservpp::Data::Gzip */

#endif /* DATA_GZIP_HPP_ */
