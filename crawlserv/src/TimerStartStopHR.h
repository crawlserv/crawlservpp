/*
 * TimerStartStopHR.h
 *
 * High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMERSTARTSTOPHR_H_
#define TIMERSTARTSTOPHR_H_

#include <chrono>
#include <string>

#include "namespaces/Helpers.h"

class TimerStartStopHR {
public:
	TimerStartStopHR();
	virtual ~TimerStartStopHR();

	void start();
	void stop();
	std::string totalStr();

protected:
	std::chrono::high_resolution_clock::time_point timePoint;
	std::chrono::high_resolution_clock::duration duration;
};

#endif /* TIMERSTARTSTOPHR_H_ */
