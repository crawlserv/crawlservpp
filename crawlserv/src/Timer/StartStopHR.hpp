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
 * StartStopHR.hpp
 *
 * High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_STARTSTOPHR_HPP_
#define TIMER_STARTSTOPHR_HPP_

#include "../Helper/DateTime.hpp"

#include <chrono>	// std::chrono
#include <string>	// std::string

namespace crawlservpp::Timer {

	class StartStopHR {
	public:
		// constructor
		StartStopHR();

		// control functions
		void start();
		void stop();
		std::string totalStr();

	protected:
		std::chrono::high_resolution_clock::time_point timePoint;
		std::chrono::high_resolution_clock::duration duration;
	};

} /* crawlservpp::Timer */

#endif /* TIMER_STARTSTOPHR_HPP_ */
