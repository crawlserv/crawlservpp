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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * mysqlcppconn.h
 *
 * Overwrite of depreceated dynamic exception specification in MySQL Connector/C++ headers.
 *
 *
 *  Created on: Mar 23, 2019
 *      Author: ans
 */

#ifndef SRC_HELPER_PORTABILITY_MYSQLCPPCONN_H_
#define SRC_HELPER_PORTABILITY_MYSQLCPPCONN_H_

#if __cplusplus >= 201703L
	// necessary includes for header
    #include <stdexcept>
    #include <string>
    #include <memory>

    // temporary removal of throw (ignoring warning by clang)
	#ifdef __clang__
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wkeyword-macro"
	#endif

    #define throw(...)

	#ifdef __clang__
		#pragma clang diagnostic pop
	#endif

    #include <cppconn/exception.h>

	// reset
    #undef throw
#endif


#endif /* SRC_HELPER_PORTABILITY_MYSQLCPPCONN_H_ */
