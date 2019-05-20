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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * FileSystem.hpp
 *
 * Namespace with global helper functions for file system.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_FILESYSTEM_HPP_
#define HELPER_FILESYSTEM_HPP_

#include "../Main/Exception.hpp"

#include <experimental/filesystem>

#include <string>	// std::string
#include <vector>	// std::vector

// for convenience
namespace std {

	namespace filesystem = experimental::filesystem;

}

namespace crawlservpp::Helper::FileSystem {

	/*
	 * DECLARATION
	 */

	// existence checks
	bool exists(const std::string& path);
	bool isValidDirectory(const std::string& path);
	bool isValidFile(const std::string& path);

	// directory functions
	char getPathSeparator();
	std::vector<std::string> listFilesInPath(const std::string& pathToDir, const std::string& fileExtension);
	bool contains(const std::string& pathToDir, const std::string& pathToCheck);
	void createDirectory(const std::string& pathToDir);
	void createDirectoryIfNotExists(const std::string& pathToDir);
	void clearDirectory(const std::string& pathToDir);
	unsigned long getFreeSpace(const std::string& path);

	/*
	 * CLASS FOR FILE SYSTEM EXCEPTIONS
	 */

	class Exception : public Main::Exception {
	public:
		Exception(const std::string& description) : Main::Exception(description) {}
		virtual ~Exception() {}
	};

	/*
	 * IMPLEMENTATION
	 */

	// check whether the specified path exists
	inline bool exists(const std::string& path) {
		return std::filesystem::exists(path);
	}

	// check whether the specified path points to a valid directory
	inline bool isValidDirectory(const std::string& path) {
		const std::filesystem::path dir(path);

		return std::filesystem::exists(dir) && std::filesystem::is_directory(dir);
	}

	// check whether the specified path points to a valid file
	inline bool isValidFile(const std::string& path) {
		const std::filesystem::path file(path);

		return std::filesystem::exists(file) && std::filesystem::is_regular_file(file);
	}

	// get the preferred separator for file paths
	inline char getPathSeparator() {
		return std::filesystem::path::preferred_separator;
	}

	// list files with specific extension in a directory and its subdirectories, throws FileSystem::Exception
	inline std::vector<std::string> listFilesInPath(
			const std::string& pathToDir,
			const std::string& fileExtension
	) {
		std::vector<std::string> result;

		// open path
		const std::filesystem::path path(pathToDir);

		if(!std::filesystem::exists(path))
			throw Exception("\'" + pathToDir + "\' does not exist");

		if(!std::filesystem::is_directory(path))
			throw Exception("\'" + pathToDir + "\' is not a directory");

		// iterate through items
		for(auto& it: std::filesystem::recursive_directory_iterator(path)) {
			if(it.path().extension().string() == fileExtension)
				result.emplace_back(it.path().string());
		}

		return result;
	}

	// check whether a path is located inside directory (including its subdirectories), throws FileSystem::Exception
	//  NOTE: While the directory needs to exist, the path does not
	inline bool contains(const std::string& pathToDir, const std::string& pathToCheck) {
		if(!std::filesystem::exists(pathToDir))
			throw Exception("\'" + pathToDir + "\' does not exist");

		if(!std::filesystem::is_directory(pathToDir))
			throw Exception("\'" + pathToDir + "\' is not a directory");

		// make both paths absolute
		const std::filesystem::path absPathToDir(std::filesystem::system_complete(pathToDir));
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

	// create directory
	inline void createDirectory(const std::string& pathToDir) {
		std::filesystem::create_directory(pathToDir);
	}

	// create directory if it does not exists already
	inline void createDirectoryIfNotExists(const std::string& pathToDir) {
		if(!isValidDirectory(pathToDir))
			createDirectory(pathToDir);
	}

	// delete all files and folders in a directory, throws FileSystem::Exception
	inline void clearDirectory(const std::string& pathToDir) {
		if(!std::filesystem::exists(pathToDir))
			throw Exception("\'" + pathToDir + "\' does not exist");

		if(!std::filesystem::is_directory(pathToDir))
			throw Exception("\'" + pathToDir + "\' is not a directory");

		const std::filesystem::path dir(pathToDir);

		for(std::filesystem::directory_iterator it(pathToDir), endIt; it != endIt; ++it)
			std::filesystem::remove_all(it->path());
	}

	// get the free disk space for a directory (in bytes)
	inline unsigned long getFreeSpace(const std::string& path) {
		return std::filesystem::space(path).available;
	}

} /* crawlservpp::Helper::FileSystem */

#endif /* HELPER_FILESYSTEM_HPP_ */
