/*
 * Thread.cpp
 *
 * Abstract implementation of the Thread interface for analyzer threads to be inherited by the algorithm classes.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.hpp"

namespace crawlservpp::Module::Analyzer {

	// constructor A: run previously interrupted analyzer
	Thread::Thread(
			Main::Database& dbBase,
			unsigned long analyzerId,
			const std::string& analyzerStatus,
			bool analyzerPaused,
			const ThreadOptions& threadOptions,
			unsigned long analyzerLast)
				: Module::Thread(dbBase, analyzerId, "analyzer", analyzerStatus, analyzerPaused, threadOptions, analyzerLast),
				  database(this->Module::Thread::database) {}

	// constructor B: start a new analyzer
	Thread::Thread(Main::Database& dbBase, const ThreadOptions& threadOptions)
				: Module::Thread(dbBase, "analyzer", threadOptions),
				  database(this->Module::Thread::database) {}

	// destructor stub
	Thread::~Thread() {}

	// initialize parser, throws std::runtime_error
	void Thread::onInit() {
		std::queue<std::string> configWarnings;

		// set ID, website and URL list
		this->database.setId(this->getId());
		this->database.setWebsite(this->getWebsite());
		this->database.setUrlList(this->getUrlList());
		this->database.setNamespaces(this->websiteNamespace, this->urlListNamespace);

		// load configuration
		this->loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);

		// show warnings if necessary
		if(this->config.generalLogging) {
			while(!configWarnings.empty()) {
				this->log("WARNING: " + configWarnings.front());
				configWarnings.pop();
			}
		}

		// set database configuration
		this->setStatusMessage("Setting database configuration...");
		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("sets database configuration...");
		this->database.setTargetTable(this->config.generalResultTable);
		this->database.setLogging(this->config.generalLogging);
		this->database.setVerbose(config.generalLogging == Config::generalLoggingVerbose);
		this->database.setSleepOnError(this->config.generalSleepMySql);
		this->database.setTimeoutTargetLock(this->config.generalTimeoutTargetLock);

		// prepare SQL queries
		this->setStatusMessage("Preparing SQL statements...");
		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("prepares SQL statements...");
		this->database.prepare();

		// initialize algorithm
		this->setStatusMessage("Initializing algorithm...");
		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("initializes algorithm...");
		this->onAlgoInit();

		this->setStatusMessage("Starting algorithm...");
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
	void Thread::onClear() {
		// clear algorithm
		this->onAlgoClear();
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

} /* crawlservpp::Module::Analyzer */
