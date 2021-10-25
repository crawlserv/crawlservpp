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
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.hpp"

namespace crawlservpp::Module::Parser {

	/*
	 * CONSTRUCTION
	 */

	//! Constructor initializing a previously interrupted parser thread.
	/*!
	 * \param dbBase Reference to the main
	 *   database connection.
	 * \param threadOptions Constant reference
	 *   to a structure containing the options
	 *   for the thread.
	 * \param threadStatus Constant reference
	 *   to a structure containing the last
	 *   known status of the thread.
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

	//! Constructor initializing a new parser thread.
	/*!
	 * \param dbBase Reference to the main
	 *   database connection.
	 * \param threadOptions Constant reference
	 *   to a structure containing the options
	 *   for the thread.
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
	 * IMPLEMENTED THREAD FUNCTIONS
	 */

	//! Initializes the parser.
	void Thread::onInit() {
		std::queue<std::string> configWarnings;

		this->setUpConfig(configWarnings);
		this->setUpLogging();

		this->logWarnings(configWarnings);

		this->setUpContainer();
		this->setUpDatabase();
		this->setUpTableNames();
		this->setUpTarget();
		this->setUpSqlStatements();
		this->setUpQueries();

		if(!(this->isRunning())) {
			return; // cancel if not running anymore
		}

		this->checkParsingTable();

		this->setUpTimers();

		// parser is ready
		this->ready();
	}

