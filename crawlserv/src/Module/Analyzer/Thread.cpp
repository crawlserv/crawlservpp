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
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	)		:	Module::Thread(
						dbBase,
						threadOptions,
						threadStatus
				),
				database(this->Module::Thread::database) {}

	// constructor B: start a new analyzer
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	)		:	Module::Thread(
						dbBase,
						threadOptions
				),
				database(this->Module::Thread::database) {}

	// destructor stub
	Thread::~Thread() {}

	// initialize parser
	void Thread::onInit() {
		std::queue<std::string> configWarnings;

		// load configuration
		this->setStatusMessage("Loading configuration...");

		this->loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);

		// show warnings if necessary
		while(!configWarnings.empty()) {
			this->log(Config::generalLoggingDefault, "WARNING: " + configWarnings.front());

			configWarnings.pop();
		}

		// set database configuration
		this->setStatusMessage("Setting database configuration...");

		this->database.setLogging(
				this->config.generalLogging,
				Config::generalLoggingDefault,
				Config::generalLoggingVerbose
		);

		this->log(Config::generalLoggingVerbose, "sets database configuration...");

		this->database.setTargetTable(this->config.generalResultTable);
		this->database.setSleepOnError(this->config.generalSleepMySql);
		this->database.setTimeoutTargetLock(this->config.generalTimeoutTargetLock);

		// prepare SQL statements for analyzer
		this->setStatusMessage("Preparing SQL statements...");

		this->log(Config::generalLoggingVerbose, "prepares SQL statements...");

		this->database.prepare();

		// initialize algorithm
		this->setStatusMessage("Initializing algorithm...");

		this->log(Config::generalLoggingVerbose, "initializes algorithm...");

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
		this->sleep(this->config.generalSleepWhenFinished);
	}

	// shadow pause function not to be used by thread
	void Thread::pause() {
		this->pauseByThread();
	}

	// hide functions not to be used by thread
	void Thread::start() {
		throw std::logic_error("Thread::start() not to be used by thread itself");
	}

	void Thread::unpause() {
		throw std::logic_error("Thread::unpause() not to be used by thread itself");
	}

	void Thread::stop() {
		throw std::logic_error("Thread::stop() not to be used by thread itself");
	}

	void Thread::interrupt() {
		throw std::logic_error("Thread::interrupt() not to be used by thread itself");
	}

} /* crawlservpp::Module::Analyzer */
