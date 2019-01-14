/*
 * Simple.cpp
 *
 * Simple timer for getting the time since creation in milliseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#include "Simple.h"

// constructor: start timer
crawlservpp::Timer::Simple::Simple() {
	this->timePoint = std::chrono::steady_clock::now();
}

// destructor stub
crawlservpp::Timer::Simple::~Simple() {}

// tick: get time since start (in milliseconds) and restart timer
unsigned long long crawlservpp::Timer::Simple::tick() {
	unsigned long long result = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
		- this->timePoint).count();
	this->timePoint = std::chrono::steady_clock::now();
	return result;
}

// tick: get time since start (in milliseconds) as string and restart timer
std::string crawlservpp::Timer::Simple::tickStr() {
	unsigned long long result = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()
		- this->timePoint).count();
	this->timePoint = std::chrono::steady_clock::now();
	return crawlservpp::Helper::DateTime::millisecondsToString(result);
}

