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
 * main.hpp
 *
 * Helper function for main() and additional commenting for doxygen.
 *
 *  Created on: Mai 24, 2020
 *      Author: ans
 */

#ifndef MAIN_HPP_
#define MAIN_HPP_

#include <string>	// std::string
#include <vector>	// std::vector

//! The global namespace of crawlserv++.
namespace crawlservpp {

	/*
	 * DECLARATION
	 */

	std::vector<std::string> vectorize(int argc, char ** argv);

	/*
	 * IMPLEMENTATION
	 */

	//! Vectorizes the given arguments to the program.
	/*!
	 * This function is used by main() exclusively
	 *  and accepts the same arguments exactly.
	 *
	 * \param argc The number of arguments, including the name
	 *   used when executing the program, i.e. the number of
	 *   elements in \p argv.
	 *
	 * \param argv A C-style array of C-style null-terminated
	 *   strings containing the arguments, including the name
	 *   used to call the program.
	 *
	 * \returns A vector with the given arguments to the program
	 *   as strings, including the name used to call the program.
	 */
	std::vector<std::string> vectorize(int argc, char ** argv) {
		std::vector<std::string> result;

		for(int n{0}; n < argc; ++n) {
			result.emplace_back(argv[n]);
		}

		return result;
	}
} /* namespace crawlservpp */

#endif /* MAIN_HPP_ */

/*
 * ADDITIONAL COMMENTING FOR DOXYGEN
 */

/**
 * \mainpage
 *
 * **crawlserv++ command-and-control server**
 *
 * See https://github.com/crawlserv/crawlservpp/ for the latest version of the application.
 */

//! Namespace for data compression.
/**
 * \namespace crawlservpp::Data::Compression
 */

//! Namespace for the import and export of data.
/**
 * \namespace crawlservpp::Data::ImportExport
 */

//! Namespace for global helper functions.
/**
 * \namespace crawlservpp::Helper
 */
