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
 * ThreadOptions.hpp
 *
 * Basic thread options (IDs of website, URL list and configuration).
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADOPTIONS_HPP_
#define STRUCT_THREADOPTIONS_HPP_

#include <string>	// std::string

namespace crawlservpp::Struct {

	struct ThreadOptions {
		std::string module; // module of the thread

		size_t website; // website ID for the thread
		size_t urlList; // URL list ID for the thread
		size_t config; // configuration ID for the thread

		// constructors
		ThreadOptions() // create empty thread options
				: website(0),
				  urlList(0),
				  config(0) {}

		ThreadOptions( // create thread options
				const std::string& setModule,
				size_t setWebsite,
				size_t setUrlList,
				size_t setConfig)
				: module(setModule),
				  website(setWebsite),
				  urlList(setUrlList),
				  config(setConfig) {}
	};

} /* crawlservpp::Struct */

#endif /* STRUCT_THREADOPTIONS_HPP_ */
