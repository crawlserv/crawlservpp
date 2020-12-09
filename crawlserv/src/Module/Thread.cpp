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
 * Thread.cpp
 *
 * Interface for a thread which implements all module-independent thread functionality like connecting to the database,
 *  managing the thread status (including pausing the thread), run the thread ticks and catching exceptions thrown by the thread.
 *
 *  Created on: Oct 10, 2018
 *      Author: ans
 */

#include "Thread.hpp"

namespace crawlservpp::Module {

	/*
	 * CONSTRUCTION
	 */

	//! Constructor initializing a previously interrupted thread.
	/*!
	 * \param dbBase Reference to the main
	 *   database connection.
	 * \param threadOptions Constant reference
	 *   to a structure containing the options
	 *   for the thread.
	 * \param threadStatus Constant reference
	 *   to a structure containing the last
	 *   known status of the thread.
	 */
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	)
			: database(dbBase.getSettings(), threadOptions.module),
			  databaseClass(dbBase),
			  paused(threadStatus.paused),
			  id(threadStatus.id),
			  module(threadOptions.module),
			  options(threadOptions),
			  last(threadStatus.last),
			  processed(threadStatus.processed) {
		// remove paused or interrupted thread status from status message
		if(
				threadStatus.status.length() >= statusPrefixInterrupted.length()
				&& threadStatus.status.substr(
						0,
						statusPrefixInterrupted.length()
				) == statusPrefixInterrupted
		) {
			this->status = threadStatus.status.substr(statusPrefixInterrupted.length());
		}
		else if(
				threadStatus.status.length() >= statusPrefixPaused.length()
				&& threadStatus.status.substr(
						0,
						statusPrefixPaused.length()
				) == statusPrefixPaused
		) {
			this->status = threadStatus.status.substr(statusPrefixPaused.length());
		}

		// get namespace of website, URL list and configuration
		this->websiteNamespace = this->databaseClass.getWebsiteNamespace(
				this->getWebsite()
		);
		this->urlListNamespace = this->databaseClass.getUrlListNamespace(
				this->getUrlList()
		);
		this->configuration = this->databaseClass.getConfiguration(
				this->getConfig()
		);

		// set general database options
		this->database.setOptions(
				ModuleOptions(
						threadStatus.id,
						this->getWebsite(),
						this->websiteNamespace,
						this->getUrlList(),
						this->urlListNamespace
				)
		);

		// update thread status in database (remove "INTERRUPTED ", add "PAUSED " before status if necessary)
		if(threadStatus.id > 0) {
			this->databaseClass.setThreadStatus(
					threadStatus.id,
					threadStatus.paused,
					this->status
			);
		}
	}

	//! Constructor initializing a new thread.
	/*!
	 * \param dbBase Reference to the main
	 *   database connection.
	 * \param threadOptions Constant reference
	 *   to a structure containing the options
	 *   for the thread.
	 */
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) : Thread(dbBase, threadOptions, ThreadStatus()) {
		// add the thread to the database and save its (new) ID
		this->id = this->databaseClass.addThread(threadOptions);

		this->database.setThreadId(this->id);
	}

	/*
	 * GETTERS
	 */

	//! Gets the ID of the thread.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns The unique ID identifying the
	 *   thread in the database.
	 */
	std::uint64_t Thread::getId() const {
		return this->id;
	}

	//! Gets the ID of the website used by the thread.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns The unique ID identifying the
	 *   used website in the database.
	 */
	std::uint64_t Thread::getWebsite() const {
		return this->options.website;
	}

	//! Gets the ID of the URL list used by the thread.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns The unique ID identifying the
	 *   used URL list in the database.
	 */
	std::uint64_t Thread::getUrlList() const {
		return this->options.urlList;
	}

	//! Gets the ID of the configuration used by the thread.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns The unique ID identifying the
	 *   used configuration in the database.
	 */
	std::uint64_t Thread::getConfig() const {
		return this->options.config;
	}

	//! Checks whether the thread is shutting down or has shut down.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns True, if the thread is shutting
	 *   down or has been shut down. False, if
	 *   the thread is continuing to run.
	 *
	 * \sa Thread::isFinished
	 */
	bool Thread::isShutdown() const {
		return this->shutdown.load();
	}

	//! Checks whether the thread is still supposed to run.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns True, if the thread is has not
	 *   been cancelled, even when it is paused.
	 *   False, if the thread is not supposed to
	 *   run any longer.
	 */
	bool Thread::isRunning() const {
		return this->running.load();
	}

	//! Checks whether the shutdown of the thread has been finished.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns True, if the thread has been
	 *   completely shut down. False otherwise.
	 */
	bool Thread::isFinished() const {
		return this->finished.load();
	}

	//! Checks whether the thread has been paused.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns True, if the thread has been
	 *   paused. False otherwise.
	 */
	bool Thread::isPaused() const {
		return this->paused.load();
	}

	/*
	 * THREAD CONTROL
	 */

	//! Starts running the thread.
	/*!
	 * \warning May not be used by the thread itself!
	 */
	void Thread::start() {
		if(this->thread.joinable()) {
			throw Exception(
					"Thread::start():"
					" A thread has already been started"
			);
		}

		// run thread
		this->thread = std::thread(&Thread::main, this);
	}

	//! Pauses the thread.
	/*!
	 * \warning May not be used by the thread itself!
	 *
	 * \returns True, if the thread has successfully
	 *   been paused. False, if the thread is not
	 *   pausable at the moment.
	 *
	 * \sa allowPausing, disallowPausing
	 */
	bool Thread::pause() {
		// check whether thread is pausable
		if(!(this->pausable.load())) {
			return false;
		}

		// change internal pause state if necessary
		bool changeIfValueIs{false};

		if(this->paused.compare_exchange_strong(changeIfValueIs, true)) {
			// set pause state in the database if the internal state has changed
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->databaseClass.setThreadStatus(this->id, true, this->status);
		}

		return true;
	}

	//! Unpauses the thread.
	/*!
	 * \warning May not be used by the thread itself!
	 */
	void Thread::unpause() {
		// change internal pause state if necessary
		bool changeIfValueIs{true};

		if(this->paused.compare_exchange_strong(changeIfValueIs, false)) {
			// lock condition variable and database status
			std::lock_guard<std::mutex> unpause(this->pauseLock);
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			// set pause state in the databas
			this->databaseClass.setThreadStatus(this->id, false, this->status);

			// update condition variable
			this->pauseCondition.notify_one();
		}
	}

	//! Shuts down the thread.
	/*!
	 * \note end() needs to be called afterwards to
	 *   wait for the thread.
	 *
	 * \warning May not be used by the thread itself!
	 */
	void Thread::stop() {
		// stop running if necessary
		if(this->running.load()) {
			// first set shutdown option...
			this->shutdown.store(true);

			// ...then stop thread if it has not been stopped in the meantime
			bool changeIfValueIs{true};

			if(this->running.compare_exchange_strong(changeIfValueIs, false)) {
				// unpause thread first if necessary
				changeIfValueIs = true;

				if(this->paused.compare_exchange_strong(changeIfValueIs, false)) {
					// notify other thread via condition variable
					std::lock_guard<std::mutex> unpause(this->pauseLock);

					this->pauseCondition.notify_one();
				}
			}
		}

		// reset interrupted status to allow thread deletion from database on manual stop
		this->interrupted.store(false);
	}

	//! Interrupts the thread due to an exception.
	/*!
	 * \note end() needs to be called afterwards to
	 *   wait for the thread.
	 *
	 * \warning May not be used by the thread itself!
	 */
	void Thread::interrupt() {
		// check whether thread exists and is running
		if(this->running.load()) {
			// interrupt thread
			this->interrupted.store(true);
			this->shutdown.store(true);

			bool changeIfRunningIs{true};
			bool changeIfPausedIs{true};

			// stop AND unpause thread if (still) necessary
			if(
					this->running.compare_exchange_strong(changeIfRunningIs, false)
					&& this->paused.compare_exchange_strong(changeIfPausedIs, false)
			) {
				// notify other thread via condition variable
				std::lock_guard<std::mutex> unpause(this->pauseLock);

				// update condition variable
				this->pauseCondition.notify_one();
			}
		}
	}

	//! Waits for the thread until shutdown is completed.
	/*!
	 * \note Either stop() or interrupt() must have
	 *   been called before calling this function.
	 *
	 * \warning May not be used by the thread itself!
	 */
	void Thread::end() {
		if(this->shutdown.load()) {
			// if thread exists and has been interrupted, wait for thread and join
			if(this->thread.joinable()) {
				this->thread.join();
			}

			// remove thread from database if it has not been interrupted due to an exception
			if(!(this->interrupted.load())) {
				this->databaseClass.deleteThread(this->id);
			}
		}
	}

	//! Will reset the thread before the next tick.
	/*
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 */
	void Thread::reset() {
		this->toReset.store(true);
	}

	/*
	 * TIME TRAVEL
	 */

	//! Jumps to the specified target ID ("time travel").
	/*!
	 * Skips the normal process of determining the next
	 *  ID once the current ID has been processed.
	 *
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \param target The target ID that should be
	 *   processed next.
	 *
	 * \throws Module::Thread::Exception if no target
	 *   is specified, i.e. the target ID is zero.
	 *
	 * \sa getWarpedOverAndReset
	 */
	void Thread::warpTo(std::uint64_t target) {
		// check argument
		if(target == 0) {
			throw Exception(
					"Thread::warpTo():"
					" No target has been specified"
			);
		}

		// set target ID to overwrite
		this->overwriteLast.store(target - 1);
	}

	/*
	 * PROTECTED GETTERS
	 */

	//! Checks whether the thread has been interrupted.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns True if the thread has been
	 *   interrupted. False otherwise.
	 */
	bool Thread::isInterrupted() const {
		return this->interrupted.load();
	}

	//! Gets the current status message.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns A copy of the current status
	 *   message.
	 */
	std::string Thread::getStatusMessage() const {
		std::lock_guard<std::mutex> statusLocked(this->statusLock);

		return this->status;
	}

	//! Gets the current progress, in percent.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \returns The current progress of the
	 *   thread, in percent – between @c 0.F
	 *   (none) and @c 1.F (done).
	 */
	float Thread::getProgress() const {
		std::lock_guard<std::mutex> progressLocked(this->progressLock);

		return this->progress;
	}

	//! Gets the value of the last ID processed by the thread.
	/*!
	 * \warning May only be used by the thread itself,
	 *  not by the main thread!
	 *
	 * \returns The ID last processed by the thread,
	 *   or zero if no ID has been processed yet.
	 */
	std::uint64_t Thread::getLast() const {
		return this->last;
	}

	//! Gets the number of IDs that have been jumped over, and resets them.
	/*!
	 * Resets the number of IDs jumped over to zero.
	 *
	 * \warning May only be used by the thread itself,
	 *  not by the main thread!
	 *
	 * \returns The number of IDs that have been jumped
	 *   over due to a call to warpTo(), or zero if no
	 *   IDs have been jumped over, at least not since
	 *   the last call to getWarpedOverAndReset(). The
	 *   result might be negative, if warpTo() resulted
	 *   in a jump to a previous ID.
	 */
	std::int64_t Thread::getWarpedOverAndReset() {
		std::int64_t result{0};

		std::swap(this->warpedOver, result);

		return result;
	}

	/*
	 * PROTECTED SETTERS
	 */

	//! Sets the status message of the thread.
	/*!
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 *
	 * \note String views cannot be used, because
	 *   they are not supported by the API for the
	 *   MySQL database.
	 *
	 * \param statusMessage Constant reference to a
	 *   string containing the new status message to
	 *   be set.
	 *
	 * \sa Main::Database::setThreadStatus
	 */
	void Thread::setStatusMessage(const std::string& statusMessage) {
		// set internal status
		{
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->status = statusMessage;
		}

		// set status in database
		//  (when interrupted, the thread has been unpaused and the pause state needs to be ignored)
		if(this->interrupted.load()) {
			this->database.setThreadStatus(
					this->id,
					statusMessage
			);
		}
		else {
			this->database.setThreadStatus(
					this->id,
					this->paused.load(),
					statusMessage
			);
		}
	}

	//! Sets the progress of the thread.
	/*!
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 *
	 * \param newProgress The new progress of the
	 *   thread, between @c 0.f (none), and @c 1.f
	 *   (done).
	 *
	 * \sa Main::Database::setThreadProgress
	 */
	void Thread::setProgress(float newProgress) {
		// set internal progress
		{
			std::lock_guard<std::mutex> progressLocked(this->progressLock);

			this->progress = newProgress;
		}

		// set progress in database
		this->database.setThreadProgress(
				this->id,
				progress,
				this->getRunTime()
		);
	}

	//! Sets the last ID processed by the thread.
	/*!
	 * Also sets the number of processed IDs, make
	 *  sure to increment it before if the ID has
	 *  been processed.
	 *
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 *
	 * \param lastId The last ID processed by the
	 *   thread.
	 *
	 * \sa incrementProcessed, incrementLast,
	 *   Main::Database::setThreadLast
	 */
	void Thread::setLast(std::uint64_t lastId) {
		if(this->last != lastId) {
			// set the last ID internally
			this->last = lastId;

			// set the last ID in the database
			this->database.setThreadLast(this->id, lastId, this->processed);
		}
	}

	//! Increments the last ID processed by the thread.
	/*!
	 * Also sets the number of processed IDs, make
	 *  sure to increment it before if the ID has
	 *  been processed.
	 *
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 *
	 * \sa incrementProcessed, setLast,
	 *   Main::Database::setThreadLast
	 */
	void Thread::incrementLast() {
		// increment the last ID internally
		++(this->last);

		// increment the last ID in the database
		this->database.setThreadLast(this->id, this->last, this->processed);
	}

	//! Increments the number of IDs processed by the thread.
	/*!
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 *
	 * \sa setLast, incrementLast
	 */
	void Thread::incrementProcessed() {
		++(this->processed);
	}

	/*
	 * PROTECTED THREAD CONTROL
	 */

	//! Lets the thread sleep for the specified number of milliseconds.
	/*!
	 * The sleep will be interrupted if the thread is
	 *  stopped.
	 *
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 * \param ms The number of milliseconds for the
	 *   thread to sleep, if it is not stopped.
	 */
	void Thread::sleep(std::uint64_t ms) const {
		while(ms > sleepMs) {
			if(!(this->running.load())) {
				return;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

			ms -= sleepMs;
		}

		if(ms > 0 && this->running.load()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
		}
	}

	//! Allows the thread to be paused.
	/*!
	 * Threads are pausable by default. Use this
	 *  function if pausing has been disallowed
	 *  via disallowPausing().
	 *
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 */
	void Thread::allowPausing() {
		this->pausable.store(true);
	}

	//! Disallows the thread to be paused.
	/*!
	 * \b Thread-safe: Can be used by both the
	 *   module and the main thread.
	 *
	 */
	void Thread::disallowPausing() {
		this->pausable.store(false);
	}

	//! Forces the thread to pause.
	/*!
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 */
	void Thread::pauseByThread() {
		// set the internal pause state if the thread is not paused already
		bool changeIfValueIs{false};

		if(this->paused.compare_exchange_strong(changeIfValueIs, true)) {
			// set the pause state in the database
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->database.setThreadStatus(this->id, true, this->status);
		}
	}

	/*
	 * LOGGING
	 */

	//! Checks whether a certain logging level is enabled.
	/*!
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 *
	 * \param level The logging level to be checked
	 *   for.
	 *
	 * \returns True, if the current logging level is
	 *   at least as high as the given level. False,
	 *   if the current logging level is lower than
	 *   the given one.
	 */
	bool Thread::isLogLevel(std::uint8_t level) const {
		return this->database.isLogLevel(level);
	}

	//! Adds a thread-specific log entry to the database, if the current logging level is high enough.
	/*!
	 * Removes invalid UTF-8 characters if
	 *  necessary.
	 *
	 * If debug logging is active, the entry
	 *  will be written to the logging file
	 *  as well.
	 *
	 * The log entry will not be written to the
	 *   database, if the current logging level
	 *   is lower than the specified logging
	 *   level. The logging level does not
	 *   affect the writing of logging entries
	 *   being to the logging file when debug
	 *   logging is active.
	 *
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 *
	 * \note String views cannot be used, because
	 *   they are not supported by the API for the
	 *   MySQL database.
	 *
	 * \param level The logging level for the
	 *   entry. The entry will only be written
	 *   to the database, if the current logging
	 *   level is at least the logging level for
	 *   the entry.
	 * \param logEntry Constant reference to a
	 *   string containing the log entry.
	 *
	 * \sa Module::Database::log(std::uint8_t, const std::string&)
	 */
	void Thread::log(std::uint8_t level, const std::string& logEntry) {
		this->database.log(level, logEntry);
	}

	//! Adds multiple thread-specific log entries to the database, if the current logging level is high enough.
	/*!
	 * Removes invalid UTF-8 characters if
	 *  necessary.
	 *
	 * If debug logging is active, the entries
	 *  will be written to the logging file
	 *  as well.
	 *
	 * The log entries will not be written to
	 *   the database, if the current logging
	 *   level is lower than the specified
	 *   logging level. The logging level does
	 *   not affect the writing of logging
	 *   entries being to the logging file when
	 *   debug logging is active.
	 *
	 * \warning May only be used by the thread itself,
	 *   not by the main thread!
	 *
	 * \note String views cannot be used, because
	 *   they are not supported by the API for the
	 *   MySQL database.
	 *
	 * \param level The logging level for the
	 *   entries. The entries will only be
	 *   written to the database, if the current
	 *   logging level is at least the logging
	 *   level for the entry.
	 * \param logEntries Reference to a queue of
	 *   strings containing the log entries to
	 *   be written. It will be emptied
	 *   regardless whether the log entries
	 *   will be written to the database.
	 *
	 * \sa Module::Database::log(std::uint8_t, std::queue<std::string>&)
	 */
	void Thread::log(std::uint8_t level, std::queue<std::string>& logEntries) {
		this->database.log(level, logEntries);
	}

	/*
	 * INTERNAL TIMING FUNCTIONS (private)
	 */

	// get the current run time of the thread in seconds
	std::uint64_t Thread::getRunTime() const {
		if(this->startTimePoint > std::chrono::steady_clock::time_point::min()) {
			return	(
							this->runTime +
							std::chrono::duration_cast<std::chrono::seconds>(
									std::chrono::steady_clock::now() - this->startTimePoint
							)
					).count();
		}

		return this->runTime.count();
	}

	// update the run time of the thread (and save it to the database)
	void Thread::updateRunTime() {
		if(this->startTimePoint > std::chrono::steady_clock::time_point::min()) {
			// add the run time
			this->runTime +=
					std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::steady_clock::now() - this->startTimePoint
					);

			// reset the start time point
			this->startTimePoint = std::chrono::steady_clock::time_point::min();

			// save the new run time to the database
			this->database.setThreadRunTime(this->id, this->runTime.count());
		}
	}

	// update the pause time of the thread and save it to database
	void Thread::updatePauseTime() {
		// check whether the pause time was measured
		if(this->pauseTimePoint > std::chrono::steady_clock::time_point::min()) {
			// add the pause time
			this->pauseTime += std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::steady_clock::now() - this->pauseTimePoint
			);

			// reset the pause time point
			this->pauseTimePoint = std::chrono::steady_clock::time_point::min();

			// save the new pause time in the database
			this->database.setThreadPauseTime(this->id, this->pauseTime.count());
		}
	}

	/*
	 * INTERNAL THREAD FUNCTIONS (private)
	 */

	// initialize the thread
	void Thread::init() {
		// get the previous run and pause times of the thread (in seconds)
		this->runTime = std::chrono::seconds(this->database.getThreadRunTime(this->id));
		this->pauseTime = std::chrono::seconds(this->database.getThreadPauseTime(this->id));

		// save the old thread status
		const std::string oldStatus(this->getStatusMessage());

#ifndef MODULE_THREAD_DEBUG_NOCATCH
		try {
#endif
			// initialize the thread
			this->onInit();

			// set the status message of the thread (useful when it is paused on startup)
			if(!oldStatus.empty()) {
				this->setStatusMessage(oldStatus);
			}
#ifndef MODULE_THREAD_DEBUG_NOCATCH
		}
		// handle exceptions by trying to log and to set the status of the thread
		catch(const std::exception& e) {
			// log error
			try {
				this->log(1, "failed - " + std::string(e.what()) + ".");

				// try to set the status of the thread
				this->setStatusMessage("ERROR " + std::string(e.what()));
			}
			catch(const std::exception& e2) {
				std::cout << "\n" << this->module << ": [#" << this->id << "] " << e.what();
				std::cout << "\n" << " [Could not write to log: " << e2.what() << "]" << std::flush;
			}

			// interrupt the thread
			this->interrupted.store(true);
		}
#endif

		// save the new start time point
		this->startTimePoint = std::chrono::steady_clock::now();
	}

	// run thread tick
	void Thread::tick() {
#ifndef MODULE_THREAD_DEBUG_NOCATCH
		try {
#endif
			this->onTick();
#ifndef MODULE_THREAD_DEBUG_NOCATCH
		}
		// handle connection exceptions by sleeping
		catch(const ConnectionException& e) {
			std::cout	<< '\n'
						<< e.what()
						<< " - sleeps for "
						<< sleepOnConnectionErrorS << "s"
						<< std::flush;

			std::this_thread::sleep_for(std::chrono::seconds(sleepOnConnectionErrorS));
		}
		// handle other exceptions by trying to log, set the status of and pause the thread
		catch(const std::exception& e) {
			// log error
			this->log(1, "failed - " + std::string(e.what()) + ".");

			// set the status of the thread
			this->setStatusMessage("ERROR " + std::string(e.what()));

			// pause the thread
			this->pauseByThread();
		}
		catch(...) {
			// log the error
			this->log(1, "failed - Unknown exception.");

			// set the status of the thread
			this->setStatusMessage("ERROR Unknown exception");

			// pause the thread
			this->pauseByThread();
		}
#endif

		// check for "time travel" to another ID
		auto newId{this->overwriteLast.load()};

		if(newId > 0 && this->overwriteLast.compare_exchange_weak(newId, 0)) {
			// save the old values for the time calculation
			const auto oldId{this->last};
			const auto oldTime{static_cast<double>(this->getRunTime())};

			// change the last ID of the thread
			this->setLast(newId);

			this->warpedOver = this->last - oldId;

			// calculate the new starting time
			if(oldId > 0 && oldTime > 0) {
				this->runTime += std::chrono::seconds(
						std::lround(
								oldTime * (static_cast<double>(this->warpedOver) / oldId)
						)
				);
			}
		}
	}

	// wait for a pause to be released
	void Thread::wait() {
		try {
			// update the run time of the thread and set the pause time point
			this->updateRunTime();

			this->pauseTimePoint = std::chrono::steady_clock::now();

			// notify the thread for pausing
			this->onPause();

			// wait for the thread to get unpaused
			std::unique_lock<std::mutex> pause(this->pauseLock);

			this->pauseCondition.wait(
					pause,
					[this]() {
						return this->isUnpaused();
					}
			);

			// notify the thread
			if(this->running.load()) {
				this->onUnpause();
			}

			// update the pause time of the thread and save the new start time point
			this->updatePauseTime();

			this->startTimePoint = std::chrono::steady_clock::now();
		}
		// handle connection exceptions by sleeping
		catch(const ConnectionException& e) {
			std::this_thread::sleep_for(std::chrono::seconds(sleepOnConnectionErrorS));
		}
	}

	// clear thread
	void Thread::clear() {
		// try to update the run time of the thread
		try {
			this->updateRunTime();
		}
		// continue after exception handling
		catch(const std::exception& e) {
			this->clearException(e, "updateRunTime");
		}
		catch(...) {
			this->clearException("updateRuntime");
		}

		// try to notify the thread for clearing
		try {
			this->onClear();
		}
		// continue after exception handling
		catch(const std::exception& e) {
			this->clearException(e, "onClear");
		}
		catch(...) {
			this->clearException("onClear");
		}

		// try final status update
		try {
			this->onEnd();
		}
		// continue after exception handling
		catch(std::exception& e) {
			this->clearException(e, "onEnd");
		}
		catch(...) {
			this->clearException("onEnd");
		}

		// shutdown is finished
		this->finished.store(true);
	}

	/*
	 * PAUSE CHECKER (private)
	 */

	// function for checking whether to unpause the thread (thread-safe)
	bool Thread::isUnpaused() const {
		return !(this->paused.load());
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// helper function: thread has been finished, set its status message if interrupted or log the timing
	void Thread::onEnd() {
		if(this->interrupted.load()) {
			this->setStatusMessage("INTERRUPTED " + this->status);
		}
		else {
			// log timing statistic
			std::string logStr{
				"stopped after "
				+ Helper::DateTime::secondsToString(this->runTime.count())
				+ " running"
			};

			if(this->pauseTime.count() > 0) {
				logStr += " and " + Helper::DateTime::secondsToString(this->pauseTime.count()) + " pausing";
			}

			logStr += ".";

			this->log(1, logStr);
		}
	}

	// helper function for handling known exceptions when clearing the thread
	void Thread::clearException(const std::exception& e, const std::string& inFunction) {
		// try to log the (known) exception
		try {
			this->database.log(
					1,
					"[WARNING] Exception in Thread::"
					+ inFunction
					+ "() - "
					+ std::string(e.what())
			);
		}
		// if that fails too, write the original exception to stdout
		catch(...) {
			std::cout	<< "\nWARNING: Exception in Thread::"
						<< inFunction
						<< "() - "
						<< e.what()
						<< std::flush;
		}
	}

	// helper function for handling unknown exceptions when clearing the thread
	void Thread::clearException(const std::string& inFunction) {
		// try to log an unknown exceptions
		try {
			this->database.log(
					1,
					"[WARNING] Unknown exception in Thread::on"
					+ inFunction
					+ "()"
			);
		}
		// if that fails too, write the original exception to stdout
		catch(...) {
			std::cout	<< "\nWARNING: Unknown exception in Thread::"
						<< inFunction
						<< "()"
						<< std::flush;
		}
	}

	/*
	 * MAIN FUNCTION (private)
	 */

	// main function of the thread
	void Thread::main() {
		// connect to the database and prepare for logging
		this->database.connect();
		this->database.prepare();

	#ifndef MODULE_THREAD_DEBUG_NOCATCH
		try
	#endif
		{
			// initialize the thread
			this->init();

			// run the thread
			while(this->running.load() && !(this->interrupted.load())) {
				// check the reset state of the thread
				bool changeIfValueIs{true};

				if(this->toReset.compare_exchange_weak(changeIfValueIs, false)) {
					this->onReset();
				}

				// check the pause state of the thread
				if(this->paused.load()) {
					// the thread is paused: wait for the pause to be released
					this->wait();
				}
				else {
					// the thread is not paused: run a tick
					this->tick();
				}
			}

			// clear the thread
			this->clear();
		}

	#ifndef MODULE_THREAD_DEBUG_NOCATCH
		// handle non-caught exceptions by the thread
		catch(const std::exception& e) {
			try {
				// log an error
				this->log(1, "failed - " + std::string(e.what()) + ".");

				// update the run or the pause time
				this->updateRunTime();
				this->updatePauseTime();
			}
			catch(const std::exception& e2) {
				// if that fails too, write all the exceptions to stdout
				std::cout << "\n> Thread terminated - " << e.what() << "." << std::flush;
				std::cout << "\n> Thread could not write to log - " << e2.what() << "." << std::flush;
			}

			// the thread has been terminated by an exception
			this->terminated.store(true);
		}
	#endif
	}

} /* namespace crawlservpp::Module */
