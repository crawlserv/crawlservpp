/*
 * FileSystem.h
 *
 * Namespace with global helper functions for file system.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef HELPER_FILESYSTEM_H_
#define HELPER_FILESYSTEM_H_

#include <experimental/filesystem>

#include <string>
#include <vector>

namespace Helper::FileSystem {
	// directory listing
	std::vector<std::string> listFilesInPath(const std::string& pathToDir, const std::string& fileExtension);
}

#endif /* HELPER_FILESYSTEM_H_ */
