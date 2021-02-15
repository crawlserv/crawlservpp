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
	inline constexpr auto precisionProgress{2};

	//! The factor for converting a fractal into a percentage.
	inline constexpr auto percentageFactor{100};

	///@}

	//! Structure containing all the data needed to keep the status of a thread updated.
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

		//! Callback function to check whether the current thread should still be running.
		std::function<bool()> callbackIsRunning;

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
		 *   function (or lambda) that will
		 *   be used to set the status
		 *   message of the current thread.
		 * \param callbackToSetProgress The
		 *   function (or lambda) that will
		 *   be used to set the progress of
		 *   the current thread, in percent.
		 * \param callbackToCheckIsRunning
		 *   The function (or lambda) that
		 *   will be used to check whether
		 *   the thread is still supposed
		 *   to run.
		 */
		StatusSetter(
				const std::string& setCurrentStatus,
				float setProgressAfter,
				std::function<void(const std::string&)> callbackToSetStatus,
				std::function<void(float)> callbackToSetProgress,
				std::function<bool()> callbackToCheckIsRunning
		) : 	currentStatus(setCurrentStatus),
				progressAfter(setProgressAfter),
				callbackSetStatus(std::move(callbackToSetStatus)),
				callbackSetProgress(std::move(callbackToSetProgress)),
				callbackIsRunning(std::move(callbackToCheckIsRunning)) {
			if(this->callbackIsRunning()) {
				this->callbackSetStatus(this->currentStatus);
				this->callbackSetProgress(0.F);
			}
		}

		//! Changes the status message and resets the current progress.
		/*!
		 * \param statusMessage Constant reference
		 *   to a string containing the new status
		 *   message.
		 *
		 * \returns True, if the thread is supposed
		 *   to continue running. False, otherwise.
		 */
		bool change(const std::string& statusMessage) { //NOLINT(modernize-use-nodiscard)
			this->currentStatus = statusMessage;
			this->callbackSetStatus(statusMessage);

			this->callbackSetProgress(0.F);

			return this->callbackIsRunning();
		}

		//! Updates the status with a fractal progress.
		/*!
		 * \param done The number of processed units.
		 * \param total The total number of units to
		 *   be processed.
		 *
		 * \returns True, if the thread is supposed
		 *   to continue running. False, otherwise.
		 */
		bool update(std::size_t done, std::size_t total) const { //NOLINT(modernize-use-nodiscard)
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

			return this->callbackIsRunning();
		}

		//! Updates the status with a percentage.
		/*!
		 * \param percentage Progress between @c
		 *   0.F (none) and @c 1.F (done).
		 * \param precise If set to true, the
		 *   progress will be added to the status
		 *   with higher precision.
		 *
		 * \returns True, if the thread is supposed
		 *   to continue running. False, otherwise.
		 *
		 * \sa precisionProgress
		 */
		bool update(float percentage, bool precise) const { //NOLINT(modernize-use-nodiscard)
			std::ostringstream statusStrStr;

			if(precise) {
				statusStrStr.precision(precisionProgress);
			}
			else {
				statusStrStr.precision(0);
			}

			statusStrStr << this->currentStatus;
			statusStrStr << " [";
			statusStrStr << std::fixed << percentage * percentageFactor;
			statusStrStr << "%]";

			this->callbackSetStatus(statusStrStr.str());
			this->callbackSetProgress(percentage);

			return this->callbackIsRunning();
		}

		//! Calculates the current percentage and updates the status accordingly.
		/*!
		 * \param done The number of processed units.
		 * \param total The total number of units to
		 *   be processed.
		 * \param precise If set to true, the
		 *   progress will be added to the status
		 *   with higher precision.
		 *
		 * \returns True, if the thread is supposed
		 *   to continue running. False, otherwise.
		 *
		 * \sa precisionProgress
		 */
		bool update(std::size_t done, std::size_t total, bool precise) const { //NOLINT(modernize-use-nodiscard)
			return this->update(static_cast<float>(done) / total, precise);
		}

		//! Checks whether the thread is still supposed to run
		/*!
		 * \returns True, if the thread is supposed
		 *   to continue running. False, otherwise.
		 */
		[[nodiscard]] bool isRunning() const {
			return this->callbackIsRunning();
		}

		//! Re-sets the progress of the thread.
		void finish() const {
			this->callbackSetStatus(this->currentStatus + " [done]");
			this->callbackSetProgress(this->progressAfter);
		}
	};

} /* namespace crawlservpp::Struct */

#endif /* STRUCT_STATUSSETTER_HPP_ */
