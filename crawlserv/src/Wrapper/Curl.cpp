/*
 * Curl.cpp
 *
 * RAII wrapper for pointer to cURL instance, also handles global instance if necessary.
 * 	Does NOT have ownership of the pointer.
 * 	The first instance has to be destructed last.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "Curl.hpp"

namespace crawlservpp::Wrapper {

	// set cURL to not initialized
	bool Curl::globalInit = false;

} /* crawlservpp::Wrapper */
