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
 * ThreadOptions.hpp
 *
 * Thread options (name of module, IDs of website, URL list and configuration).
 *
 *  Created on: Oct 13, 2018
 *      Author: ans
 */

#ifndef STRUCT_THREADOPTIONS_HPP_
#define STRUCT_THREADOPTIONS_HPP_

#include <cstdint>	// std::uint64_t
#include <string>	// std::string

namespace crawlservpp::Struct {

	//! Thread options containing the name of the module run, as well as the IDs of the website, URL list, and configuration used.
	struct ThreadOptions {
		///@name Properties
		///@{

		//! The name of the module run by the thread.
		std::string module;

		//! The ID of the website used by the thread
		std::uint64_t website{0};

		//! The ID of the URL list used by the thread.
		std::uint64_t urlList{0};

		//! The ID of the configuration used by the thread.
		std::uint64_t config{0};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		ThreadOptions() = default;

		//! Constructor setting the options of the thread.
		/*!
		 * \param setModule Constant reference
		 *   to a string containing the name
		 *   of the module run by the thread.
		 * \param setWebsite ID of the website
		 *   used by the thread.
		 * \param setUrlList ID of the URl list
		 *   used by the thread.
		 * \param setConfig ID of the
		 *   configuration used by the thread.
		 */
		ThreadOptions(
				const std::string& setModule,
				std::uint64_t setWebsite,
				std::uint64_t setUrlList,
				std::uint64_t setConfig)
				: module(setModule),
				  website(setWebsite),
				  urlList(setUrlList),
				  config(setConfig) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_THREADOPTIONS_HPP_ */
