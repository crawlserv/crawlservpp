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
 * ModuleOptions.hpp
 *
 * Basic module options (thread ID, website ID and namespace, URL list ID and namespace).
 *
 *  Created on: May 8, 2019
 *      Author: ans
 */

#ifndef STRUCT_MODULEOPTIONS_HPP_
#define STRUCT_MODULEOPTIONS_HPP_

#include <string>	// std::string

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
