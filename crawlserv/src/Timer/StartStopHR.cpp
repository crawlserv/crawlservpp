/*
 * StartStopHR.cpp
 *
 * High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "StartStopHR.h"

// constructor: start timer
crawlservpp::Timer::StartStopHR::StartStopHR() {
	this->timePoint = std::chrono::high_resolution_clock::time_point::min();
	this->duration = std::chrono::high_resolution_clock::duration::zero();
}

// desctructor stub
crawlservpp::Timer::StartStopHR::~StartStopHR() {}

// start timer
void crawlservpp::Timer::StartStopHR::start() {
	if(this->timePoint != std::chrono::high_resolution_clock::time_point::min()) this->stop();
	this->timePoint = std::chrono::high_resolution_clock::now();
}

// stop timer
void crawlservpp::Timer::StartStopHR::stop() {
	this->duration += std::chrono::high_resolution_clock::now() - this->timePoint;
	this->timePoint = std::chrono::high_resolution_clock::time_point::min();
}

// get total duration as string
std::string crawlservpp::Timer::StartStopHR::totalStr() {
	if(this->timePoint != std::chrono::high_resolution_clock::time_point::min()) this->stop();
	return crawlservpp::Helper::DateTime::microsecondsToString(std::chrono::duration_cast<std::chrono::microseconds>(this->duration).count());
}
