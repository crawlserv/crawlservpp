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
 * ThreadStatus.hpp
 *
 * Thread status (thread ID, status message, pause state and progress, i.e. last ID processed)
 *
 *  Created on: Apr 30, 2019
 *      Author: ans
 */

#ifndef STRUCT_THREADSTATUS_HPP_
#define STRUCT_THREADSTATUS_HPP_

#include <cstdint>	// std::uint64_t
#include <string>	// std::string

namespace crawlservpp::Struct {

	//! Thread status containing its ID, status message, pause state, and progress (i.e. the last ID processed by the thread).
	/*!
	 * \note The thread ID is part of the status,
	 *   because it will not be assigned until
	 *   after the threas has been added to
	 *   the database.
	 *
	 * \note It will effectively be assigned
	 *   by the database when the thread is
	 *   added to its table of threads.
	 */
	struct ThreadStatus {
		///@name Properties
		///@{

		//! The ID of the thread.
		std::uint64_t id{0};

		//! The status message of the thread.
		std::string status;

		//! Indicates whether the thread is paused at the moment.
		bool paused{false};

		//! The progress of, i.e. the last ID processed by the thread.
		std::uint64_t last{0};

		///@}
		///@name Construction
		///@{

		//! Default constructor.
		ThreadStatus() = default;

		//! Constructor setting the status of the thread.
		/*!
		 * \param setId The ID of the thread.
		 * \param setStatus Constant reference
		 *   to a string containing the status
		 *   message of the thread.
		 * \param setPaused Set whether the
		 *   thread is currently paused.
		 * \param setLast The progress of,
		 *   i.e. the last processed ID by
		 *   the thread.
		 */
		ThreadStatus(
				std::uint64_t setId,
				const std::string& setStatus,
				bool setPaused,
				std::uint64_t setLast
		) : id(setId), status(setStatus), paused(setPaused), last(setLast) {}

		///@}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_THREADSTATUS_HPP_ */
