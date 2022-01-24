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
 * StartStopHR.hpp
 *
 * High resolution start/stop watch timer for getting the
 *  elapsed time in microseconds, including pausing functionality.
 *
 *  Created on: Oct 17, 2018
 *      Author: ans
 */

#ifndef TIMER_STARTSTOPHR_HPP_
#define TIMER_STARTSTOPHR_HPP_

#include "../Helper/DateTime.hpp"

#include <chrono>	// std::chrono
#include <string>	// std::string

namespace crawlservpp::Timer {

	/*
	 * DECLARATION
	 */

	//! A simple start/stop watch with high resolution.
	/*!
	 * Accumulates the number of passed microseconds while runnning.
	 *
	 * \note This timer needs to be started manually.
	 */
	class StartStopHR {
	public:
		///@name Construction
		///@{

		StartStopHR();

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
		 * Equals @c std::chrono::high_resolution_clock::time_point::min()
		 *  if the timer has not been started yet or is currently stopped.
		 */
		std::chrono::high_resolution_clock::time_point timePoint;

		//! Duration of previous runs.
		/*!
		 * Equals @c std::chrono::high_resolution_clock::duration::zero()
		 *  if no time has been measured yet (excluding the current run).
		 */
		std::chrono::high_resolution_clock::duration duration;

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
	inline StartStopHR::StartStopHR()
	 : timePoint(std::chrono::high_resolution_clock::time_point::min()),
	   duration(std::chrono::high_resolution_clock::duration::zero()) {}

	//! Starts the timer.
	/*!
	 * Microseconds will be accumulated starting from now on.
	 *
	 * If the timer is already running, it will first be stopped, i.e.
	 *  the number of microseconds stored internally will be refreshed.
	 *
	 * \sa stop
	 */
	inline void StartStopHR::start() {
		if(this->timePoint != std::chrono::high_resolution_clock::time_point::min()) {
			this->stop();
		}

		this->timePoint = std::chrono::high_resolution_clock::now();
	}

	//! Stops the timer.
	/*!
	 * The number of microseconds stored internally will be refreshed.
	 *
	 * \sa start
	 */
	inline void StartStopHR::stop() {
		this->duration += std::chrono::high_resolution_clock::now() - this->timePoint;

		this->timePoint = std::chrono::high_resolution_clock::time_point::min();
	}

	//! Resets the timer.
	/*!
	 * \warning The number of microseconds stored internally will be lost.
	 */
	inline void StartStopHR::reset() {
		this->timePoint = std::chrono::high_resolution_clock::time_point::min();
		this->duration = std::chrono::high_resolution_clock::duration::zero();
	}

	//! Gets the total duration as formatted string.
	/*!
	 * If the timer is currenty running, it will first be stopped,
	 *  i.e. the number of microseconds passed will be refreshed.
	 *
	 * \returns The number of microseconds stored internally,
	 *   formatted as string.
	 *
	 * \sa Helper::DateTime::microsecondsToString
	 */
	inline std::string StartStopHR::totalStr() {
		if(
				this->timePoint !=
						std::chrono::high_resolution_clock::time_point::min()
		) {
			this->stop();
		}

		return Helper::DateTime::microsecondsToString(
				std::chrono::duration_cast<std::chrono::microseconds>(
						this->duration
				).count()
		);
	}

	//! Resets the internal state of the timer.
	inline void StartStopHR::clear() {
		this->timePoint = std::chrono::high_resolution_clock::time_point{};
		this->duration = std::chrono::high_resolution_clock::duration{};
	}

} /* namespace crawlservpp::Timer */

#endif /* TIMER_STARTSTOPHR_HPP_ */
