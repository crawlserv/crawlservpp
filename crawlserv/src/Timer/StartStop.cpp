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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * StartStop.cpp
 *
 * Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "StartStop.hpp"

namespace crawlservpp::Timer {

	// constructor: start timer
	StartStop::StartStop() : timePoint(std::chrono::steady_clock::time_point::min()),
							 duration(std::chrono::steady_clock::duration::zero()) {}

	// start timer
	void StartStop::start() {
		if(this->timePoint != std::chrono::steady_clock::time_point::min())
			this->stop();

		this->timePoint = std::chrono::steady_clock::now();
	}

	// stop timer
	void StartStop::stop() {
		this->duration += std::chrono::steady_clock::now() - this->timePoint;

		this->timePoint = std::chrono::steady_clock::time_point::min();
	}

	// get total duration as string
	std::string StartStop::totalStr() {
		if(this->timePoint != std::chrono::steady_clock::time_point::min())
			this->stop();

		return Helper::DateTime::millisecondsToString(
				std::chrono::duration_cast<std::chrono::milliseconds>(
						this->duration
				).count()
		);
	}

} /* crawlservpp::Timer */