	//! Performs a parser tick.
	/*!
	 * If successful, this will parse data from
	 *  one URL. If not, the URL will either be
	 *  skipped, or retried in the next tick.
	 *
	 * \throws Module::Parser::Thread::Exception
	 *   if the size of the URL list could not be
	 *   retrieved from the database.
	 */
	void Thread::onTick() {
		bool skip{false};
		std::size_t parsed{};

		// check for jump in last ID ("time travel")
		const auto jumpOffset{this->getWarpedOverAndReset()};

		if(jumpOffset != 0) {
			// save cached results
			this->parsingSaveResults(true);

			// unlock and discard old URLs
			this->database.unLockUrlsIfOk(
					this->urls,
					this->cacheLockTime
			);

			// overwrite last URL ID
			this->lastUrl = this->getLast();

			// adjust tick counter
			this->tickCounter += jumpOffset;
		}

		// URL selection if no URLs are left to parse
		if(this->urls.empty()) {
			this->parsingUrlSelection();
		}

		if(this->urls.empty()) {
			// no URLs left in database: set timer if just finished, sleep
			if(this->idleTime == std::chrono::steady_clock::time_point::min()) {
				this->idleTime = std::chrono::steady_clock::now();
			}

			this->sleep(this->config.generalSleepIdle);

			return;
		}

		// check whether next URL(s) ought to be skipped
		this->parsingCheckUrls();

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
				"parses "
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
			this->log(generalLoggingExtended, "skips (locked) " + this->urls.front().second);
		}
		else {
			// set status
			this->setStatusMessage(this->urls.front().second);

			// approximate progress
			if(this->total == 0) {
				throw Exception(
						"Parser::Thread::onTick():"
						" Could not retrieve the size of the URL list"
				);
			}

			if(this->idDist > 0) {
				// cache progress = (current ID - first ID) / (last ID - first ID)
				const auto cacheProgress{
					static_cast<float>(
							this->urls.front().first
							- this->idFirst)
					/ this->idDist
				};

				// approximate position = first position + cache progress
				//							* (last position - first position)
				const float approxPosition{
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

			// parse content(s)
			parsed = this->parsingNext();

			// check whether thread has been cancelled
			if(!(this->isRunning())) {
				// unlock URL if necesssary
				this->database.unLockUrlIfOk(this->urls.front().first, this->lockTime);

				return;
			}

			// stop timer
			if(this->config.generalTiming) {
				timerStr = timer.tickStr();
			}

			// save expiration time of URL lock if parsing was successful or unlock URL if parsing failed
			if(parsed > 0) {
				this->finished.emplace(this->urls.front().first, this->lockTime);
			}
			else {
				// unlock URL if necesssary
				this->database.unLockUrlIfOk(this->urls.front().first, this->lockTime);
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

				logStrStr.imbue(std::locale(""));

				if(parsed > 1) {
					logStrStr << "parsed " << parsed << " versions of ";
				}
				else if(parsed == 1) {
					logStrStr << "parsed ";
				}
				else {
					logStrStr << "skipped ";
				}

				logStrStr << this->urls.front().second;

				if(this->config.generalTiming) {
					logStrStr << " in " << timerStr;
				}

				this->log(logLevel, logStrStr.str());
			}
		}

		// URL finished
		this->parsingUrlFinished(!skip);
	}

	//! Pauses the parser.
	/*!
	 * Stores the current time for keeping track
	 *  of the time, the parser is paused.
	 */
	void Thread::onPause() {
		// save pause start time
		this->pauseTime = std::chrono::steady_clock::now();

		// save results if necessary
		this->parsingSaveResults(false);
	}

	//! Unpauses the parser.
	/*!
	 * Calculates the time, the parser was paused.
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

	//! Clears the parser.
	void Thread::onClear() {
		// check counter and process timers if necessary
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
		this->parsingSaveResults(false);

		// save status message
		const auto oldStatus{this->getStatusMessage()};

		// set status message
		this->setStatusMessage("Finishing up...");

		// unlock remaining URLs
		this->database.unLockUrlsIfOk(this->urls, this->cacheLockTime);

		// delete queries
		this->queriesSkip.clear();
		this->queriesDateTime.clear();
		this->queriesFields.clear();
		this->queriesId.clear();

		this->clearQueries();

		// restore previous status message
		this->setStatusMessage(oldStatus);
	}

	//! Resets the parser.
	void Thread::onReset() {
		this->onClear();

		this->resetBase();

		this->onInit();
	}

	/*
	 * INITIALIZING FUNCTION (private)
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

	// set query container options
	void Thread::setUpContainer() {
		this->setRepairCData(this->config.parsingRepairCData);
		this->setRepairComments(this->config.parsingRepairComments);
		this->setRemoveXmlInstructions(this->config.parsingRemoveXmlInstructions);
		this->setTidyErrorsAndWarnings(
				this->config.parsingTidyWarnings,
				this->config.parsingTidyErrors
		);
	}

	// set database options
	void Thread::setUpDatabase() {
		this->setStatusMessage("Setting database options...");

		this->log(generalLoggingVerbose, "sets database options...");

		this->database.setCacheSize(this->config.generalCacheSize);
		this->database.setMaxBatchSize(this->config.generalMaxBatchSize);
		this->database.setReparse(this->config.generalReParse);
		this->database.setParseCustom(this->config.generalParseCustom);
		this->database.setTargetTable(this->config.generalResultTable);
		this->database.setTargetFields(this->config.parsingFieldNames);
		this->database.setSleepOnError(this->config.generalSleepMySql);

		if(this->config.generalDbTimeOut > 0) {
			this->database.setTimeOut(this->config.generalDbTimeOut);
		}
	}

	// create table names for locking
	void Thread::setUpTableNames() {
		const std::string urlListTable{
			"crawlserv_"
			+ this->websiteNamespace
			+ "_"
			+ this->urlListNamespace
		};

		this->parsingTable = urlListTable
				+ "_parsing";
		this->targetTable = urlListTable
				+ "_parsed_"
				+ this->config.generalResultTable;
	}

	// initialize target table
	void Thread::setUpTarget() {
		this->setStatusMessage("Initializing target table...");

		this->log(generalLoggingVerbose, "initializes target table...");

		this->database.initTargetTable();
	}

	void Thread::setUpSqlStatements() {
		// prepare SQL statements for parser
		this->setStatusMessage("Preparing SQL statements...");

		this->log(generalLoggingVerbose, "prepares SQL statements...");

		this->database.prepare();
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

	// check parsing table
	void Thread::checkParsingTable() {
		// wait for parsing table lock
		this->setStatusMessage("Waiting for parsing table...");

		this->log(
				generalLoggingVerbose,
				"waits for parsing table..."
		);

		DatabaseLock(
				this->database,
				"parsingTable." + this->parsingTable,
				[this]() {
					return this->isRunning();
				}
		);

		// cancel if not running anymore
		if(!(this->isRunning())) {
			return;
		}

		// check parsing table
		this->setStatusMessage("Checking parsing table...");

		this->log(
				generalLoggingVerbose,
				"checks parsing table..."
		);

		const auto deleted{this->database.checkParsingTable()};

		// log deleted URL locks if necessary
		if(this->isLogLevel(generalLoggingDefault)) {
			switch(deleted) {
			case 0:
				break;

			case 1:
				this->log(
						generalLoggingDefault,
						"WARNING: Deleted a duplicate URL lock."
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

	void Thread::ready() {
		this->setStatusMessage("Ready.");

		this->log(generalLoggingExtended, "is ready.");
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
	 * QUERY FUNCTIONS (private)
	 */

	// initialize queries, throws Thread::Exception
	void Thread::initQueries() {
		try {
			this->addQueries(
					this->config.generalSkip,
					this->queriesSkip
			);
			this->addQueries(
					this->config.parsingContentIgnoreQueries,
					this->queriesContentIgnore
			);

			/*
			 * NOTE: The following queries need to be added even if they are of type 'none'
			 * 		  as their index needs to correspond to other options.
			 */
			this->addQueriesTo(
					this->config.parsingIdQueries,
					this->queriesId
			);
			this->addQueriesTo(
					this->config.parsingDateTimeQueries,
					this->queriesDateTime
			);

			this->addQueriesTo(
					"field",
					this->config.parsingFieldNames,
					this->config.parsingFieldQueries,
					this->queriesFields
			);
		}
		catch(const QueryException& e) {
			throw Exception(
					"Parser::Thread::initQueries(): "
					+ std::string(e.view())
			);
		}

		// check whether the ID is exclusively parsed from the URL
		this->idFromUrlOnly = std::find(
				this->config.parsingIdSources.cbegin(),
				this->config.parsingIdSources.cend(),
				parsingSourceContent
		) == this->config.parsingIdSources.cend();
	}

	// delete queries
	inline void Thread::deleteQueries() {
		queriesSkip.clear();
		queriesContentIgnore.clear();
		queriesId.clear();
		queriesDateTime.clear();
		queriesFields.clear();
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
	) {
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
	}

	/*
	 * PARSING FUNCTIONS (private)
	 */

	// URL selection
	void Thread::parsingUrlSelection() {
		Timer::Simple timer;

		// get number of URLs
		this->total = this->database.getNumberOfUrls();

		this->setStatusMessage("Fetching URLs...");

		// fill cache with next URLs
		this->log(generalLoggingExtended, "fetches URLs...");

		// get next URL(s)
		this->parsingFetchUrls();

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
			// no more URLs to parse
			if(!(this->idle)) {
				this->log(
						generalLoggingExtended,
						"finished."
				);

				this->setStatusMessage(
						"IDLE Waiting for new content to parse."
				);

				this->setProgress(1.F);
			}

			return;
		}

		// reset idling status
		this->idle = false;
	}

