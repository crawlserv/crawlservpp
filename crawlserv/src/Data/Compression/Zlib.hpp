/*
 * Zlib.hpp
 *
 * Namespace with functions for zlib compression.
 *
 *  Created on: May 2, 2019
 *      Author: ans
 */

#ifndef DATA_ZLIB_HPP_
#define DATA_ZLIB_HPP_

// hard-coded constant
#define DATA_ZLIB_BUFFER_SIZE 65536

#include <zlib.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace crawlservpp::Data::Zlib {
	/*
	 * DECLARATION
	 */

	std::string compress(const std::string& content);
	std::string decompress(const std::string& compressedContent);

	/*
	 * IMPLEMENTATION
	 */

	// compress content, throws std::runtime_error
	inline std::string compress(const std::string& content) {
		z_stream control = z_stream();
		int returnValue = 0;
		char buffer[DATA_ZLIB_BUFFER_SIZE] = { 0 };
		std::string result;

		result.reserve(content.size());

		if(deflateInit(&control, Z_BEST_COMPRESSION) != Z_OK)
			throw std::runtime_error("Could not initialize zlib compression");

		control.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(content.data()));
		control.avail_in = content.size();

		do {
			control.next_out = reinterpret_cast<Bytef *>(buffer);
			control.avail_out = DATA_ZLIB_BUFFER_SIZE;

			returnValue = deflate(&control, Z_FINISH);

			if(result.size() < control.total_out)
				result.append(buffer, control.total_out - result.size());
		}
		while(returnValue == Z_OK);

		deflateEnd(&control);

		if(returnValue != Z_STREAM_END)
			throw std::runtime_error("zlib error: " + std::string(control.msg));

		return result;
	}

	// decompress content, throws std::runtime_error
	inline std::string decompress(const std::string& compressedContent) {
		z_stream control = z_stream();
		int returnValue = 0;
		char buffer[DATA_ZLIB_BUFFER_SIZE] = { 0 };
		std::string result;

		if(inflateInit(&control) != Z_OK)
			throw std::runtime_error("Could not initialize zlib decompression");

		control.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressedContent.data()));
		control.avail_in = compressedContent.size();

		do {
			control.next_out = reinterpret_cast<Bytef *>(buffer);
			control.avail_out = DATA_ZLIB_BUFFER_SIZE;

			returnValue = inflate(&control, 0);

			if(result.size() < control.total_out)
				result.append(buffer, control.total_out - result.size());
		}
		while(returnValue == Z_OK);

		inflateEnd(&control);

		if(returnValue != Z_STREAM_END)
			throw std::runtime_error("zlib error: " + std::string(control.msg));

		return result;
	}

} /* crawlservpp::Data::File */

#endif /* DATA_ZLIB_HPP_ */
