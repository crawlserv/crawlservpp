/*
 * System.hpp
 *
 * Namespace with global system helper function.
 *
 *  Created on: Apr 17, 2019
 *      Author: ans
 */

#include "../Main/Exception.hpp"

#include <array>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>

namespace crawlservpp::Helper::System {
	// execute a system command and return the stdout of the program, throws Main::Exception
	inline std::string exec(const char* cmd) {
	    std::array<char, 128> buffer;
	    std::string result;
	    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

	    if(!pipe)
	        throw Main::Exception("popen() failed");

	    while(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
	        result += buffer.data();

	    return result;
	}
}
