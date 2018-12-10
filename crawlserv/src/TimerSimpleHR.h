/*
 * TimerSimpleHR.h
 *
 * Simple high resolution timer for getting the time since creation in microseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMERSIMPLEHR_H_
#define TIMERSIMPLEHR_H_

#include "namespaces/DateTime.h"

#include <chrono>
#include <string>

class TimerSimpleHR {
public:
	TimerSimpleHR();
	virtual ~TimerSimpleHR();

	unsigned long long tick();
	std::string tickStr();

protected:
	std::chrono::high_resolution_clock::time_point timePoint;
};

#endif /* TIMERSIMPLEHR_H_ */
