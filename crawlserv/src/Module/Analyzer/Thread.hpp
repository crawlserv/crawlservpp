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
 * Thread.hpp
 *
 * Abstract implementation of the Thread interface
 *  for analyzer threads to be inherited by the algorithm class.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_THREAD_HPP_
#define MODULE_ANALYZER_THREAD_HPP_

#include "Config.hpp"
#include "Database.hpp"

#include "../Config.hpp"
#include "../Thread.hpp"

#include "../../Main/Exception.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"

#include <functional>	// std::bind
#include <queue>		// std::queue
#include <stdexcept>	// std::logic_error
#include <string>		// std::string
#include <tuple>		// std::tuple

namespace crawlservpp::Module::Analyzer {

	//! Abstract class providing thread functionality to algorithm (child) classes.
	class Thread : public Module::Thread, protected Config {
		// for convenience
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

	public:
		///@name Construction
		///@{

		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);
		Thread(Main::Database& dbBase, const ThreadOptions& threadOptions);

		///@}

	protected:
		///@name Database Connection
		///@{

		//! %Database connection for the analyzer thread.
		Database database;

		///@}
		///@name Implemented Thread Functions
		///@{

		void onInit() override;
		void onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear() override;

		///@}
		///@name Algorithm Events
		///@{

		//! Initializes the target table for the algorithm.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 *
		 * \note The SQL statements for the analyzer will
		 *   not be yet prepared when this function is
		 *   called.
		 */
		virtual void onAlgoInitTarget() = 0;

		//! Initializes the algorithm.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 */
		virtual void onAlgoInit() = 0;

		//! Performs an algorithm tick.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 */
		virtual void onAlgoTick() = 0;

		//! Pauses the algorithm.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 */
		virtual void onAlgoPause() = 0;

		//! Unpauses the algorithm.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 */
		virtual void onAlgoUnpause() = 0;

		//! Clears the algorithm.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific algorithm.
		 */
		virtual void onAlgoClear() = 0;

		///@}
		///@name Thread Control for Algorithms
		///@{

		void finished();
		void pause();

		///@}
		///@name Helper Function for Algorithms
		///@{

		[[nodiscard]] std::string getTargetTableName() const;

		///@}

		//! Class for analyzer exceptions to be used by algorithms.
		MAIN_EXCEPTION_CLASS();

	private:
		// hide other functions not to be used by the thread
		void start();
		void unpause();
		void stop();
		void interrupt();
	};

} /* namespace crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_THREAD_HPP_ */
