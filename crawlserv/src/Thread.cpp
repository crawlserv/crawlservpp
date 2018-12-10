/*
 * Thread.cpp
 *
 * Interface for a thread which implements all module-independent thread functionality like connecting to the database,
 *  managing the thread status (including pausing the thread), run the thread ticks and catching exceptions thrown by the thread.
 *
 *  Created on: Oct 10, 2018
 *      Author: ans
 */

#include "Thread.h"

// constructor A: run previously interrupted thread
Thread::Thread(Database& dbBase, unsigned long threadId, const std::string& threadModule, const std::string& threadStatus,
		bool threadPaused, const ThreadOptions& threadOptions,	unsigned long threadLast)
		: database(dbBase.getSettings()), databaseClass(dbBase), module(threadModule), options(threadOptions) {
	// set thread pointer to NULL (will be initialized by Thread::start())
	this->threadPointer = NULL;

	// set status variables
	this->running = true;
	this->paused = threadPaused;
	this->interrupted = false;
	this->resumed = true;
	this->terminated = false;
	this->last = threadLast;
	this->startTimePoint = std::chrono::steady_clock::time_point::min();
	this->pauseTimePoint = std::chrono::steady_clock::time_point::min();
	this->runTime = std::chrono::duration<unsigned long>::zero();
	this->pauseTime = std::chrono::duration<unsigned long>::zero();

	if(threadId) {
		// set id
		this->id = threadId;

		// create id string
		std::ostringstream idStrStr;
		idStrStr << threadId;
		this->idString = idStrStr.str();
	}

	// get namespace of website, URL list and configuration
	this->websiteNameSpace = this->databaseClass.getWebsiteNameSpace(this->getWebsite());
	this->urlListNameSpace = this->databaseClass.getUrlListNameSpace(this->getUrlList());
	this->configuration = this->databaseClass.getConfiguration(this->getConfig());
}

// constructor B: start new thread (using constructor A to initialize values)
Thread::Thread(Database& dbBase, const std::string& threadModule, const ThreadOptions& threadOptions)
		: Thread(dbBase, 0, threadModule, "", false, threadOptions, 0) {
	// add thread to database and save id
	this->id = this->databaseClass.addThread(threadModule, threadOptions);
	std::ostringstream idStrStr;
	idStrStr << this->id;
	this->idString = idStrStr.str();
}

// destructor
Thread::~Thread() {
	if(this->threadPointer) {
		// THIS SHOULD NOT HAPPEN!
		std::cout << "WARNING: Thread pointer still active in Thread::~Thread() - Thread::interrupt() should have been called."
				<< std::endl;
		delete this->threadPointer;
		this->threadPointer = NULL;
	}
}

// start the thread (may not be used by the thread itself!)
void Thread::start() {
	// run thread
	if(!(this->threadPointer)) this->threadPointer = new std::thread(&Thread::main, this);
}

// pause the thread (may not be used by the thread itself!)
void Thread::pause() {
	// ignore if thread is paused
	if(this->paused) return;

	// set internal pause state
	this->paused = true;

	// set pause state in database
	{
		std::lock_guard<std::mutex> statusLocked(this->statusLock);
		this->databaseClass.setThreadStatus(this->id, true, this->status);
	}
}

// unpause the thread (may not be used by the thread itself!)
void Thread::unpause() {
	// ignore if thread is not paused
	if(!(this->paused)) return;

	// set pause state in database
	{
		std::lock_guard<std::mutex> statusLocked(this->statusLock);
		this->databaseClass.setThreadStatus(this->id, false, this->status);
	}

	// set internal pause state
	std::lock_guard<std::mutex> unpause(this->pauseLock);
	this->paused = false;

	// update condition variable
	this->pauseCondition.notify_one();
}

// stop the thread for good (may not be used by the thread itself!)
void Thread::stop() {
	// stop running
	if(this->threadPointer && this->running) {
		this->running = false;

		// check whether thread has to be unpaused
		if(this->paused) {
			// set internal pause state
			std::lock_guard<std::mutex> unpause(this->pauseLock);
			this->paused = false;

			// update condition variable
			this->pauseCondition.notify_one();
		}

		if(this->threadPointer) {
			this->threadPointer->join();
			delete this->threadPointer;
			this->threadPointer = NULL;
		}
	}

	// remove thread from database
	this->databaseClass.deleteThread(this->id);
}

// interrupt the thread for shutdown (may not be used by the thread itself!)
// NOTE:	Thread::finishInterrupt() has to be called afterwards to wait for the thread!
//			This enables the interruption of all threads simultaneously before waiting for their conclusion
void Thread::sendInterrupt() {
	// check whether thread exists and is running
	if(this->threadPointer && this->running) {
		// interrupt thread
		this->interrupted = true;
		this->running = false;

		// check whether thread has to be unpaused
		if(this->paused) {
			// set internal pause state
			std::lock_guard<std::mutex> unpause(this->pauseLock);
			this->paused = false;

			// update condition variable
			this->pauseCondition.notify_one();
		}
	}
}

// wait for the thread until interrupt is completed (may not be used by the thread itself!)
// NOTE:	Thread::sendInterrupt() has to be called beforehand to interrupt the thread!
//			This enables the interruption of all threads simultaneously before waiting for their conclusion
void Thread::finishInterrupt() {
	// check whether thread exists and has been interrupted
	if(this->threadPointer && this->interrupted) {
		// wait for thread
		this->threadPointer->join();

		// delete thread
		delete this->threadPointer;
		this->threadPointer = NULL;
	}
}

