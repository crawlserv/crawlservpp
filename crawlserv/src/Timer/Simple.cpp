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
 * Simple.cpp
 *
 * Simple timer for getting the time since creation in milliseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "Simple.hpp"

namespace crawlservpp::Timer {

	// constructor: start timer
	Simple::Simple() : timePoint(std::chrono::steady_clock::now()) {}

	// tick: get time since start (in milliseconds) and restart timer
	unsigned long long Simple::tick() {
		const unsigned long long result =
				std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - this->timePoint
				).count();

		this->timePoint = std::chrono::steady_clock::now();

		return result;
	}

	// tick: get time since start (in milliseconds) as string and restart timer
	std::string Simple::tickStr() {
		const unsigned long long result =
				std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::steady_clock::now() - this->timePoint
				).count();

		this->timePoint = std::chrono::steady_clock::now();

		return Helper::DateTime::millisecondsToString(result);
	}

} /* crawlservpp::Timer */
