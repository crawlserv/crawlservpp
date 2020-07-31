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
 * File.hpp
 *
 * Namespace for functions for file access.
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

//! Namespace for functions accessing files.
namespace crawlservpp::Data::File {

	/*
	 * DECLARATION
	 */

	///@name Reading and Writing
	///@{

	std::string read(const std::string& fileName, bool binary);
	void write(const std::string& fileName, const std::string& content, bool binary);

	///@}

	/*
	 * CLASS FOR FILE EXCEPTIONS
	 */

	//! Class for file exceptions.
	/*!
	 * This exception is being thrown when
	 * - A file could not be opened for reading.
	 * - A file could not be opened for writing.
	 */
	MAIN_EXCEPTION_CLASS();

	/*
	 * IMPLEMENTATION
	 */

	//! Reads the content of the given file.
	/*!
	 * \param fileName Constant reference to a string
	 *   containing the name of the file to be read.
	 * \param binary If true, the contents will be
	 *   read binarily. If false, they will be read
	 *   as text.
	 *
	 * \returns The copy of a string containing the
	 *   content of the given file.
	 *
	 * \throws File::Exception if the file could not
	 *   be opened for reading, e.g. if it does not
	 *   exist.
	 */
	inline std::string read(const std::string& fileName, bool binary) {
		// open file for reading
		std::ifstream in(
				fileName.c_str(),
				binary ? std::ios_base::binary : std::ios_base::in
		);

		if(!in.is_open()) {
			throw Exception(
					"Could not open \'"
					+ fileName
					+ "\' for "
					+ (binary ? "binary " : "")
					+ "reading"
			);
		}

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

	//! Writes the given content to the given file.
	/*!
	 * \param fileName Constant reference to a string
	 *   containing the name of the file to be written.
	 * \param content Constant reference to a string
	 *   containing the content to be written to the
	 *   given file.
	 * \param binary If true, the contents will be
	 *   written binarily. If false, they will be
	 *   written as text.
	 *
	 * \throws File::Exception if the file could not
	 *   be opened for writing.
	 */
	inline void write(const std::string& fileName, const std::string& content, bool binary) {
		// open file for writing
		std::ofstream out(
				fileName.c_str(),
				binary ? std::ios_base::binary : std::ios_base::out
		);

		if(!out.is_open()) {
			throw Exception(
					"Could not open \'"
					+ fileName
					+ "\' for "
					+ (binary ? "binary " : "")
					+ "writing"
			);
		}

		// write content to file
		out << content;

		// close file
		out.close();
	}

} /* namespace crawlservpp::Data::File */

#endif /* DATA_FILE_HPP_ */
