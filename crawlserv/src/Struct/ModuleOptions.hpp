/*
 * ModuleOptions.hpp
 *
 * Basic module options (thread ID, website ID and namespace, URL list ID and namespace).
 *
 *  Created on: May 8, 2019
 *      Author: ans
 */

#ifndef STRUCT_MODULEOPTIONS_HPP_
#define STRUCT_MODULEOPTIONS_HPP_

#include <string>

namespace crawlservpp::Struct {

	struct ModuleOptions {
		unsigned long threadId;
		unsigned long websiteId;
		std::string websiteNamespace;
		unsigned long urlListId;
		std::string urlListNamespace;

		ModuleOptions() : threadId(0), websiteId(0), urlListId(0) {}

		ModuleOptions(
				unsigned long setThreadId,
				unsigned long setWebsiteId,
				const std::string& setWebsiteNamespace,
				unsigned long setUrlListId,
				const std::string& setUrlListNamespace
		) : threadId(setThreadId),
			websiteId(setWebsiteId),
			websiteNamespace(setWebsiteNamespace),
			urlListId(setUrlListId),
			urlListNamespace(setUrlListNamespace) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_MODULEOPTIONS_HPP_ */