// get id of the thread (thread-safe)
unsigned long Thread::getId() const {
	return this->id;
}

// get id of the website (thread-safe)
unsigned long Thread::getWebsite() const {
	return this->options.website;
}

// get id of URL list (thread-safe)
unsigned long Thread::getUrlList() const {
	return this->options.urlList;
}

// get id of the configuration (thread-safe)
unsigned long Thread::getConfig() const {
	return this->options.config;
}

// get whether thread was terminated due to an exception
bool Thread::isTerminated() const {
	return this->terminated;
}

// get whether thread is still supposed to run
bool Thread::isRunning() const {
	return this->running;
}

// set the status messsage of the thread (to be used by the thread only)
void Thread::setStatusMessage(const std::string& statusMessage) {
	// set internal status
	{
		std::lock_guard<std::mutex> statusLocked(this->statusLock);
		this->status = statusMessage;
	}

	// set status in database
	this->database.setThreadStatus(this->id, this->paused, statusMessage);
}

// set the progress of the thread (to be used by the thread only)
void Thread::setProgress(float progress) {
	// set progress in database
	this->database.setThreadProgress(this->id, progress);
}

// add a log entry for the thread to the database using the module of the thread (to be used by the thread only)
void Thread::log(const std::string& entry) {
	this->database.log(this->module, "[#" + this->idString + "] " + entry);
}

// get value of last id (to be used by the thread only)
unsigned long Thread::getLast() const {
	return this->last;
}

// set last id (to be used by the thread only)
void Thread::setLast(unsigned long last) {
	// set last id internally
	this->last = last;

	// set last id in database
	this->database.setThreadLast(this->id, last);
}

// get a copy of the current status message
std::string Thread::getStatusMessage() {
	std::lock_guard<std::mutex> statusLocked(this->statusLock);
	return this->status;
}

// update run time of thread (and save it to database)
void Thread::updateRunTime() {
	if(this->startTimePoint > std::chrono::steady_clock::time_point::min()) {
		// add run time
		this->runTime += std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->startTimePoint);

		// reset start time point
		this->startTimePoint = std::chrono::steady_clock::time_point::min();

		// save new run time to database
		this->database.setThreadRunTime(this->id, this->runTime.count());
	}
}

// uüdate pause time of thread and save it to database
void Thread::updatePauseTime() {
	// check whether pause time was running
	if(this->pauseTimePoint > std::chrono::steady_clock::time_point::min()) {
		// add pause time
		this->pauseTime += std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->pauseTimePoint);

		// reset pause time point
		this->pauseTimePoint = std::chrono::steady_clock::time_point::min();

		// save new pause time in database
		this->database.setThreadPauseTime(this->id, this->pauseTime.count());
	}
}

// function for checking whether to unpause the thread (multi-threading safe)
bool Thread::isUnpaused() const {
	return !(this->paused);
}

// main function of the thread
void Thread::main() {
	// connect to database and prepare logging
	if(this->database.connect() && this->database.prepare()) {
#ifndef CRAWLSERV_DEBUG_THREAD_NOCATCH
		try
#endif
		{
			// get previous run and pause times (in seconds)
			this->runTime = std::chrono::seconds(this->database.getThreadRunTime(this->id));
			this->pauseTime = std::chrono::seconds(this->database.getThreadPauseTime(this->id));

			// notify thread for initialization
			if(this->onInit(this->resumed)) {
				// save new start time point
				this->startTimePoint = std::chrono::steady_clock::now();

				// run thread
				while(this->running) {
					// run a tick if thread is not paused
					if(this->paused) {
						// update run time and save new pause time point
						this->updateRunTime();
						this->pauseTimePoint = std::chrono::steady_clock::now();

						// notify thread for pausing
						this->onPause();

						// wait for unpausing
						std::unique_lock<std::mutex> pause(this->pauseLock);
						this->pauseCondition.wait(pause, std::bind(&Thread::isUnpaused, this));

						// notify thread for unpausing
						if(this->running) this->onUnpause();

						// update pause time and save new start time point
						this->updatePauseTime();
						this->startTimePoint = std::chrono::steady_clock::now();
					}
					// run thread tick
					else if(!this->onTick()) this->running = false;
				}
			}

			// update run time
			this->updateRunTime();

			// notify thread for clearing
			this->onClear(this->interrupted);

			// update status
			if(this->interrupted) this->setStatusMessage("INTERRUPTED " + this->status);
			else {
				// log timing statistic
				std::string logStr = "Stopped after " + DateTime::secondsToString(this->runTime.count()) + " running";
				if(this->pauseTime.count()) logStr += " and " + DateTime::secondsToString(this->pauseTime.count()) + " pausing";
				logStr += ".";
				this->log(logStr);
			}
		}
#ifndef CRAWLSERV_DEBUG_THREAD_NOCATCH
		// handle exceptions by thread
		catch(const std::exception& e) {
			try {
				// release table locks
				this->database.releaseLocks();

				// log error
				std::ostringstream logStrStr;
				logStrStr << "Failed - " << std::string(e.what()) << ".";
				this->log(logStrStr.str());

				// update run or pause time if necessary
				this->updateRunTime();
				this->updatePauseTime();
			}
			catch(const std::exception& e2) {
				// log exceptions -> send to stdout
				std::cout << std::endl << "Failed - " << e.what() << ".";
				std::cout << std::endl << "Failed to write to log - " << e2.what() << ".";
			}

			this->terminated = true;
		}
#endif
	}
	else throw std::runtime_error(this->database.getErrorMessage());
}
