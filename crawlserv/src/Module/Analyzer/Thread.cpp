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

	//! Initializes the analyzer, the target table, and the algorithm.
	/*!
	 * \sa onAlgoInit, onAlgoInitTarget
	 */
	void Thread::onInit() {
		std::queue<std::string> configWarnings;

		this->setUpConfig(configWarnings);
		this->setUpLogging();

		this->logWarnings(configWarnings);

		this->setUpDatabase();
		this->setUpTarget();
		this->setUpSqlStatements();
		this->setUpQueries();

		if(!(this->isRunning())) {
			return; // cancel if not running anymore
		}

		this->setUpAlgorithm();
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

		// clear queries
		this->clearQueries();
	}

	//! Resets the algorithm.
	void Thread::onReset() {
		this->onClear();

		this->resetAlgo();
		this->resetBase();

		this->onInit();
	}

	/*
	 * QUERY INITIALIZATION (protected)
	 */

	//! Does nothing.
	/*!
	 * To be overwritten by algorithms
	 *  that use their own queries.
	 */
	void Thread::initQueries() {}

	//! Adds an optional query.
	/*!
	 * Does nothing, if the query ID is
	 *  zero.
	 *
	 * \param queryId ID of the query, as
	 *   specified in the configuration.
	 * \param propertiesTo Reference to a
	 *   structure to which the properties
	 *   of the query will be written.
	 *
	 * \sa Container::addQuery
	 */
	void Thread::addOptionalQuery(std::uint64_t queryId, QueryStruct& propertiesTo) {
		if(queryId > 0) {
			QueryProperties properties;

			this->database.getQueryProperties(queryId, properties);

			propertiesTo = this->addQuery(queryId, properties);
		}
	}

	//! Adds multiple queries at once, ignoring empty ones.
	/*!
	 * Ignores query IDs that are zero.
	 *
	 * \param queryIds IDs of the queries,
	 *   as specified in the configuration.
	 * \param propertiesTo Reference to a
	 *   vector to which the properties
	 *   of the queries will be written.
	 *
	 * \sa Container::addQuery
	 */
	void Thread::addQueries(
			const std::vector<std::uint64_t>& queryIds,
			std::vector<QueryStruct>& propertiesTo
	) {
		// reserve memory first
		propertiesTo.reserve(queryIds.size());

		for(const auto& queryId : queryIds) {
			if(queryId > 0) {
				QueryProperties properties;

				this->database.getQueryProperties(queryId, properties);

				propertiesTo.emplace_back(this->addQuery(queryId, properties));
			}
		}
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
		// set status and progress
		this->setStatusMessage("IDLE Finished.");
		this->setProgress(1.F);

		this->log(generalLoggingDefault, "is done.");

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
	 * HELPER FUNCTIONS FOR ALGORITHMS (protected)
	 */

	//! Gets the full name of the target table.
	/*!
	 * \returns A new string containing the full
	 *   name of the target table.
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

	//! Gets the contents of the specified corpus.
	/*!
	 * If the corpus is filtered by queries
	 *  provided in the configuration, tokens
	 *  of articles in which no single token
	 *  fulfills any of the queries will be
	 *  emptied, but not deleted. Make sure to
	 *  re-tokenize the corpus if all empty
	 *  tokens need to be removed.
	 *
	 * \param index Index of the corpus to add.
	 *   Needs to be a valid index of
	 *   Module::Analyzer::Config::generalInputSources.
	 * \param statusSetter Reference to a
	 *   Struct::StatusSetter to be used for
	 *   updating the status while adding the
	 *   corpus.
	 *
	 * \returns True, if the corpus has been
	 *   added, which requires that it had not
	 *   been empty after filtering.
	 *   False otherwise.
	 *
	 * \sa
	 *   Module::Analyzer::Config::filterQueryQueries
	 */
	bool Thread::addCorpus(
			std::size_t index,
			StatusSetter& statusSetter
	) {
		std::size_t corpusSources{};

		this->corpora.emplace_back(this->config.generalCorpusChecks);

		std::string statusStr;

		if(this->config.generalInputSources.size() > 1) {
			std::ostringstream corpusStatusStrStr;

			corpusStatusStrStr.imbue(std::locale(""));

			corpusStatusStrStr << "Getting text corpus #";
			corpusStatusStrStr << index + 1;
			corpusStatusStrStr << "/";
			corpusStatusStrStr << this->config.generalInputSources.size();
			corpusStatusStrStr << "...";

			statusStr = corpusStatusStrStr.str();
		}
		else {
			statusStr = "Getting text corpus...";
		}

		statusSetter.change(statusStr);

		if(!(this->database.getCorpus(
				CorpusProperties(
						this->config.generalInputSources.at(index),
						this->config.generalInputTables.at(index),
						this->config.generalInputFields.at(index),
						this->config.tokenizerSentenceManipulators,
						this->config.tokenizerSentenceModels,
						this->config.tokenizerWordManipulators,
						this->config.tokenizerWordModels,
						this->config.tokenizerSavePoints,
						this->config.tokenizerFreeMemoryEvery
				),
				this->config.filterDateEnable ? this->config.filterDateFrom : std::string{},
				this->config.filterDateEnable ? this->config.filterDateTo : std::string{},
				this->corpora.back(),
				corpusSources,
				statusSetter
		))) {
			this->corpora.pop_back();

			return false;
		}

		if(!(this->isRunning())) {
			return false;
		}

		if(this->corpora.back().empty()) {
			this->corpora.pop_back();

			return false;
		}

		// filter corpus by query
		corpusSources -= this->filterCorpusByQuery(statusSetter);

		// log corpus statistics
		std::ostringstream logStrStr;

		logStrStr.imbue(std::locale(""));

		logStrStr << "corpus #" << index + 1 << "/" << this->config.generalInputSources.size();
		logStrStr << ": " << this->corpora.back().getNumTokens() << " tokens";

		logStrStr << " from " << corpusSources << " source(s).";

		this->log(generalLoggingDefault, logStrStr.str());

		return true;
	}

	//! Checks the specified sources for creating the corpus.
	/*!
	 * \param statusSetter Reference to a
	 *   Struct::StatusSetter to be used for
	 *   updating the status before checking
	 *   the sources.
	 */
	void Thread::checkCorpusSources(StatusSetter& statusSetter) {
		statusSetter.change("Checking sources...");

		this->database.checkSources(
				this->config.generalInputSources,
				this->config.generalInputTables,
				this->config.generalInputFields
		);
	}

	/*
	 * INITIALIZATION FUNCTIONS (private)
	 */

	// load configuration
	void Thread::setUpConfig(std::queue<std::string>& warningsTo) {
		this->setStatusMessage("Loading configuration...");

		this->loadConfig(
				this->database.getConfiguration(
						this->getConfig()
				),
				warningsTo
		);
	}

	// set up logging
	void Thread::setUpLogging() {
		this->database.setLogging(
				this->config.generalLogging,
				generalLoggingDefault,
				generalLoggingVerbose
		);
	}

	// set database configuration
	void Thread::setUpDatabase() {
		this->setStatusMessage("Setting database configuration...");

		this->log(generalLoggingVerbose, "sets database configuration...");

		this->database.setTargetTable(this->config.generalTargetTable);
		this->database.setSleepOnError(this->config.generalSleepMySql);
		this->database.setCorpusSlicing(this->config.generalCorpusSlicing);
		this->database.setIsRunningCallback([this]() {
			return this->isRunning();
		});
	}

	// initialize target table
	void Thread::setUpTarget() {
		this->setStatusMessage("Initializing target table...");

		this->log(generalLoggingVerbose, "initializes target table...");

		this->onAlgoInitTarget();
	}

	// prepare SQL statements for analyzer
	void Thread::setUpSqlStatements() {
		this->setStatusMessage("Preparing SQL statements...");

		this->log(generalLoggingVerbose, "prepares SQL statements...");

		this->database.prepare();
	}

	// initialize queries
	void Thread::setUpQueries() {
		this->setStatusMessage("Initializing queries...");

		this->log(generalLoggingVerbose, "initializes queries...");

		this->addQueries(this->config.filterQueryQueries, this->queryFilterQueries);

		this->initQueries();
	}

	// initialize algorithm
	void Thread::setUpAlgorithm() {
		const std::string algoName{this->getName()};

		this->setStatusMessage("Initializing " + algoName + "...");

		this->log(generalLoggingDefault, "initializes " + algoName + "...");

		this->onAlgoInit();

		if(this->isRunning()) {
			this->log(generalLoggingDefault, "starts " + algoName + "...");

			this->setStatusMessage("Starting " + algoName + "...");
		}
		else {
			this->log(generalLoggingVerbose, "cancelled on startup.");

			this->setStatusMessage("Cancelled on startup.");
		}
	}

	// log warnings received by external function
	void Thread::logWarnings(std::queue<std::string>& warnings) {
		while(!warnings.empty()) {
			this->log(
					generalLoggingDefault,
					"WARNING: "
					+ warnings.front()
			);

			warnings.pop();
		}
	}

	/*
	 * INTERNAL HELPER FUNCTION (private)
	 */

	// filter the corpus using the queries provided in the configuration, return the number of removed articles
	std::size_t Thread::filterCorpusByQuery(StatusSetter& statusSetter) {
		std::queue<std::string> warnings;

		if(this->queryFilterQueries.empty()) {
			return 0;
		}

		// start timer
		Timer::Simple timer;

		// filter by query
		std::size_t removed{
			this->corpora.back().filterArticles(
					[this, &warnings](
							const auto& tokens,
							auto articlePos,
							auto articleLen
					) {
						return std::any_of(
								this->queryFilterQueries.begin(),
								this->queryFilterQueries.end(),
								[this, &tokens, articlePos, articleLen, &warnings](const auto& query) {
									for(std::size_t index{articlePos}; index < articlePos + articleLen; ++index) {
										this->setQueryTarget(tokens[index], "");

										bool result{false};

										if(this->getBoolFromQuery(query, result, warnings)) {
											if(result) {
												return true;
											}
										}
									}

									return false;
								}
						);
					},
					statusSetter
			)
		};

		while(!warnings.empty()) {
			this->log(generalLoggingDefault, warnings.front());

			warnings.pop();
		}

		if(!(this->isRunning())) {
			return 0;
		}

		if(removed > 0) {
			// log new corpus size
			std::ostringstream logStrStr;

			logStrStr.imbue(std::locale(""));

			logStrStr	<< "filtered corpus (by query) to "
						<< this->corpora.back().size()
						<< " bytes (removed ";

			switch(removed)  {
			case 1:
				logStrStr << " one article";

				break;

			default:
				logStrStr << removed << " articles";
			}

			logStrStr << ") in " << timer.tickStr() << ".";

			this->log(generalLoggingDefault, logStrStr.str());
		}

		return removed;
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
