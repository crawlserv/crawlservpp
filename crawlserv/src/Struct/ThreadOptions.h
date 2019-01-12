/*
 * ThreadOptions.h
 *
 * Basic thread options (IDs of website, URL list and configuration).
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADOPTIONS_H_
#define STRUCT_THREADOPTIONS_H_

namespace crawlservpp::Struct {
	struct ThreadOptions {
		unsigned long website; // website id for the thread
		unsigned long urlList; // URL list id for the thread
		unsigned long config; // configuration id for the thread
	};
}

#endif /* STRUCT_THREADOPTIONS_H_ */
