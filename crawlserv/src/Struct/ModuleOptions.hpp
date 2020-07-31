/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * Module options (thread ID, website ID and namespace, URL list ID and namespace).
 *
 *  Created on: May 8, 2019
 *      Author: ans
 */

#ifndef STRUCT_MODULEOPTIONS_HPP_
#define STRUCT_MODULEOPTIONS_HPP_

#include <cstdint>	// std::uint64_t
#include <string>	// std::string

namespace crawlservpp::Struct {

	//! %Module options containing the thread ID, as well as ID and namespace of website and URL list used by the thread.
	struct ModuleOptions {
		///@name Properties
		///@{

		//! The ID of the thread.
		std::uint64_t threadId{0};

		//! The ID of the website used by the thread.
		std::uint64_t websiteId{0};

		//! The namespace of the website used by the thread.
		std::string websiteNamespace;

		//! The ID of the URL list used by the thread.
		std::uint64_t urlListId{0};

		//! The namespace of the URL list used by the thread.
		std::string urlListNamespace;

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		ModuleOptions() = default;

		//! Constructor setting the basic module options for the thread.
		/*!
		 * \param setThreadId The ID of the thread.
		 * \param setWebsiteId The ID of the website
		 *   used by the thread.
		 * \param setWebsiteNamespace Constant reference
		 *   to a string containing the namespace of the
		 *   website used by the thread.
		 * \param setUrlListId The ID of the URL list
		 *   used by the thread.
		 * \param setUrlListNamespace Constant reference
		 *   to a string containing the namespace of the
		 *   URL list used by the thread.
		 */
		ModuleOptions(
				std::uint64_t setThreadId,
				std::uint64_t setWebsiteId,
				const std::string& setWebsiteNamespace,
				std::uint64_t setUrlListId,
				const std::string& setUrlListNamespace
		) : threadId(setThreadId),
			websiteId(setWebsiteId),
			websiteNamespace(setWebsiteNamespace),
			urlListId(setUrlListId),
			urlListNamespace(setUrlListNamespace) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_MODULEOPTIONS_HPP_ */
