/*
 * StartStop.h
 *
 * Start/stop watch timer for getting the elapsed time in milliseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_STARTSTOP_HPP_
#define TIMER_STARTSTOP_HPP_

#include "../Helper/DateTime.hpp"

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

} /* crawlservpp::Timer */

#endif /* TIMER_STARTSTOPHR_HPP_ */
