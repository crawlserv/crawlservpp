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
 * System.hpp
 *
 * Namespace for global system helper function.
 *
 *  Created on: Apr 17, 2019
 *      Author: ans
 */

#include "../Helper/Portability/pipe.h"

#include "../Main/Exception.hpp"

#include <array>	// std::array
#include <cstdio>	// ::fgets, FILE, pclose, popen
#include <memory>	// std::unique_ptr
#include <string>	// std::string

//! Namespace for global system helper functions.
namespace crawlservpp::Helper::System {

	///@name Constants
	///@{

	//! The length of the buffer for executing system commands.
	inline constexpr auto cmdBufferLength{128};

	///@}

	//! Executes a system command and returns the stdout of the program.
	/*!
	 * At the moment, this function is used exclusively by
	 *  Helper::Portability::enumLocales().
	 *
	 * \warning Not compliant with
	 *   <a href="https://www.securecoding.cert.org/confluence/pages/viewpage.action?pageId=2130132">
	 *   ENV33-C. Do not call system()</a>!
	 *
	 * \returns A string containing the output the program has written to stdout.
	 *
	 * \throws Main::Exception if the execution of the command failed.
	 */
	inline std::string exec(const char* cmd) {
	    std::array<char, cmdBufferLength> buffer{};
	    std::string result;

	    // NOLINTNEXTLINE(cert-env33-c)
	    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

	    if(!pipe) {
	        throw Main::Exception("popen() failed");
	    }

	    while(::fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
	        result += buffer.data();
	    }

	    return result;
	}

} /* namespace crawlservpp::Helper::System */
