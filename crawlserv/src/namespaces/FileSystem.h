/*
 * FileSystem.h
 *
 * Helper functions for file system.
 *
 *  Created on: Dec 10, 2018
 *      Author: ans
 */

#ifndef SRC_NAMESPACES_FILESYSTEM_H_
#define SRC_NAMESPACES_FILESYSTEM_H_

#include <experimental/filesystem>

#include <string>
#include <vector>

namespace FileSystem {
	// directory listing
	std::vector<std::string> listFilesInPath(const std::string& pathToDir, const std::string& fileExtension);
}



#endif /* SRC_NAMESPACES_FILESYSTEM_H_ */
