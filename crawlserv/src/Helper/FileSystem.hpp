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
 * FileSystem.hpp
 *
 * Namespace for global file system helper functions.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_FILESYSTEM_HPP_
#define HELPER_FILESYSTEM_HPP_

#include "../Main/Exception.hpp"

#include "Portability/filesystem.h"

#include <algorithm>	// std::equal
#include <cstdint>		// std::uintmax_t
#include <iterator>		// std::distance
#include <string>		// std::string
#include <string_view>	// std::string_view
#include <vector>		// std::vector

//! Namespace for global file system helper functions.
namespace crawlservpp::Helper::FileSystem {

	/*
	 * DECLARATION
	 */

	///@name Existence and Validity
	///@{

	bool exists(std::string_view path);
	bool isValidDirectory(std::string_view path);
	bool isValidFile(std::string_view path);

	///@}
	///@name Paths and Directories
	///@{

	char getPathSeparator();
	std::vector<std::string> listFilesInPath(
			std::string_view pathToDir,
			std::string_view fileExtension
	);
	bool contains(std::string_view pathToDir, std::string_view pathToCheck);
	void createDirectory(std::string_view pathToDir);
	void createDirectoryIfNotExists(std::string_view pathToDir);
	void clearDirectory(std::string_view pathToDir);

	///@}
	///@name Disk Space
	///@{

	std::uintmax_t getFreeSpace(std::string_view path);

	///@}

	/*
	 * CLASS FOR FILE SYSTEM EXCEPTIONS
	 */

	//! Class for file system exceptions.
	/*!
	 * This exception is being thrown when
	 * - a file system error occurs while checking the
	 *    existence or validity of a file or directory
	 * - the given path does to be analyzed does not exist
	 * - a directory could not be created
	 * - a directory could not be cleared or one of its
	 *    contents could not be removed
	 * - free disk space in the file system could not
	 *    be determined for the specified path
	 */
	MAIN_EXCEPTION_CLASS();

	/*
	 * IMPLEMENTATION
	 */

	//! Checks whether the specified path exists.
	/*!
	 * \param path A string view containing the path to be
	 *   checked for existence.
	 *
	 * \returns True if the path exists. False otherwise.
	 *
	 * \throws FileSystem::Exception if the existence of the path
	 *   could not be checked.
	 */
	inline bool exists(std::string_view path) {
		try {
			return stdfs::exists(path);
		}
		catch(const stdfs::filesystem_error& e) {
			std::string exceptionString{"Could not check the existence of the path '"};

			exceptionString += path;
			exceptionString += "': ";
			exceptionString += e.what();

			throw Exception(exceptionString);
		}
	}

	//! Checks whether the given path points to a valid directory.
	/*!
	 * \param path A string view containing the path to check.
	 *
	 * \returns True if the path points to a valid directory.
	 *   False otherwise.
	 *
	 * \throws FileSystem::Exception if the validity of the directory
	 *   could not be checked.
	 */
	inline bool isValidDirectory(std::string_view path) {
		const stdfs::path dir(path);

		try {
			return stdfs::exists(dir) && stdfs::is_directory(dir);
		}
		catch(const stdfs::filesystem_error& e) {
			std::string exceptionString{"Could not check the existence of the directory '"};

			exceptionString += path;
			exceptionString += "': ";
			exceptionString += e.what();

			throw Exception(exceptionString);
		}
	}

	//! Checks whether the given path points to a valid file.
	/*!
	 * \param path A string view containing the path to check.
	 *
	 * \returns True if the path points to a valid file.
	 *   False otherwise.
	 *
	 * \throws FileSystem::Exception if the validity of the file
	 *   could not be checked.
	 */
	inline bool isValidFile(std::string_view path) {
		const stdfs::path file(path);

		try {
			return stdfs::exists(file) && stdfs::is_regular_file(file);
		}
		catch(const stdfs::filesystem_error& e) {
			std::string exceptionString{"Could not check the validity of the file '"};

			exceptionString += path;
			exceptionString += "': ";
			exceptionString += e.what();

			throw Exception(exceptionString);
		}
	}

