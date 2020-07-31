/*
 * ---
 *
 *  Copyright (C) 2018 Yannick Schinko (aka BrainStone)
 *
 *  See https://stackoverflow.com/a/53365539 .
 *
 * ---
 *
 * filesystem.h
 *
 * Selects the right head for std::filesystem and aliases it to the namespace "stdfs"
 *
 *  Created on: Apr 18, 2020
 *      Author: ans
 */

#ifndef HELPER_PORTABILITY_FILESYSTEM_H_
#define HELPER_PORTABILITY_FILESYSTEM_H_

// We haven't checked which filesystem to include yet
#ifndef INCLUDE_STD_FILESYSTEM_EXPERIMENTAL

// Check for feature test macro for <filesystem>
#   if defined(__cpp_lib_filesystem)
#       define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 0

// added by Ans: Check for clang-tidy
#	elif defined(__clang_analyzer__)
#       define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 1
// End of added by Ans

// Check for feature test macro for <experimental/filesystem>
#   elif defined(__cpp_lib_experimental_filesystem)
#       define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 1

// We can't check if headers exist...
// Let's assume experimental to be safe
#   elif !defined(__has_include)
#       define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 1

// Check if the header "<filesystem>" exists
#   elif __has_include(<filesystem>)

// If we're compiling on Visual Studio and are not compiling with C++17, we need to use experimental
#       ifdef _MSC_VER

// Check and include header that defines "_HAS_CXX17"
#           if __has_include(<yvals_core.h>)
#               include <yvals_core.h>

// Check for enabled C++17 support
#               if defined(_HAS_CXX17) && _HAS_CXX17
// We're using C++17, so let's use the normal version
#                   define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 0
#               endif
#           endif

// If the marco isn't defined yet, that means any of the other VS specific checks failed, so we need to use experimental
#           ifndef INCLUDE_STD_FILESYSTEM_EXPERIMENTAL
#               define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 1
#           endif

// Not on Visual Studio. Let's use the normal version
#       else // #ifdef _MSC_VER
#           define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 0
#       endif

// Check if the header "<filesystem>" exists
#   elif __has_include(<experimental/filesystem>)
#       define INCLUDE_STD_FILESYSTEM_EXPERIMENTAL 1

// Fail if neither header is available with a nice error message
#   else
#       error Could not find system header "<filesystem>" or "<experimental/filesystem>"
#   endif

// We priously determined that we need the exprimental version
#   if INCLUDE_STD_FILESYSTEM_EXPERIMENTAL
// Include it
#       include <experimental/filesystem>

// Alias it
		namespace stdfs = std::experimental::filesystem;

// We have a decent compiler and can use the normal version
#   else
// Include it
#       include <filesystem>

// Alias it
		namespace stdfs = std::filesystem;
#   endif

#endif // #ifndef INCLUDE_STD_FILESYSTEM_EXPERIMENTAL

#endif /* HELPER_PORTABILITY_FILESYSTEM_H_ */
