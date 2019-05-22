/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * File.hpp
 *
 * Namespace with function for file access.
 *
 *  Created on: Apr 25, 2019
 *      Author: ans
 */

#ifndef DATA_FILE_HPP_
#define DATA_FILE_HPP_

#include "../Main/Exception.hpp"

#include <fstream>		// std::ifstream, std::ofstream
#include <ios>			// std::ios_base
#include <streambuf>	// std::istreambuf_iterator
#include <string>		// std::string

namespace crawlservpp::Data::File {

	/*
	 * DECLARATION
	 */

	std::string read(const std::string& fileName, bool binary);
	void write(const std::string& fileName, const std::string& content, bool binary);

	/*
	 * CLASS FOR FILE EXCEPTIONS
	 */

	MAIN_EXCEPTION_CLASS();

	/*
	 * IMPLEMENTATION
	 */

	// read file content to string, throws File::Exception
	inline std::string read(const std::string& fileName, bool binary) {
		// open file for reading
		std::ifstream in(fileName.c_str(), binary ? std::ios_base::binary : std::ios_base::in);

		if(!in.is_open())
			throw Exception(
					"Could not open \'"
					+ fileName
					+ "\' for "
					+ (binary ? "binary " : "")
					+ "reading"
			);

		// reserve memory
		std::string result;

		in.seekg(0, std::ios::end);

		result.reserve(in.tellg());

		in.seekg(0, std::ios::beg);

		// write file content to string
		result.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

		// close file
		in.close();

		// return string
		return result;
	}

	// write content to file, throws File::Exception
	inline void write(const std::string& fileName, const std::string& content, bool binary) {
		// open file for writing
		std::ofstream out(fileName.c_str(), binary ? std::ios_base::binary : std::ios_base::out);

		if(!out.is_open())
			throw Exception(
					"Could not open \'"
					+ fileName
					+ "\' for "
					+ (binary ? "binary " : "")
					+ "writing"
			);

		// write content to file
		out << content;

		// close file
		out.close();
	}

} /* crawlservpp::Data::File */

#endif /* DATA_FILE_HPP_ */