	// fetch next URLs from database
	void Thread::parsingFetchUrls() {
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

		const auto posFirst{this->database.getUrlPosition(this->idFirst)};

		this->posFirstF = static_cast<float>(posFirst);
		this->posDist = this->database.getUrlPosition(this->urls.back().first) - posFirst;
	}

	// check whether next URL(s) ought to be skipped
	void Thread::parsingCheckUrls() {
		std::queue<std::string> queryWarnings;

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
				this->parsingUrlFinished(false);

				continue;
			}

			// check whether URL needs to be skipped because of query
			bool skip{false};

			if(!(this->config.generalSkip.empty())) {
				// loop over skip queries
				for(const auto& query : this->queriesSkip) {
					if(
							this->getBoolFromRegEx(
									query,
									urls.front().second,
									skip,
									queryWarnings
							)
							&& skip
					) {
						break;
					}
				}

				// log warnings if necessary
				this->log(generalLoggingDefault, queryWarnings);

				if(skip) {
					// skip URL because of query
					this->log(
							generalLoggingExtended,
							"skips (query) "
							+ this->urls.front().second
					);

					// unlock URL if necessary
					this->database.unLockUrlIfOk(this->urls.front().first, this->cacheLockTime);

					// finish skipped URL
					this->parsingUrlFinished(false);

					continue;
				}
			}

