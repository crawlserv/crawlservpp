/*
 *
 * ---
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
 * Namespace with global system helper function.
 *
 *  Created on: Apr 17, 2019
 *      Author: ans
 */

#include "../Helper/Portability/pipe.h"

#include "../Main/Exception.hpp"

#include <array>	// std::array
#include <cstdio>	// ::fgets, FILE, pclose, pipe, popen
#include <memory>	// std::unique_ptr
#include <string>	// std::string

namespace crawlservpp::Helper::System {

	// execute a system command and return the stdout of the program, throws Main::Exception
	inline std::string exec(const char* cmd) {
	    std::array<char, 128> buffer;
	    std::string result;
	    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

	    if(!pipe)
	        throw Main::Exception("popen() failed");

	    while(::fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
	        result += buffer.data();

	    return result;
	}
}
