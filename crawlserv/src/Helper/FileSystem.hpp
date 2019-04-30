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

	// directory functions
	std::vector<std::string> listFilesInPath(const std::string& pathToDir, const std::string& fileExtension);
	bool contains(const std::string& pathToDir, const std::string& pathToCheck);
	void clearDirectory(const std::string& pathToDir);
	std::string getPathSeparator();

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
	inline std::vector<std::string> listFilesInPath(
			const std::string& pathToDir,
			const std::string& fileExtension
	) {
		std::vector<std::string> result;

		// open path
		std::filesystem::path path(pathToDir);

		if(!std::filesystem::exists(path))
			throw std::runtime_error("\'" + pathToDir + "\' does not exist");

		if(!std::filesystem::is_directory(path))
			throw std::runtime_error("\'" + pathToDir + "\' is not a directory");

		// iterate through items
		for(auto& it: std::filesystem::recursive_directory_iterator(path)) {
			if(it.path().extension().string() == fileExtension) result.emplace_back(it.path().string());
		}

		return result;
	}

	// check whether file or directory is located inside directory (including its subdirectories)
	inline bool contains(const std::string& pathToDir, const std::string& pathToCheck) {
		if(!std::filesystem::exists(pathToDir))
			throw std::runtime_error("\'" + pathToDir + "\' does not exist");

		if(!std::filesystem::is_directory(pathToDir))
			throw std::runtime_error("\'" + pathToDir + "\' is not a directory");

		// make both paths absolute
		std::filesystem::path absPathToDir(std::filesystem::system_complete(pathToDir));
		std::filesystem::path absPathToCheck(std::filesystem::system_complete(pathToCheck));

		// remove filename if necessary
		if(absPathToCheck.has_filename())
			absPathToCheck.remove_filename();

		// compare number of path components
		if(
				std::distance(absPathToDir.begin(), absPathToDir.end())
				> std::distance(absPathToCheck.begin(), absPathToCheck.end())
		)
			// pathToCheck cannot be contained in a path with more components
			return false;

		// compare as many components as there are in pathToDir
		return std::equal(pathToDir.begin(), pathToDir.end(), pathToCheck.begin());
	}

	// delete all files and folders in a directory
	inline void clearDirectory(const std::string& pathToDir) {
		if(!std::filesystem::exists(pathToDir))
			throw std::runtime_error("\'" + pathToDir + "\' does not exist");

		if(!std::filesystem::is_directory(pathToDir))
			throw std::runtime_error("\'" + pathToDir + "\' is not a directory");

		std::filesystem::path dir(pathToDir);

		for(std::filesystem::directory_iterator it(pathToDir), endIt; it != endIt; ++it)
			std::filesystem::remove_all(it->path());
	}

	// get the preferred separator for file paths
	inline std::string getPathSeparator() {
		return std::string(1, std::filesystem::path::preferred_separator);
	}

} /* crawlservpp::Helper::FileSystem */

#endif /* HELPER_FILESYSTEM_HPP_ */
