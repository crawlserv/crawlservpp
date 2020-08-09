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
 * Interface for a thread which implements all module-independent thread functionality
 *  like connecting to the database, managing the thread status (including pausing the thread),
 *  running the thread ticks and catching exceptions thrown by the thread.
 *
 *  Created on: Oct 10, 2018
 *      Author: ans
 */

#ifndef MODULE_THREAD_HPP_
#define MODULE_THREAD_HPP_

// do not catch thread exceptions: use only for debugging!
//#define MODULE_THREAD_DEBUG_NOCATCH

#include "Database.hpp"

#include "../Helper/DateTime.hpp"
#include "../Main/Database.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/ModuleOptions.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/ThreadStatus.hpp"
#include "../Wrapper/DatabaseLock.hpp"

#include <atomic>				// std::atomic
#include <chrono>				// std::chrono
#include <cmath>				// std::lround
#include <condition_variable>	// std::condition_variable
#include <cstdint>				// std::uint8_t, std::int64_t, std::uint64_t
#include <exception>			// std::exception
#include <iostream>				// std::cout, std::flush
#include <mutex>				// std::lock_guard, std::mutex, std::unique_lock
#include <queue>				// std::queue
#include <string>				// std::string
#include <string_view>			// std::string_view, std::string_view_literals
#include <thread>				// std::this_thread, std::thread
#include <utility>				// std::swap

namespace crawlservpp::Module {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! Number of seconds to sleep on connection errors.
	inline constexpr auto sleepOnConnectionErrorS{10};

	//! Number of milliseconds to sleep before checking whether the thread is still running.
	inline constexpr auto sleepMs{800};

	//! Status message prefix for interrupted threads.
	inline constexpr auto statusPrefixInterrupted{"INTERRUPTED "sv};

	//! Status message prefix for paused threads.
	inline constexpr auto statusPrefixPaused{"PAUSED "sv};

	///@}

	/*
	 * DECLARATION
	 */

	//! Abstract class providing module-independent thread functionality.
	class Thread {
		// for convenience
		using ConnectionException = Main::Database::ConnectionException;

		using ModuleOptions = Struct::ModuleOptions;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using DatabaseLock = Wrapper::DatabaseLock<Database>;

