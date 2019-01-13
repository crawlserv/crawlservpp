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

#include "../Module/Analyzer/Algo/Enum.h"

namespace crawlservpp::Struct {
	struct ThreadOptions {
		unsigned long website; // website id for the thread
		unsigned long urlList; // URL list id for the thread
		unsigned long config; // configuration id for the thread

		// default values
		ThreadOptions() {
			this->website = 0;
			this->urlList = 0;
			this->config = 0;
		}
	};
}

#endif /* STRUCT_THREADOPTIONS_H_ */
