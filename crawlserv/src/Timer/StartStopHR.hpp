/*
 * StartStopHR.hpp
 *
 * High resolution start/stop watch timer for getting the elapsed time in microseconds including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_STARTSTOPHR_HPP_
#define TIMER_STARTSTOPHR_HPP_

#include "../Helper/DateTime.hpp"

#include <chrono>
#include <string>

namespace crawlservpp::Timer {

	class StartStopHR {
	public:
		// constructor
		StartStopHR();

		// control functions
		void start();
		void stop();
		std::string totalStr();

	protected:
		std::chrono::high_resolution_clock::time_point timePoint;
		std::chrono::high_resolution_clock::duration duration;
	};

} /* crawlservpp::Timer */

#endif /* TIMER_STARTSTOPHR_HPP_ */