	//! Gets the preferred separator for file paths in the current operating system.
	/*!
	 * \returns The preferred separator for file paths
	 *   in the current operating system.
	 */
	inline char getPathSeparator() {
		return stdfs::path::preferred_separator;
	}

	//! Lists all files with the given extension in the given directory and its subdirectories.
	/*!
	 * \note While the given directory (@c pathToDir) needs to exist,
	 *   the given path to check (@c pathToCheck) does not.
	 * \param pathToDir A string view containing the directory to
	 *   search for files.
	 * \param fileExtension A string view containing the extension
	 *   of the files to search for. If empty, all files will be
	 *   returned.
	 *
	 * \returns A vector of strings containing the paths of the
	 *   files with the given extension in the given directory
	 *   and its subdirectories.
	 *
	 * \throws FileSystem::Exception if the given path is invalid
	 *   or does not exist, or when the iteration over all the
	 *   contents of the directory has failed.
	 */
	inline std::vector<std::string> listFilesInPath(
			std::string_view pathToDir,
			std::string_view fileExtension
	) {
		std::vector<std::string> result;

		// open path
		const stdfs::path path(pathToDir);

		if(!stdfs::exists(path)) {
			std::string exceptionString{"'"};

			exceptionString += pathToDir;
			exceptionString += "' does not exist";

			throw Exception(exceptionString);
		}

		if(!stdfs::is_directory(path)) {
			std::string exceptionString{"'"};

			exceptionString += pathToDir;
			exceptionString += "' is not a directory";

			throw Exception(exceptionString);
		}

		// iterate through items
		try {
			for(const auto& it: stdfs::recursive_directory_iterator(path)) {
				if(fileExtension.empty() || it.path().extension().string() == fileExtension) {
					result.emplace_back(it.path().string());
				}
			}
		}
		catch(const stdfs::filesystem_error& e) {
			std::string exceptionString{"Could not iterate over the files in '"};

			exceptionString += pathToDir;
			exceptionString += "': ";
			exceptionString += e.what();

			throw Exception(exceptionString);
		}

		return result;
	}

	//! Checks whether the given path is located inside the given directory.
	/*!
	 * \param pathToDir A string view containing the directory in which
	 *   the given path should be located.
	 * \param pathToCheck A string view containing the path which should
	 *   be located inside the directory.
	 *
	 * \returns True, if the given path is or would be located inside the
	 *   given directory, including its subdirectories. False otherwise.
	 *
	 * \throws FileSystem::Exception if the given directory does not exist,
	 *   or the path to check contains an unsupported symlink.
	 */
	inline bool contains(std::string_view pathToDir, std::string_view pathToCheck) {
		if(!FileSystem::isValidDirectory(pathToDir)) {
			std::string exceptionString{"'"};

			exceptionString += pathToDir;
			exceptionString += "' is not a valid directory";

			throw Exception(exceptionString);
		}

		// make paths absolute
		try {
			const auto absPathToDir{
				stdfs::canonical(pathToDir)
			};

#ifndef INCLUDE_STD_FILESYSTEM_EXPERIMENTAL
			// create weakly canonical path
			auto absPathToCheck{
				stdfs::weakly_canonical(pathToCheck)
			};
#else
			// create approximate canonical path – no symlinks supported!
			auto completePathToCheck{
				stdfs::absolute(pathToCheck)
			};

			stdfs::path absPathToCheck;

			for(const auto& segment : completePathToCheck) {
				if(segment == "..") {
					absPathToCheck = absPathToCheck.parent_path();
				}
				else if(segment == ".") {
					continue;
				}
				else {
					absPathToCheck /= segment;
				}

				if(stdfs::is_symlink(absPathToCheck)) {
					std::string exceptionString{
						"Path contains unsupported symlink: '"
					};

					exceptionString += absPathToCheck;
					exceptionString += "'";

					throw Exception(exceptionString);
				}
			}
#endif

			// remove filename if necessary
			if(absPathToCheck.has_filename()) {
				absPathToCheck.remove_filename();
			}

			// compare number of path components
			if(
					std::distance(absPathToDir.begin(), absPathToDir.end())
					> std::distance(absPathToCheck.begin(), absPathToCheck.end())
			) {
				// pathToCheck cannot be contained in a path with more components
				return false;
			}

			// compare as many components as there are in pathToDir
			return std::equal(absPathToDir.begin(), absPathToDir.end(), absPathToCheck.begin());
		}
		catch(const stdfs::filesystem_error& e) {
			std::string exceptionString{
				"Could not make paths absolute: '"
			};

			exceptionString += pathToDir;
			exceptionString += "' and '";
			exceptionString += pathToCheck;
			exceptionString += "': ";
			exceptionString += e.what();

			throw Exception(exceptionString);
		}
	}

