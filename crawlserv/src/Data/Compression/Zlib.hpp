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

#include "../../Main/Exception.hpp"

#include <zlib.h>

#include <string>	// std::string

namespace crawlservpp::Data::Compression::Zlib {
	/*
	 * DECLARATION
	 */

	std::string compress(const std::string& content);
	std::string decompress(const std::string& compressedContent);

	/*
	 * CLASS FOR ZLIB EXCEPTIONS
	 */

	class Exception : public Main::Exception {
	public:
		Exception(const std::string& description) : Main::Exception(description) {}
		virtual ~Exception() {}
	};

	/*
	 * IMPLEMENTATION
	 */

	// compress content, throws Zlib::Exception
	inline std::string compress(const std::string& content) {
		if(content.empty())
			return "";

		z_stream control = z_stream();
		int returnValue = 0;
		char buffer[DATA_ZLIB_BUFFER_SIZE] = { 0 };
		std::string result;

		result.reserve(content.size());

		if(deflateInit(&control, Z_BEST_COMPRESSION) != Z_OK)
			throw Exception("Could not initialize zlib compression");

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
			throw Exception("zlib error: " + std::string(control.msg));

		return result;
	}

	// decompress content, throws Zlib::Exception
	inline std::string decompress(const std::string& compressedContent) {
		if(compressedContent.empty())
			return "";

		z_stream control = z_stream();
		int returnValue = 0;
		char buffer[DATA_ZLIB_BUFFER_SIZE] = { 0 };
		std::string result;

		if(inflateInit(&control) != Z_OK)
			throw Exception("Could not initialize zlib decompression");

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
			throw Exception("zlib error: " + std::string(control.msg));

		return result;
	}

} /* crawlservpp::Data::Compression::Zlib */

#endif /* DATA_ZLIB_HPP_ */
