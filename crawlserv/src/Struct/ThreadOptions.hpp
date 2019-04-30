/*
 * ThreadOptions.hpp
 *
 * Basic thread options (IDs of website, URL list and configuration).
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADOPTIONS_HPP_
#define STRUCT_THREADOPTIONS_HPP_

#include <string>

namespace crawlservpp::Struct {

	struct ThreadOptions {
		std::string module; // module of the thread

		unsigned long website; // website ID for the thread
		unsigned long urlList; // URL list ID for the thread
		unsigned long config; // configuration ID for the thread

		// constructors
		ThreadOptions() // create empty thread options
				: website(0),
				  urlList(0),
				  config(0) {}

		ThreadOptions( // create thread options
				const std::string& setModule,
				unsigned long setWebsite,
				unsigned long setUrlList,
				unsigned long setConfig)
				: module(setModule),
				  website(setWebsite),
				  urlList(setUrlList),
				  config(setConfig) {}

		ThreadOptions( // copy thread options and set module
				const std::string& setModule,
				const ThreadOptions& options
				) : module(setModule),
					website(options.website),
					urlList(options.urlList),
					config(options.config) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_THREADOPTIONS_HPP_ */
