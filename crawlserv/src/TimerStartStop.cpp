/*
 * TimerStartStop.cpp
 *
 * Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "TimerStartStop.h"

// constructor: start timer
TimerStartStop::TimerStartStop() {
	this->timePoint = std::chrono::steady_clock::time_point::min();
	this->duration = std::chrono::steady_clock::duration::zero();
}

// desctructor stub
TimerStartStop::~TimerStartStop() {}

// start timer
void TimerStartStop::start() {
	if(this->timePoint != std::chrono::steady_clock::time_point::min()) this->stop();
	this->timePoint = std::chrono::steady_clock::now();
}

// stop timer
void TimerStartStop::stop() {
	this->duration += std::chrono::steady_clock::now() - this->timePoint;
	this->timePoint = std::chrono::steady_clock::time_point::min();
}

// get total duration as string
std::string TimerStartStop::totalStr() {
	if(this->timePoint != std::chrono::steady_clock::time_point::min()) this->stop();
	return DateTime::millisecondsToString(std::chrono::duration_cast<std::chrono::milliseconds>(this->duration).count());
}
