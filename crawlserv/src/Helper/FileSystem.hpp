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

// for convenience
namespace std {
	namespace filesystem = experimental::filesystem;
}

namespace crawlservpp::Helper::FileSystem {
	/*
	 * DECLARATION
	 */

	// existence checks
	bool isValidDirectory(const std::string& path);
	bool isValidFile(const std::string& path);

	// directory listing
	std::vector<std::string> listFilesInPath(const std::string& pathToDir, const std::string& fileExtension);

	/*
	 * IMPLEMENTATION
	 */

	// check whether the specified path points to a valid directory
	inline bool isValidDirectory(const std::string& path) {
		std::filesystem::path dir(path);

		return std::filesystem::exists(dir) && std::filesystem::is_directory(dir);
	}

	// check whether the specified path points to a valid file
	inline bool isValidFile(const std::string& path) {
		std::filesystem::path file(path);

		return std::filesystem::exists(file) && std::filesystem::is_regular_file(file);
	}

	// list files with specific extension in a directory and its subdirectories
	inline std::vector<std::string> listFilesInPath(const std::string& pathToDir,
			const std::string& fileExtension) {
		std::vector<std::string> result;

		// open path
		std::filesystem::path path(pathToDir);
		if(!std::filesystem::exists(path)) throw std::runtime_error("\'" + pathToDir + "\' does not exist");
		if(!std::filesystem::is_directory(path)) throw std::runtime_error("\'" + pathToDir + "\' is not a directory");

		// iterate through items
		for(auto& it: std::filesystem::recursive_directory_iterator(path)) {
			if(it.path().extension().string() == fileExtension) result.emplace_back(it.path().string());
		}

		return result;
	}

} /* crawlservpp::Helper::FileSystem */

#endif /* HELPER_FILESYSTEM_HPP_ */