	public:
		///@name Construction and Destruction
		///@{

		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);

		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);

		//! Default destructor.
		virtual ~Thread() = default;

		///@}
		///@name Getters
		///@{

		std::uint64_t getId() const;
		std::uint64_t getWebsite() const;
		std::uint64_t getUrlList() const;
		std::uint64_t getConfig() const;
		bool isShutdown() const;
		bool isRunning() const;
		bool isFinished() const;
		bool isPaused() const;

		///@}
		///@name Thread Control
		///@{

		void start();
		bool pause();
		void unpause();
		void stop();
		void interrupt();
		void end();

		///@}
		///@name Time Travel
		///@{

		void warpTo(std::uint64_t target);

		///@}

		//! Class for generic thread exceptions.
		MAIN_EXCEPTION_CLASS();

		/**@name Copy and Move
		 * The class is neither copyable, nor movable.
		 */
		///@{

		//! Deleted copy constructor.
		Thread(Thread&) = delete;

		//! Deleted copy assignment operator.
		Thread& operator=(Thread&) = delete;

		//! Deleted move constructor.
		Thread(Thread&&) = delete;

		//! Deleted move assignment operator.
		Thread& operator=(Thread&&) = delete;

		///@}

	protected:
		///@name Database Connection
		///@{

		//! %Database connection for the thread.
		Database database;

		///@}
		///@name Configuration
		///@{

		//! Namespace of the website used by the thread.
		std::string websiteNamespace;

		//! Namespace of the URL list used by the thread.
		std::string urlListNamespace;

		//! JSON string of the configuration used by the thread.
		/*!
		 * \sa Main::Database::getConfiguration
		 */
		std::string configuration;

		///@}
		///@name Protected Getters
		///@{

		bool isInterrupted() const;
		std::string getStatusMessage() const;
		float getProgress() const;
		std::uint64_t getLast() const;
		std::int64_t getWarpedOverAndReset();

		///@}
		///@name Protected Setters
		///@{

		void setStatusMessage(const std::string& statusMessage);
		void setProgress(float newProgress);
		void setLast(std::uint64_t lastId);
		void incrementLast();

		///@}
		///@name Protected Thread Control
		///@{

		void sleep(std::uint64_t ms) const;
		void allowPausing();
		void disallowPausing();
		void pauseByThread();

		///@}
		///@name Logging
		///@{

		bool isLogLevel(std::uint8_t level) const;
		void log(std::uint8_t level, const std::string& logEntry);
		void log(std::uint8_t level, std::queue<std::string>& logEntries);

		///@}
		///@name Events
		///@{

		//! Initializes the module.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific module.
		 */
		virtual void onInit() = 0;

		//! Performs a module tick.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific module.
		 */
		virtual void onTick() = 0;

		//! Pauses the module.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific module.
		 */
		virtual void onPause() = 0;

		//! Unpauses the module.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific module.
		 */
		virtual void onUnpause() = 0;

		//! Clears the module.
		/*!
		 * Needs to be implemented by the (child) class
		 *  for the specific module.
		 */
		virtual void onClear() = 0;

		///@}

	private:
		Main::Database& databaseClass;				// access to the database for the class

		std::atomic<bool> pausable{true};			// thread is pausable
 		std::atomic<bool> running{true};			// thread is running (or paused)
		std::atomic<bool> paused{false};			// thread is paused
		std::atomic<bool> interrupted{false};		// thread has been interrupted by shutdown
		std::atomic<bool> terminated{false};		// thread has been terminated due to an exception
		std::atomic<bool> shutdown{false};			// shutdown in progress
		std::atomic<bool> finished{false};			// shutdown is finished

		std::uint64_t id{0};						// the ID of the thread in the database
		std::string module;							// the module of the thread (used for logging)
		ThreadOptions options;						// options for the thread

		std::uint64_t last{0};						// last ID for the thread
		std::atomic<std::uint64_t> overwriteLast{0};// ID to overwrite last ID with ("time travel")
		std::int64_t warpedOver{0};					// no. of IDs that have been skipped (might be negative, ONLY for threads!)

		std::condition_variable pauseCondition;		// condition variable to wait for unpause
		mutable std::mutex pauseLock;				// lock for accessing the condition variable

		std::string status; 						// status message of the thread (without pause state)
		mutable std::mutex statusLock;				// lock for accessing the status message

		float progress{0.F};						// current progress of the thread, in percent
		mutable std::mutex progressLock;			// lock for accessing the current progress

		std::thread thread;							// pointer to the thread

		// timing statistics (in seconds)
		std::chrono::steady_clock::time_point startTimePoint{std::chrono::steady_clock::time_point::min()};
		std::chrono::steady_clock::time_point pauseTimePoint{std::chrono::steady_clock::time_point::min()};
		std::chrono::duration<std::uint64_t> runTime{std::chrono::duration<std::uint64_t>::zero()};
		std::chrono::duration<std::uint64_t> pauseTime{std::chrono::duration<std::uint64_t>::zero()};

		// internal timing functions
		std::uint64_t getRunTime() const;
		void updateRunTime();
		void updatePauseTime();

		// internal thread functions
		void init();
		void tick();
		void wait();
		void clear();

		// pause checker
		bool isUnpaused() const;

		// internal helper functions
		void onEnd();
		void clearException(const std::exception& e, const std::string& inFunction);
		void clearException(const std::string& inFunction);

		// main function
		void main();
	};

} /* namespace crawlservpp::Module */

#endif /* MODULE_THREAD_HPP_ */
