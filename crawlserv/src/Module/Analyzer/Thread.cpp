/*
 * Thread.cpp
 *
 * Abstract implementation of the Thread interface for analyzer threads to be inherited by the algorithm classes.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.h"

namespace crawlservpp::Module::Analyzer {

// constructor A: run previously interrupted analyzer
Thread::Thread(crawlservpp::Main::Database& dbBase, unsigned long analyzerId,
		const std::string& analyzerStatus, bool analyzerPaused, const crawlservpp::Struct::ThreadOptions& threadOptions,
		unsigned long analyzerLast)
	: crawlservpp::Module::Thread(dbBase, analyzerId, "analyzer", analyzerStatus, analyzerPaused, threadOptions, analyzerLast),
	  	  database(this->crawlservpp::Module::Thread::database) {}

// constructor B: start a new analyzer
Thread::Thread(Main::Database& dbBase, const crawlservpp::Struct::ThreadOptions& threadOptions)
	: crawlservpp::Module::Thread(dbBase, "analyzer", threadOptions), database(this->crawlservpp::Module::Thread::database) {}

// destructor stub
Thread::~Thread() {}

// initialize parser, throws std::runtime_error
void Thread::onInit(bool resumed) {
	std::vector<std::string> configWarnings;
	std::vector<std::string> fields;

	// get configuration and show warnings if necessary
	this->config.loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);
	if(this->config.generalLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);

	// check configuration
	bool verbose = config.generalLogging == Config::generalLoggingVerbose;
	if(verbose) this->log("checks configuration...");
	if(this->config.generalResultTable.empty()) throw std::runtime_error("No target table specified.");

	// set database configuration
	if(verbose) this->log("sets database configuration...");
	this->database.setId(this->getId());
	this->database.setWebsite(this->getWebsite());
	this->database.setUrlList(this->getUrlList());
	this->database.setWebsiteNamespace(this->websiteNamespace);
	this->database.setUrlListNamespace(this->urlListNamespace);
	this->database.setTargetTable(this->config.generalResultTable);
	this->database.setLogging(this->config.generalLogging);
	this->database.setVerbose(verbose);
	this->database.setSleepOnError(this->config.generalSleepMySql);
	this->database.setTimeoutTargetLock(this->config.generalTimeoutTargetLock);

	// prepare SQL queries
	if(verbose) this->log("prepares SQL statements...");
	this->database.prepare();

	// initialize algorithm
	if(verbose) this->log("initializes algorithm...");
	this->onAlgoInit(resumed);
}

// analyzer tick
void Thread::onTick() {
	// algorithm tick
	this->onAlgoTick();
}

// analyzer paused
void Thread::onPause() {
	// pause algorithm
	this->onAlgoPause();
}

// analyzer unpaused
void Thread::onUnpause() {
	// unpause algorithm
	this->onAlgoUnpause();
}

// clear analyzer
void Thread::onClear(bool interrupted) {
	// clear algorithm
	this->onAlgoClear(interrupted);
}

// algorithm is finished
void Thread::finished() {
	// set status
	this->setStatusMessage("IDLE Finished.");

	// sleep
	std::this_thread::sleep_for(std::chrono::milliseconds(this->config.generalSleepWhenFinished));
}

// shadow pause function not to be used by thread
void Thread::pause() {
	this->pauseByThread();
}

// hide functions not to be used by thread
void Thread::start() {
	throw(std::logic_error("Thread::start() not to be used by thread itself"));
}
void Thread::unpause() {
	throw(std::logic_error("Thread::unpause() not to be used by thread itself"));
}
void Thread::stop() {
	throw(std::logic_error("Thread::stop() not to be used by thread itself"));
}
void Thread::interrupt() {
	throw(std::logic_error("Thread::interrupt() not to be used by thread itself"));
}

}
