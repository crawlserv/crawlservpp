/*
 * ThreadOptions.h
 *
 * Basic thread options (IDs of website, URL list, algorithm and configuration).
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADOPTIONS_H_
#define STRUCT_THREADOPTIONS_H_

#include "../Module/Analyzer/Algo/List.h"

namespace crawlservpp::Struct {
	struct ThreadOptions {
		unsigned long website; // website id for the thread
		unsigned long urlList; // URL list id for the thread
		crawlservpp::Module::Analyzer::Algo::List algo; // algorithm for the thread (used by analyzer threads only)
		unsigned long config; // configuration id for the thread

		// default values
		ThreadOptions() {
			this->website = 0;
			this->urlList = 0;
			this->algo = crawlservpp::Module::Analyzer::Algo::List::none;
			this->config = 0;
		}
	};
}

#endif /* STRUCT_THREADOPTIONS_H_ */
