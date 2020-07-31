/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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

	/*
	 * DECLARATION
	 */

	//! A simple timer with high resolution.
	/*!
	 * Starting from its creation, this timer counts the number
	 *  of microseconds until tick() or tickStr() is called.
	 *
	 * The timer is restarted after each tick.
	 */
	class SimpleHR {
	public:
		///@name Construction
		///@{

		SimpleHR();

		///@}
		///@name Tick control
		///@{

		std::uint64_t tick();
		std::string tickStr();

		///@}

	protected:
		///@name Internal state
		///@{

		//! (Time) point of creation or last tick.
		std::chrono::high_resolution_clock::time_point timePoint;

		///@}
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor starting the timer.
	inline SimpleHR::SimpleHR() : timePoint(std::chrono::high_resolution_clock::now()) {}

	//! %Timer tick returning the number of microseconds passed.
	/*!
	 * Restarts the timer.
	 *
	 * \returns the time since creation or last tick in microseconds.
	 */
	inline std::uint64_t SimpleHR::tick() {
		const auto result{
			std::chrono::duration_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now() - this->timePoint
			).count()
		};

		this->timePoint = std::chrono::high_resolution_clock::now();

		return result;
	}

	//! %Timer tick returning the number of microseconds passed as string.
	/*!
	 * Restarts the timer.
	 *
	 * \returns the time since creation or last tick as formatted string.
	 *   The resolution of the result remains in microseconds.
	 *
	 * \sa Helper::DateTime::microsecondsToString
	 */
	inline std::string SimpleHR::tickStr() {
		const auto result{
			std::chrono::duration_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now() - this->timePoint
			).count()
		};

		this->timePoint = std::chrono::high_resolution_clock::now();

		return Helper::DateTime::microsecondsToString(result);
	}

} /* namespace crawlservpp::Timer */

#endif /* TIMER_SIMPLEHR_HPP_ */
