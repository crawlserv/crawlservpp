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
 * StatusSetter.hpp
 *
 * Allows the receiving function to update the status of the current thread.
 *
 *  Created on: Aug 2, 2020
 *      Author: ans
 */

#ifndef STRUCT_STATUSSETTER_HPP_
#define STRUCT_STATUSSETTER_HPP_

#include <cstddef>		// std::size_t
#include <functional>	// std::function
#include <ios>			// std::fixed
#include <locale>		// std::locale
#include <sstream>		// std::ostringstream
#include <string>		// std::string
#include <utility>		// std::move

namespace crawlservpp::Struct {

	///@name Constants
	///@{

	//! The precision (number of fractal digits) when showing the progress in percent.
	constexpr auto decimalsProgress{2};

	//! The factor for converting a fractal into a percentage.
	constexpr auto percentageFactor{100};

	///@}

	//! Data needed to keep the status updated.
	struct StatusSetter {
		///@name Properties
		///@{

		//! The current status, to which the current progress will be added.
		std::string currentStatus;

		//! The progress to which the thread will be (re-)set when the current task has been finished, in percent.
		/*!
		 * Set to a value between @c 0.F (none)
		 *  to @c 1.F (done).
		 */
		float progressAfter;

		///@}
		///@name Callback Functions
		///@{

		//! Callback function to update the status message of the current thread.
		std::function<void(const std::string&)> callbackSetStatus;

		//! Callback function to update the progress of the current thread, in percent.
		std::function<void(float)> callbackSetProgress;

		///@}
		///@name Constructor
		///@{

		//! Constructor setting the initial status.
		/*!
		 * \note The constructor will already use
		 *   use the given callback function!
		 *
		 * \param setCurrentStatus The current
		 *   status that will be set on
		 *   construction and updated on
		 *   further progress.
		 * \param setProgressAfter The
		 *   progress to which the thread
		 *   should be set after finishing
		 *   the task, in percent, or @c
		 *   1.F, if the progress should
		 *   remain at @c 100% in the end.
		 * \param callbackToSetStatus The
		 *   function that will be used to
		 *   set the status message of the
		 *   current thread.
		 * \param callbackToSetProgress The
		 *   function that will be used to
		 *   set the progress of the current
		 *   thread, in percent.
		 */
		StatusSetter(
				const std::string& setCurrentStatus,
				float setProgressAfter,
				std::function<void(const std::string&)> callbackToSetStatus,
				std::function<void(float)> callbackToSetProgress
		) : 	currentStatus(setCurrentStatus),
				progressAfter(setProgressAfter),
				callbackSetStatus(std::move(callbackToSetStatus)),
				callbackSetProgress(std::move(callbackToSetProgress)) {
			this->callbackSetStatus(this->currentStatus);
			this->callbackSetProgress(0.F);
		}

		//! Updates the status with a fractal progress.
		/*!
		 * \param done The number of processed units.
		 * \param total The total number of units to
		 *   be processed.
		 */
		void update(std::size_t done, std::size_t total) const {
			std::ostringstream statusStrStr;

			statusStrStr.imbue(std::locale(""));

			statusStrStr << this->currentStatus;
			statusStrStr << " [";
			statusStrStr << done;
			statusStrStr << "/";
			statusStrStr << total;
			statusStrStr << "]";

			this->callbackSetStatus(statusStrStr.str());
			this->callbackSetProgress(static_cast<float>(done) / total);
		}

		//! Updates the status with a percentage.
		/*!
		 * \param percentage Progress between @c
		 *   0.F (none) and @c 1.F (done).
		 */
		void update(float percentage) const {
			std::ostringstream statusStrStr;

			statusStrStr.precision(decimalsProgress);

			statusStrStr << this->currentStatus;
			statusStrStr << " [";
			statusStrStr << std::fixed << percentage * percentageFactor;
			statusStrStr << "%]";

			this->callbackSetStatus(statusStrStr.str());
			this->callbackSetProgress(percentage);
		}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_STATUSSETTER_HPP_ */
