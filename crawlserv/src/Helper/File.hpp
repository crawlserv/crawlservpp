/*
 * File.hpp
 *
 * Namespace with global helper function for file access.
 *
 *  Created on: Apr 25, 2019
 *      Author: ans
 */

#ifndef HELPER_FILE_HPP_
#define HELPER_FILE_HPP_

#include <fstream>
#include <streambuf>
#include <string>

namespace crawlservpp::Helper::File {
	/*
	 * DECLARATION
	 */

	// read file content to string
	std::string readToString(const std::string& fileName);

	/*
	 * IMPLEMENTATION
	 */
	// read file content to string
	inline std::string readToString(const std::string& fileName) {
		std::ifstream in(fileName.c_str());
		std::string result;

		// reserve memory
		in.seekg(0, std::ios::end);

		result.reserve(in.tellg());

		in.seekg(0, std::ios::beg);

		// read file to string
		result.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

		// return string
		return result;
	}

} /* crawlservpp::Helper::File */

#endif /* HELPER_FILE_HPP_ */