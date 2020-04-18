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

	// constructor A: run a previously interrupted thread
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	)
			: database(dbBase.getSettings(), threadOptions.module),
			  databaseClass(dbBase),
			  pausable(true),
			  running(true),
			  paused(threadStatus.paused),
			  interrupted(false),
			  terminated(false),
			  shutdown(false),
			  finished(false),
			  id(threadStatus.id),
			  module(threadOptions.module),
			  options(threadOptions),
			  last(threadStatus.last),
			  overwriteLast(0),
			  warpedOver(0),
			  startTimePoint(std::chrono::steady_clock::time_point::min()),
			  pauseTimePoint(std::chrono::steady_clock::time_point::min()),
			  runTime(std::chrono::duration<std::uint64_t>::zero()),
			  pauseTime(std::chrono::duration<std::uint64_t>::zero()) {
		// remove paused or interrupted thread status from status message
		if(
				threadStatus.status.length() >= 12
				&& threadStatus.status.substr(0, 12) == "INTERRUPTED "
		)
			this->status = threadStatus.status.substr(12);
		else if(
				threadStatus.status.length() >= 7
				&& threadStatus.status.substr(0, 7) == "PAUSED "
		)
			this->status = threadStatus.status.substr(7);

		// get namespace of website, URL list and configuration
		this->websiteNamespace = this->databaseClass.getWebsiteNamespace(this->getWebsite());
		this->urlListNamespace = this->databaseClass.getUrlListNamespace(this->getUrlList());
		this->configuration = this->databaseClass.getConfiguration(this->getConfig());

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
		if(threadStatus.id)
			this->databaseClass.setThreadStatus(
					threadStatus.id,
					threadStatus.paused,
					this->status
			);
	}

	// constructor B: start a new thread (using constructor A to initialize values)
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) : Thread(dbBase, threadOptions, ThreadStatus()) {
		// add the thread to the database and save its (new) ID
		this->id = this->databaseClass.addThread(threadOptions);

		this->database.setThreadId(this->id);
	}

	// destructor stub
	Thread::~Thread() {}

	// get the ID of the thread (thread-safe)
	std::uint64_t Thread::getId() const {
		return this->id;
	}

	// get the ID of the website (thread-safe)
	std::uint64_t Thread::getWebsite() const {
		return this->options.website;
	}

	// get the ID of the URL list (thread-safe)
	std::uint64_t Thread::getUrlList() const {
		return this->options.urlList;
	}

	// get the ID of the configuration (thread-safe)
	std::uint64_t Thread::getConfig() const {
		return this->options.config;
	}

	// get whether a shutdown is in progress (or has finished, see Thread::isFinished()) (thread-safe)
	bool Thread::isShutdown() const {
		return this->shutdown.load();
	}

	// get whether the thread is still supposed to run (thread-safe)
	bool Thread::isRunning() const {
		return this->running.load();
	}

	// get whether the shutdown of the thread was finished (thread-safe)
	bool Thread::isFinished() const {
		return this->finished.load();
	}

	// get whether the thread has been paused (thread-safe)
	bool Thread::isPaused() const {
		return this->paused.load();
	}

	// start the thread (may not be used by the thread itself!)
	void Thread::start() {
		if(this->thread.joinable())
			throw Exception("Thread::start(): A thread has already been started");

		// run thread
		this->thread = std::thread(&Thread::main, this);
	}

	// pause the thread, return whether the thread is pausable (may not be used by the thread itself!)
	bool Thread::pause() {
		// check whether thread is pausable
		if(!(this->pausable.load()))
			return false;

		// change internal pause state if necessary
		bool changeIfValueIs = false;

		if(this->paused.compare_exchange_strong(changeIfValueIs, true)) {
			// set pause state in the database if the internal state has changed
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->databaseClass.setThreadStatus(this->id, true, this->status);
		}

		return true;
	}

	// unpause the thread (may not be used by the thread itself!)
	void Thread::unpause() {
		// change internal pause state if necessary
		bool changeIfValueIs = true;

		if(this->paused.compare_exchange_strong(changeIfValueIs, false)) {
			// locks for condition variable and database status
			std::lock_guard<std::mutex> unpause(this->pauseLock);
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			// set pause state in the databas
			this->databaseClass.setThreadStatus(this->id, false, this->status);

			// update condition variable
			this->pauseCondition.notify_one();
		}
	}

	// shutdown the thread (may not be used by the thread itself!)
	//  NOTE: Module::Thread::end() must be called afterwards to wait for the thread!
	void Thread::stop() {
		// stop running if necessary
		if(this->running.load()) {
			// first set shutdown option...
			this->shutdown.store(true);

			// ...then stop thread if it has not been stopped in the meantime
			bool changeIfValueIs = true;

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

	// interrupt the thread due to an exception (may not be used by the thread itself!)
	//  NOTE: Module::Thread::end() must be called afterwards to wait for the thread!
	void Thread::interrupt() {
		// check whether thread exists and is running
		if(this->running.load()) {
			// interrupt thread
			this->interrupted.store(true);
			this->shutdown.store(true);

			bool changeIfRunningIs = true;
			bool changeIfPausedIs = true;

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

	// wait for the thread until shutdown is completed (may not be used by the thread itself!)
	//  NOTE: Module::Thread::stop() or ::interrupt() must be called beforehand!
	void Thread::end() {
		if(this->shutdown.load()) {
			// if thread exists and has been interrupted, wait for thread and join
			if(this->thread.joinable())
				this->thread.join();

			// remove thread from database if it has not been interrupted due to an exception
			if(!(this->interrupted.load()))
					this->databaseClass.deleteThread(this->id);
		}
	}

	// jump to specified target ID ("time travel"), throws Thread::Exception (thread-safe)
	void Thread::warpTo(std::uint64_t target) {
		// check argument
		if(!target)
			throw Exception("Thread::warpTo(): Target ID cannot be zero");

		// set target ID to overwrite
		this->overwriteLast.store(target - 1);
	}

	// sleep for the specified number of milliseconds (unless the thread is stopped; thread-safe)
	void Thread::sleep(std::uint64_t ms) const {
		while(ms > MODULE_THREAD_SLEEP_ON_SLEEP_MS) {
			if(!(this->running.load()))
				return;

			std::this_thread::sleep_for(std::chrono::milliseconds(MODULE_THREAD_SLEEP_ON_SLEEP_MS));

			ms -= MODULE_THREAD_SLEEP_ON_SLEEP_MS;
		}

		if(ms && this->running.load())
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	// return whether thread has been interrupted by shutdown (thread-safe)
	bool Thread::isInterrupted() const {
		return this->interrupted.load();
	}

	// force the thread to pause (to be used by the thread only!)
	void Thread::pauseByThread() {
		// set the internal pause state if the thread is not paused already
		bool changeIfValueIs = false;

		if(this->paused.compare_exchange_strong(changeIfValueIs, true)) {
			// set the pause state in the database
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->database.setThreadStatus(this->id, true, this->status);
		}
	}

	// set the status messsage of the thread (to be used by the thread only!)
	void Thread::setStatusMessage(const std::string& statusMessage) {
		// set internal status
		{
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->status = statusMessage;
		}

		// set status in database
		//  (when interrupted, the thread has been unpaused and the pause state needs to be ignored)
		if(this->interrupted.load())
			this->database.setThreadStatus(this->id, statusMessage);
		else
			this->database.setThreadStatus(this->id, this->paused.load(), statusMessage);
	}

	// set the progress of the thread (to be used by the thread only!)
	void Thread::setProgress(float progress) {
		// set progress in database
		this->database.setThreadProgress(this->id, progress, this->getRunTime());
	}

	// add a thread-specific log entry to the database if the current logging level is high enough
	//  (to be used by the thread only!)
	void Thread::log(unsigned short level, const std::string& logEntry) {
		this->database.log(level, logEntry);
	}

	// add multiple thread-specific log entries to the database if the current logging level is high enough
	//  (to be used by the thread only!)
	void Thread::log(unsigned short level, std::queue<std::string>& logEntries) {
		this->database.log(level, logEntries);
	}

	// check whether a certain log level is active
	bool Thread::isLogLevel(unsigned short level) const {
		return this->database.isLogLevel(level);
	}

	// allow the thread to be paused (enabled by default; thread-safe)
	void Thread::allowPausing() {
		this->pausable.store(true);
	}

	// do not allow the thread to be paused (thread-safe)
	void Thread::disallowPausing() {
		this->pausable.store(false);
	}

	// get the value of the last ID used by the thread (to be used by the thread only!)
	std::uint64_t Thread::getLast() const {
		return this->last;
	}

	// set the last ID used by the thread (to be used by the thread only!)
	void Thread::setLast(std::uint64_t last) {
		if(this->last != last) {
			// set the last ID internally
			this->last = last;

			// set the last ID in the database
			this->database.setThreadLast(this->id, last);
		}
	}

	// increment the last ID used by the thread (to be used by the thread only!)
	void Thread::incrementLast() {
		// increment the last ID internally
		++(this->last);

		// increment the last ID in the database
		this->database.setThreadLast(this->id, this->last);
	}

	// get a copy of the current status message (thread-safe)
	std::string Thread::getStatusMessage() const {
		std::lock_guard<std::mutex> statusLocked(this->statusLock);

		return this->status;
	}

	// return and reset the number of IDs that have been jumped over
	//  (might be negative; to be used by the thread only!)
	std::int64_t Thread::getWarpedOverAndReset() {
		std::int64_t result = 0;

		std::swap(this->warpedOver, result);

		return result;
	}

	// get the current run time of the thread in seconds
	std::uint64_t Thread::getRunTime() const {
		if(this->startTimePoint > std::chrono::steady_clock::time_point::min())
			return	(
							this->runTime +
							std::chrono::duration_cast<std::chrono::seconds>(
									std::chrono::steady_clock::now() - this->startTimePoint
							)
					).count();
		else
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
			if(!oldStatus.empty())
				this->setStatusMessage(oldStatus);
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
						<< MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SEC << "s"
						<< std::flush;

			std::this_thread::sleep_for(std::chrono::seconds(MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SEC));
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
		auto newId = this->overwriteLast.load();

		if(newId && this->overwriteLast.compare_exchange_strong(newId, 0)) {
			// save the old values for the time calculation
			const auto oldId = this->last;
			const auto oldTime = static_cast<double>(this->getRunTime());

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

			this->pauseCondition.wait(pause, std::bind(&Thread::isUnpaused, this));

			// notify the thread
			if(this->running.load())
				this->onUnpause();

			// update the pause time of the thread and save the new start time point
			this->updatePauseTime();

			this->startTimePoint = std::chrono::steady_clock::now();
		}
		// handle connection exceptions by sleeping
		catch(const ConnectionException& e) {
			std::this_thread::sleep_for(std::chrono::seconds(MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SEC));
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

	// function for checking whether to unpause the thread (thread-safe)
	bool Thread::isUnpaused() const {
		return !(this->paused.load());
	}

	// helper function: thread has been finished, set its status message if interrupted or log the timing
	void Thread::onEnd() {
		if(this->interrupted.load())
			this->setStatusMessage("INTERRUPTED " + this->status);
		else {
			// log timing statistic
			std::string logStr =
					"stopped after "
					+ Helper::DateTime::secondsToString(this->runTime.count())
					+ " running";

			if(this->pauseTime.count())
				logStr += " and " + Helper::DateTime::secondsToString(this->pauseTime.count()) + " pausing";

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

} /* crawlservpp::Module */
