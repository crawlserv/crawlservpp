/*
 * StartStopHR.h
 *
 * High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_STARTSTOPHR_H_
#define TIMER_STARTSTOPHR_H_

#include "../Helper/DateTime.h"

#include <chrono>
#include <string>

namespace crawlservpp::Timer {
	class StartStopHR {
	public:
		StartStopHR();
		virtual ~StartStopHR();

		void start();
		void stop();
		std::string totalStr();

	protected:
		std::chrono::high_resolution_clock::time_point timePoint;
		std::chrono::high_resolution_clock::duration duration;
	};
}

#endif /* TIMER_STARTSTOPHR_H_ */
