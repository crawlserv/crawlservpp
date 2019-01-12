/*
 * SimpleHR.cpp
 *
 * Simple high resolution timer for getting the time since creation in microseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "SimpleHR.h"

// constructor: start timer
crawlservpp::Timer::SimpleHR::SimpleHR() {
	this->timePoint = std::chrono::high_resolution_clock::now();
}

// destructor stub
crawlservpp::Timer::SimpleHR::~SimpleHR() {}

// tick: get time since start (in microseconds) and restart timer
unsigned long long crawlservpp::Timer::SimpleHR::tick() {
	unsigned long long result = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()
		- this->timePoint).count();
	this->timePoint = std::chrono::high_resolution_clock::now();
	return result;
}

// tick: get time since start (in microseconds) as string and restart timer
std::string crawlservpp::Timer::SimpleHR::tickStr() {
	unsigned long long result = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now()
		- this->timePoint).count();
	this->timePoint = std::chrono::high_resolution_clock::now();
	return crawlservpp::Helper::DateTime::microsecondsToString(result);
}

