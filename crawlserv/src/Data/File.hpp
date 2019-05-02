/*
 * File.hpp
 *
 * Namespace with function for file access.
 *
 *  Created on: Apr 25, 2019
 *      Author: ans
 */

#ifndef DATA_FILE_HPP_
#define DATA_FILE_HPP_

#include <fstream>
#include <streambuf>
#include <string>

namespace crawlservpp::Data::File {
	/*
	 * DECLARATION
	 */

	// read file content to string
	std::string readToString(const std::string& fileName, bool binary);

	/*
	 * IMPLEMENTATION
	 */

	// read file content to string
	inline std::string readToString(const std::string& fileName, bool binary) {
		std::ifstream in(fileName.c_str(), binary ? std::ios_base::binary : std::ios_base::in);
		std::string result;

		// reserve memory
		in.seekg(0, std::ios::end);

		result.reserve(in.tellg());

		in.seekg(0, std::ios::beg);

		// write file content to string
		result.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

		// return string
		return result;
	}

} /* crawlservpp::Data::File */

#endif /* DATA_FILE_HPP_ */
