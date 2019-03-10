/*
 * StartStopHR.cpp
 *
 * High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "StartStopHR.hpp"

namespace crawlservpp::Timer {

	// constructor: start timer
	StartStopHR::StartStopHR() : timePoint(std::chrono::high_resolution_clock::time_point::min()),
								 duration(std::chrono::high_resolution_clock::duration::zero()) {}

	// start timer
	void StartStopHR::start() {
		if(this->timePoint != std::chrono::high_resolution_clock::time_point::min()) this->stop();
		this->timePoint = std::chrono::high_resolution_clock::now();
	}

	// stop timer
	void StartStopHR::stop() {
		this->duration += std::chrono::high_resolution_clock::now() - this->timePoint;
		this->timePoint = std::chrono::high_resolution_clock::time_point::min();
	}

	// get total duration as string
	std::string StartStopHR::totalStr() {
		if(this->timePoint != std::chrono::high_resolution_clock::time_point::min()) this->stop();
		return Helper::DateTime::microsecondsToString(
				std::chrono::duration_cast<std::chrono::microseconds>(this->duration).count());
	}

} /* crawlservpp::Timer */
