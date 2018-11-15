/*
 * TimerStartStop.h
 *
 * Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMERSTARTSTOP_H_
#define TIMERSTARTSTOP_H_

#include <chrono>
#include <string>

#include "Helpers.h"

class TimerStartStop {
public:
	TimerStartStop();
	virtual ~TimerStartStop();

	void start();
	void stop();
	std::string totalStr();

protected:
	std::chrono::steady_clock::time_point timePoint;
	std::chrono::steady_clock::duration duration;
};

#endif /* TIMERSTARTSTOPHR_H_ */
