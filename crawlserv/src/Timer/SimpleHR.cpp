/*
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
