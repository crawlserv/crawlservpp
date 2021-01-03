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
 * Implementation of the Thread interface for extractor threads.
 *
 *  Created on: May 9, 2019
 *      Author: ans
 */

#include "Thread.hpp"

#define DBG_BEG	try {
#define DBG_END(x) } catch(const std::out_of_range& e) { throw Exception(std::string(e.what()) + " in " + #x); }

namespace crawlservpp::Module::Extractor {

	/*
	 * CONSTRUCTION
	 */

	//! Constructor initializing a previously interrupted extractor thread.
	/*!
	 * \param dbBase Reference to the main
	 *   database connection.
	 * \param cookieDirectory View of a string
	 *   containing the (sub-)directory for
	 *   storing cookie files.
	 * \param threadOptions Constant reference
	 *   to a structure containing the options
	 *   for the thread.
	 * \param networkSettings Network settings.
	 * \param threadStatus Constant reference
	 *   to a structure containing the last
	 *   known status of the thread.
	 */
	Thread::Thread(
			Main::Database& dbBase,
			std::string_view cookieDirectory,
			const ThreadOptions& threadOptions,
			const NetworkSettings& networkSettings,
			const ThreadStatus& threadStatus
	)		:	Module::Thread(
						dbBase,
						threadOptions,
						threadStatus
				),
				database(this->Module::Thread::database),
				networking(cookieDirectory, networkSettings),
				torControl(
						networkSettings.torControlServer,
						networkSettings.torControlPort,
						networkSettings.torControlPassword
				) {}

	//! Constructor initializing a new extractor thread.
	/*!
	 * \param dbBase Reference to the main
	 *   database connection.
	 * \param cookieDirectory View of a string
	 *   containing the (sub-)directory for
	 *   storing cookie files.
	 * \param threadOptions Constant reference
	 *   to a structure containing the options
	 *   for the thread.
	 * \param networkSettings Network settings.
	 */
	Thread::Thread(
			Main::Database& dbBase,
			std::string_view cookieDirectory,
			const ThreadOptions& threadOptions,
			const NetworkSettings& networkSettings
	)		:	Module::Thread(dbBase, threadOptions),
				database(this->Module::Thread::database),
				networking(cookieDirectory, networkSettings),
				torControl(
						networkSettings.torControlServer,
						networkSettings.torControlPort,
						networkSettings.torControlPassword
				) {}

	/*
	 * IMPLEMENTED THREAD FUNCTIONS
	 */

	//! Initializes the extractor.
	/*!
	 * \throws Module::Extractor::Thread::Exception
	 *   if no query for dataset or ID extraction
	 *   has been specified.
	 */
	void Thread::onInit() {
		std::queue<std::string> configWarnings;

		this->setUpConfig(configWarnings);

		this->checkQueries();

		this->setUpLogging();

		this->logWarnings(configWarnings);

		this->setUpContainer();
		this->setUpDatabase();
		this->setUpSources();
		this->setUpTableNames();
		this->setUpTarget();
		this->setUpSqlStatements();
		this->setUpNetworking();
		this->setUpTor();
		this->setUpQueries();

		if(!(this->isRunning())) {
			return; // cancel if not running anymore
		}

		this->checkExtractingTable();

		this->setUpTimers();

		// extractor is ready
		this->log(generalLoggingExtended, "is ready.");
	}

	//! Performs an extractor tick.
	/*!
	 * If successful, this will extract data from
	 *  one URL. If not, the URL will either be
	 *  skipped, or retried in the next tick.
	 *
	 * \throws Module::Extractor::Thread::Exception
	 *   if the size of the URL list could not be
	 *   retrieved from the database.
	 */
	void Thread::onTick() {
		bool skip{false};

		// check whether a new TOR identity needs to be requested
		if(this->torControl.active()) {
			this->torControl.tick();
		}

		// check for jump in last ID ("time travel")
		const auto jump{this->getWarpedOverAndReset()};

		if(jump != 0) {
			// save cached results
			this->extractingSaveResults(true);

			// unlock and discard old URLs
			this->database.unLockUrlsIfOk(this->urls, this->cacheLockTime);

			// overwrite last URL ID
			this->lastUrl = this->getLast();

			// adjust tick counter
			this->tickCounter += jump;
		}

		// URL selection if no URLs are left to extract
		if(this->urls.empty()) {
			this->extractingUrlSelection();
		}

		if(this->urls.empty()) {
			// no URLs left in database: set timer if just finished and sleep
			if(this->idleTime == std::chrono::steady_clock::time_point::min()) {
				this->idleTime = std::chrono::steady_clock::now();
			}

			this->sleep(this->config.generalSleepIdle);

			return;
		}

		// check whether next URL(s) ought to be skipped
		this->extractingCheckUrls();

		// update timers if idling just stopped
		if(this->idleTime > std::chrono::steady_clock::time_point::min()) {
			// idling stopped
			this->startTime += std::chrono::steady_clock::now() - this->idleTime;

			this->pauseTime = std::chrono::steady_clock::time_point::min();
			this->idleTime = std::chrono::steady_clock::time_point::min();
		}

		// increase tick counter
		++(this->tickCounter);

		// check whether all URLs in the cache have been skipped
		if(this->urls.empty()) {
			return;
		}

		// write log entry if necessary
		this->log(
				generalLoggingExtended,
				"extracts data for "
				+ this->urls.front().second
				+ "..."
		);

		// try to renew URL lock
		this->lockTime = this->database.renewUrlLockIfOk(
				this->urls.front().first,
				this->cacheLockTime,
				this->config.generalLock
		);

		skip = this->lockTime.empty();

		if(skip) {
			// skip locked URL
			this->log(
					generalLoggingExtended,
					"skips (locked) "
					+ this->urls.front().second
			);
		}
		else {
			// set status
			this->setStatusMessage(this->urls.front().second);

			// approximate progress
			if(this->total == 0) {
				throw Exception(
						"Extractor::Thread::onTick():"
						" Could not retrieve the size of the URL list"
				);
			}

			if(this->idDist > 0) {
				// cache progress = (current ID - first ID) / (last ID - first ID)
				const auto cacheProgress{
					static_cast<float>(
							this->urls.front().first - this->idFirst
					) / this->idDist
				};

				// approximate position = first position + cache progress * (last position - first position)
				const auto approxPosition{
					this->posFirstF + cacheProgress * this->posDist
				};

				this->setProgress(approxPosition / this->total);
			}
			else if(this->total > 0) {
				this->setProgress(this->posFirstF / this->total);
			}

			// start timer
			Timer::Simple timer;
			std::string timerStr;

			// extract from content
			const auto extracted{this->extractingNext()};

			// clear ID cache
			if(this->config.extractingRemoveDuplicates) {
				this->ids.clear();
			}

			// save expiration time of URL lock if extracting was successful or unlock URL if extracting failed
			if(extracted > 0) {
				this->finished.emplace(this->urls.front().first, this->lockTime);
			}
			else {
				// unlock URL if necesssary
				this->database.unLockUrlIfOk(this->urls.front().first, this->lockTime);
			}

			// stop timer
			if(this->config.generalTiming) {
				timerStr = timer.tickStr();
			}

			// reset lock time
			this->lockTime = "";

			// write to log if necessary
			const auto logLevel{
				this->config.generalTiming ?
						generalLoggingDefault
						: generalLoggingExtended
			};

			if(this->isLogLevel(logLevel)) {
				std::ostringstream logStrStr;

				switch(extracted) {
				case 0:
					logStrStr << "no dataset from ";

					break;

				case 1:
					logStrStr << "extracted one dataset from ";

					break;

				default:
					logStrStr.imbue(std::locale(""));

					logStrStr << "extracted " << extracted << " datasets from ";
				}

				logStrStr << this->urls.front().second;

				if(this->config.generalTiming) {
					logStrStr << " in " << timerStr;
				}

				this->log(logLevel, logStrStr.str());
			}
		}

		// URL finished
		this->extractingUrlFinished(!skip);
	}

	//! Pauses the extractor.
	/*!
	 * Stores the current time for keeping track
	 *  of the time, the extractor is paused.
	 */
	void Thread::onPause() {
		// save pause start time
		this->pauseTime = std::chrono::steady_clock::now();

		// save results if necessary
		this->extractingSaveResults(false);
	}

	//! Unpauses the extractor.
	/*!
	 * Calculates the time, the extractor was paused.
	 */
	void Thread::onUnpause() {
		// add pause time to start or idle time to ignore pause
		if(this->idleTime > std::chrono::steady_clock::time_point::min()) {
			this->idleTime += std::chrono::steady_clock::now() - this->pauseTime;
		}
		else {
			this->startTime += std::chrono::steady_clock::now() - this->pauseTime;
		}

		this->pauseTime = std::chrono::steady_clock::time_point::min();
	}

	//! Clears the extractor.
	void Thread::onClear() {
		// check counter and process timers
		if(this->tickCounter > 0) {
			// write ticks per second to log
			std::ostringstream tpsStrStr;

			if(this->pauseTime != std::chrono::steady_clock::time_point::min()) {
				// add pause time to start time to ignore pause
				this->startTime += std::chrono::steady_clock::now() - this->pauseTime;

				this->pauseTime = std::chrono::steady_clock::time_point::min();
			}

			if(this->idleTime > std::chrono::steady_clock::time_point::min()) {
				// add idle time to start time to ignore idling
				this->startTime += std::chrono::steady_clock::now() - this->idleTime;

				this->idleTime = std::chrono::steady_clock::time_point::min();
			}

			const auto tps{
				static_cast<long double>(this->tickCounter) /
				std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::steady_clock::now()
						- this->startTime
				).count()
			};

			tpsStrStr.imbue(std::locale(""));

			tpsStrStr << std::setprecision(2) << std::fixed << tps;

			this->log(
					generalLoggingDefault,
					"average speed: "
					+ tpsStrStr.str()
					+ " ticks per second."
			);
		}

		// save results if necessary
		this->extractingSaveResults(false);

		// save status message
		const auto oldStatus(this->getStatusMessage());

		// set status message
		this->setStatusMessage("Finishing up...");

		// unlock remaining URLs
		this->database.unLockUrlsIfOk(this->urls, this->cacheLockTime);

		// clean up queries
		this->deleteQueries();
		this->clearQueries();

