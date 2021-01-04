/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * Zip.hpp
 *
 * Namespace for functions for zip compression.
 *
 *  Created on: Jan 4, 2021
 *      Author: ans
 */

#ifndef DATA_ZIP_HPP_
#define DATA_ZIP_HPP_

#include "../../Main/Exception.hpp"
#include "../../Wrapper/ZipArchive.hpp"

#include <zip.h>

#include <string>	// std::string
#include <utility>	// std::pair
#include <vector>	// std::vector

//! Namespace for compressing and decompressing zip.
namespace crawlservpp::Data::Compression::Zip {

	/*
	 * DECLARATION
	 */

	//! A pair of strings.
	using StringString = std::pair<std::string, std::string>;

	///@name Compression and Decompression
	///@{

	std::string compress(const std::vector<StringString>& fileContents);
	std::vector<StringString> decompress(const std::string& compressedContent);

	/*
	 * CLASS FOR ZIP EXCEPTIONS
	 */

	//! Class for zip exceptions.
	/*!
	 * This exception is being thrown when the
	 *  libzip library generates an error.
	 *
	 * \sa compress, decompress
	 */
	MAIN_EXCEPTION_CLASS();

	/*
	 * IMPLEMENTATION
	 */

	//! Compresses files using zip.
	/*!
	 * \param content A const reference to a vector
	 * 	 containing pairs of strings containing the
	 * 	 file names and contents of all the files
	 * 	 to be compressed.
	 *
	 * \returns The zip-compressed content as string.
	 *
	 * \throws Zip::Exception if the libzip library
	 *   generates an error.
	 */
	inline std::string compress(const std::vector<StringString>& fileContents) {
		// create ZIP archive
		Wrapper::ZipArchive archive;

		if(!archive.valid()) {
			throw Exception("Zip::compress(): " + archive.getError());
		}

		// add files
		for(const auto& fileContent : fileContents) {
			if(!archive.addFile(fileContent.first, fileContent.second, false)) {
				throw Exception("Zip::compress(): " + archive.getError());
			}
		}

		// close archive and dump compressed content
		std::string result;

		archive.close(result);

		return result;
	}

	//! Decompresses zip-compressed content.
	/*!
	 * \param compressed A const reference to the
	 *   zip-compressed content to be decompressed.
	 *
	 * \returns The decompressed files as pairs of
	 *   strings containing their names and their
	 *   contents, or an empty pair if the given
	 *   content is empty.
	 *
	 * \throws Zip::Exception if the libzip library
	 *   generates an error.
	 *
	 * \todo Not implemented yet.
	 */
	inline std::vector<StringString> decompress(const std::string& compressed) {
		if(compressed.empty()) {
			return {};
		}

		//TODO Not implemented yet.

		return {};
	}

} /* namespace crawlservpp::Data::Compression::Zip */

#endif /* DATA_ZIP_HPP_ */
