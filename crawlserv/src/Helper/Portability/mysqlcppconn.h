/*
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
