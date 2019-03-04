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

namespace crawlservpp::Module {

// constructor A: run previously interrupted thread
Thread::Thread(crawlservpp::Main::Database& dbBase, unsigned long threadId, const std::string& threadModule,
		const std::string& threadStatus, bool threadPaused, const crawlservpp::Struct::ThreadOptions& threadOptions,
		unsigned long threadLast) : database(dbBase.getSettings(), threadModule), databaseClass(dbBase),
									pausable(true), running(true), paused(threadPaused), interrupted(false),
									resumed(true), terminated(false), module(threadModule), options(threadOptions),
									last(threadLast), startTimePoint(std::chrono::steady_clock::time_point::min()),
									pauseTimePoint(std::chrono::steady_clock::time_point::min()),
									runTime(std::chrono::duration<unsigned long>::zero()),
									pauseTime(std::chrono::duration<unsigned long>::zero()) {

	if(threadStatus.length() >= 12 && threadStatus.substr(0, 12) == "INTERRUPTED ") this->status = threadStatus.substr(12);
	else if(threadStatus.length() >= 7 && threadStatus.substr(0, 7) == "PAUSED ") this->status = threadStatus.substr(7);

	if(threadId) {
		// set ID
		this->id = threadId;

		// create ID string
		std::ostringstream idStrStr;
		idStrStr << threadId;
		this->idString = idStrStr.str();
	}

	// get namespace of website, URL list and configuration
	this->websiteNamespace = this->databaseClass.getWebsiteNamespace(this->getWebsite());
	this->urlListNamespace = this->databaseClass.getUrlListNamespace(this->getUrlList());
	this->configuration = this->databaseClass.getConfiguration(this->getConfig());

	// update thread status in database (remove "INTERRUPTED ", add "PAUSED " before status if necessary)
	if(threadId) this->databaseClass.setThreadStatus(threadId, threadPaused, this->status);
}

// constructor B: start new thread (using constructor A to initialize values)
Thread::Thread(crawlservpp::Main::Database& dbBase, const std::string& threadModule,
		const crawlservpp::Struct::ThreadOptions& threadOptions) : Thread(dbBase, 0, threadModule, "", false, threadOptions, 0) {
	// add thread to database and save ID
	this->id = this->databaseClass.addThread(threadModule, threadOptions);
	std::ostringstream idStrStr;
	idStrStr << this->id;
	this->idString = idStrStr.str();
}

// destructor stub
Thread::~Thread() {}

// get ID of the thread (thread-safe)
unsigned long Thread::getId() const {
	return this->id;
}

// get ID of the website (thread-safe)
unsigned long Thread::getWebsite() const {
	return this->options.website;
}

// get ID of URL list (thread-safe)
unsigned long Thread::getUrlList() const {
	return this->options.urlList;
}

// get ID of the configuration (thread-safe)
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

// start the thread (may not be used by the thread itself!)
void Thread::start() {
	// run thread
	this->thread = std::thread(&Thread::main, this);
}

// pause the thread (may not be used by the thread itself!)
bool Thread::pause() {
	// ignore if thread is paused
	if(this->paused) return true;

	// check whether thread is pausable
	if(!(this->pausable)) return false;

	// set internal pause state
	this->paused = true;

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
	if(this->running) {
		this->running = false;

		// check whether thread has to be unpaused
		if(this->paused) {
			// set internal pause state
			std::lock_guard<std::mutex> unpause(this->pauseLock);
			this->paused = false;

			// update condition variable
			this->pauseCondition.notify_one();
		}

		// wait for thread if necessary
		if(this->thread.joinable()) this->thread.join();
	}

	// remove thread from database
	this->databaseClass.deleteThread(this->id);
}

// interrupt the thread for shutdown (may not be used by the thread itself!)
// NOTE:	Module::Thread::finishInterrupt() has to be called afterwards to wait for the thread!
//			This enables the interruption of all threads simultaneously before waiting for their conclusion
void Thread::sendInterrupt() {
	// check whether thread exists and is running
	if(this->running) {
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
// NOTE:	Module::Thread::sendInterrupt() has to be called beforehand to interrupt the thread!
//			This enables the interruption of all threads simultaneously before waiting for their conclusion
void Thread::finishInterrupt() {
	// if thread exists and has been interrupted, wait for thread and join
	if(this->interrupted) this->thread.join();
}

// force the thread to pause (to be used by the thread only)
void Thread::pauseByThread() {
	// ignore if thread is paused
	if(this->paused) return;

	// set internal pause state
	this->paused = true;

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
	if(this->interrupted) this->database.setThreadStatus(this->id, statusMessage);
	else this->database.setThreadStatus(this->id, this->paused, statusMessage);
}

// set the progress of the thread (to be used by the thread only)
void Thread::setProgress(float progress) {
	// set progress in database
	this->database.setThreadProgress(this->id, progress);
}

// add a log entry for the thread to the database using the module of the thread (to be used by the thread only)
void Thread::log(const std::string& entry) {
	this->database.log("[#" + this->idString + "] " + entry);
}

// allow the thread to be paused (enabled by default)
void Thread::allowPausing() {
	this->pausable = true;
}

// do not allow the thread to be paused
void Thread::disallowPausing() {
	this->pausable = false;
}

// get value of last ID (to be used by the thread only)
unsigned long Thread::getLast() const {
	return this->last;
}

// set last ID (to be used by the thread only)
void Thread::setLast(unsigned long last) {
	// set last ID internally
	this->last = last;

	// set last ID in database
	this->database.setThreadLast(this->id, last);
}

// increment last ID (to be used by the thread only)
void Thread::incrementLast() {
	// increment last ID internally
	(this->last)++;

	// increment last ID in database
	this->database.setThreadLast(this->id, this->last);
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

// update pause time of thread and save it to database
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

// initialize thread
void Thread::init() {
	// get previous run and pause times (in seconds)
	this->runTime = std::chrono::seconds(this->database.getThreadRunTime(this->id));
	this->pauseTime = std::chrono::seconds(this->database.getThreadPauseTime(this->id));

	// notify thread for initialization
	try {
		this->onInit(this->resumed);
	}
	// handle exceptions by trying to log and set status
	catch(const std::exception& e) {
		// release table locks
		this->database.releaseLocks();

		// log error
		std::ostringstream logStrStr;
		logStrStr << "failed - " << e.what() << ".";
		this->log(logStrStr.str());

		// try to set status
		this->setStatusMessage("ERROR " + std::string(e.what()));

		// interrupt thread
		this->interrupted = true;
	}

	// save new start time point
	this->startTimePoint = std::chrono::steady_clock::now();
}

// run thread tick
void Thread::tick() {
	try {
		this->onTick();
	}
	// handle connection exception by sleeping
	catch(const crawlservpp::Main::Database::ConnectionException& e) {
		std::cout << std::endl << e.what() << " - sleeps for " << MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SECONDS << "s" << std::flush;
		std::this_thread::sleep_for(std::chrono::seconds(MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SECONDS));
	}
	// handle other exceptions by trying to log, set status and pause thread
	catch(const std::exception& e) {
		// release table locks
		this->database.releaseLocks();

		// log error
		std::ostringstream logStrStr;
		logStrStr << "failed - " << e.what() << "." << std::flush;
		this->log(logStrStr.str());

		// try to set status
		this->setStatusMessage("ERROR " + std::string(e.what()));

		// try to pause thread
		this->pauseByThread();
	}
	catch(...) {
		// release table locks
		this->database.releaseLocks();

		// log error
		std::ostringstream logStrStr;
		logStrStr << "failed - Unknown exception" << std::flush;
		this->log(logStrStr.str());

		// try to set status
		this->setStatusMessage("ERROR Unknown exception");

		// try to pause thread
		this->pauseByThread();
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
		if(this->running) this->onUnpause();

		// update pause time and save new start time point
		this->updatePauseTime();
		this->startTimePoint = std::chrono::steady_clock::now();
	}
	// handle connection exception by sleeping
	catch(const crawlservpp::Main::Database::ConnectionException& e) {
		std::this_thread::sleep_for(std::chrono::seconds(MODULE_THREAD_SLEEP_ON_CONNECTION_ERROR_SECONDS));
	}
}

// clear thread
void Thread::clear() {
	// update run time
	this->updateRunTime();

	// notify thread for clearing
	this->onClear(this->interrupted);

	// update status
	if(this->interrupted) this->setStatusMessage("INTERRUPTED " + this->status);
	else {
		// log timing statistic
		std::string logStr = "stopped after "
				+ crawlservpp::Helper::DateTime::secondsToString(this->runTime.count()) + " running";
		if(this->pauseTime.count())
			logStr += " and " + crawlservpp::Helper::DateTime::secondsToString(this->pauseTime.count()) + " pausing";
		logStr += ".";
		this->log(logStr);
	}
}

// function for checking whether to unpause the thread (multi-threading safe)
bool Thread::isUnpaused() const {
	return !(this->paused);
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
		while(this->running && !(this->interrupted)) {
			// check pause state
			if(this->paused) {
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
			// release table locks
			this->database.releaseLocks();

			// log error
			std::ostringstream logStrStr;
			logStrStr << "failed - " << std::string(e.what()) << ".";
			this->log(logStrStr.str());

			// update run or pause time if necessary
			this->updateRunTime();
			this->updatePauseTime();
		}
		catch(const std::exception& e2) {
			// log exceptions -> send to stdout
			std::cout << std::endl << "> Thread terminated - " << e.what() << "." << std::flush;
			std::cout << std::endl << "> Thread could not write to log - " << e2.what() << "." << std::flush;
		}

		this->terminated = true;
	}
#endif
}

}
