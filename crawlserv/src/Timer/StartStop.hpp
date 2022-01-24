/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
 * StartStop.hpp
 *
 * Start/stop watch timer for getting the elapsed time
 *  in milliseconds, including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_STARTSTOP_HPP_
#define TIMER_STARTSTOP_HPP_

#include "../Helper/DateTime.hpp"

#include <chrono>	// std::chrono
#include <string>	// std::string

namespace crawlservpp::Timer {

	/*
	 * DECLARATION
	 */

	//! A simple start/stop watch.
	/*!
	 * Accumulates the number of passed milliseconds while running.
	 *
	 * \note This timer needs to be started manually.
	 */
	class StartStop {
	public:
		///@name Construction
		///@{

		StartStop();

		///@}
		///@name Control functions
		///@{

		void start();
		void stop();
		void reset();

		///@}
		///@name Getter
		///@{

		std::string totalStr();

		///@}
		///@name Reset
		///@{

		void clear();

		///@}

	protected:
		///@name Internal state
		///@{

		//! (Time) point of start.
		/*!
		 * Equals @c std::chrono::steady_clock::time_point::min()
		 *  if the timer has not been started yet or is currently stopped.
		 */
		std::chrono::steady_clock::time_point timePoint;

		//! Duration of previous runs.
		/*!
		 * Equals @c std::chrono::steady_clock::duration::zero()
		 *  if no time has been measured yet (excluding the current run).
		 */
		std::chrono::steady_clock::duration duration;

		///@}
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor initializing the values.
	/*!
	 * \note Other than Timer::Simple and Timer::SimpleHR,
	 *   this timer will not be started automatically on creation.
	 *
	 * \sa start
	 */
	inline StartStop::StartStop()
	 : timePoint(std::chrono::steady_clock::time_point::min()),
	   duration(std::chrono::steady_clock::duration::zero()) {}

	//! Starts the timer.
	/*!
	 * Milliseconds will be accumulated starting from now on.
	 *
	 * If the timer is already running, it will first be stopped, i.e.
	 *  the number of milliseconds stored internally will be refreshed.
	 *
	 * \sa stop
	 */
	inline void StartStop::start() {
		if(this->timePoint != std::chrono::steady_clock::time_point::min()) {
			this->stop();
		}

		this->timePoint = std::chrono::steady_clock::now();
	}

	//! Stops the timer.
	/*!
	 * The number of milliseconds stored internally will be refreshed
	 *
	 * \sa start
	 */
	inline void StartStop::stop() {
		this->duration += std::chrono::steady_clock::now() - this->timePoint;

		this->timePoint = std::chrono::steady_clock::time_point::min();
	}

	//! Resets the timer.
	/*!
	 * \warning The number of milliseconds stored internally will be lost.
	 */
	inline void StartStop::reset() {
		this->timePoint = std::chrono::steady_clock::time_point::min();
		this->duration = std::chrono::steady_clock::duration::zero();
	}

	//! Gets the total duration as formatted string.
	/*!
	 * If the timer is currenty running, it will first be stopped,
	 *  i.e. the number of milliseconds passed will be refreshed.
	 *
	 * \returns The number of milliseconds stored internally,
	 *   formatted as string.
	 *
	 * \sa Helper::DateTime::millisecondsToString
	 */
	inline std::string StartStop::totalStr() {
		if(this->timePoint != std::chrono::steady_clock::time_point::min()) {
			this->stop();
		}

		return Helper::DateTime::millisecondsToString(
				std::chrono::duration_cast<std::chrono::milliseconds>(
						this->duration
				).count()
		);
	}

	//! Resets the internal state of the timer.
	inline void StartStop::clear() {
		this->timePoint = std::chrono::steady_clock::time_point{};
		this->duration = std::chrono::steady_clock::duration{};
	}

} /* namespace crawlservpp::Timer */

#endif /* TIMER_STARTSTOPHR_HPP_ */
