/*
 * ThreadOptions.h
 *
 * Basic thread options (IDs of website, URL list and configuration).
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADOPTIONS_HPP_
#define STRUCT_THREADOPTIONS_HPP_

#include "../Module/Analyzer/Algo/Enum.hpp"

namespace crawlservpp::Struct {

	struct ThreadOptions {
		unsigned long website; // website id for the thread
		unsigned long urlList; // URL list id for the thread
		unsigned long config; // configuration id for the thread

		// constructors
		ThreadOptions() : website(0), urlList(0), config(0) {}
		ThreadOptions(unsigned long setWebsite, unsigned long setUrlList, unsigned long setConfig)
				: website(setWebsite), urlList(setUrlList), config(setConfig) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_THREADOPTIONS_HPP_ */
