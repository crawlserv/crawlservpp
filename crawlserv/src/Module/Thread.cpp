/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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

	// constructor A: run previously interrupted thread
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
			  module(threadOptions.module),
			  options(threadOptions),
			  last(threadStatus.last),
			  overwriteLast(0),
			  warpedOver(0),
			  startTimePoint(std::chrono::steady_clock::time_point::min()),
			  pauseTimePoint(std::chrono::steady_clock::time_point::min()),
			  runTime(std::chrono::duration<size_t>::zero()),
			  pauseTime(std::chrono::duration<size_t>::zero()) {
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

		// set ID
		this->id = threadStatus.id;

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

	// constructor B: start new thread (using constructor A to initialize values)
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	) : Thread(dbBase, threadOptions, ThreadStatus()) {
		// add thread to database and save ID
		this->id = this->databaseClass.addThread(threadOptions);

		this->database.setThreadId(this->id);
	}

	// destructor stub
	Thread::~Thread() {}

	// get ID of the thread (thread-safe)
	size_t Thread::getId() const {
		return this->id;
	}

	// get ID of the website (thread-safe)
	size_t Thread::getWebsite() const {
		return this->options.website;
	}

	// get ID of URL list (thread-safe)
	size_t Thread::getUrlList() const {
		return this->options.urlList;
	}

	// get ID of the configuration (thread-safe)
	size_t Thread::getConfig() const {
		return this->options.config;
	}

	// get whether thread was terminated due to an exception
	bool Thread::isShutdown() const {
		return this->shutdown.load();
	}

	// get whether thread is still supposed to run
	bool Thread::isRunning() const {
		return this->running.load();
	}

	// get whether shutdown was finished
	bool Thread::isFinished() const {
		return this->finished.load();
	}

	// get whether thread has been paused
	bool Thread::isPaused() const {
		return this->paused.load();
	}

	// start the thread (may not be used by the thread itself!)
	void Thread::start() {
		// run thread
		this->thread = std::thread(&Thread::main, this);
	}

	// pause the thread, return whether thread is pausable (may not be used by the thread itself!)
	bool Thread::pause() {
		// ignore if thread is paused
		if(this->paused.load())
			return true;

		// check whether thread is pausable
		if(!(this->pausable.load()))
			return false;

		// set internal pause state
		this->paused.store(true);

		// set pause state in database
		{
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->databaseClass.setThreadStatus(this->id, true, this->status);
		}

		return true;
	}

	// unpause the thread (may not be used by the thread itself!)
	void Thread::unpause() {
		// ignore if thread is not paused
		if(!(this->paused.load()))
			return;

		// set pause state in database
		{
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->databaseClass.setThreadStatus(this->id, false, this->status);
		}

		// set internal pause state
		std::lock_guard<std::mutex> unpause(this->pauseLock);

		this->paused.store(false);

		// update condition variable
		this->pauseCondition.notify_one();
	}

	// shutdown the thread (may not be used by the thread itself!)
	//  NOTE: Module::Thread::end() must be called afterwards to wait for the thread!
	void Thread::stop() {
		// stop running
		if(this->running.load()) {
			this->running.store(false);
			this->shutdown.store(true);

			// check whether thread has to be unpaused
			if(this->paused.load()) {
				// set internal pause state
				std::lock_guard<std::mutex> unpause(this->pauseLock);

				this->paused.store(false);

				// update condition variable
				this->pauseCondition.notify_one();
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
			this->running.store(false);
			this->shutdown.store(true);

			// check whether thread has to be unpaused
			if(this->paused.load()) {
				// set internal pause state
				std::lock_guard<std::mutex> unpause(this->pauseLock);

				this->paused.store(false);

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

	// jump to specified target ID ("time travel"), throws Thread::Exception
	void Thread::warpTo(size_t target) {
		// check argument
		if(!target)
			throw Exception("Target ID cannot be zero");

		// set target ID to overwrite
		this->overwriteLast = target - 1;
	}

	// sleep for the specified number of milliseconds (unless the thread is stopped)
	void Thread::sleep(size_t ms) const {
		while(ms > MODULE_THREAD_SLEEP_ON_SLEEP_MS) {
			if(!(this->isRunning()))
				return;

			std::this_thread::sleep_for(std::chrono::milliseconds(MODULE_THREAD_SLEEP_ON_SLEEP_MS));

			ms -= MODULE_THREAD_SLEEP_ON_SLEEP_MS;
		}

		if(ms && this->isRunning())
			std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}

	// return whether thread has been interrupted by shutdown
	bool Thread::isInterrupted() const {
		return this->interrupted.load();
	}

	// force the thread to pause (to be used by the thread only)
	void Thread::pauseByThread() {
		// ignore if thread is paused
		if(this->paused.load())
			return;

		// set internal pause state
		this->paused.store(true);

		// set pause state in database
		{
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->database.setThreadStatus(this->id, true, this->status);
		}
	}

	// set the status messsage of the thread (to be used by the thread only)
	void Thread::setStatusMessage(const std::string& statusMessage) {
		// set internal status
		{
			std::lock_guard<std::mutex> statusLocked(this->statusLock);

			this->status = statusMessage;
		}

		// set status in database (when interrupted, the thread has been unpaused and the pause state needs to be ignored)
		if(this->interrupted.load())
			this->database.setThreadStatus(this->id, statusMessage);
		else
			this->database.setThreadStatus(this->id, this->paused.load(), statusMessage);
	}

	// set the progress of the thread (to be used by the thread only)
	void Thread::setProgress(float progress) {
		// set progress in database
		this->database.setThreadProgress(this->id, progress, this->getRunTime());
	}

	// add thread-specific log entry to the database if logging level is high enough (to be used by the thread only)
	void Thread::log(unsigned short level, const std::string& logEntry) {
		this->database.log(level, logEntry);
	}

	// add multiple thread-specific log entries to the database if logging level is high enough (to be used by the thread only)
	void Thread::log(unsigned short level, std::queue<std::string>& logEntries) {
		this->database.log(level, logEntries);
	}

	// check whether a certain log level is active
	bool Thread::isLogLevel(unsigned short level) const {
		return this->database.isLogLevel(level);
	}

	// allow the thread to be paused (enabled by default)
	void Thread::allowPausing() {
		this->pausable.store(true);
	}

	// do not allow the thread to be paused
	void Thread::disallowPausing() {
		this->pausable.store(false);
	}

	// get value of last ID (to be used by the thread only)
	size_t Thread::getLast() const {
		return this->last;
	}

	// set last ID (to be used by the thread only)
	void Thread::setLast(size_t last) {
		// set last ID internally
		this->last = last;

		// set last ID in database
		this->database.setThreadLast(this->id, last);
	}

	// increment last ID (to be used by the thread only)
	void Thread::incrementLast() {
		// increment last ID internally
		++(this->last);

		// increment last ID in database
		this->database.setThreadLast(this->id, this->last);
	}

	// get a copy of the current status message
	std::string Thread::getStatusMessage() const {
		std::lock_guard<std::mutex> statusLocked(this->statusLock);

		return this->status;
	}

	// return and reset number of IDs that have been jumped over (might be negative, to be used by the thread only)
	long Thread::getWarpedOverAndReset() {
		const long result = this->warpedOver;

		this->warpedOver = 0;

		return result;
	}

	// get current run time in seconds
	size_t Thread::getRunTime() const {
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

	// update run time of thread (and save it to database)
	void Thread::updateRunTime() {
		if(this->startTimePoint > std::chrono::steady_clock::time_point::min()) {
			// add run time
			this->runTime +=
					std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::steady_clock::now() - this->startTimePoint
					);

			// reset start time point
			this->startTimePoint = std::chrono::steady_clock::time_point::min();

			// save new run time to database
			this->database.setThreadRunTime(this->id, this->runTime.count());
		}
	}

	// update pause time of thread and save it to database
	void Thread::updatePauseTime() {
		// check whether pause time was running
		if(this->pauseTimePoint > std::chrono::steady_clock::time_point::min()) {
			// add pause time
			this->pauseTime += std::chrono::duration_cast<std::chrono::seconds>(
					std::chrono::steady_clock::now() - this->pauseTimePoint
			);

			// reset pause time point
			this->pauseTimePoint = std::chrono::steady_clock::time_point::min();

			// save new pause time in database
			this->database.setThreadPauseTime(this->id, this->pauseTime.count());
		}
	}

	// initialize thread
	void Thread::init() {
		// get previous run and pause times (in seconds)
		this->runTime = std::chrono::seconds(this->database.getThreadRunTime(this->id));
		this->pauseTime = std::chrono::seconds(this->database.getThreadPauseTime(this->id));

		// save old thread status
		const std::string oldStatus(this->getStatusMessage());

#ifndef MODULE_THREAD_DEBUG_NOCATCH
		try {
#endif
			// initialize thread
			this->onInit();

			// set status message (useful when the thread is paused on startup)
			if(!oldStatus.empty())
				this->setStatusMessage(oldStatus);
#ifndef MODULE_THREAD_DEBUG_NOCATCH
		}
		// handle exceptions by trying to log and set status
		catch(const std::exception& e) {
			// log error
			try {
				this->log(1, "failed - " + std::string(e.what()) + ".");

				// try to set status
				this->setStatusMessage("ERROR " + std::string(e.what()));
			}
			catch(const std::exception& e2) {
				std::cout << "\n" << this->module << ": [#" << this->id << "] " << e.what();
				std::cout << "\n" << " [Could not write to log: " << e2.what() << "]" << std::flush;
			}

			// interrupt thread
			this->interrupted.store(true);
		}
#endif

		// save new start time point
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
		// handle connection exception by sleeping
		catch(const ConnectionException& e) {
			std::cout	<< '\n'
						<< e.what()
						<< " - sleeps for "
						<< MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SEC << "s"
						<< std::flush;

			std::this_thread::sleep_for(std::chrono::seconds(MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SEC));
		}
		// handle other exceptions by trying to log, set status and pause thread
		catch(const std::exception& e) {
			// log error
			this->log(1, "failed - " + std::string(e.what()) + ".");

			// set status
			this->setStatusMessage("ERROR " + std::string(e.what()));

			// pause thread
			this->pauseByThread();
		}
		catch(...) {
			// log error
			this->log(1, "failed - Unknown exception.");

			// set status
			this->setStatusMessage("ERROR Unknown exception");

			// pause thread
			this->pauseByThread();
		}
#endif

		// check for "time travel" to another ID
		if(this->overwriteLast) {
			// save old values for time calculation
			const size_t oldId = this->last;
			const double oldTime = static_cast<double>(this->getRunTime());

			// change status
			this->setLast(this->overwriteLast);

			this->overwriteLast = 0;
			this->warpedOver = this->last - oldId;

			// calculate new starting time
			if(oldId > 0 && oldTime > 0) {
				this->runTime += std::chrono::seconds(
						std::lround(
								oldTime * (static_cast<double>(this->warpedOver) / oldId)
						)
				);
			}
		}
	}

	// wait for pause to be released
	void Thread::wait() {
		try {
			// update run time and set pause time point
			this->updateRunTime();

			this->pauseTimePoint = std::chrono::steady_clock::now();

			// notify thread for pausing
			this->onPause();

			// wait for unpausing
			std::unique_lock<std::mutex> pause(this->pauseLock);

			this->pauseCondition.wait(pause, std::bind(&Thread::isUnpaused, this));

			// notify thread for unpausing
			if(this->running.load())
				this->onUnpause();

			// update pause time and save new start time point
			this->updatePauseTime();

			this->startTimePoint = std::chrono::steady_clock::now();
		}
		// handle connection exception by sleeping
		catch(const ConnectionException& e) {
			std::this_thread::sleep_for(std::chrono::seconds(MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SEC));
		}
	}

	// clear thread
	void Thread::clear() {
		// try to update run time
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

		// try to notify thread for clearing
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

	// function for checking whether to unpause the thread (multi-threading safe)
	bool Thread::isUnpaused() const {
		return !(this->paused.load());
	}

	// helper function: thread finished, set status message if interrupted and log timing statistics
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
		// if that fails too, write the original exception to the console
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
		// try to log the unknown exception
		try {
			this->database.log(
					1,
					"[WARNING] Unknown exception in Thread::on"
					+ inFunction
					+ "()"
			);
		}
		// if that fails too, write the original exception to the console
		catch(...) {
			std::cout	<< "\nWARNING: Unknown exception in Thread::"
						<< inFunction
						<< "()"
						<< std::flush;
		}
	}

	// main function of the thread
	void Thread::main() {
		// connect to database and prepare logging
		this->database.connect();
		this->database.prepare();

	#ifndef MODULE_THREAD_DEBUG_NOCATCH
		try
	#endif
		{
			// init thread
			this->init();

			// run thread
			while(this->running.load() && !(this->interrupted.load())) {
				// check pause state
				if(this->paused.load()) {
					// thread paused: wait for pause to be released
					this->wait();
				}
				else {
					// thread not paused: run tick
					this->tick();
				}
			}

			// clear thread
			this->clear();
		}

	#ifndef MODULE_THREAD_DEBUG_NOCATCH
		// handle non-caught exceptions by thread
		catch(const std::exception& e) {
			try {
				// log error
				this->log(1, "failed - " + std::string(e.what()) + ".");

				// update run or pause time
				this->updateRunTime();
				this->updatePauseTime();
			}
			catch(const std::exception& e2) {
				// log exceptions -> send to stdout
				std::cout << "\n> Thread terminated - " << e.what() << "." << std::flush;
				std::cout << "\n> Thread could not write to log - " << e2.what() << "." << std::flush;
			}

			this->terminated.store(true);
		}
	#endif
	}

} /* crawlservpp::Module */