			// found URL to process
			break;
		} // end of loop over URLs in cache

		// log warnings if necessary
		this->log(generalLoggingDefault, queryWarnings);
	}

	// parse URL and content(s) of next URL, return number of successfully parsed contents
	std::size_t Thread::parsingNext() {
		std::queue<std::string> queryWarnings;
		std::string parsedId;

		if(this->idFromUrlOnly) {
			// parse ID from URL only
			for(const auto& query : this->queriesId) {
				// parse ID by running RegEx on URL
				if(
						this->getSingleFromRegEx(
								query,
								this->urls.front().second,
								parsedId,
								queryWarnings
						) && !parsedId.empty()
				) {
					// ID has been found
					break;
				}
			}

			// write warnings to log if necessary
			this->log(generalLoggingDefault, queryWarnings);

			// check ID
			if(parsedId.empty()) {
				return 0;
			}

			if(
					!(this->config.parsingIdIgnore.empty())
					&& std::find(
							this->config.parsingIdIgnore.cbegin(),
							this->config.parsingIdIgnore.cend(),
							parsedId
					) != this->config.parsingIdIgnore.cend()
			) {
				// ignore ID
				return 0;
			}
		}

		if(this->config.generalNewestOnly) {
			// parse newest content of URL
			std::size_t numberOfContents{};
			std::string lastDateTime{};
			std::uint64_t contentCounter{};
			std::uint8_t statusCounter{};
			bool changedStatus{false};

			while(this->isRunning()) {
				IdString latestContent;

				if(
						this->database.getLatestContent(
								this->urls.front().first,
								lastDateTime,
								latestContent,
								lastDateTime
						)
				) {
					// check whether thread is still running
					if(!(this->isRunning())) {
						break;
					}

					if(this->parsingContent(latestContent, parsedId)) {
						if(changedStatus) {
							this->setStatusMessage(this->urls.front().second);
						}

						return 1;
					}

					++contentCounter;
					++statusCounter;

					if(statusCounter == updateContentCounterEvery) {
						if(numberOfContents == 0) {
							numberOfContents = this->database.getNumberOfContents(
									this->urls.front().first
							);
						}

						statusCounter = 0;

						std::ostringstream statusStrStr;

						statusStrStr.imbue(std::locale(""));

						statusStrStr	<< "["
										<< contentCounter
										<< "/"
										<< numberOfContents
										<< "] "
										<< this->urls.front().second;

						this->setStatusMessage(statusStrStr.str());

						changedStatus = true;
					}
				}
				else {
					break;
				}
			}

			if(changedStatus) {
				this->setStatusMessage(this->urls.front().second);
			}
		}
		else {
			// parse all contents of URL
			std::size_t counter{};

			auto contents{
				this->database.getAllContents(this->urls.front().first)
			};

			while(this->isRunning() && !contents.empty()) {
				if(this->parsingContent(contents.front(), parsedId)) {
					++counter;
				}

				contents.pop();
			}

			return counter;
		}

		return 0;
	}

	// parse ID-specific content, return whether parsing was successfull (i.e. an ID could be parsed)
	bool Thread::parsingContent(const IdString& content, std::string_view parsedId) {
		DataEntry parsedData(content.first);
		std::queue<std::string> queryWarnings;

		// set content as target for subsequent queries
		this->setQueryTarget(content.second, this->urls.front().second);

		// parse ID (if still necessary)
		if(!(this->idFromUrlOnly)) {
			for(auto it{this->queriesId.cbegin()}; it != this->queriesId.cend(); ++it) {
				// check query source
				const auto index{it - this->queriesId.cbegin()};

				switch(this->config.parsingIdSources.at(index)) {
				case parsingSourceUrl:
					// parse ID by running RegEx query on URL
					this->getSingleFromRegEx(
							*it,
							this->urls.front().second,
							parsedData.dataId,
							queryWarnings
					);

					break;

				case parsingSourceContent:
					// parse ID by running query on content
					this->getSingleFromQuery(*it, parsedData.dataId, queryWarnings);

					break;

				default:
					queryWarnings.emplace("WARNING: Invalid source for ID.");
				}

				if(!parsedData.dataId.empty()) {
					// ID successfully parsed
					break;
				}
			}

			// log warnings if necessary
			this->log(generalLoggingDefault, queryWarnings);
		}
		else {
			parsedData.dataId = parsedId;
		}

		// check whether no ID has been parsed
		if(parsedData.dataId.empty()) {
			// clear query target before continuing to next URL (or finishing up)
			this->clearQueryTarget();

			return false;
		}

		// check whether parsed ID is ought to be ignored
		if(
				std::find(
						this->config.parsingIdIgnore.cbegin(),
						this->config.parsingIdIgnore.cend(),
						parsedData.dataId
				) != this->config.parsingIdIgnore.cend()
		) {
			this->log(
					generalLoggingExtended,
					"ignored parsed ID '"
					+ parsedData.dataId
					+ "' ["
					+ this->urls.front().second
					+ "]."
			);

			// clear query target before continuing to next URL (or finishing up)
			this->clearQueryTarget();

			return false;
		}

		// check whether content is ought to be ignored
		for(const auto& query : this->queriesContentIgnore) {
			bool ignore{false};

			this->getBoolFromQuery(query, ignore, queryWarnings);

			if(ignore) {
				this->log(
						generalLoggingExtended,
						"ignored because of query on content ["
						+ this->urls.front().second
						+ "]."
				);

				// clear query target before continuing to next URL (or finishing up)
				this->clearQueryTarget();

				return false;
			}
		}

		// check whether content with the parsed ID already exists and the current one differs from the one in the database
		const auto contentId{
			this->database.getContentIdFromParsedId(parsedData.dataId)
		};
		bool duplicateInCache{false};

		if(contentId == 0) {
			// copy the queue and checking the cached results
			auto cachedResults{this->results};

			while(!cachedResults.empty()) {
				if(cachedResults.front().dataId == parsedData.dataId) {
					duplicateInCache = true;

					break;
				}

				cachedResults.pop();
			}
		}

		if((contentId > 0 && contentId != content.first) || duplicateInCache) {
			this->log(
					generalLoggingDefault,
					"skipped content with already existing ID '"
					+ parsedData.dataId
					+ "' ["
					+ this->urls.front().second
					+ "]."
			);

			// clear query target before continuing to next URL (or finishing up)
			this->clearQueryTarget();

			return false;
		}

		// parse date/time
		for(
				auto it{this->queriesDateTime.cbegin()};
				it != this->queriesDateTime.cend();
				++it
		) {
			const auto index{it - this->queriesDateTime.cbegin()};

			// check query source
			switch(this->config.parsingDateTimeSources.at(index)) {
			case parsingSourceUrl:
				// parse date/time by running RegEx query on URL
				this->getSingleFromRegEx(
						*it,
						this->urls.front().second,
						parsedData.dateTime,
						queryWarnings
				);

				break;

			case parsingSourceContent:
				// parse date/time by running query on content
				this->getSingleFromQuery(
						*it,
						parsedData.dateTime,
						queryWarnings
				);

				break;

			default:
				this->log(
						generalLoggingDefault,
						"WARNING: Invalid source for date/time ignored."
				);

				continue;
			}

			// log warnings if necessary
			this->log(generalLoggingDefault, queryWarnings);

			if(!parsedData.dateTime.empty()) {
				// found date/time: try to convert it to SQL time stamp
				auto format{this->config.parsingDateTimeFormats.at(index)};

				// use "%F %T" as default date/time format
				if(format.empty()) {
					format = "%F %T";
				}

				try {
					Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
							parsedData.dateTime,
							format,
							this->config.parsingDateTimeLocales.at(index)
					);
				}
				catch(const LocaleException& e) {
					std::string logString{e.view()};

					logString += " - locale ignored.";

					this->log(generalLoggingDefault, logString);

					try {
						Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								parsedData.dateTime,
								format
						);
					}
					catch(const DateTimeException& e2) {
						logString = e2.view();

						logString += " - date/time query skipped [";
						logString += this->urls.front().second;
						logString += "]";

						this->log(generalLoggingExtended, logString);

						parsedData.dateTime.clear();
					}
				}
				catch(const DateTimeException& e) {
					std::string logString{e.view()};

					logString += " - date/time query skipped [";
					logString += this->urls.front().second;
					logString += "]";

					this->log(generalLoggingExtended, logString);

					parsedData.dateTime.clear();
				}

				if(!parsedData.dateTime.empty()) {
					// date/time successfully parsed and converted
					break;
				}
			}
		}

		// warn about empty date/time if necessary
		if(
				this->config.parsingDateTimeWarningEmpty
				&& parsedData.dateTime.empty()
				&& !(this->queriesDateTime.empty())
		) {
			this->log(
					generalLoggingDefault,
					"WARNING: date/time is empty for "
					+ this->urls.front().second
			);
		}

		// parse custom fields
		parsedData.fields.reserve(this->queriesFields.size());

		for(
				auto it{this->queriesFields.cbegin()};
				it != this->queriesFields.cend();
				++it
		) {
			const auto index{it - this->queriesFields.cbegin()};
			const auto dateTimeFormat{this->config.parsingFieldDateTimeFormats.at(index)};

			// determinate whether to get all or just the first match (as string or boolean value) from the query result
			if(it->resultMulti) {
				// parse multiple elements
				std::vector<std::string> parsedFieldValues;

				// check query source
				switch(this->config.parsingFieldSources.at(index)) {
				case parsingSourceUrl:
					// parse field values by using RegEx query on URL
					this->getMultiFromRegEx(
							*it,
							this->urls.front().second,
							parsedFieldValues,
							queryWarnings
					);

					break;

				case parsingSourceContent:
					// parse field values by using query on content
					this->getMultiFromQuery(
							*it,
							parsedFieldValues,
							queryWarnings
					);

					break;

				default:
					this->log(
							generalLoggingDefault,
							"WARNING: Invalid source type for field ignored."
					);

					continue;
				}

				// log warnings if necessary
				this->log(generalLoggingDefault, queryWarnings);

				// if necessary, try to convert the parsed values to date/times
				if(!dateTimeFormat.empty()) {
					for(auto& value : parsedFieldValues) {
						try {
							Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
									value,
									dateTimeFormat,
									this->config.parsingFieldDateTimeLocales.at(index)
							);
						}
						catch(const LocaleException& e) {
							try {
								this->parsingFieldWarning(
										e.view(),
										this->config.parsingFieldNames.at(index),
										this->urls.front().second
								);

								Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
										value,
										dateTimeFormat
								);
							}
							catch(const DateTimeException& e2) {
								this->parsingFieldWarning(
										e2.view(),
										this->config.parsingFieldNames.at(index),
										this->urls.front().second
								);

								value.clear();
							}
						}
						catch(const DateTimeException& e) {
							this->parsingFieldWarning(
									e.view(),
									this->config.parsingFieldNames.at(index),
									this->urls.front().second
							);

							value.clear();
						}
					}
				}

				// if necessary, check whether array or all values are empty
				if(this->config.parsingFieldWarningsEmpty.at(index)) {
					if(
							std::find_if(
									parsedFieldValues.cbegin(),
									parsedFieldValues.cend(),
									[](auto const& value) {
										return !value.empty();
									}
							) == parsedFieldValues.cend()
					) {
						this->log(
								generalLoggingDefault,
								"WARNING: '"
								+ this->config.parsingFieldNames.at(index) + "'"
								" is empty for "
								+ this->urls.front().second
						);
					}
				}

				// determine how to save result: JSON array or concatenate using delimiting character
				if(this->config.parsingFieldJSON.at(index)) {
					// if necessary, tidy texts
					if(
							dateTimeFormat.empty()
							&& this->config.parsingFieldTidyTexts.at(index)
					) {
						for(auto& value : parsedFieldValues) {
							Helper::Strings::utfTidy(value);
						}
					}

					// stringify and add parsed elements as JSON array
					parsedData.fields.emplace_back(
							Helper::Json::stringify(parsedFieldValues)
					);
				}
				else {
					// concatenate elements
					auto result{
						Helper::Strings::join(
								parsedFieldValues,
								this->config.parsingFieldDelimiters.at(index),
								this->config.parsingFieldIgnoreEmpty.at(index)
						)
					};

					// if necessary, tidy text
					if(
							dateTimeFormat.empty()
							&& this->config.parsingFieldTidyTexts.at(index)
					) {
						Helper::Strings::utfTidy(result);
					}

					parsedData.fields.emplace_back(result);
				}
			}
			else if(it->resultSingle) {
				// parse first element only (as string)
				std::string parsedFieldValue;

				// check query source
				switch(this->config.parsingFieldSources.at(index)) {
				case parsingSourceUrl:
					// parse single field value by using RegEx query on URL
					this->getSingleFromRegEx(
							*it,
							this->urls.front().second,
							parsedFieldValue,
							queryWarnings
					);

					break;

				case parsingSourceContent:
					// parse single field value by using query on content
					this->getSingleFromQuery(*it, parsedFieldValue, queryWarnings);

					break;

				default:
					this->log(
							generalLoggingDefault,
							"WARNING: Invalid source type for field ignored."
					);

					continue;
				}

				// log warnings if necessary
				this->log(generalLoggingDefault, queryWarnings);

				// if necessary, try to convert the parsed value to date/time
				if(!dateTimeFormat.empty()) {
					try {
						Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								parsedFieldValue,
								dateTimeFormat,
								this->config.parsingFieldDateTimeLocales.at(index)
						);
					}
					catch(const LocaleException& e) {
						try {
							this->parsingFieldWarning(
									e.view(),
									this->config.parsingFieldNames.at(index),
									this->urls.front().second
							);

							Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
									parsedFieldValue,
									dateTimeFormat
							);
						}
						catch(const DateTimeException& e2) {
							this->parsingFieldWarning(
									e2.view(),
									this->config.parsingFieldNames.at(index),
									this->urls.front().second
							);

							parsedFieldValue.clear();
						}
					}
					catch(const DateTimeException& e) {
						this->parsingFieldWarning(
								e.view(),
								this->config.parsingFieldNames.at(index),
								this->urls.front().second
						);

						parsedFieldValue.clear();
					}
				}

				// if necessary, check whether value is empty
				if(
						this->config.parsingFieldWarningsEmpty.at(index)
						&& parsedFieldValue.empty()
				) {
					std::string logString{"WARNING: '"};

					logString += this->config.parsingFieldNames.at(index);
					logString += "' is empty for ";
					logString += this->urls.front().second;

					this->log(generalLoggingDefault, logString);
				}

				// if necessary, tidy text
				if(
						dateTimeFormat.empty()
						&& this->config.parsingFieldTidyTexts.at(index)
				) {
					Helper::Strings::utfTidy(parsedFieldValue);
				}

				// determine how to save result: JSON array or string as is
				if(this->config.parsingFieldJSON.at(index)) {
					// stringify and add parsed element as JSON array with one element
					parsedData.fields.emplace_back(
							Helper::Json::stringify(parsedFieldValue)
					);
				}
				else {
					// save as is
					parsedData.fields.emplace_back(parsedFieldValue);
				}
			}
			else if(it->resultBool) {
				// only save whether a match for the query exists
				bool booleanResult{false};

				// check query source
				switch(this->config.parsingFieldSources.at(index)) {
				case parsingSourceUrl:
					// parse boolean field value by using RegEx query on URL
					this->getBoolFromRegEx(
							*it,
							this->urls.front().second,
							booleanResult,
							queryWarnings
					);

					break;

				case parsingSourceContent:
					// parse boolean field value by using query on content
					this->getBoolFromQuery(*it, booleanResult, queryWarnings);

					break;

				default:
					this->log(
							generalLoggingDefault,
							"WARNING: Invalid source type for field ignored."
					);

					continue;
				}

				// log warnings if necessary
				this->log(generalLoggingDefault, queryWarnings);

				// date/time conversion is not possible for boolean values
				if(!dateTimeFormat.empty()) {
					std::string logString{
						"WARNING: Cannot convert boolean value for field '"
					};

					logString += this->config.parsingFieldNames.at(index);
					logString += "' to date/time' [";
					logString += this->urls.front().second;
					logString += "]";

					this->log(generalLoggingDefault, logString);
				}

				// determine how to save result: JSON array or boolean value as string
				if(this->config.parsingFieldJSON.at(index)) {
					// stringify and add parsed element as JSON array with one boolean value as string
					parsedData.fields.emplace_back(
							Helper::Json::stringify(
									booleanResult ? std::string("true") : std::string("false")
							)
					);
				}
				else {
					// save boolean value as string
					parsedData.fields.emplace_back(booleanResult ? "true" : "false");
				}
			}
			else {
				if(it->type != QueryStruct::typeNone) {
					this->log(
							generalLoggingDefault,
							"WARNING: Ignored query for '"
							+ this->config.parsingFieldNames.at(index)
							+ "' without specified result type."
					);
				}

				parsedData.fields.emplace_back();
			}
		}

		// clear query target before continuing to next URL (or finishing up)
		this->clearQueryTarget();

		// add parsed data to results
		this->results.push(parsedData);

		return true;
	}

	// URL has been processed (skipped or parsed)
	void Thread::parsingUrlFinished(bool success) {
		// check whether the finished URL is the last URL in the cache
		if(this->urls.size() == 1) {
			// if yes, save results to database
			this->parsingSaveResults(false);

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

	// save results to database
	void Thread::parsingSaveResults(bool warped) {
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
		const auto status{this->getStatusMessage()};

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

	// log error when parsing specific field as warning
	inline void Thread::parsingFieldWarning(
			std::string_view error,
			std::string_view fieldName,
			std::string_view url
	) {
		std::string logString{"WARNING: "};

		logString += error;
		logString += " for field '";
		logString += fieldName;
		logString += "' [";
		logString += url;
		logString += "]";

		this->log(generalLoggingDefault, logString);
	}

	/*
	 *  shadowing functions not to be used by thread (private)
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

} /* namespace crawlservpp::Module::Parser */
