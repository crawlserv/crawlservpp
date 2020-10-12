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
 * Abstract implementation of the Thread interface
 *  for analyzer threads to be inherited by the algorithm classes.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.hpp"

namespace crawlservpp::Module::Analyzer {

	/*
	 * CONSTRUCTION
	 */

	//! Constructor initializing a previously interrupted analyzer thread.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
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

	//! Constructor initializing a new analyzer thread.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&)
	 */
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	)		:	Module::Thread(
						dbBase,
						threadOptions
				),
				database(this->Module::Thread::database) {}

	/*
	 * IMPLEMENTED THREAD FUNCTIONS (protected)
	 */

	//! Initializes the analyzer, and the algorithm
	/*!
	 * \sa onAlgoInit
	 */
	void Thread::onInit() {
		std::queue<std::string> configWarnings;

		// load configuration
		this->setStatusMessage("Loading configuration...");

		this->loadConfig(
				this->database.getConfiguration(
						this->getConfig()
				),
				configWarnings
		);

		// show warnings if necessary
		while(!configWarnings.empty()) {
			this->log(
					generalLoggingDefault,
					"WARNING: "
					+ configWarnings.front()
			);

			configWarnings.pop();
		}

		// set database configuration
		this->setStatusMessage("Setting database configuration...");

		this->database.setLogging(
				this->config.generalLogging,
				generalLoggingDefault,
				generalLoggingVerbose
		);

		this->log(generalLoggingVerbose, "sets database configuration...");

		this->database.setTargetTable(this->config.generalTargetTable);
		this->database.setSleepOnError(this->config.generalSleepMySql);
		this->database.setCorpusSlicing(this->config.generalCorpusSlicing);
		this->database.setIsRunningCallback([this]() {
					return this->isRunning();
		});

		// prepare SQL statements for analyzer
		this->setStatusMessage("Preparing SQL statements...");

		this->log(generalLoggingVerbose, "prepares SQL statements...");

		this->database.prepare();

		// initialize algorithm
		this->setStatusMessage("Initializing algorithm...");

		this->log(generalLoggingVerbose, "initializes algorithm...");

		this->onAlgoInit();

		this->setStatusMessage("Starting algorithm...");
	}

	//! Performs an algorithm tick.
	/*!
	 * \sa onAlgoTick
	 */
	void Thread::onTick() {
		// algorithm tick
		this->onAlgoTick();
	}

	//! Pauses the analyzer.
	/*!
	 * \sa onAlgoPause
	 */
	void Thread::onPause() {
		// pause algorithm
		this->onAlgoPause();
	}

	//! Unpauses the analyzer.
	/*!
	 * \sa onAlgoUnpause
	 */
	void Thread::onUnpause() {
		// unpause algorithm
		this->onAlgoUnpause();
	}

	//! Clears the algorithm.
	/*!
	 * \sa onAlgoClear()
	 */
	void Thread::onClear() {
		// clear algorithm
		this->onAlgoClear();
	}

	/*
	 * THREAD CONTROL FOR ALGORITHMS (protected)
	 */

	//! Sets the status of the analyzer to finished, and sleeps.
	/*!
	 * Call this function when the algorithm has
	 *  finished.
	 */
	void Thread::finished() {
		// set status
		this->setStatusMessage("IDLE Finished.");

		// sleep
		this->sleep(this->config.generalSleepWhenFinished);
	}

	//! Pauses the thread.
	/*!
	 * Shadows Module::Thread::pause(), which should
	 *  not be used by the thread.
	 *
	 * \sa pauseByThread
	 */
	void Thread::pause() {
		this->pauseByThread();
	}

	/*
	 * HELPER FUNCTION FOR ALGORITHMS (protected)
	 */
	std::string Thread::getTargetTableName() const {
		std::string name{"crawlserv_"};

		name += this->websiteNamespace;
		name += '_';
		name += this->urlListNamespace;
		name += "_analyzed_";
		name += this->config.generalTargetTable;

		return name;
	}

	/*
	 *  shadowing functions not to be used by thread (private)
	 */

	// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
	void Thread::start() {
		throw std::logic_error(
				"Thread::start() not to be used by thread itself"
		);
	}

	// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
	void Thread::unpause() {
		throw std::logic_error(
				"Thread::unpause() not to be used by thread itself"
		);
	}

	// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
	void Thread::stop() {
		throw std::logic_error(
				"Thread::stop() not to be used by thread itself"
		);
	}

	// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
	void Thread::interrupt() {
		throw std::logic_error(
				"Thread::interrupt() not to be used by thread itself"
		);
	}

} /* namespace crawlservpp::Module::Analyzer */
