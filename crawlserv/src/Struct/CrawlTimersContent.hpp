/*
 *
 * ---
 *
 *  Copyright (C) 2023 Anselm Schmidt (ans[ät]ohai.su)
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
 * CrawlTimersContent.hpp
 *
 *  Created on: Oct 12, 2023
 *      Author: ans
 */

#ifndef STRUCT_CRAWLTIMERSCONTENT_HPP_
#define STRUCT_CRAWLTIMERSCONTENT_HPP_

#include "../Timer/StartStop.hpp"

//! Namespace for data structures.
namespace crawlservpp::Struct {

	//! Timers for crawling content.
	struct CrawlTimersContent {

		///@name Properties
		///@{

		//! Timer measuring the time spent sleeping.
		Timer::StartStop sleep;

		//! Timer measuring the time spent receiving content via HTTP.
		Timer::StartStop http;

		//! Timer measuring the time spent parsing the content.
		Timer::StartStop parse;

		//! Timer measuring the time spent updating the database.
		Timer::StartStop update;

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_CRAWLTIMERSCONTENT_HPP_ */