	//! Creates a directory at the given path.
	/*!
	 * \param pathToDir A string view containing the path
	 *   to the directory to be created.
	 *
	 * \throws FileSystem::Exception if the directory
	 *   could not be created.
	 *
	 * \sa createDirectoryIfNotExists
	 */
	inline void createDirectory(std::string_view pathToDir) {
		try {
			stdfs::create_directory(pathToDir);
		}
		catch(const stdfs::filesystem_error& e) {
			std::string exceptionString{"Could not create directory '"};

			exceptionString += pathToDir;
			exceptionString += "': ";
			exceptionString += e.what();

			throw Exception(exceptionString);
		}
	}

	//! Creates a directory at the given path, if it does not exist already.
	/*!
	 * \param pathToDir A string view containing the path
	 *   to the directory to be created, if it does not exist already
	 *
	 * \throws FileSystem::Exception if the directory
	 *   does not exist, but could not be created.
	 *
	 * \sa createDirectory
	 */
	inline void createDirectoryIfNotExists(std::string_view pathToDir) {
		if(!FileSystem::isValidDirectory(pathToDir)) {
			FileSystem::createDirectory(pathToDir);
		}
	}

	//! Deletes all files and folders in the given directory.
	/*!
	 * \param pathToDir A string view containing a path to the
	 *   directory which contents should be deleted.
	 *
	 * \throws FileSystem::Exception if the given path does not
	 *   point to a valid directory or one of its contents
	 *   could not be removed.
	 */
	inline void clearDirectory(std::string_view pathToDir) {
		if(!FileSystem::isValidDirectory(pathToDir)) {
			std::string exceptionString{"'"};

			exceptionString += pathToDir;
			exceptionString += "' is not a valid directory";

			throw Exception(exceptionString);
		}

		const stdfs::path dir(pathToDir);

		for(stdfs::directory_iterator it(pathToDir), endIt; it != endIt; ++it) {
			try {
				stdfs::remove_all(it->path());
			}
			catch(const stdfs::filesystem_error& e) {
				std::string exceptionString{"Could not remove '"};

				exceptionString += it->path();
				exceptionString += "' with all its subdirectories: ";
				exceptionString += e.what();

				throw Exception(exceptionString);
			}
		}
	}

	//! Gets the available disk space at the given location in bytes.
	/*!
	 * \param path A string view containing a path to the directory
	 *   for which the available disk space should be determined.
	 *
	 * \returns The available disk space at the given location in bytes.
	 *
	 * \throws FileSystem::Exception if the available disk space could
	 *   not be dettermined at the given location.
	 */
	inline std::uintmax_t getFreeSpace(std::string_view path) {
		try {
			return stdfs::space(path).available;
		}
		catch(const stdfs::filesystem_error& e) {
			std::string exceptionString{"Could not get the available disk space at '"};

			exceptionString += path;
			exceptionString += "': ";
			exceptionString += e.what();

			throw Exception(exceptionString);
		}
	}

} /* namespace crawlservpp::Helper::FileSystem */

#endif /* HELPER_FILESYSTEM_HPP_ */
