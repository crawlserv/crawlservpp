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

		// constructors
		ThreadOptions() { this->website = 0; this->urlList = 0; this->config = 0; }
		ThreadOptions(unsigned long setWebsite, unsigned long setUrlList, unsigned long setConfig) {
			this->website = setWebsite;
			this->urlList = setUrlList;
			this->config = setConfig;
		}
	};
}

#endif /* STRUCT_THREADOPTIONS_H_ */
