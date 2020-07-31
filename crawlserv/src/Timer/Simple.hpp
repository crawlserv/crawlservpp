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
 * Simple.hpp
 *
 * Simple timer for getting the time since creation in microseconds.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_SIMPLE_HPP_
#define TIMER_SIMPLE_HPP_

#include "../Helper/DateTime.hpp"

#include <chrono>	// std::chrono
#include <string>	// std::string

//! Namespace for timers.
namespace crawlservpp::Timer {

	/*
	 * DECLARATION
	 */

	//! A simple timer.
	/*!
	 * Starting from its creation, this timer counts the number
	 *  of milliseconds until tick() or tickStr() is called.
	 *
	 * The timer is restarted after each tick.
	 */
	class Simple {
	public:
		///@name Construction
		///@{

		Simple();

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
		std::chrono::steady_clock::time_point timePoint;

		///@}
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor starting the timer.
	inline Simple::Simple() : timePoint(std::chrono::steady_clock::now()) {}

	//! %Timer tick returning the number of milliseconds passed.
	/*!
	 * Restarts the timer.
	 *
	 * \returns the time since creation or last tick in milliseconds.
	 */
	inline std::uint64_t Simple::tick() {
		const auto result{
			std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - this->timePoint
			).count()
		};

		this->timePoint = std::chrono::steady_clock::now();

		return result;
	}

	//! %Timer tick returning the number of milliseconds passed as string.
	/*!
	 * Restarts the timer.
	 *
	 * \returns the time since creation or last tick as formatted string.
	 *   The resolution of the result remains in milliseconds.
	 *
	 * \sa Helper::DateTime::millisecondsToString
	 */
	inline std::string Simple::tickStr() {
		const auto result{
			std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - this->timePoint
			).count()
		};

		this->timePoint = std::chrono::steady_clock::now();

		return Helper::DateTime::millisecondsToString(result);
	}

} /* namespace crawlservpp::Timer */

#endif /* TIMER_SIMPLE_HPP_ */
