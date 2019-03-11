/*
 * FileSystem.hpp
 *
 * Namespace with global helper functions for file system.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_FILESYSTEM_HPP_
#define HELPER_FILESYSTEM_HPP_

#include <experimental/filesystem>

#include <string>
#include <vector>

namespace crawlservpp::Helper::FileSystem {

	/*
	 * DECLARATION
	 */

	// directory listing
	std::vector<std::string> listFilesInPath(const std::string& pathToDir, const std::string& fileExtension);

	/*
	 * IMPLEMENTATION
	 */

	// list files with specific extension in a directory and its subdirectories
	inline std::vector<std::string> listFilesInPath(const std::string& pathToDir,
			const std::string& fileExtension) {
		std::vector<std::string> result;

		// open path
		std::experimental::filesystem::path path(pathToDir);
		if(!std::experimental::filesystem::exists(path)) throw std::runtime_error("\'" + pathToDir + "\' does not exist");
		if(!std::experimental::filesystem::is_directory(path)) throw std::runtime_error("\'" + pathToDir + "\' is not a directory");

		// iterate through items
		for(auto& it: std::experimental::filesystem::recursive_directory_iterator(path)) {
			if(it.path().extension().string() == fileExtension) result.emplace_back(it.path().string());
		}

		return result;
	}

} /* crawlservpp::Helper::FileSystem */

#endif /* HELPER_FILESYSTEM_HPP_ */