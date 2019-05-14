/*
 * SimpleHR.hpp
 *
 * Simple high resolution timer for getting the time since creation in microseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_SIMPLEHR_HPP_
#define TIMER_SIMPLEHR_HPP_

#include "../Helper/DateTime.hpp"

#include <chrono>	// std::chrono
#include <string>	// std::string

namespace crawlservpp::Timer {

	class SimpleHR {
	public:
		// constructor
		SimpleHR();

		// control functions
		unsigned long long tick();
		std::string tickStr();

	protected:
		std::chrono::high_resolution_clock::time_point timePoint;
	};

} /* crawlservpp::Timer */

#endif /* TIMER_SIMPLEHR_HPP_ */
