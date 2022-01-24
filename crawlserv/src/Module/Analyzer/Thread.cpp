/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
		// has algorithm been finished?
		if(this->idleStart > std::chrono::time_point<std::chrono::steady_clock>{}) {
			// restart algorithm?
			if(
					this->config.generalRestartAfter >= 0
					&& std::chrono::duration_cast<std::chrono::seconds>(
								std::chrono::steady_clock::now()
								- this->idleStart
					).count() >= this->config.generalRestartAfter
			) {
				this->idleStart = std::chrono::time_point<std::chrono::steady_clock>{};

				this->onReset();
			}
			else {
				this->sleep(this->config.generalSleepWhenFinished);
			}
		}

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

		// clean up corpora and queries
		this->cleanUpCorpora();
		this->cleanUpQueries();
	}

	//! Resets the algorithm.
	void Thread::onReset() {
		this->onClear();

		this->resetAlgo();
		this->resetBase();

		this->log(generalLoggingDefault, "reset.");

		this->onInit();
	}

	/*
	 * QUERY FUNCTIONS (protected)
	 */

	//! Does nothing.
	/*!
	 * To be overwritten by algorithms
	 *  that use their own queries.
	 */
	void Thread::initQueries() {}

	//! Does nothing.
	/*!
	 * To be overwritten by algorithms
	 *   that use their own queries.
	 */
	void Thread::deleteQueries() {}

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
		// clear corpora and queries (if necessary)
		this->cleanUpCorpora();
		this->cleanUpQueries();

		// set status and progress
		this->setStatusMessage("IDLE Finished.");
		this->setProgress(1.F);

		this->log(generalLoggingDefault, "is done.");

		this->idleStart = std::chrono::steady_clock::now();
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

	//! Gets the contents of all corpora, filters and combines them if necessary.
	/*!
	 * \param isCombine Specifies whether to combine
	 *   multiple corpora, if applicable.
	 * \param statusSetter Reference to a
	 *   Struct::StatusSetter to be used for
	 *   updating the status while adding the
	 *   corpus.
	 *
	 * \returns True, if any non-empty corpus
	 *   has been added.
	 */
	bool Thread::addCorpora(bool isCombine, StatusSetter& statusSetter) {
		// get corpora
		for(std::size_t index{}; index < this->config.generalInputSources.size(); ++index) {
			this->addCorpus(index, statusSetter);
		}

		// combine corpora, if necessary
		if(isCombine && this->corpora.size() > 1) {
			this->log(generalLoggingDefault, "combines corpora...");

			this->combineCorpora(statusSetter);
		}

		// filter corpora by query, if necessary
		for(std::size_t index{}; index < this->corpora.size(); ++index) {
			this->filterCorpusByQuery(index, statusSetter);
		}

		// remove empty corpora
		this->corpora.erase(
				std::remove_if(
						this->corpora.begin(),
						this->corpora.end(),
						[](const auto& corpus) {
							return corpus.empty();
						}
				),
				this->corpora.end()
		);

		// return whether any corpus remains
		return !(this->corpora.empty());
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
	 * CLEANUP FUNCTIONS (protected)
	 */

	//! Clean up all corpora and free their memory.
	void Thread::cleanUpCorpora() {
		Helper::Memory::free(this->corpora);
	}

	//! Clean up all queries and free their memory.
	void Thread::cleanUpQueries() {
		Helper::Memory::free(this->queryFilterQueries);

		this->deleteQueries();
		this->clearQueries();
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
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// gets the contents of the specified corpus
	void Thread::addCorpus(std::size_t index, StatusSetter& statusSetter) {
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

		if(
				this->database.getCorpus(
						CorpusProperties(
								this->config.generalInputSources.at(index),
								this->config.generalInputTables.at(index),
								this->config.generalInputFields.at(index),
								this->config.tokenizerManipulators,
								this->config.tokenizerModels,
								this->config.tokenizerDicts,
								this->config.tokenizerLanguages,
								this->config.tokenizerSavePoints,
								this->config.tokenizerFreeMemoryEvery
						),
						this->config.filterDateEnable ? this->config.filterDateFrom : std::string{},
						this->config.filterDateEnable ? this->config.filterDateTo : std::string{},
						this->corpora.back(),
						corpusSources,
						statusSetter
				) && !(this->corpora.back().empty())
		) {
			// log corpus statistics
			std::ostringstream logStrStr;

			logStrStr.imbue(std::locale(""));

			logStrStr << "corpus";

			if(this->config.generalInputSources.size() > 1) {
				logStrStr	<< " #"
							<< index + 1
							<< "/"
							<< this->config.generalInputSources.size();
			}

			logStrStr << ": " << this->corpora.back().getNumTokens() << " tokens";

			logStrStr << " from " << corpusSources << " source(s).";

			this->log(generalLoggingDefault, logStrStr.str());
		}
		else {
			this->corpora.pop_back();
		}
	}

	// combines all added corpora into one, concatenating articles with the same ID.
	void Thread::combineCorpora(StatusSetter& statusSetter) {
		std::vector<Corpus> combined;

		combined.emplace_back(this->corpora, this->config.generalCorpusChecks, statusSetter);

		if(statusSetter.isRunning()) {
			std::swap(this->corpora, combined);
		}
	}

	// filter the corpus using the queries provided in the configuration
	void Thread::filterCorpusByQuery(std::size_t index, StatusSetter& statusSetter) {
		std::queue<std::string> warnings;

		if(this->queryFilterQueries.empty()) {
			return;
		}

		// start timer
		Timer::Simple timer;

		// filter by query
		std::size_t removed{
			this->corpora.at(index).filterArticles(
					[this, &warnings](
							const auto& tokens,
							auto articlePos,
							auto articleEnd
					) {
						auto lambda = [this, &tokens, articlePos, articleEnd, &warnings](const auto& query) {
							for(std::size_t index{articlePos}; index < articleEnd; ++index) {
								this->setQueryTarget(tokens.at(index), "");

								bool result{false};

								if(this->getBoolFromQuery(query, result, warnings)) {
									if(result) {
										return true;
									}
								}
							}

							return false;
						};

						if(this->config.filterQueryAll) {
							return std::all_of(
									this->queryFilterQueries.begin(),
									this->queryFilterQueries.end(),
									lambda
							);
						}

						return std::any_of(
								this->queryFilterQueries.begin(),
								this->queryFilterQueries.end(),
								lambda
						);
					},
					statusSetter
			)
		};

		while(!warnings.empty()) {
			this->log(generalLoggingDefault, warnings.front());

			warnings.pop();
		}

		if(removed > 0) {
			// log new corpus size
			std::ostringstream logStrStr;

			logStrStr.imbue(std::locale(""));

			logStrStr << "filtered corpus";

			if(this->corpora.size() > 1) {
				logStrStr << " #" << index;
			}

			logStrStr	<< " (by query) to "
						<< this->corpora[index].size()
						<< " bytes (removed ";

			if(removed == 1) {
				logStrStr << " one article";
			}
			else {
				logStrStr << removed << " articles";
			}

			logStrStr << ") in " << timer.tickStr() << ".";

			this->log(generalLoggingDefault, logStrStr.str());
		}
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