		// restore previous status message
		this->setStatusMessage(oldStatus);
	}

	//! Resets the extractor.
	void Thread::onReset() {
		this->onClear();

		this->resetBase();

		this->onInit();
	}

	/*
	 * INITIALIZING FUNCTIONS (private)
	 */

	// load configuration
	void Thread::setUpConfig(std::queue<std::string>& warningsTo) {
		this->setStatusMessage("Loading configuration...");

		this->loadConfig(this->database.getConfiguration(this->getConfig()), warningsTo);
	}

	// check required queries
	void Thread::checkQueries() {
		if(this->config.extractingDatasetQueries.empty()) {
			throw Exception(
					"Extractor::Thread::checkQueries():"
					" No dataset extraction query has been specified"
			);
		}

		if(this->config.extractingIdQueries.empty()) {
			throw Exception(
					"Extractor::Thread::checkQueries():"
					" No ID extraction query has been specified"
			);
		}
	}

	// set up logging
	void Thread::setUpLogging() {
		this->database.setLogging(
				this->config.generalLogging,
				generalLoggingDefault,
				generalLoggingVerbose
		);
	}

	// set query container options
	void Thread::setUpContainer() {
		this->setRepairCData(this->config.extractingRepairCData);
		this->setRepairComments(this->config.extractingRepairComments);
		this->setRemoveXmlInstructions(this->config.extractingRemoveXmlInstructions);
		this->setMinimizeMemory(this->config.generalMinimizeMemory);
		this->setTidyErrorsAndWarnings(
				this->config.generalTidyWarnings,
				this->config.generalTidyErrors
		);
	}

	// set database options
	void Thread::setUpDatabase() {
		this->setStatusMessage("Setting database options...");

		this->log(generalLoggingVerbose, "sets database options...");

		this->database.setCacheSize(this->config.generalCacheSize);
		this->database.setMaxBatchSize(this->config.generalMaxBatchSize);
		this->database.setReExtract(this->config.generalReExtract);
		this->database.setExtractCustom(this->config.generalExtractCustom);
		this->database.setTargetTable(this->config.generalTargetTable);
		this->database.setLinkedTable(this->config.linkedTargetTable);
		this->database.setTargetFields(this->config.extractingFieldNames);
		this->database.setLinkedFields(this->config.linkedFieldNames);
		this->database.setLinkedField(this->config.linkedLink);
		this->database.setOverwrite(this->config.extractingOverwrite);
		this->database.setOverwriteLinked(this->config.linkedOverwrite);
		this->database.setSleepOnError(this->config.generalSleepMySql);

		this->database.setRawContentIsSource(
				std::find(
						this->config.variablesSource.cbegin(),
						this->config.variablesSource.cend(),
						variablesSourcesContent
				) != this->config.variablesSource.cend()
		);
	}

	// set sources
	void Thread::setUpSources() { DBG_BEG
		std::queue<StringString> sources;

		for(std::size_t index{}; index < this->config.variablesName.size(); ++index) {
			if(this->config.variablesSource.at(index) == variablesSourcesParsed) {
				if(
						this->config.variablesParsedColumn.at(index) == "id"
						|| this->config.variablesParsedColumn.at(index) == "datetime"
				) {
					sources.push(
							StringString(
									this->config.variablesParsedTable.at(index),
									"parsed_"
									+ this->config.variablesParsedColumn.at(index)
							)
					);
				}
				else {
					sources.push(
							StringString(
									this->config.variablesParsedTable.at(index),
									"parsed__"
									+ this->config.variablesParsedColumn.at(index)
							)
					);
				}
			}
		}

		this->database.setSources(sources);

		DBG_END(setUpSources)
	}

	// create table names for locking
	void Thread::setUpTableNames() {
		const std::string urlListTable(
				"crawlserv_"
				+ this->websiteNamespace
				+ "_" + this->urlListNamespace
		);

		this->extractingTable = urlListTable
				+ "_extracting";
		this->targetTable = urlListTable
				+ "_extracted_"
				+ this->config.generalTargetTable;

		if(!(this->config.linkedTargetTable.empty())) {
			this->linkedTable = urlListTable
					+ "_extracted_"
					+ this->config.linkedTargetTable;
		}
	}

	// initialize target tables
	void Thread::setUpTarget() {
		this->setStatusMessage("Initializing target tables...");

		this->log(generalLoggingVerbose, "initializes target tables...");

		this->database.initTargetTables();
	}

	// prepare SQL statements for extractor
	void Thread::setUpSqlStatements() {
		this->setStatusMessage("Preparing SQL statements...");

		this->log(generalLoggingVerbose, "prepares SQL statements...");

		this->database.prepare();
	}

	// set network configuration
	void Thread::setUpNetworking() {
		std::queue<std::string> configWarnings;

		this->setStatusMessage("Setting network configuration...");

		this->log(generalLoggingVerbose, "sets network configuration...");

		this->networking.setConfigGlobal(*this, false, configWarnings);

		this->logWarnings(configWarnings);
	}

	// set TOR options
	void Thread::setUpTor() {
		if(this->networkConfig.resetTorAfter > 0) {
			this->torControl.setNewIdentityMax(this->networkConfig.resetTorAfter);
		}

		if(this->networkConfig.resetTorOnlyAfter > 0) {
			this->torControl.setNewIdentityMin(this->networkConfig.resetTorOnlyAfter);
		}
	}

	// initialize queries
	void Thread::setUpQueries() {
		this->setStatusMessage("Initializing custom queries...");

		this->log(
				generalLoggingVerbose,
				"initializes custom queries..."
		);

		this->initQueries();
	}

	// check extracting table
	void Thread::checkExtractingTable() {
		// wait for extracting table lock
		this->setStatusMessage("Waiting for extracting table...");

		DatabaseLock(
				this->database,
				"extractingTable." + this->extractingTable,
				[this]() {
					return this->isRunning();
				}
		);

		// cancel if not running anymore
		if(!(this->isRunning())) {
			return;
		}

		// check extracting table
		this->setStatusMessage("Checking extracting table...");

		this->log(
				generalLoggingVerbose,
				"checks extracting table..."
		);

		const auto deleted{this->database.checkExtractingTable()};

		// log deletion warning if necessary
		if(this->isLogLevel(generalLoggingDefault)) {
			switch(deleted) {
			case 0:
				break;

			case 1:
				this->log(
						generalLoggingDefault,
						"WARNING:"
						" Deleted a duplicate URL lock."
				);

				break;

			default:
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr << "WARNING: Deleted "
						<< deleted
						<< " duplicate URL locks!";

				this->log(generalLoggingDefault, logStrStr.str());
			}
		}
	}

	// save start time and initialize counter
	void Thread::setUpTimers() {
		this->startTime = std::chrono::steady_clock::now();
		this->pauseTime = std::chrono::steady_clock::time_point::min();

		this->tickCounter = 0;
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

	// log warnings received by external function when extracting a URL
	void Thread::logWarningsUrl(std::queue<std::string>& warnings) {
		while(!warnings.empty()) {
			std::string logEntry{
				"WARNING: "
			};

			logEntry += warnings.front();
			logEntry += " [";
			logEntry += this->urls.front().second;
			logEntry += "]";

			this->log(
					generalLoggingDefault,
					"WARNING: "
					+ warnings.front()
			);

			warnings.pop();
		}
	}

	// log warnings received by external function from a specific source when extracting a URL
	void Thread::logWarningsSource(std::queue<std::string>& warnings, std::string_view source) {
		while(!warnings.empty()) {
			std::string logEntry{
				"WARNING: "
			};

			logEntry += warnings.front();
			logEntry += " from ";
			logEntry += source;
			logEntry += " [";
			logEntry += this->urls.front().second;
			logEntry += "]";

			this->log(generalLoggingDefault, logEntry);

			warnings.pop();
		}
	}

	/*
	 * QUERY FUNCTIONS (private)
	 */

	// initialize queries, throws Thread::Exception
	void Thread::initQueries() { DBG_BEG
		try {
			this->addQueries(
					this->config.extractingErrorFail,
					this->queriesErrorFail
			);
			this->addQueries(
					this->config.extractingErrorRetry,
					this->queriesErrorRetry
			);
			this->addQueries(
					this->config.extractingDatasetQueries,
					this->queriesDatasets
			);
			this->addQueries(
					this->config.extractingIdQueries,
					this->queriesId
			);
			this->addQueries(
					this->config.extractingRecursive,
					this->queriesRecursive
			);
			this->addQueries(
					this->config.linkedDatasetQueries,
					this->queriesLinkedDatasets
			);
			this->addQueries(
					this->config.linkedIdQueries,
					this->queriesLinkedId
			);

			/*
			 * NOTE: The following queries need to be added even if they are of type 'none'
			 * 		  as their index needs to correspond to other options.
			 */

			this->addQueriesTo(
					this->config.extractingDateTimeQueries,
					this->queriesDateTime
			);
			this->addQueriesTo(
					"field",
					this->config.extractingFieldNames,
					this->config.extractingFieldQueries,
					this->queriesFields
			);
			this->addQueriesTo(
					"linked field",
					this->config.linkedFieldNames,
					this->config.linkedFieldQueries,
					this->queriesLinkedFields
			);

			/*
			 * only add queries for valid tokens
			 */

			this->queriesTokens.reserve(this->config.variablesTokensQuery.size());

			for(
					auto it{this->config.variablesTokensQuery.cbegin()};
					it != this->config.variablesTokensQuery.cend();
					++it
			) {
				QueryProperties properties;
				const auto index{
					it - this->config.variablesTokensQuery.cbegin()
				};

				if(*it > 0) {
					this->database.getQueryProperties(*it, properties);

					if(
							!properties.resultSingle
							&& !properties.resultBool
					) {
						const auto& name{this->config.variablesTokens.at(index)};

						if(!name.empty()) {
							this->log(
									generalLoggingDefault,
									"WARNING:"
									" Ignores token '"
									+ this->config.variablesTokens.at(index)
									+ "' because of wrong query result type."
							);
						}
					}
				}
				else {
					const auto& name{this->config.variablesTokens.at(index)};

					if(!name.empty()) {
						this->log(
								generalLoggingDefault,
								"WARNING:"
								" Ignores token '"
								+ name
								+ "' because of missing query."
						);
					}
				}

				this->queriesTokens.emplace_back(this->addQuery(*it, properties));
			}

			/*
			 * only add queries for valid variables,
			 *  not extracted from parsed data
			 */

			this->queriesVariables.reserve(
					std::count_if(
							this->config.variablesSource.cbegin(),
							this->config.variablesSource.cend(),
							[](const auto source) {
								return	source == variablesSourcesContent
										|| source == variablesSourcesUrl;
							}
					)
			);

			this->queriesVariablesSkip.reserve(
					this->config.variablesName.size()
			);

			for(std::size_t index{0}; index < this->config.variablesName.size(); ++index) {
				const auto source{this->config.variablesSource.at(index)};

				if(
						source == variablesSourcesContent
						|| source == variablesSourcesUrl
				) {
					QueryProperties queryProperties;

					const auto query{
						this->config.variablesQuery.at(index)
					};

					if(query > 0) {
						this->database.getQueryProperties(query, queryProperties);

						if(!queryProperties.resultSingle && !queryProperties.resultBool) {
							const auto& name{
								this->config.variablesName[index]
							};

							if(!name.empty()) {
								this->log(
										generalLoggingDefault,
										"WARNING:"
										" Ignores variable '"
										+ name
										+ "' because of wrong query result type."
								);
							}
						}
						else if(
								source == variablesSourcesUrl
								&& !queryProperties.type.empty()
								&& queryProperties.type != "regex"
						) {
							const auto& name{
								this->config.variablesName[index]
							};

							if(!name.empty()) {
								this->log(
										generalLoggingDefault,
										"WARNING:"
										" Ignores variable '"
										+ name
										+ "' because of wrong query type for URL."
								);
							}
						}
					}
					else {
						const auto& name{
							this->config.variablesName.at(index)
						};

						if(!name.empty()) {
							this->log(
									generalLoggingDefault,
									"WARNING:"
									" Ignores variable '"
									+ name
									+ "' because of missing query."
							);
						}
					}

					this->queriesVariables.emplace_back(
							this->addQuery(query, queryProperties)
					);
				}

				/*
				 * add a skip query for EACH variable
				 */

				QueryProperties skipQueryProperties;
				const auto skipQuery{
					this->config.variablesSkipQuery.at(index)
				};

				if(skipQuery > 0) {
					this->database.getQueryProperties(skipQuery, skipQueryProperties);
				}

				this->queriesVariablesSkip.emplace_back(
						this->addQuery(skipQuery, skipQueryProperties)
				);
			}

			this->addOptionalQuery(
					this->config.pagingIsNextFrom,
					this->queryPagingIsNextFrom
			);
			this->addOptionalQuery(
					this->config.pagingNextFrom,
					this->queryPagingNextFrom
			);
			this->addOptionalQuery(
					this->config.pagingNumberFrom,
					this->queryPagingNumberFrom
			);
			this->addOptionalQuery(
					this->config.extractingSkipQuery,
					this->queryExtractingSkip
			);
			this->addOptionalQuery(
					this->config.expectedQuery,
					this->queryExpected
			);
		}
		catch(const QueryException& e) {
			throw Exception(
					"Extractor::Thread::initQueries(): "
					+ std::string(e.view())
			);
		}

		DBG_END(initQueries)
	}

	// delete queries
	void Thread::deleteQueries() {
		queriesVariables.clear();
		queriesVariablesSkip.clear();
		queriesTokens.clear();
		queriesErrorFail.clear();
		queriesErrorRetry.clear();
		queriesDatasets.clear();
		queriesId.clear();
		queriesDateTime.clear();
		queriesFields.clear();
		queriesRecursive.clear();
		queriesLinkedDatasets.clear();
		queriesLinkedId.clear();
		queriesLinkedFields.clear();

		queryPagingIsNextFrom = {};
		queryPagingNextFrom = {};
		queryPagingNumberFrom = {};
		queryExtractingSkip = {};
		queryExpected = {};
	}

	// add optional query
	inline void Thread::addOptionalQuery(std::uint64_t queryId, QueryStruct& propertiesTo) {
		if(queryId > 0) {
			QueryProperties properties;

			this->database.getQueryProperties(queryId, properties);

			propertiesTo = this->addQuery(queryId, properties);
		}
	}

	// add multiple queries at once, ignoring empty ones
	inline void Thread::addQueries(
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

	// add multiple queries at once, even empty ones, so that their index corresponds to other options
	inline void Thread::addQueriesTo(
			const std::vector<std::uint64_t>& queryIds,
			std::vector<QueryStruct>& propertiesTo
	) {
		// reserve memory first
		propertiesTo.reserve(queryIds.size());

		for(const auto& queryId : queryIds) {
			QueryProperties properties;

			if(queryId > 0) {
				this->database.getQueryProperties(queryId, properties);
			}

			propertiesTo.emplace_back(this->addQuery(queryId, properties));
		}
	}

	// add multiple queries at once, even empty ones, so that their index corresponds to other options
	inline void Thread::addQueriesTo(
			std::string_view type,
			const std::vector<std::string>& names,
			const std::vector<std::uint64_t>& queryIds,
			std::vector<QueryStruct>& propertiesTo
	) { DBG_BEG
		// reserve memory first
		propertiesTo.reserve(queryIds.size());

		for(auto it{queryIds.cbegin()}; it != queryIds.cend(); ++it) {
			QueryProperties properties;

			if(*it > 0) {
				this->database.getQueryProperties(*it, properties);
			}
			else {
				const auto& name{
					names.at(it - queryIds.cbegin())
				};

				if(!name.empty()) {
					std::string logString{
						"WARNING: Ignores "
					};

					logString += type;
					logString += " '";
					logString += name;
					logString += "' , because of missing query.";

					this->log(
							generalLoggingDefault,
							logString
					);
				}
			}

			// add even empty queries
			propertiesTo.emplace_back(this->addQuery(*it, properties));
		}

		DBG_END(addQueriesTo)
	}

	/*
	 * EXTRACTING FUNCTIONS (private)
	 */

	// URL selection
	void Thread::extractingUrlSelection() {
		Timer::Simple timer;

		// get number of URLs
		this->total = this->database.getNumberOfUrls();

		this->setStatusMessage("Fetching URLs...");

		// fill cache with next URLs
		this->log(generalLoggingExtended, "fetches URLs...");

		// get next URL(s)
		this->extractingFetchUrls();

		// write to log if necessary
		if(this->config.generalTiming) {
			this->log(
					generalLoggingDefault,
					"fetched URLs in "
					+ timer.tickStr()
			);
		}

		// update status
		this->setStatusMessage("Checking URLs...");

		// check whether URLs have been fetched
		if(this->urls.empty()) {
			// no more URLs to extract data from
			if(!(this->idle)) {
				this->log(generalLoggingExtended, "finished.");

				this->setStatusMessage(
						"IDLE Waiting for new content to extract data from."
				);

				this->setProgress(1.F);
			}

			return;
		}


		// reset idling status
		this->idle = false;
	}

	// fetch next URLs from database
	void Thread::extractingFetchUrls() {
		// fetch URLs from database to cache
		this->cacheLockTime = this->database.fetchUrls(
				this->getLast(),
				this->urls,
				this->config.generalLock
		);

		// check whether URLs have been fetched
		if(this->urls.empty()) {
			return;
		}

		// save properties of fetched URLs and URL list for progress calculation
		this->idFirst = this->urls.front().first;
		this->idDist = this->urls.back().first - this->idFirst;

		const auto posFirst{
			this->database.getUrlPosition(this->idFirst)
		};

		this->posFirstF = static_cast<float>(posFirst);
		this->posDist = this->database.getUrlPosition(
				this->urls.back().first
		) - posFirst;
	}

	// check whether next URL(s) ought to be skipped
	void Thread::extractingCheckUrls() {
		// loop over next URLs in cache
		while(!(this->urls.empty()) && this->isRunning()) {
			// check whether URL needs to be skipped because of invalid ID
			if(this->urls.front().first == 0) {
				this->log(
						generalLoggingDefault,
						"skips (INVALID ID) "
						+ this->urls.front().second
				);

				// unlock URL if necessary
				this->database.unLockUrlIfOk(this->urls.front().first, this->cacheLockTime);

				// finish skipped URL
				this->extractingUrlFinished(false);

				continue;
			}

			break; // found URL to process
		} // end of loop over URLs in cache
	}

	// extract data from next URL, return number of extracted datasets
	std::size_t Thread::extractingNext() {
		std::queue<std::string> queryWarnings;
		std::size_t expected{};
		std::size_t extracted{};
		std::size_t linked{};
		bool expecting{false};

		// get datasets
		for(const auto& query : this->queriesDatasets) {
			// reserve memory for subsets if possible
			if(expecting) {
				this->reserveForSubSets(query, expected);
			}
		}

		// get content ID and - if necessary - the whole content
		IdString content;

		this->database.getContent(this->urls.front().first, content);

		// set raw crawled content as target for subsequent queries
		this->setQueryTarget(content.second, this->urls.front().second);

		// check content ID
		if(content.first == 0) {
			return 0;
		}

		// get values for variables and check whether URL needs to be skipped
		std::vector<StringString> variables;

		this->log(
				generalLoggingVerbose,
				"gets values for variables..."
		);

		this->extractingGetVariableValues(variables);

		if(this->extractingIsSkip(variables)) {
			// skip the URL
			return 0;
		}

		// get values for global tokens
		this->log(
				generalLoggingVerbose,
				"gets values for global tokens..."
		);

		this->extractingGetTokenValues(variables);

		// clear query target
		this->clearQueryTarget();

		// loop over pages
		this->log(
				generalLoggingVerbose,
				"loops over pages..."
		);

		std::queue<std::string> pageNames;
		std::int64_t pageNum{this->config.pagingFirst};
		bool pageFirst{true};
		bool noPageString{this->config.pagingFirstString.empty()};
		bool queryTargetSet{false};
		std::size_t pageCounter{};
		std::size_t pageTotal{};

		// add first page
		if(noPageString) {
			pageNames.emplace(std::to_string(pageNum));
		}
		else {
			pageNames.emplace(this->config.pagingFirstString);
		}

		while(this->isRunning() && !pageNames.empty()) {
			// resolve alias for paging variable
			std::string pageAlias;

			if(this->config.pagingAliasAdd > 0) {
				try {
					pageAlias = std::to_string(
							std::stol(pageNames.front())
							+ this->config.pagingAliasAdd
					);
				}
				catch(const std::exception& e) {
					this->log(
							generalLoggingDefault,
							"WARNING:"
							" Could not create numeric alias '"
							+ this->config.pagingAlias
							+ "' for non-numeric variable '"
							+ this->config.pagingVariable
							+ "' [= '"
							+ pageNames.front()
							+ "']."
					);
				}
			}
			else {
				pageAlias = pageNames.front();
			}

			// get page-specific tokens
			std::vector<StringString> pageTokens;

			this->extractingGetPageTokenValues(pageNames.front(), pageTokens, variables);

			// get custom HTTP headers (including cookies)
			std::string cookies(this->config.sourceCookies);
			std::vector<std::string> headers(this->config.sourceHeaders);

			// get source URL
			std::string sourceUrl;

			if(pageFirst) {
				if(this->config.sourceUrlFirst.empty()) {
					sourceUrl = this->config.sourceUrl;
				}
				else {
					sourceUrl = this->config.sourceUrlFirst;
				}
			}
			else {
				sourceUrl = this->config.sourceUrl;
			}

			// replace variables, their aliases and tokens
			Helper::Strings::replaceAll(cookies, this->config.pagingVariable, pageNames.front());
			Helper::Strings::replaceAll(cookies, this->config.pagingAlias, pageAlias);
			Helper::Strings::replaceAll(sourceUrl, this->config.pagingVariable, pageNames.front());
			Helper::Strings::replaceAll(sourceUrl, this->config.pagingAlias, pageAlias);

			for(auto& header : headers) {
				Helper::Strings::replaceAll(header, this->config.pagingVariable, pageNames.front());
				Helper::Strings::replaceAll(header, this->config.pagingAlias, pageAlias);
			}

			for(const auto& variable : variables) {
				Helper::Strings::replaceAll(cookies, variable.first, variable.second);
				Helper::Strings::replaceAll(sourceUrl, variable.first, variable.second);

				for(auto& header : headers) {
					Helper::Strings::replaceAll(header, variable.first, variable.second);
				}
			}

			for(const auto& token : pageTokens) {
				Helper::Strings::replaceAll(cookies, token.first, token.second);
				Helper::Strings::replaceAll(sourceUrl, token.first, token.second);

				for(auto& header : headers) {
					Helper::Strings::replaceAll(header, token.first, token.second);
				}
			}

			// check URL
			if(sourceUrl.empty()) {
				// remove current page from queue
				pageNames.pop();

				continue;	// continue with next page (if one exists)
			}

			// get and check content of current page
			this->log(
					generalLoggingVerbose,
					"fetches "
					+ sourceUrl
					+ "..."
			);

			if(!cookies.empty()) {
				this->log(
						generalLoggingVerbose,
						"[cookies] "
						+ cookies
				);
			}

			for(auto& header : headers) {
				if(!header.empty()) {
					this->log(
							generalLoggingVerbose,
							"[header] "
							+ header
					);
				}
			}

			std::string pageContent;

			this->extractingPageContent(sourceUrl, cookies, headers, pageContent);

			// log progress if necessary
			if(this->isLogLevel(generalLoggingExtended)) {
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr
					<< "fetched "
					<< pageContent.size()
					<< " byte(s) from "
					<< sourceUrl
					<< " ["
					<< this->urls.front().second
					<< "]";

				this->log(generalLoggingExtended, logStrStr.str());
			}

			if(pageContent.empty()) {
				// remove current page from queue
				pageNames.pop();

				continue;	// continue with next page (if one exists)
			}

			// set page content as target for subsequent queries
			this->setQueryTarget(pageContent, sourceUrl);

			queryTargetSet = true;

			// check whether to skip the URL
			const bool skip{
				this->extractingPageIsSkip(queryWarnings)
			};

			this->logWarningsSource(queryWarnings, sourceUrl);

			if(skip) {
				// cancel current URL
				this->log(
						generalLoggingExtended,
						"skipped "
						+ this->urls.front().second
						+ " due to query on "
						+ sourceUrl
				);

				break;
			}

			// check for an error in the page because of which the page needs to be retried
			if(this->extractingPageIsRetry(queryWarnings)) {
				std::string error("Error in data");
				std::string target;

				if(this->getTarget(target)) {
					error += ": '";
					error += target;
					error += "'";
				}

				this->extractingReset(error, sourceUrl);

				continue;
			}

			// remove current page from queue
			pageNames.pop();

			// check for first page
			if(pageFirst) {
				// get total number of pages if available
				if(this->queryPagingNumberFrom.valid()) {
					std::string pageTotalString;

					// perform query on content to get the number of pages
					this->getSingleFromQuery(
							this->queryPagingNumberFrom,
							pageTotalString,
							queryWarnings
					);

					// log warnings if necessary
					this->logWarningsSource(queryWarnings, sourceUrl);

					// try to convert number of pages to numeric value
					try {
						pageTotal = std::stoul(pageTotalString);
					}
					catch(const std::exception& e) {
						std::string logString{
							"WARNING: Could convert non-numeric query result '"
						};

						logString += pageTotalString;
						logString += "' to number of pages from ";
						logString += sourceUrl;
						logString += " [";
						logString += this->urls.front().second;
						logString += "]";

						this->log(generalLoggingDefault, logString);
					}

					if(pageTotal == 0) {
						return 0; // no pages, no data
					}
				}

				// get expected number of datasets if necessary
				if(this->queryExpected.valid()) {
					std::string expectedStr;

					this->getSingleFromQuery(
							this->queryExpected,
							expectedStr,
							queryWarnings
					);

					// log warnings if necessary
					this->logWarningsSource(queryWarnings, sourceUrl);

					// try to convert expected number of datasets
					if(!expectedStr.empty()) {
						try {
							expected = std::stoul(expectedStr);

							expecting = true;
						}
						catch(const std::logic_error& e) {
							std::string logString{"WARNING: '"};

							logString += expectedStr;
							logString += "' cannot be converted to a numeric value"
											" when extracting the expected number of URLs from ";
							logString += sourceUrl;
							logString += " [";
							logString += this->urls.front().second;
							logString += "]";

							this->log(
									generalLoggingDefault,
									logString
							);
						}
					}
				}

				pageFirst = false;
			}

			// extract data from content
			extracted += this->extractingPage(content.first, sourceUrl);

			// extract linked data from content
			linked += this->extractingLinked(content.first, sourceUrl);

			// check for next page
			bool noLimit{false};

			if(pageTotal > 0) {
				// determine whether next page exists by the extracted total number of pages
				++pageCounter;

				if(pageCounter >= pageTotal) {
					break;	// always cancel when maximum number of pages is reached
				}
			}
			else if(this->queryPagingIsNextFrom.valid()) {
				// determine whether next page exists by boolean query on page content
				bool isNext{false};

				this->getBoolFromQuery(
						this->queryPagingIsNextFrom,
						isNext,
						queryWarnings
				);

				// log warnings if necessary
				this->logWarningsSource(queryWarnings, sourceUrl);

				if(!isNext) {
					// always cancel when query says that the last page is reached
					break;
				}
			}
			else {
				noLimit = true;
			}

			// get ID(s) of next pages
			if(this->queryPagingNextFrom.valid()) {
				if(this->queryPagingNextFrom.resultMulti) {
					// get possibly multiple IDs by performing query on page content
					std::vector<std::string> pagesToAdd;

					this->getMultiFromQuery(
							this->queryPagingNextFrom,
							pagesToAdd,
							queryWarnings
					);

					// copy non-empty new ID(s) into page queue
					for(const auto& page : pagesToAdd) {
						if(!page.empty()) {
							// add only non-empty pages
							pageNames.push(page);
						}
					}
				}
				else {
					// get possibly one ID by performing query on page content
					std::string page;

					this->getSingleFromQuery(
							this->queryPagingNextFrom,
							page,
							queryWarnings
					);

					if(!page.empty()) {
						pageNames.push(page);
					}
				}

				// log warnings if necessary
				this->logWarningsSource(queryWarnings, sourceUrl);
			}
			else if(this->config.pagingStep > 0 && noPageString && !noLimit) {
				// get ID by incrementing old ID
				pageNum += this->config.pagingStep;
			}

			// clear query target before continuing to next page
			this->clearQueryTarget();

			queryTargetSet = false;
		}

		// clear query target before continuing to next URL (or finish)
		if(queryTargetSet) {
			this->clearQueryTarget();
		}

		// if necessary, compare the number of extracted datasets with the number of expected datatsets
		if(expecting) {
			std::ostringstream expectedStrStr;

			expectedStrStr.imbue(std::locale(""));

			if(extracted < expected) {
				// number of datasets is smaller than expected
				expectedStrStr	<< "number of extracted datasets ["
								<< extracted
								<< "] is smaller than expected ["
								<< expected
								<< "] ["
								<< this->urls.front().second
								<< "]";

				if(this->config.expectedErrorIfSmaller) {
					throw Exception(expectedStrStr.str());
				}

				this->log(
						generalLoggingDefault,
						"WARNING: "
						+ expectedStrStr.str()
						+ "."
				);
			}
			else if(extracted > expected) {
				// number of datasets is larger than expected
				expectedStrStr	<< "number of extracted datasets ["
								<< extracted
								<< "] is larger than expected ["
								<< expected
								<< "] ["
								<< this->urls.front().second
								<< "]";

				// number of URLs is smaller than expected
				if(this->config.expectedErrorIfLarger) {
					throw Exception(expectedStrStr.str());
				}

				this->log(
						generalLoggingDefault,
						"WARNING: "
						+ expectedStrStr.str()
						+ "."
				);
			}
			else {
				expectedStrStr	<< "number of extracted datasets ["
								<< extracted
								<< "] as expected ["
								<< expected
								<< "] ["
								<< this->urls.front().second
								<< "].";

				this->log(generalLoggingVerbose, expectedStrStr.str());
			}
		}

		return extracted;
	}

	// get values of variables
	void Thread::extractingGetVariableValues(std::vector<StringString>& variables) { DBG_BEG
		std::size_t parsedSource{};
		std::size_t queryCounter{};
		std::string logEntry;

		// loop over variables (and their aliases)
		for(
				auto it{this->config.variablesName.cbegin()};
				it != this->config.variablesName.cend();
				++it
		) {
			const auto index{it - this->config.variablesName.cbegin()};

			// get value for variable
			std::string value;

			switch(this->config.variablesSource.at(index)) {
			case variablesSourcesParsed:
				this->database.getLatestParsedData(
						this->urls.front().first,
						parsedSource,
						value
				);

				++parsedSource;

				break;

			case variablesSourcesContent:
				this->extractingGetValueFromContent(
						this->queriesVariables.at(queryCounter),
						value
				);

				++queryCounter;

				break;

			case variablesSourcesUrl:
				this->extractingGetValueFromUrl(
						this->queriesVariables.at(queryCounter),
						value
				);

				++queryCounter;

				break;

			default:
				logEntry = "WARNING: Invalid source for value of variable '";

				logEntry += *it;
				logEntry += "' [";
				logEntry += this->urls.front().second;
				logEntry += "]";

				this->log(generalLoggingDefault, logEntry);
			}

			const auto& dateTimeFormat{this->config.variablesDateTimeFormat.at(index)};

			if(!dateTimeFormat.empty()) {
				// perform date/time conversion for variable
				try {
					Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
							value,
							dateTimeFormat,
							this->config.variablesDateTimeLocale.at(index)
					);
				}
				catch(const LocaleException& e) {
					std::string logString{e.view()};

					logString += " - locale for date/time variable '";
					logString += *it;
					logString += "' ignored [";
					logString += this->urls.front().second;
					logString += "]";

					this->log(generalLoggingDefault, logString);

					try {
						Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								value,
								dateTimeFormat
						);
					}
					catch(const DateTimeException& e2) {
						logString = e2.view();

						logString += " - empty date/time variable '";
						logString += *it;
						logString += "' [";
						logString += this->urls.front().second;
						logString += "]";

						this->log(generalLoggingDefault, logString);

						value.clear();
					}
				}
				catch(const DateTimeException& e) {
					std::string logString{e.view()};

					logString += " - empty date/time variable '";
					logString += *it;
					logString += "' [";
					logString += this->urls.front().second;
					logString += "]";

					this->log(generalLoggingDefault, logString);

					value.clear();
				}
			}

			// add variable
			variables.emplace_back(*it, value);

			// get value for alias
			const auto& alias{this->config.variablesAlias.at(index)};

			if(!alias.empty()) {
				const auto& aliasAdd{this->config.variablesAliasAdd.at(index)};

				if(aliasAdd != 0) {
					// try to add value to variable
					std::string aliasValue;

					try {
						aliasValue = std::to_string(
								std::stol(value)
								+ aliasAdd
						);
					}
					catch(const std::exception& e) {
						std::string logString{
							"WARNING: Could not create numeric alias '"
						};

						logString += alias;
						logString += "' for non-numeric variable '";
						logString += *it;
						logString += "' (= '";
						logString += value;
						logString += "') [";
						logString += this->urls.front().second;
						logString += "]";

						this->log(generalLoggingDefault, logString);
					}

					// set variable alias to new value
					variables.emplace_back(alias, aliasValue);
				}
				else {
					// set variable alias to same value
					variables.emplace_back(alias, value);
				}
			}
		}

		DBG_END(extractingGetVariableValues)
	}

	// check values of variables, return true if the current URL needs to be skipped
	bool Thread::extractingIsSkip(const std::vector<StringString>& variables) { DBG_BEG
		std::queue<std::string> warnings;

		bool skip{false};

		for(std::size_t index{}; index < variables.size(); ++index) {
			const auto& variable{
				variables[index]
			};
			const auto& query{
				this->queriesVariablesSkip.at(index)
			};

			if(
					this->getBoolFromRegEx(query, variable.second, skip, warnings)
					&& skip
			) {
				// write log entry, if necessary, and skip
				std::string logEntry{"skipped "};

				logEntry += this->urls.front().second;
				logEntry += ", because ";
				logEntry += variable.first;
				logEntry += " = '";
				logEntry += variable.second;
				logEntry += "'.";

				this->log(generalLoggingDefault, logEntry);

				break;
			}
		}

		this->logWarningsUrl(warnings);

		return skip;

		DBG_END(extractingIsSkip)
	}

	// get values of global tokens
	void Thread::extractingGetTokenValues(std::vector<StringString>& variables) { DBG_BEG
		if(this->config.pagingVariable.empty()) {
			// copy headers
			std::vector<std::string> headers(this->config.variablesTokenHeaders);

			// replace already existing variables in headers
			for(auto& header : headers) {
				for(const auto& variable : variables) {
					Helper::Strings::replaceAll(header, variable.first, variable.second);
				}
			}

			// no paging variable: resolve all tokens
			for(
					auto it{this->config.variablesTokens.cbegin()};
					it != this->config.variablesTokens.cend();
					++it
			) {
				const auto index{it - this->config.variablesTokens.cbegin()};

				// copy source URL and cookies
				auto source{this->config.variablesTokensSource.at(index)};
				auto cookies{this->config.variablesTokensCookies.at(index)};

				// replace already existing variables in source URL and cookies
				for(const auto& variable : variables) {
					Helper::Strings::replaceAll(source, variable.first, variable.second);
					Helper::Strings::replaceAll(cookies, variable.first, variable.second);
				}

				// add token
				variables.emplace_back(
						*it,
						this->extractingGetTokenValue(
								*it,
								source,
								cookies,
								headers,
								this->config.variablesTokensUsePost.at(index),
								this->queriesTokens.at(index)
						)
				);
			}
		}
		else if(
				std::none_of(
						this->config.variablesTokenHeaders.cbegin(),
						this->config.variablesTokenHeaders.cend(),
						[this](const auto& header) {
							return header.find(this->config.pagingVariable) != std::string::npos;
						}
				)
		) { /* if headers are page-dependent, all tokens are also dependent on the current page */
			// copy headers
			std::vector<std::string> headers(this->config.variablesTokenHeaders);

			// replace already existing variables in headers
			for(auto& header : headers) {
				for(const auto& variable : variables) {
					Helper::Strings::replaceAll(
							header,
							variable.first,
							variable.second
					);
				}
			}

			// paging variable exists: resolve only page-independent tokens
			for(
					auto it{this->config.variablesTokens.cbegin()};
					it != this->config.variablesTokens.cend();
					++it
			) {
				const auto index{it - this->config.variablesTokens.cbegin()};
				const auto& sourceRef{this->config.variablesTokensSource.at(index)};
				const auto& cookiesRef{this->config.variablesTokensCookies.at(index)};

				if(
						sourceRef.find(this->config.pagingVariable) == std::string::npos
						&& cookiesRef.find(this->config.pagingVariable) == std::string::npos
				) {
					// copy source URL and cookies
					auto source{sourceRef};
					auto cookies{cookiesRef};

					// replace already existing variables in source URL and cookies
					for(const auto& variable : variables) {
						Helper::Strings::replaceAll(source, variable.first, variable.second);
						Helper::Strings::replaceAll(cookies, variable.first, variable.second);
					}

					// get value of variable
					variables.emplace_back(
							*it,
							this->extractingGetTokenValue(
									*it,
									source,
									cookies,
									headers,
									this->config.variablesTokensUsePost.at(index),
									this->queriesTokens.at(index)
							)
					);
				}
			}
		}

		DBG_END(extractingGetTokenValues)
	}

	// get values of page-specific tokens
	void Thread::extractingGetPageTokenValues(
			const std::string& page,
			std::vector<StringString>& tokens,
			const std::vector<StringString>& variables
	) { DBG_BEG
		if(this->config.pagingVariable.empty()) {
			return;
		}

		// copy headers
		std::vector<std::string> headers(this->config.variablesTokenHeaders);

		// replace variables in headers
		for(auto& header : headers) {
			for(const auto& variable : variables) {
				Helper::Strings::replaceAll(header, variable.first, variable.second);
			}

			Helper::Strings::replaceAll(header, this->config.pagingVariable, page);
		}

		// check whether all tokens are page-specific
		bool allTokens{
			std::any_of(
					headers.cbegin(),
					headers.cend(),
					[this](const auto& header) {
						return header.find(this->config.pagingVariable) != std::string::npos;
					}
			)
		};

		for(
				auto it{this->config.variablesTokens.cbegin()};
				it != this->config.variablesTokens.cend();
				++it
		) {
			const auto index{it - this->config.variablesTokens.cbegin()};
			const auto& sourceRef{this->config.variablesTokensSource.at(index)};
			const auto& cookiesRef{this->config.variablesTokensCookies.at(index)};

			// check whether token is page-specific
			if(
					allTokens
					|| sourceRef.find(this->config.pagingVariable) != std::string::npos
					|| cookiesRef.find(this->config.pagingVariable) != std::string::npos
			) {
				// copy source URL and cookies
				std::string source(sourceRef);
				std::string cookies(cookiesRef);

				// replace variables in source URL and cookies
				for(const auto& variable : variables) {
					Helper::Strings::replaceAll(source, variable.first, variable.second);
					Helper::Strings::replaceAll(cookies, variable.first, variable.second);
				}

				Helper::Strings::replaceAll(source, this->config.pagingVariable, page);
				Helper::Strings::replaceAll(cookies, this->config.pagingVariable, page);

				tokens.emplace_back(
						*it,
						this->extractingGetTokenValue(
								*it,
								source,
								cookies,
								headers,
								this->config.variablesTokensUsePost.at(index),
								this->queriesTokens.at(index)
						)
				);

				this->log(
						generalLoggingVerbose,
						"got token: "
						+ tokens.back().first
						+ "="
						+ tokens.back().second
				);
			}
		}

		DBG_END(extractingGetPageTokenValues)
	}

	// get value of token
	std::string Thread::extractingGetTokenValue(
			const std::string& name,
			const std::string& source,
			const std::string& setCookies,
			const std::vector<std::string>& setHeaders,
			bool usePost,
			const QueryStruct& query
	) {
		// ignore if invalid query is specified
		if(!query.resultBool && !query.resultSingle) {
			return "";
		}

		// get content for extracting token
		std::string content;
		std::string result;
		bool success{false};
		std::uint64_t retryCounter{};

		while(this->isRunning()) {
			try {
				// set local network configuration
				this->networking.setConfigCurrent(*this);

				// set custom HTTP headers (including cookies) if necessary
				if(!setCookies.empty()) {
					this->networking.setCookies(setCookies);
				}

				if(!setHeaders.empty()) {
					this->networking.setHeaders(setHeaders);
				}

				// get content
				this->networking.getContent(
						this->getProtocol() + source,
						usePost,
						content,
						this->config.generalRetryHttp
				);

				// unset custom HTTP headers (including cookies) if necessary
				this->extractingUnset(setCookies, setHeaders);

				success = true;

				break;
			}
			catch(const CurlException& e) { // error while getting content
				// unset custom HTTP headers (including cookies) if necessary
				this->extractingUnset(setCookies, setHeaders);

				// check type of error i.e. last libcurl code
				if(this->extractingCheckCurlCode(
						this->networking.getCurlCode(),
						source
				)) {
					// reset connection and retry
					this->extractingReset(e.view(), source);
				}
				else {
					std::string logString{"WARNING: Could not get token '"};

					logString += name;
					logString += "' from ";
					logString += source;
					logString += ": ";
					logString += e.view();

					this->log(generalLoggingDefault, logString);

					break;
				}
			}
			catch(const Utf8Exception& e) {
				// unset custom HTTP headers (including cookies) if necessary
				this->extractingUnset(setCookies, setHeaders);

				// write UTF-8 error to log if neccessary
				std::string logString{"WARNING: "};

				logString += e.view();
				logString += " [";
				logString += source;
				logString += "]";

				this->log(generalLoggingDefault, logString);

				break;
			}

			if(this->config.generalReTries < 0) {
				continue;
			}

			if(retryCounter == static_cast<std::uint64_t>(this->config.generalReTries)) {
				this->log(
						generalLoggingDefault,
						"Retried "
						+ std::to_string(retryCounter)
						+ " times, skipping "
						+ source
				);

				break;
			}

			++retryCounter;
		}

		if(success) {
			std::queue<std::string> queryWarnings;

			// set token page content as target for subsequent query
			this->setQueryTarget(content, source);

			// get token from content
			if(query.resultSingle) {
				this->getSingleFromQuery(query, result, queryWarnings);
			}
			else {
				bool booleanResult{false};

				if(this->getBoolFromQuery(query, booleanResult, queryWarnings)) {
					result = booleanResult ? "true" : "false";
				}
			}

			// clear query target
			this->clearQueryTarget();

			// logging if necessary
			this->logWarningsSource(queryWarnings, source);

			if(this->isLogLevel(generalLoggingExtended)) {
				std::string logString{
					"fetched token '"
				};

				logString += name;
				logString += "' from ";
				logString += source;
				logString += " (= '";
				logString += result;
				logString += "') [";
				logString += this->urls.front().second;
				logString += "]";

				this->log(generalLoggingExtended, logString);
			}
		}

		return result;
	}

	// get page content from URL
	void Thread::extractingPageContent(
			const std::string& url,
			const std::string& setCookies,
			const std::vector<std::string>& setHeaders,
			std::string& resultTo
	) {
		std::uint64_t retryCounter{};

		while(this->isRunning()) {
			try {
				// set local network configuration
				this->networking.setConfigCurrent(*this);

				// set custom HTTP headers (including cookies) if necessary
				if(!setCookies.empty()) {
					this->networking.setCookies(setCookies);
				}

				if(!setHeaders.empty()) {
					this->networking.setHeaders(setHeaders);
				}

				// get content
				this->networking.getContent(
						this->getProtocol() + url,
						this->config.sourceUsePost,
						resultTo,
						this->config.generalRetryHttp
				);

				// unset custom HTTP headers (including cookies) if necessary
				this->extractingUnset(setCookies, setHeaders);

				break;
			}
			catch(const CurlException& e) { // error while getting content
				// unset custom HTTP headers (including cookies) if necessary
				this->extractingUnset(setCookies, setHeaders);

				// error while getting content: check type of error i.e. last libcurl code
				if(this->extractingCheckCurlCode(
						this->networking.getCurlCode(),
						url
				)) {
					// reset connection and retry
					this->extractingReset(e.view(), url);
				}
				else {
					std::string logString{
						"WARNING:"
						" Could not extract data from "
					};

					logString += url;
					logString += ": ";
					logString += e.view();

					this->log(generalLoggingDefault, logString);

					break;
				}
			}
			catch(const Utf8Exception& e) {
				// unset custom HTTP headers (including cookies) if necessary
				this->extractingUnset(setCookies, setHeaders);

				// write UTF-8 error to log if neccessary
				std::string logString{"WARNING: "};

				logString += e.view();
				logString += " [";
				logString += url;
				logString += "]";

				this->log(generalLoggingDefault, logString);

				break;
			}

			if(this->config.generalReTries < 0) {
				continue;
			}

			if(retryCounter == static_cast<std::uint64_t>(this->config.generalReTries)) {
				this->log(
						generalLoggingDefault,
						"Retried "
						+ std::to_string(retryCounter)
						+ " times, skipping "
						+ url
				);

				break;
			}

			++retryCounter;
		}
	}

	// extract data from crawled content
	void Thread::extractingGetValueFromContent(const QueryStruct& query, std::string& resultTo) {
		// ignore if invalid query is specified
		if(!query.resultBool && !query.resultSingle) {
			return;
		}

		// get value by running query of any type on page content
		std::queue<std::string> queryWarnings;

		if(query.resultSingle) {
			this->getSingleFromQuery(query, resultTo, queryWarnings);
		}
		else {
			bool booleanResult{false};

			if(this->getBoolFromQuery(query, booleanResult, queryWarnings)) {
				resultTo = booleanResult ? "true" : "false";
			}
		}

		// log warnings if necessary
		this->logWarningsUrl(queryWarnings);
	}

	// extract data from URL
	void Thread::extractingGetValueFromUrl(const QueryStruct& query, std::string& resultTo) {
		// ignore if invalid query is specified
		if(
				(!query.resultBool && !query.resultSingle)
				|| query.type != QueryStruct::typeRegEx
		) {
			return;
		}

		// get value by running RegEx query on URL
		std::queue<std::string> queryWarnings;

		if(query.resultSingle) {
			this->getSingleFromRegEx(
					query,
					this->urls.front().second,
					resultTo,
					queryWarnings
			);
		}
		else {
			bool booleanResult{false};

			if(
					this->getBoolFromRegEx(
							query,
							this->urls.front().second,
							booleanResult,
							queryWarnings
					)
			) {
				resultTo = booleanResult ? "true" : "false";
			}
		}

		// log warnings if necessary
		this->logWarningsUrl(queryWarnings);
	}

	// check whether to skip the page and proceed to the next URL
	bool Thread::extractingPageIsSkip(std::queue<std::string>& queryWarningsTo) {
		bool skip{false};

		if(
				this->getBoolFromQuery(
						this->queryExtractingSkip,
						skip,
						queryWarningsTo
				)
		) {
			return skip;
		}

		return false;
	}

	// check for an error in the page because of which the page needs to be retried
	bool Thread::extractingPageIsRetry(std::queue<std::string>& queryWarningsTo) {
		for(const auto& query : this->queriesErrorRetry) {
			bool error{false};

			if(
					this->getBoolFromQuery(query, error, queryWarningsTo)
					&& error
			) {
				return true;
			}
		}

		return false;
	}

	// extract data by parsing page content, return number of extracted datasets
	std::size_t Thread::extractingPage(std::uint64_t contentId, const std::string& url) { DBG_BEG
		std::queue<std::string> queryWarnings;

		// check for errors if necessary
		for(const auto& query : this->queriesErrorFail) {
			bool error{false};

			if(this->getBoolFromQuery(query, error, queryWarnings) && error) {
				std::string target;

				if(this->getTarget(target)) {
					std::string error("Error in data :");

					error += target;
					error += " [";
					error += url;
					error += "]";

					throw Exception(error);
				}

				throw Exception("Error in data from " + url);
			}
		}

		// get datasets
		for(const auto& query : this->queriesDatasets) {
			// get datasets by performing query of any type on page content
			this->setSubSetsFromQuery(query, queryWarnings);

			// log warnings if necessary
			this->log(generalLoggingDefault, queryWarnings);

			// check whether datasets have been extracted
			if(this->getNumberOfSubSets() > 0) {
				break;
			}
		}

		// check whether no dataset has been extracted
		if(this->getNumberOfSubSets() == 0) {
			return 0;
		}

		// save old number of results
		const auto before{this->results.size()};

		// go through all datasets
		while(this->nextSubSet()) {
			DataEntry dataset(contentId);

			// extract IDs
			for(const auto& query : this->queriesId) {
				// get ID by performing query on current subset
				this->getSingleFromQueryOnSubSet(query, dataset.dataId, queryWarnings);

				// log warnings if necessary
				this->log(generalLoggingDefault, queryWarnings);

				// check whether ID has been extracted
				if(!dataset.dataId.empty()) {
					break;
				}
			}

			// check whether no ID has been extracted
			if(dataset.dataId.empty()) {
				continue;
			}

			// check whether extracted ID ought to be ignored
			if(
					std::find(
							this->config.extractingIdIgnore.cbegin(),
							this->config.extractingIdIgnore.cend(),
							dataset.dataId
					) != this->config.extractingIdIgnore.cend()
			) {
				this->log(
						generalLoggingExtended,
						"ignored parsed ID '"
						+ dataset.dataId
						+ "' ["
						+ url
						+ "]."
				);

				continue;
			}

			// extract date/time
			for(
					auto it{this->queriesDateTime.cbegin()};
					it != this->queriesDateTime.cend();
					++it
			) {
				// extract date/time by performing query on current subset
				this->getSingleFromQueryOnSubSet(*it, dataset.dateTime, queryWarnings);

				// log warnings if necessary
				this->log(generalLoggingDefault, queryWarnings);

				// check whether date/time has been extracted
				if(!dataset.dateTime.empty()) {
					const auto index{it - this->queriesDateTime.cbegin()};

					// found date/time: try to convert it to SQL time stamp
					std::string format(this->config.extractingDateTimeFormats.at(index));

					// use "%F %T" as default date/time format
					if(format.empty()) {
						format = "%F %T";
					}

					try {
						Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								dataset.dateTime,
								format,
								this->config.extractingDateTimeLocales.at(index)
						);
					}
					catch(const LocaleException& e) {
						this->log(
								generalLoggingDefault,
								"WARNING: "
								+ std::string(e.view())
								+ " - locale ignored."
						);

						try {
								Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
										dataset.dateTime,
										format
								);
						}
						catch(const DateTimeException& e) {
							std::string logString{e.view()};

							logString += " - query skipped [";
							logString += url;
							logString += "]";

							this->log(
									generalLoggingExtended,
									logString
							);

							dataset.dateTime.clear();
						}
					}
					catch(const DateTimeException& e) {
						std::string logString{e.view()};

						logString += " - query skipped [";
						logString += url;
						logString += "]";

						this->log(generalLoggingExtended, logString);

						dataset.dateTime.clear();
					}

					if(!dataset.dateTime.empty()) {
						break;
					}
				}
			}

			// extract custom fields
			dataset.fields.reserve(this->queriesFields.size());

			for(
					auto it{this->queriesFields.cbegin()};
					it != this->queriesFields.cend();
					++it
			) {
				const auto index{it - this->queriesFields.cbegin()};
				const auto fieldName{this->config.extractingFieldNames.at(index)};
				const auto dateTimeFormat{this->config.extractingFieldDateTimeFormats.at(index)};

				// determinate whether to get all or just the first match (as string or boolean value) from the query result
				if(it->resultMulti) {
					// extract multiple elements
					std::vector<std::string> extractedFieldValues;

					// extract field values by using query on content
					this->getMultiFromQueryOnSubSet(*it, extractedFieldValues, queryWarnings);

					// log warnings if necessary
					this->log(generalLoggingDefault, queryWarnings);

					// if necessary, try to convert the parsed values to date/times
					if(!dateTimeFormat.empty()) {
						for(auto& value : extractedFieldValues) {
							try {
								Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
										value,
										dateTimeFormat,
										this->config.extractingFieldDateTimeLocales.at(index)
								);
							}
							catch(const LocaleException& e) {
								try {
									this->extractingFieldWarning(
											e.view(),
											fieldName,
											url,
											false
									);

									Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
											value,
											dateTimeFormat
									);
								}
								catch(const DateTimeException& e) {
									this->extractingFieldWarning(
											e.view(),
											fieldName,
											url,
											false
									);

									value.clear();
								}
							}
							catch(const DateTimeException& e) {
								this->extractingFieldWarning(
										e.view(),
										fieldName,
										url,
										false
								);

								value.clear();
							}
						}
					}

					// if necessary, check whether array or all values are empty
					if(this->config.extractingFieldWarningsEmpty.at(index)) {
						if(
								std::all_of(
										extractedFieldValues.cbegin(),
										extractedFieldValues.cend(),
										[](auto const& value) {
											return value.empty();
										}
								)
						) {
							this->extractingFieldWarning(
									"is empty",
									fieldName,
									url,
									false
							);
						}
					}

					// determine how to save result: JSON array or concatenate using delimiting character
					if(this->config.extractingFieldJSON.at(index)) {
						// if necessary, tidy texts
						if(this->config.extractingFieldTidyTexts.at(index)) {
							for(auto& value : extractedFieldValues) {
								Helper::Strings::utfTidy(value);
							}
						}

						// stringify and add extracted elements as JSON array
						dataset.fields.emplace_back(Helper::Json::stringify(extractedFieldValues));
					}
					else {
						// concatenate elements
						std::string result(
								Helper::Strings::join(
										extractedFieldValues,
										this->config.extractingFieldDelimiters.at(index),
										this->config.extractingFieldIgnoreEmpty.at(index)
								)
							);

						// if necessary, tidy text
						if(this->config.extractingFieldTidyTexts.at(index)) {
							Helper::Strings::utfTidy(result);
						}

						dataset.fields.emplace_back(result);
					}
				}
				else if(it->resultSingle) {
					// extract first element only (as string)
					std::string extractedFieldValue;

					// extract single field value by using query on content
					this->getSingleFromQueryOnSubSet(*it, extractedFieldValue, queryWarnings);

					// log warnings if necessary
					this->log(generalLoggingDefault, queryWarnings);

					// if necessary, try to convert the parsed value to date/time
					if(!dateTimeFormat.empty()) {
						try {
							Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
									extractedFieldValue,
									dateTimeFormat,
									this->config.extractingFieldDateTimeLocales.at(index)
							);
						}
						catch(const LocaleException& e) {
							try {
								this->extractingFieldWarning(
										e.view(),
										fieldName,
										url,
										false
								);

								Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
										extractedFieldValue,
										dateTimeFormat
								);
							}
							catch(const DateTimeException& e) {
								this->extractingFieldWarning(
										e.view(),
										fieldName,
										url,
										false
								);

								extractedFieldValue.clear();
							}
						}
						catch(const DateTimeException& e) {
							this->extractingFieldWarning(
									e.view(),
									fieldName,
									url,
									false
							);

							extractedFieldValue.clear();
						}
					}

					// if necessary, check whether value is empty
					if(
							this->config.extractingFieldWarningsEmpty.at(index)
							&& extractedFieldValue.empty()
					) {
						this->extractingFieldWarning(
								"is empty",
								fieldName,
								url,
								false
						);
					}

					// if necessary, tidy text
					if(this->config.extractingFieldTidyTexts.at(index)) {
						Helper::Strings::utfTidy(extractedFieldValue);
					}

					// determine how to save result: JSON array or string as is
					if(this->config.extractingFieldJSON.at(index)) {
						// stringify and add extracted element as JSON array with one element
						dataset.fields.emplace_back(Helper::Json::stringify(extractedFieldValue));
					}
					else {
						// save as is
						dataset.fields.emplace_back(extractedFieldValue);
					}
				}
				else if(it->resultBool) {
					// only save whether a match for the query exists
					bool booleanResult{false};

					// extract boolean field value by using query on content
					this->getBoolFromQueryOnSubSet(*it, booleanResult, queryWarnings);

					// log warnings if necessary
					this->log(generalLoggingDefault, queryWarnings);

					// date/time conversion is not possible for boolean values
					if(!dateTimeFormat.empty()) {
						this->extractingFieldWarning(
								"Cannot convert boolean value to date/time",
								fieldName,
								url,
								false
						);
					}

					// determine how to save result: JSON array or boolean value as string
					if(this->config.extractingFieldJSON.at(index)) {
						// stringify and add extracted element as JSON array with one boolean value as string
						dataset.fields.emplace_back(
								Helper::Json::stringify(
										booleanResult ? std::string("true") : std::string("false")
								)
						);
					}
					else {
						// save boolean value as string
						dataset.fields.emplace_back(booleanResult ? "true" : "false");
					}
				}
				else {
					if(it->type != QueryStruct::typeNone) {
						this->extractingFieldWarning(
								"Ignored query without specified result type",
								fieldName,
								url,
								false
						);
					}

					dataset.fields.emplace_back();
				}
			}

			// check for duplicate IDs if necessary
			if(!(this->config.extractingRemoveDuplicates) || this->ids.insert(dataset.dataId).second) {
				// add extracted dataset to results
				this->results.push(dataset);
			}

			// recursive extracting
			for(const auto& query : this->queriesRecursive) {
				if(this->addSubSetsFromQueryOnSubSet(query, queryWarnings)) {
					break;
				}
			}

			// log warnings if necessary
			this->log(generalLoggingDefault, queryWarnings);
		}

		return this->results.size() - before;

		DBG_END(extractingPage)
	}

	// extract linked data by parsing page content, return number of extracted datasets
	std::size_t Thread::extractingLinked(std::uint64_t contentId, const std::string& url) { DBG_BEG
		std::queue<std::string> queryWarnings;

		// get datasets for linked data
		for(const auto& query : this->queriesLinkedDatasets) {
			// get datasets by performing query of any type on page content
			this->setSubSetsFromQuery(query, queryWarnings);

			// log warnings if necessary
			this->log(generalLoggingDefault, queryWarnings);

			// check whether datasets have been extracted
			if(this->getNumberOfSubSets() > 0) {
				break;
			}
		}

		// check whether no dataset has been extracted
		if(this->getNumberOfSubSets() == 0) {
			return 0;
		}

		// save old number of results
		const auto before{this->linked.size()};

		// go through all datasets
		while(this->nextSubSet()) {
			DataEntry dataset(contentId);

			// extract IDs
			for(const auto& query : this->queriesLinkedId) {
				// get ID by performing query on current subset
				this->getSingleFromQueryOnSubSet(query, dataset.dataId, queryWarnings);

				// log warnings if necessary
				this->log(generalLoggingDefault, queryWarnings);

				// check whether ID has been extracted
				if(!dataset.dataId.empty()) {
					break;
				}
			}

			// check whether no ID has been extracted
			if(dataset.dataId.empty()) {
				continue;
			}

			// check whether extracted ID ought to be ignored
			if(
					std::find(
							this->config.linkedIdIgnore.cbegin(),
							this->config.linkedIdIgnore.cend(),
							dataset.dataId
					) != this->config.linkedIdIgnore.cend()
			) {
				this->log(
						generalLoggingExtended,
						"ignored linked ID '"
						+ dataset.dataId
						+ "' ["
						+ url
						+ "]."
				);

				continue;
			}

			// extract linked fields
			dataset.fields.reserve(this->queriesLinkedFields.size());

			for(
					auto it{this->queriesLinkedFields.cbegin()};
					it != this->queriesLinkedFields.cend();
					++it
			) {
				const auto index{it - this->queriesLinkedFields.cbegin()};
				const auto& fieldName{this->config.linkedFieldNames.at(index)};
				const auto dateTimeFormat{this->config.linkedDateTimeFormats.at(index)};

				// determinate whether to get all or just the first match (as string or boolean value) from the query result
				if(it->resultMulti) {
					// extract multiple elements
					std::vector<std::string> linkedValues;

					// extract field values by using query on content
					this->getMultiFromQueryOnSubSet(*it, linkedValues, queryWarnings);

					// log warnings if necessary
					this->log(generalLoggingDefault, queryWarnings);

					// if necessary, try to convert the parsed values to date/times
					if(!dateTimeFormat.empty()) {
						for(auto& value : linkedValues) {
							try {
								Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
										value,
										dateTimeFormat,
										this->config.linkedDateTimeLocales.at(index)
								);
							}
							catch(const LocaleException& e) {
								try {
									this->extractingFieldWarning(
											e.view(),
											fieldName,
											url,
											true
									);

									Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
											value,
											dateTimeFormat
									);
								}
								catch(const DateTimeException& e) {
									this->extractingFieldWarning(
											e.view(),
											fieldName,
											url,
											true
									);

									value.clear();
								}
							}
							catch(const DateTimeException& e) {
								this->extractingFieldWarning(
										e.view(),
										fieldName,
										url,
										true
								);

								value.clear();
							}
						}
					}

					// if necessary, check whether array or all values are empty
					if(this->config.linkedWarningsEmpty.at(index)) {
						if(
								std::all_of(
										linkedValues.cbegin(),
										linkedValues.cend(),
										[](auto const& value) {
											return value.empty();
										}
								)
						) {
							this->extractingFieldWarning(
									"is empty",
									fieldName,
									url,
									true
							);
						}
					}

					// determine how to save result: JSON array or concatenate using delimiting character
					if(this->config.linkedJSON.at(index)) {
						// if necessary, tidy texts
						if(this->config.linkedTidyTexts.at(index)) {
							for(auto& value : linkedValues) {
								Helper::Strings::utfTidy(value);
							}
						}

						// stringify and add extracted elements as JSON array
						dataset.fields.emplace_back(Helper::Json::stringify(linkedValues));
					}
					else {
						// concatenate elements
						std::string result(
								Helper::Strings::join(
										linkedValues,
										this->config.linkedDelimiters.at(index),
										this->config.linkedIgnoreEmpty.at(index)
								)
							);

						// if necessary, tidy text
						if(this->config.linkedTidyTexts.at(index)) {
							Helper::Strings::utfTidy(result);
						}

						dataset.fields.emplace_back(result);
					}
				}
				else if(it->resultSingle) {
					// extract first element only (as string)
					std::string linkedValue;

					// extract single field value by using query on content
					this->getSingleFromQueryOnSubSet(*it, linkedValue, queryWarnings);

					// log warnings if necessary
					this->log(generalLoggingDefault, queryWarnings);

					// if necessary, try to convert the parsed value to date/time
					if(!dateTimeFormat.empty()) {
						try {
							Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
									linkedValue,
									dateTimeFormat,
									this->config.linkedDateTimeLocales.at(index)
							);
						}
						catch(const LocaleException& e) {
							try {
								this->extractingFieldWarning(
										e.view(),
										fieldName,
										url,
										true
								);

								Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
										linkedValue,
										dateTimeFormat
								);
							}
							catch(const DateTimeException& e) {
								this->extractingFieldWarning(
										e.view(),
										fieldName,
										url,
										true
								);

								linkedValue.clear();
							}
						}
						catch(const DateTimeException& e) {
							this->extractingFieldWarning(
									e.view(),
									fieldName,
									url,
									true
							);

							linkedValue.clear();
						}
					}

					// if necessary, check whether value is empty
					if(
							this->config.linkedWarningsEmpty.at(index)
							&& linkedValue.empty()
					) {
						this->extractingFieldWarning(
								"is empty",
								fieldName,
								url,
								true
						);
					}

					// if necessary, tidy text
					if(this->config.linkedTidyTexts.at(index)) {
						Helper::Strings::utfTidy(linkedValue);
					}

					// determine how to save result: JSON array or string as is
					if(this->config.linkedJSON.at(index)) {
						// stringify and add extracted element as JSON array with one element
						dataset.fields.emplace_back(Helper::Json::stringify(linkedValue));
					}
					else {
						// save as is
						dataset.fields.emplace_back(linkedValue);
					}
				}
				else if(it->resultBool) {
					// only save whether a match for the query exists
					bool booleanResult{false};

					// extract boolean field value by using query on content
					this->getBoolFromQueryOnSubSet(*it, booleanResult, queryWarnings);

					// log warnings if necessary
					this->log(generalLoggingDefault, queryWarnings);

					// date/time conversion is not possible for boolean values
					if(!dateTimeFormat.empty()) {
						this->extractingFieldWarning(
								"Cannot convert boolean value to date/time",
								fieldName,
								url,
								true
						);
					}

					// determine how to save result: JSON array or boolean value as string
					if(this->config.linkedJSON.at(index)) {
						// stringify and add extracted element as JSON array with one boolean value as string
						dataset.fields.emplace_back(
								Helper::Json::stringify(
										booleanResult ? std::string("true") : std::string("false")
								)
						);
					}
					else {
						// save boolean value as string
						dataset.fields.emplace_back(booleanResult ? "true" : "false");
					}
				}
				else {
					if(it->type != QueryStruct::typeNone) {
						this->extractingFieldWarning(
								"Ignored query without specified result type",
								fieldName,
								url,
								true
						);
					}

					// add empty field
					dataset.fields.emplace_back();
				}
			}

			// add extracted dataset to results
			this->linked.push(dataset);

			// log warnings if necessary
			this->log(generalLoggingDefault, queryWarnings);
		}

		return this->linked.size() - before;

		DBG_END(extractingLinked)
	}

	// check libcurl code and decide whether to retry or skip
	bool Thread::extractingCheckCurlCode(CURLcode curlCode, const std::string& url) {
		if(curlCode == CURLE_TOO_MANY_REDIRECTS) {
			// redirection error: skip URL
			this->log(
					generalLoggingDefault,
					"redirection error at "
					+ url
					+ " - skips..."
			);

			return false;
		}

		return true;
	}

	// check the HTTP response code for an error and decide whether to continue or skip
	bool Thread::extractingCheckResponseCode(const std::string& url, std::uint32_t responseCode) {
		if(responseCode >= httpResponseCodeMin && responseCode <= httpResponseCodeMax) {
			this->log(
					generalLoggingDefault,
					"HTTP error "
					+ std::to_string(responseCode)
					+ " from "
					+ url
					+ " - skips..."
			);

			return false;
		}

		if(responseCode != httpResponseCodeIgnore) {
			this->log(
					generalLoggingDefault,
					"WARNING:"
					" HTTP response code "
					+ std::to_string(responseCode)
					+ " from "
					+ url
					+ "."
			);
		}

		return true;
	}

	// URL has been processed (skipped or used for extraction)
	void Thread::extractingUrlFinished(bool success) {
		// check whether the finished URL is the last URL in the cache
		if(this->urls.size() == 1) {
			// if yes, save results to database
			this->extractingSaveResults(false);

			// reset URL properties
			this->idFirst = 0;
			this->idDist = 0;
			this->posFirstF = 0;
			this->posDist = 0;
		}

		if(success) {
			this->incrementProcessed();
		}

		// save URL ID as last processed URL
		this->lastUrl = this->urls.front().first;

		// delete current URL from cache
		this->urls.pop();
	}

	// save linked data to database
	void Thread::extractingSaveLinked() {
		if(this->linked.empty()) {
			// no results: done!
			return;
		}

		// timer
		Timer::Simple timer;

		// save status message
		const auto oldStatus{this->getStatusMessage()};

		this->setStatusMessage("Waiting for linked target table...");

		{ // lock linked target table
			DatabaseLock(
					this->database,
					"targetTable." + this->linkedTable,
					[]() {
						return true; //should run until the end
					}
			);

			// save linked data
			StatusSetter statusSetter(
					"Saving linked data...",
					this->getProgress(),
					[this](const auto& status) {
						this->setStatusMessage(status);
					},
					[this](const auto progress) {
						this->setProgress(progress);
					},
					[]() {
						return true; //should run until the end
					}
			);

			this->log(generalLoggingExtended, "saves linked data...");

			// update or add entries in/to database
			this->database.updateOrAddLinked(this->linked, statusSetter);
		} // linked target table unlocked

		// reset status
		this->setStatusMessage(oldStatus);

		if(this->config.generalTiming) {
			this->log(
					generalLoggingDefault,
					"saved linked data in "
					+ timer.tickStr()
			);
		}
	}

	// save results to database
	void Thread::extractingSaveResults(bool warped) {
		// save linked data first
		/*
		 * NOTE: because the other data will be linked
		 *  to this data when it is added, this data
		 *  has to already exist
		 */
		this->extractingSaveLinked();

		// check whether there are no results
		if(this->results.empty()) {
			// set last URL
			if(!warped && this->lastUrl > 0) {
				this->setLast(this->lastUrl);
			}

			// no results: done!
			return;
		}

		// timer
		Timer::Simple timer;

		// save status message
		const auto status(this->getStatusMessage());

		this->setStatusMessage("Waiting for target table...");

		{ // lock target table
			DatabaseLock(
					this->database,
					"targetTable." + this->targetTable,
					[]() {
						return true; // should run until the end
					}
			);

			// save results
			StatusSetter statusSetter(
					"Saving results...",
					this->getProgress(),
					[this](const auto& status) {
						this->setStatusMessage(status);
					},
					[this](const auto progress) {
						this->setProgress(progress);
					},
					[]() {
						return true; // should run until the end
					}
			);

			this->log(generalLoggingExtended, "saves results...");

			// update or add entries in/to database
			this->database.updateOrAddEntries(this->results, statusSetter);

			// update target table
			this->database.updateTargetTable();
		} // target table unlocked

		// set last URL
		if(!warped) {
			this->setLast(this->lastUrl);
		}

		// set those URLs to finished whose URL lock is okay (still locked or re-lockable)
		this->database.setUrlsFinishedIfLockOk(this->finished);

		// update status
		this->setStatusMessage("Results saved. [" + status + "]");

		if(this->config.generalTiming) {
			this->log(
					generalLoggingDefault,
					"saved results in "
					+ timer.tickStr()
			);
		}
	}

	// reset connection and retry
	void Thread::extractingReset(std::string_view error, std::string_view source) {
		// clear query target
		this->clearQueryTarget();

		// show error
		std::string errorString{error};

		errorString += " - retrying ";
		errorString += source;
		errorString += " [";
		errorString += this->urls.front().second;
		errorString += "]";

		this->log(generalLoggingDefault, errorString);

		this->setStatusMessage("ERROR " + errorString);

		// reset connection and retry (if still running)
		if(this->isRunning()) {
			this->log(generalLoggingDefault, "resets connection...");

			this->extractingResetTor();

			this->networking.resetConnection(
					this->config.generalSleepError,
					[this]() {
						return this->isRunning();
					}
			);

			this->log(
					generalLoggingDefault,
					"public IP: "
					+ this->networking.getPublicIp()
			);
		}
	}

	// request a new TOR identity if necessary
	void Thread::extractingResetTor() {
		try {
			if(
					this->torControl.active()
					&& this->networkConfig.resetTor
					&& this->torControl.newIdentity()
			) {
				this->log(
						generalLoggingDefault,
						"requested a new TOR identity."
				);
			}
		}
		catch(const TorControlException& e) {
			this->log(
					generalLoggingDefault,
					"could not request a new TOR identity - "
					+ std::string(e.view())
			);
		}
	}

	// unset cookies and/or headers if necessary
	inline void Thread::extractingUnset(
			const std::string& unsetCookies,
			const std::vector<std::string>& unsetHeaders
	) {
		if(!unsetCookies.empty()) {
			this->networking.unsetCookies();
		}

		if(!unsetHeaders.empty()) {
			this->networking.unsetHeaders();
		}
	}

	// log error when extracting specific field as warning
	inline void Thread::extractingFieldWarning(
			std::string_view error,
			std::string_view fieldName,
			std::string_view url,
			bool isLinked
	) {
		std::string logString{"WARNING: "};

		logString += error;

		if(isLinked) {
			logString += " for linked field '";
		}
		else {
			logString += " for field '";
		}

		logString += fieldName;
		logString += "' [";
		logString += url;
		logString += "]";

		this->log(generalLoggingDefault, logString);
	}

	/*
	 * shadowing functions not to be used by thread (private)
	 */

	// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
	void Thread::pause() {
		this->pauseByThread();
	}

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

} /* namespace crawlservpp::Module::Extractor */
