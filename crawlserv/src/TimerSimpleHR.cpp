/*
 * TimerSimpleHR.cpp
 *
 * Simple high resolution timer for getting the time since creation in microseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "TimerSimpleHR.h"

// constructor: start timer
TimerSimpleHR::TimerSimpleHR() {
	this->timePoint = std::chrono::high_resolution_clock::now();
}

// destructor stub
TimerSimpleHR::~TimerSimpleHR() {}

// tick: get time since start (in microseconds) and restart timer
unsigned long long TimerSimpleHR::tick() {
	unsigned long long result = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()
		- this->timePoint).count();
	this->timePoint = std::chrono::high_resolution_clock::now();
	return result;
}

// tick: get time since start (in microseconds) as string and restart timer
std::string TimerSimpleHR::tickStr() {
	unsigned long long result = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()
		- this->timePoint).count();
	this->timePoint = std::chrono::high_resolution_clock::now();
	return Helpers::microsecondsToString(result);
}

