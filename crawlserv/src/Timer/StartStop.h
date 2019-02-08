/*
 * StartStop.h
 *
 * Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_STARTSTOP_H_
#define TIMER_STARTSTOP_H_

#include "../Helper/DateTime.h"

#include <chrono>
#include <string>

namespace crawlservpp::Timer {
	class StartStop {
	public:
		// constructor
		StartStop();

		// control functions
		void start();
		void stop();
		std::string totalStr();

	protected:
		std::chrono::steady_clock::time_point timePoint;
		std::chrono::steady_clock::duration duration;
	};
}

#endif /* TIMER_STARTSTOPHR_H_ */
