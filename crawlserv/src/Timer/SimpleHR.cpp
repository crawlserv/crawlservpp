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
 * SimpleHR.cpp
 *
 * Simple high resolution timer for getting the time since creation in microseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "SimpleHR.hpp"

namespace crawlservpp::Timer {

	// constructor: start timer
	SimpleHR::SimpleHR() : timePoint(std::chrono::high_resolution_clock::now()) {}

	// tick: get time since start (in microseconds) and restart timer
	unsigned long long SimpleHR::tick() {
		const unsigned long long result =
				std::chrono::duration_cast<std::chrono::microseconds>(
						std::chrono::high_resolution_clock::now() - this->timePoint
				).count();

		this->timePoint = std::chrono::high_resolution_clock::now();

		return result;
	}

	// tick: get time since start (in microseconds) as string and restart timer
	std::string SimpleHR::tickStr() {
		const unsigned long long result =
				std::chrono::duration_cast<std::chrono::microseconds>(
						std::chrono::high_resolution_clock::now() - this->timePoint
		).count();

		this->timePoint = std::chrono::high_resolution_clock::now();

		return Helper::DateTime::microsecondsToString(result);
	}

} /* crawlservpp::Timer */
