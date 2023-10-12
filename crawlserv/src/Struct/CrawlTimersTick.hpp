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
 * CrawlTimersTick.hpp
 *
 *  Created on: Oct 12, 2023
 *      Author: ans
 */

#ifndef STRUCT_CRAWLTIMERSTICK_HPP_
#define STRUCT_CRAWLTIMERSTICK_HPP_

#include "../Timer/StartStop.hpp"

//! Namespace for data structures.
namespace crawlservpp::Struct {

	//! Timers for crawling tick.
	struct CrawlTimersTick {

		///@name Properties
		///@{

		//! Timer measuring the time spent on selecting the URL.
		Timer::StartStop select;

		//! Timer measuring the time spent on receiving archived content.
		Timer::StartStop archives;

		//! Timer measuring the time spent for crawling a URL in total.
		Timer::StartStop total;

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_CRAWLTIMERSTICK_HPP_ */
