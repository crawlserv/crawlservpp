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
 * Interface for a thread which implements all module-independent thread functionality like connecting to the database,
 *  managing the thread status (including pausing the thread), run the thread ticks and catching exceptions thrown by the thread.
 *
 *  Created on: Oct 10, 2018
 *      Author: ans
 */

#ifndef MODULE_THREAD_HPP_
#define MODULE_THREAD_HPP_

// do not catch thread exceptions: use only for debugging!
//#define MODULE_THREAD_DEBUG_NOCATCH

// hard-coded constants
#define MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SEC 10
#define MODULE_THREAD_SLEEP_ON_SLEEP_MS 800

#include "Database.hpp"

#include "../Main/Database.hpp"
#include "../Main/Exception.hpp"
#include "../Helper/DateTime.hpp"
#include "../Struct/ModuleOptions.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/ThreadStatus.hpp"
#include "../Wrapper/DatabaseLock.hpp"

#include <atomic>				// std::atomic
#include <chrono>				// std::chrono
#include <cmath>				// std::lround
#include <cstdint>				// std::int64_t, std::uint64_t
#include <condition_variable>	// std::condition_variable
#include <exception>			// std::exception
#include <iostream>				// std::cout, std::flush
#include <mutex>				// std::lock_guard, std::mutex, std::unique_lock
#include <queue>				// std::queue
#include <string>				// std::string
#include <thread>				// std::this_thread, std::thread
#include <utility>				// std::swap

namespace crawlservpp::Module {

	class Thread {
		// for convenience
		using ConnectionException = Main::Database::ConnectionException;

		using ModuleOptions = Struct::ModuleOptions;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using DatabaseLock = Wrapper::DatabaseLock<Database>;

	public:
		// constructors
		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);

		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);

		// destructor
		virtual ~Thread();

		// getters
		std::uint64_t getId() const;
		std::uint64_t getWebsite() const;
		std::uint64_t getUrlList() const;
		std::uint64_t getConfig() const;
		bool isShutdown() const;
		bool isRunning() const;
		bool isFinished() const;
		bool isPaused() const;

		// control functions
		void start();
		bool pause();
		void unpause();
		void stop();
		void interrupt();
		void end();

		// time travel
		void warpTo(std::uint64_t target);

		// class for Thread exceptions
		MAIN_EXCEPTION_CLASS();

		// not moveable, not copyable
		Thread(Thread&) = delete;
		Thread(Thread&&) = delete;
		Thread& operator=(Thread&) = delete;
		Thread& operator=(Thread&&) = delete;

	protected:
		Database database; 							// access to the database for the thread

		std::string websiteNamespace; 				// namespace of website (used by thread)
		std::string urlListNamespace; 				// namespace of URL list (used by thread)
		std::string configuration; 					// configuration

		// thread helper functions
		bool isInterrupted() const;
		void sleep(std::uint64_t ms) const;

		void pauseByThread();
		void setStatusMessage(const std::string& statusMessage);
		void setProgress(float progress);
		void log(unsigned short level, const std::string& logEntry);
		void log(unsigned short level, std::queue<std::string>& logEntries);
		bool isLogLevel(unsigned short level) const;
		void allowPausing();
		void disallowPausing();

		std::uint64_t getLast() const;
		void setLast(std::uint64_t last);
		void incrementLast();
		std::string getStatusMessage() const;
		std::int64_t getWarpedOverAndReset();

		virtual void onInit() = 0;
		virtual void onTick() = 0;
		virtual void onPause() = 0;
		virtual void onUnpause() = 0;
		virtual void onClear() = 0;

	private:
		Main::Database& databaseClass;				// access to the database for the class

		std::atomic<bool> pausable;					// thread is pausable
 		std::atomic<bool> running;					// thread is running (or paused)
		std::atomic<bool> paused;					// thread is paused
		std::atomic<bool> interrupted;				// thread has been interrupted by shutdown
		std::atomic<bool> terminated;				// thread has been terminated due to an exception
		std::atomic<bool> shutdown;					// shutdown in progress
		std::atomic<bool> finished;					// shutdown is finished

		std::uint64_t id;							// the ID of the thread in the database
		std::string module;							// the module of the thread (used for logging)
		ThreadOptions options;						// options for the thread

		std::uint64_t last;							// last ID for the thread
		std::atomic<std::uint64_t> overwriteLast;	// ID to overwrite last ID with ("time travel")
		std::int64_t warpedOver;					// no. of IDs that have been skipped (might be negative, ONLY for threads!)

		std::condition_variable pauseCondition;		// condition variable to wait for unpause
		mutable std::mutex pauseLock;				// lock for accessing the condition variable

		std::string status; 						// status message of the thread (without pause state)
		mutable std::mutex statusLock;				// lock for accessing the status message

		std::thread thread;							// pointer to the thread

		// timing statistics (in seconds)
		std::chrono::steady_clock::time_point startTimePoint;
		std::chrono::steady_clock::time_point pauseTimePoint;
		std::chrono::duration<std::uint64_t> runTime;
		std::chrono::duration<std::uint64_t> pauseTime;
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

		// private helper functions
		void onEnd();
		void clearException(const std::exception& e, const std::string& inFunction);
		void clearException(const std::string& inFunction);

		// main function
		void main();
	};

} /* crawlservpp::Module */

#endif /* MODULE_THREAD_HPP_ */
