/*
 * Simple.h
 *
 * Simple timer for getting the time since creation in microseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_SIMPLE_HPP_
#define TIMER_SIMPLE_HPP_

#include "../Helper/DateTime.hpp"

#include <chrono>
#include <string>

namespace crawlservpp::Timer {

	class Simple {
	public:
		// constructor
		Simple();

		// control functions
		unsigned long long tick();
		std::string tickStr();

	protected:
		std::chrono::steady_clock::time_point timePoint;
	};

} /* crawlservpp::Timer */

#endif /* TIMER_SIMPLE_HPP_ */
