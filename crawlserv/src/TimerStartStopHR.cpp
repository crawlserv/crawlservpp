/*
 * TimerStartStopHR.cpp
 *
 * High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "TimerStartStopHR.h"

// constructor: start timer
TimerStartStopHR::TimerStartStopHR() {
	this->timePoint = std::chrono::high_resolution_clock::time_point::min();
	this->duration = std::chrono::high_resolution_clock::duration::zero();
}

// desctructor stub
TimerStartStopHR::~TimerStartStopHR() {}

// start timer
void TimerStartStopHR::start() {
	if(this->timePoint != std::chrono::high_resolution_clock::time_point::min()) this->stop();
	this->timePoint = std::chrono::high_resolution_clock::now();
}

// stop timer
void TimerStartStopHR::stop() {
	this->duration += std::chrono::high_resolution_clock::now() - this->timePoint;
	this->timePoint = std::chrono::high_resolution_clock::time_point::min();
}

// get total duration as string
std::string TimerStartStopHR::totalStr() {
	if(this->timePoint != std::chrono::high_resolution_clock::time_point::min()) this->stop();
	return Helpers::microsecondsToString(std::chrono::duration_cast<std::chrono::microseconds>(this->duration).count());
}
