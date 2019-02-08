/*
 * StartStop.cpp
 *
 * Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "StartStop.h"

namespace crawlservpp::Timer {

// constructor: start timer
StartStop::StartStop() {
	this->timePoint = std::chrono::steady_clock::time_point::min();
	this->duration = std::chrono::steady_clock::duration::zero();
}

// start timer
void StartStop::start() {
	if(this->timePoint != std::chrono::steady_clock::time_point::min()) this->stop();
	this->timePoint = std::chrono::steady_clock::now();
}

// stop timer
void StartStop::stop() {
	this->duration += std::chrono::steady_clock::now() - this->timePoint;
	this->timePoint = std::chrono::steady_clock::time_point::min();
}

// get total duration as string
std::string StartStop::totalStr() {
	if(this->timePoint != std::chrono::steady_clock::time_point::min()) this->stop();
	return crawlservpp::Helper::DateTime::millisecondsToString(
			std::chrono::duration_cast<std::chrono::milliseconds>(this->duration).count());
}

}
