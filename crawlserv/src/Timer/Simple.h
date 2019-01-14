/*
 * Simple.h
 *
 * Simple timer for getting the time since creation in microseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_SIMPLE_H_
#define TIMER_SIMPLE_H_

#include "../Helper/DateTime.h"

#include <chrono>
#include <string>

namespace crawlservpp::Timer {
	class Simple {
	public:
		Simple();
		virtual ~Simple();

		unsigned long long tick();
		std::string tickStr();

	protected:
		std::chrono::steady_clock::time_point timePoint;
	};
}

#endif /* TIMER_SIMPLE_H_ */
