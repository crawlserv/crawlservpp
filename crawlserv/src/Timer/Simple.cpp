/*
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
