/*
 *
 * ---
 *
 *  Copyright (C) 2019-2020 Anselm Schmidt (ans[ät]ohai.su)
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

namespace crawlservpp::Module::Extractor {

	// constructor A: run previously interrupted extractor
	Thread::Thread(
			Main::Database& dbBase,
			const std::string& cookieDirectory,
			const ThreadOptions& threadOptions,
			const NetworkSettings& serverNetworkSettings,
			const ThreadStatus& threadStatus
	)		:	Module::Thread(
						dbBase,
						threadOptions,
						threadStatus
				),
				database(this->Module::Thread::database),
				networking(cookieDirectory, serverNetworkSettings),
				torControl(
						serverNetworkSettings.torControlServer,
						serverNetworkSettings.torControlPort,
						serverNetworkSettings.torControlPassword
				),
				tickCounter(0),
				startTime(std::chrono::steady_clock::time_point::min()),
				pauseTime(std::chrono::steady_clock::time_point::min()),
				idleTime(std::chrono::steady_clock::time_point::min()),
				idle(false),
				lastUrl(0),
				idFirst(0),
				idDist(0),
				posFirstF(0.),
				posDist(0),
				total(0) {}

	// constructor B: start a new extractor
	Thread::Thread(
			Main::Database& dbBase,
			const std::string& cookieDirectory,
			const ThreadOptions& threadOptions,
			const NetworkSettings& serverNetworkSettings
	)		:	Module::Thread(dbBase, threadOptions),
				database(this->Module::Thread::database),
				networking(cookieDirectory, serverNetworkSettings),
				torControl(
						serverNetworkSettings.torControlServer,
						serverNetworkSettings.torControlPort,
						serverNetworkSettings.torControlPassword
				),
				tickCounter(0),
				startTime(std::chrono::steady_clock::time_point::min()),
				pauseTime(std::chrono::steady_clock::time_point::min()),
				idleTime(std::chrono::steady_clock::time_point::min()),
				idle(false),
				lastUrl(0),
				idFirst(0),
				idDist(0),
				posFirstF(0.),
				posDist(0),
				total(0) {}

	// destructor stub
	Thread::~Thread() {}

	// initialize extractor
	void Thread::onInit() {
		std::queue<std::string> configWarnings;
		std::vector<std::string> fields;

		// load configuration
		this->setStatusMessage("Loading configuration...");

		this->loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);

		// show warnings if necessary
		while(!configWarnings.empty()) {
			this->log(Config::generalLoggingDefault, "WARNING: " + configWarnings.front());

			configWarnings.pop();
		}

		// check required queries
		if(this->config.extractingDataSetQueries.empty())
			throw Exception("Extractor::Thread::onInit(): No dataset extraction query specified");

		if(this->config.extractingIdQueries.empty())
			throw Exception("Extractor::Thread::onInit(): No ID extraction query specified");

		// set query container options
		this->setRepairCData(this->config.extractingRepairCData);
		this->setRepairComments(this->config.extractingRepairComments);
		this->setMinimizeMemory(this->config.generalMinimizeMemory);
		this->setTidyErrorsAndWarnings(this->config.generalTidyErrors, this->config.generalTidyWarnings);

		// set database options
		this->setStatusMessage("Setting database options...");

		this->database.setLogging(
				this->config.generalLogging,
				Config::generalLoggingDefault,
				Config::generalLoggingVerbose
		);

		this->log(Config::generalLoggingVerbose, "sets database options...");

		this->database.setCacheSize(this->config.generalCacheSize);
		this->database.setReextract(this->config.generalReExtract);
		this->database.setExtractCustom(this->config.generalExtractCustom);
		this->database.setTargetTable(this->config.generalResultTable);
		this->database.setTargetFields(this->config.extractingFieldNames);
		this->database.setOverwrite(this->config.extractingOverwrite);
		this->database.setSleepOnError(this->config.generalSleepMySql);

		this->database.setRawContentIsSource(
				std::find(
						this->config.variablesSource.begin(),
						this->config.variablesSource.end(),
						static_cast<unsigned char>(Config::variablesSourcesContent)
				) != this->config.variablesSource.end()
		);

		// set sources
		std::queue<StringString> sources;

		for(size_t n = 0; n < this->config.variablesName.size(); ++n)
			if(this->config.variablesSource.at(n) == Config::variablesSourcesParsed) {
				if(
						this->config.variablesParsedColumn.at(n) == "id"
						|| this->config.variablesParsedColumn.at(n) == "datetime"
				)
					sources.push(
							StringString(
									this->config.variablesParsedTable.at(n),
									"parsed_" + this->config.variablesParsedColumn.at(n)
							)
					);
				else
					sources.push(
							StringString(
									this->config.variablesParsedTable.at(n),
									"parsed__" + this->config.variablesParsedColumn.at(n)
							)
					);
			}

		this->database.setSources(sources);

		// create table names for locking
		const std::string urlListTable(
				"crawlserv_"
				+ this->websiteNamespace
				+ "_" + this->urlListNamespace
		);

		this->extractingTable = urlListTable + "_extracting";
		this->targetTable = urlListTable + "_extracted_" + this->config.generalResultTable;

		// initialize target table
		this->setStatusMessage("Initializing target table...");

		this->log(Config::generalLoggingVerbose, "initializes target table...");

		this->database.initTargetTable();

		// prepare SQL statements for extractor
		this->setStatusMessage("Preparing SQL statements...");

		this->log(Config::generalLoggingVerbose, "prepares SQL statements...");

		this->database.prepare();

		// set network configuration
		this->setStatusMessage("Setting network configuration...");

		this->log(Config::generalLoggingVerbose, "sets network configuration...");

		this->networking.setConfigGlobal(*this, false, configWarnings);

		while(!configWarnings.empty()) {
			this->log(Config::generalLoggingDefault, "WARNING: " + configWarnings.front());

			configWarnings.pop();
		}

		if(this->resetTorAfter)
			this->torControl.setNewIdentityTimer(resetTorAfter);

		// initialize queries
		this->setStatusMessage("Initializing custom queries...");

		this->log(Config::generalLoggingVerbose, "initializes custom queries...");

		this->initQueries();

		{
			// wait for extracting table lock
			this->setStatusMessage("Waiting for extracting table...");

			this->log(Config::generalLoggingVerbose, "waits for extracting table...");

			DatabaseLock(
					this->database,
					"extractingTable." + this->extractingTable,
					std::bind(&Thread::isRunning, this)
			);

			if(!(this->isRunning()))
				return;

			// check extracting table
			this->setStatusMessage("Checking extracting table...");

			this->log(Config::generalLoggingVerbose, "checks extracting table...");

			const unsigned int deleted = this->database.checkExtractingTable();

			switch(deleted) {
			case 0:
				break;

			case 1:
				this->log(Config::generalLoggingDefault, "WARNING: Deleted a duplicate URL lock.");

				break;

			default:
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr << "WARNING: Deleted " << deleted << " duplicate URL locks!";

				this->log(Config::generalLoggingDefault, logStrStr.str());
			}
		}

		// save start time and initialize counter
		this->startTime = std::chrono::steady_clock::now();
		this->pauseTime = std::chrono::steady_clock::time_point::min();

		this->tickCounter = 0;

		// extractor is ready
		this->log(Config::generalLoggingExtended, "is ready.");
	}

	// extractor tick, throws Thread::Exception
	void Thread::onTick() {
		bool skip = false;

		// check whether a new TOR identity needs to be requested
		if(this->torControl)
			this->torControl.tick();

		// check for jump in last ID ("time travel")
		long warpedOver = this->getWarpedOverAndReset();

		if(warpedOver != 0) {
			// save cached results
			this->extractingSaveResults(true);

			// unlock and discard old URLs
			this->database.unLockUrlsIfOk(this->urls, this->cacheLockTime);

			// overwrite last URL ID
			this->lastUrl = this->getLast();

			// adjust tick counter
			this->tickCounter += warpedOver;
		}

		// URL selection if no URLs are left to extract
		if(this->urls.empty())
			this->extractingUrlSelection();

		if(this->urls.empty()) {
			// no URLs left in database: set timer if just finished and sleep
			if(this->idleTime == std::chrono::steady_clock::time_point::min())
				this->idleTime = std::chrono::steady_clock::now();

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
		if(this->urls.empty())
			return;

		// write log entry if necessary
		this->log(Config::generalLoggingExtended, "extracts data for " + this->urls.front().second + "...");

		// try to renew URL lock
		this->lockTime = this->database.renewUrlLockIfOk(
				this->urls.front().first,
				this->cacheLockTime,
				this->config.generalLock
		);

		skip = this->lockTime.empty();

		if(skip)
			// skip locked URL
			this->log(Config::generalLoggingExtended, "skips (locked) " + this->urls.front().second);
		else {
			// set status
			this->setStatusMessage(this->urls.front().second);

			// approximate progress
			if(!(this->total))
				throw Exception("Extractor::Thread::onTick(): Could not get URL list size");

			if(this->idDist) {
				const float cacheProgress = static_cast<float>(this->urls.front().first - this->idFirst) / this->idDist;
					// cache progress = (current ID - first ID) / (last ID - first ID)

				const float approxPosition = this->posFirstF + cacheProgress * this->posDist;
					// approximate position = first position + cache progress * (last position - first position)

				this->setProgress(approxPosition / this->total);
			}
			else if(this->total)
				this->setProgress(this->posFirstF / this->total);

			// start timer
			Timer::Simple timer;
			std::string timerStr;

			// extract from content
			const auto extracted = this->extractingNext();

			// clear ID cache
			this->ids.clear();

			// save expiration time of URL lock if extracting was successful or unlock URL if extracting failed
			if(extracted)
				this->finished.emplace(this->urls.front().first, this->lockTime);
			else
				// unlock URL if necesssary
				this->database.unLockUrlIfOk(this->urls.front().first, this->lockTime);

			// stop timer
			if(this->config.generalTiming)
				timerStr = timer.tickStr();

			// reset lock time
			this->lockTime = "";

			// write to log if necessary
			std::ostringstream logStrStr;

			switch(extracted) {
			case 0:
				logStrStr << "skipped ";

				break;

			case 1:
				logStrStr << "extracted one dataset from ";

				break;

			default:
				logStrStr.imbue(std::locale(""));

				logStrStr << "extracted " << extracted << " datasets from ";
			}

			logStrStr << this->urls.front().second;

			if(this->config.generalTiming)
				logStrStr << " in " << timerStr;

			this->log(
					this->config.generalTiming ? Config::generalLoggingDefault : Config::generalLoggingExtended,
					logStrStr.str()
			);
		}

		// URL finished
		this->extractingUrlFinished();
	}

	// extractor paused
	void Thread::onPause() {
		// save pause start time
		this->pauseTime = std::chrono::steady_clock::now();

		// save results if necessary
		this->extractingSaveResults(false);
	}

	// extractor unpaused
	void Thread::onUnpause() {
		// add pause time to start or idle time to ignore pause
		if(this->idleTime > std::chrono::steady_clock::time_point::min())
			this->idleTime += std::chrono::steady_clock::now() - this->pauseTime;
		else
			this->startTime += std::chrono::steady_clock::now() - this->pauseTime;

		this->pauseTime = std::chrono::steady_clock::time_point::min();
	}

	// clear extractor
	void Thread::onClear() {
		// check counter and process timers
		if(this->tickCounter) {
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

			const long double tps =
					static_cast<long double>(this->tickCounter) /
					std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::steady_clock::now()
							- this->startTime
					).count();

			tpsStrStr.imbue(std::locale(""));

			tpsStrStr << std::setprecision(2) << std::fixed << tps;

			this->log(Config::generalLoggingDefault, "average speed: " + tpsStrStr.str() + " ticks per second.");
		}

		// save results if necessary
		this->extractingSaveResults(false);

		// save status message
		const std::string oldStatus(this->getStatusMessage());

		// set status message
		this->setStatusMessage("Finishing up...");

		// unlock remaining URLs
		this->database.unLockUrlsIfOk(this->urls, this->cacheLockTime);

		// delete queries
		this->queriesDateTime.clear();
		this->queriesFields.clear();
		this->queriesId.clear();

		this->clearQueries();

		// restore previous status message
		this->setStatusMessage(oldStatus);
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

	// initialize queries, throws Thread::Exception
	void Thread::initQueries() {
		// reserve memory for queries
		this->queriesTokens.reserve(this->config.variablesTokensQuery.size());
		this->queriesErrorFail.reserve(this->config.extractingErrorFail.size());
		this->queriesErrorRetry.reserve(this->config.extractingErrorRetry.size());
		this->queriesDatasets.reserve(this->config.extractingDataSetQueries.size());
		this->queriesId.reserve(this->config.extractingIdQueries.size());
		this->queriesDateTime.reserve(this->config.extractingDateTimeQueries.size());
		this->queriesFields.reserve(this->config.extractingFieldQueries.size());
		this->queriesRecursive.reserve(this->config.extractingRecursive.size());

		this->queriesVariables.reserve(
				std::count_if(
						this->config.variablesSource.begin(),
						this->config.variablesSource.end(),
						[](const unsigned char source) {
							return	source == Config::variablesSourcesContent
									|| source == Config::variablesSourcesUrl;
						}
				)
		);

		try {
			// create queries and get query properties
			for(const auto& query : this->config.extractingErrorFail) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesErrorFail.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.extractingErrorRetry) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesErrorRetry.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.extractingDataSetQueries) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesDatasets.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.extractingIdQueries) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesId.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.extractingRecursive) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesRecursive.emplace_back(this->addQuery(properties));
				}
			}

			// NOTE: The following queries need to be added even if they are of type 'none'
			//			as their index needs to correspond to other options
			for(auto& query : this->config.extractingDateTimeQueries) {
				QueryProperties properties;

				if(query)
					this->database.getQueryProperties(query, properties);

				this->queriesDateTime.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.extractingFieldQueries.begin();
					i != this->config.extractingFieldQueries.end();
					++i
			) {
				QueryProperties properties;

				if(*i)
					this->database.getQueryProperties(*i, properties);
				else {
					const auto& name =
							this->config.extractingFieldNames.at(
									i - this->config.extractingFieldQueries.begin()
							);

					if(!(name.empty()))
						this->log(
								Config::generalLoggingDefault,
								"WARNING: Ignores field \'"
								+ name
								+ "\' because of missing query."
						);
				}

				this->queriesFields.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.variablesTokensQuery.begin();
					i != this->config.variablesTokensQuery.end();
					++i
			) {
				QueryProperties properties;

				if(*i) {
					this->database.getQueryProperties(*i, properties);

					if(
							!properties.resultSingle
							&& !properties.resultBool
					)
						this->log(
								Config::generalLoggingDefault,
								"WARNING: Ignores token \'"
								+ this->config.variablesTokens.at(
										i - this->config.variablesTokensQuery.begin()
								)
								+ "\' because of wrong query result type."
						);
				}
				else {
					const auto& name =
							this->config.variablesTokens.at(
									i - this->config.variablesTokensQuery.begin()
							);

					if(!(name.empty()))
						this->log(
								Config::generalLoggingDefault,
								"WARNING: Ignores token \'"
								+ name
								+ "\' because of missing query."
						);
				}

				this->queriesTokens.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.variablesSource.begin();
					i != this->config.variablesSource.end();
					++i
			) {
				if(
						*i == Config::variablesSourcesContent
						|| *i == Config::variablesSourcesUrl
				) {
					const auto& query =
							this->config.variablesQuery.at(
									i - this->config.variablesSource.begin()
							);

					QueryProperties properties;

					if(query) {
						this->database.getQueryProperties(query, properties);

						if(!properties.resultSingle && !properties.resultBool)
							this->log(
									Config::generalLoggingDefault,
									"WARNING: Ignores variable \'"
									+ this->config.variablesName.at(
											i - this->config.variablesSource.begin()
									)
									+ "\' because of wrong query result type."
							);
						else if(
								*i == Config::variablesSourcesUrl
								&& !properties.type.empty()
								&& properties.type != "regex"
						)
							this->log(
									Config::generalLoggingDefault,
									"WARNING: Ignores variable \'"
									+ this->config.variablesName.at(
											i - this->config.variablesSource.begin()
									)
									+ "\' because of wrong query type for URL."
							);

					}
					else {
						const auto& name =
								this->config.variablesName.at(
										i - this->config.variablesSource.begin()
								);

						if(!(name.empty()))
							this->log(
									Config::generalLoggingDefault,
									"WARNING: Ignores variable \'"
									+ name
									+ "\' because of missing query."
							);
					}

					this->queriesVariables.emplace_back(this->addQuery(properties));
				}
			}

			if(this->config.pagingIsNextFrom) {
				QueryProperties properties;

				this->database.getQueryProperties(this->config.pagingIsNextFrom, properties);

				this->queryPagingIsNextFrom = this->addQuery(properties);
			}

			if(this->config.pagingNextFrom) {
				QueryProperties properties;

				this->database.getQueryProperties(this->config.pagingNextFrom, properties);

				this->queryPagingNextFrom = this->addQuery(properties);
			}

			if(this->config.pagingNumberFrom) {
				QueryProperties properties;

				this->database.getQueryProperties(this->config.pagingNumberFrom, properties);

				this->queryPagingNumberFrom = this->addQuery(properties);
			}

			if(this->config.expectedQuery) {
				QueryProperties properties;

				this->database.getQueryProperties(this->config.expectedQuery, properties);

				this->queryExpected = this->addQuery(properties);
			}
		}
		catch(const QueryException& e) {
			throw Exception("Extractor::Thread::initQueries(): " + e.whatStr());
		}
	}

	// URL selection
	void Thread::extractingUrlSelection() {
		Timer::Simple timer;

		// get number of URLs
		this->total = this->database.getNumberOfUrls();

		this->setStatusMessage("Fetching URLs...");

		// fill cache with next URLs
		this->log(Config::generalLoggingExtended, "fetches URLs...");

		// get next URL(s)
		this->extractingFetchUrls();

		// write to log if necessary
		if(this->config.generalTiming)
			this->log(Config::generalLoggingDefault, "fetched URLs in " + timer.tickStr());

		// update status
		this->setStatusMessage("Checking URLs...");

		// check whether URLs have been fetched
		if(this->urls.empty()) {
			// no more URLs to extract data from
			if(!(this->idle)) {
				this->log(Config::generalLoggingExtended, "finished.");

				this->setStatusMessage("IDLE Waiting for new URLs to extract data from.");
				this->setProgress(1.f);
			}

			return;
		}
		else // reset idling status
			this->idle = false;
	}

	// fetch next URLs from database
	void Thread::extractingFetchUrls() {
		// fetch URLs from database to cache
		this->cacheLockTime = this->database.fetchUrls(this->getLast(), this->urls, this->config.generalLock);

		// check whether URLs have been fetched
		if(this->urls.empty())
			return;

		// save properties of fetched URLs and URL list for progress calculation
		this->idFirst = this->urls.front().first;
		this->idDist = this->urls.back().first - this->idFirst;

		const auto posFirst = this->database.getUrlPosition(this->idFirst);

		this->posFirstF = static_cast<float>(posFirst);
		this->posDist = this->database.getUrlPosition(this->urls.back().first) - posFirst;
	}

	// check whether next URL(s) ought to be skipped
	void Thread::extractingCheckUrls() {
		// loop over next URLs in cache
		while(!(this->urls.empty()) && this->isRunning()) {
			// check whether URL needs to be skipped because of invalid ID
			if(!(this->urls.front().first)) {
				this->log(Config::generalLoggingDefault, "skips (INVALID ID) " + this->urls.front().second);

				// unlock URL if necessary
				this->database.unLockUrlIfOk(this->urls.front().first, this->cacheLockTime);

				// finish skipped URL
				this->extractingUrlFinished();

				continue;
			}

			break; // found URL to process
		} // end of loop over URLs in cache
	}

	// extract data from next URL, return number of extracted datasets
	size_t Thread::extractingNext() {
		std::queue<std::string> queryWarnings;
		size_t expected = 0, extracted = 0;
		bool expecting = false;

		// get datasets
		for(const auto& query : this->queriesDatasets) {
			// reserve memory for subsets if possible
			if(expecting)
				this->reserveForSubSets(query, expected);
		}

		// get content ID and - if necessary - the whole content
		IdString content;

		this->database.getContent(this->urls.front().first, content);

		// set raw crawled content as target for subsequent queries
		this->setQueryTarget(content.second, this->urls.front().second);

		// check content ID
		if(!content.first)
			return 0;

		// get values for variables
		std::vector<StringString> variables;

		this->log(Config::generalLoggingVerbose, "gets values for variables...");

		this->extractingGetVariableValues(variables);

		// get values for global tokens
		this->log(Config::generalLoggingVerbose, "gets values for global tokens...");

		this->extractingGetTokenValues(variables);

		// clear query target
		this->clearQueryTarget();

		// loop over pages
		this->log(Config::generalLoggingVerbose, "loops over pages...");

		std::queue<std::string> pageNames;
		long pageNum = this->config.pagingFirst;
		bool pageFirst = true;
		bool noPageString = this->config.pagingFirstString.empty();
		bool queryTargetSet = false;
		size_t pageCounter = 0;
		size_t pageTotal = 0;

		// add first page
		if(noPageString)
			pageNames.emplace(std::to_string(pageNum));
		else
			pageNames.emplace(this->config.pagingFirstString);

		while(this->isRunning() && !pageNames.empty()) {
			// resolve alias for paging variable
			std::string pageAlias;

			if(this->config.pagingAliasAdd) {
				try {
					pageAlias = std::to_string(
							boost::lexical_cast<long>(pageNames.front())
							+ this->config.pagingAliasAdd
					);
				}
				catch(const boost::bad_lexical_cast& e) {
					this->log(
							Config::generalLoggingDefault,
							"WARNING: Could not create numeric alias \'"
							+ this->config.pagingAlias
							+ "\' for non-numeric variable \'"
							+ this->config.pagingVariable
							+ "\' [= \'"
							+ pageNames.front()
							+ "\']."
					);
				}
			}
			else
				pageAlias = pageNames.front();

			// get page-specific tokens
			std::vector<StringString> pageTokens;

			this->extractingGetPageTokenValues(pageNames.front(), pageTokens, variables);

			// get cookies and custom headers
			std::string cookies(this->config.sourceCookies);
			std::vector<std::string> headers(this->config.sourceHeaders);

			// get source URL
			std::string sourceUrl;

			if(pageFirst) {
				if(this->config.sourceUrlFirst.empty())
					sourceUrl = this->config.sourceUrl;
				else
					sourceUrl = this->config.sourceUrlFirst;
			}
			else
				sourceUrl = this->config.sourceUrl;

			// replace variables, their aliases and tokens
			Helper::Strings::replaceAll(cookies, this->config.pagingVariable, pageNames.front(), true);
			Helper::Strings::replaceAll(cookies, this->config.pagingAlias, pageAlias, true);
			Helper::Strings::replaceAll(sourceUrl, this->config.pagingVariable, pageNames.front(), true);
			Helper::Strings::replaceAll(sourceUrl, this->config.pagingAlias, pageAlias, true);

			for(auto& header : headers) {
				Helper::Strings::replaceAll(header, this->config.pagingVariable, pageNames.front(), true);
				Helper::Strings::replaceAll(header, this->config.pagingAlias, pageAlias, true);
			}

			for(const auto& variable : variables) {
				Helper::Strings::replaceAll(cookies, variable.first, variable.second, true);
				Helper::Strings::replaceAll(sourceUrl, variable.first, variable.second, true);

				for(auto& header : headers)
					Helper::Strings::replaceAll(header, variable.first, variable.second, true);
			}

			for(const auto& token : pageTokens) {
				Helper::Strings::replaceAll(cookies, token.first, token.second, true);
				Helper::Strings::replaceAll(sourceUrl, token.first, token.second, true);

				for(auto& header : headers)
					Helper::Strings::replaceAll(header, token.first, token.second, true);
			}

			// check URL
			if(sourceUrl.empty()) {
				// remove current page from queue
				pageNames.pop();

				continue;	// continue with next page (if one exists)
			}

			// get and check content of current page
			this->log(Config::generalLoggingVerbose, "fetches " + sourceUrl + "...");

			if(!cookies.empty())
				this->log(Config::generalLoggingVerbose, "[cookies] " + cookies);

			for(auto& header : headers)
				if(!header.empty())
					this->log(Config::generalLoggingVerbose, "[header] " + header);

			std::string pageContent;

			this->extractingPageContent(sourceUrl, cookies, headers, pageContent);

			// log progress if necessary
			if(this->config.generalLogging >= Config::generalLoggingExtended) {
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr << "fetched " << pageContent.size() << " byte(s) from " << sourceUrl;

				this->log(Config::generalLoggingExtended, logStrStr.str());
			}

			if(pageContent.empty()) {
				// remove current page from queue
				pageNames.pop();

				continue;	// continue with next page (if one exists)
			}

			// set page content as target for subsequent queries
			this->setQueryTarget(pageContent, sourceUrl);

			queryTargetSet = true;

			// check for an error in the page because of which the page needs to be retried
			if(this->extractingPageIsRetry(queryWarnings)) {
				std::string error;

				if(this->getTarget(error))
					error = "Error in data: " + error;
				else
					error = "Error in data";

				this->extractingReset(error, sourceUrl);

				continue;
			}

			// remove current page from queue
			pageNames.pop();

			// check for first page
			if(pageFirst) {
				// get total number of pages if available
				if(this->queryPagingNumberFrom) {
					std::string pageTotalString;

					// perform query on content to get the number of pages
					this->getSingleFromQuery(this->queryPagingNumberFrom, pageTotalString, queryWarnings);

					// log warnings if necessary
					this->log(Config::generalLoggingDefault, queryWarnings);

					// try to convert number of pages to numeric value
					try {
						pageTotal = boost::lexical_cast<size_t>(pageTotalString);
					}
					catch(const boost::bad_lexical_cast& e) {
						this->log(
								Config::generalLoggingDefault,
								"WARNING: Could convert non-numeric query result \'"
								+ pageTotalString
								+ "to number of pages."
						);
					}

					if(!pageTotal)
						return 0;	// no pages, no data
				}

				// get expected number of datasets if necessary
				if(this->queryExpected) {
					std::string expectedStr;

					this->getSingleFromQuery(this->queryExpected, expectedStr, queryWarnings);

					// log warnings if necessary
					this->log(Config::generalLoggingDefault, queryWarnings);

					// try to convert expected number of datasets
					if(!expectedStr.empty()) {
						try {
							expected = std::stoul(expectedStr);

							expecting = true;
						}
						catch(const std::logic_error& e) {
							this->log(
									Config::generalLoggingDefault,
									"WARNING: \'"
									+ expectedStr
									+ "\' cannot be converted to a numeric value when extracting the expected number of URLs ["
									+ sourceUrl
									+ "]."
							);
						}
					}
				}

				pageFirst = false;
			}

			// extract data from content
			extracted += this->extractingPage(content.first, sourceUrl);

			// check for next page
			bool noLimit = false;

			if(pageTotal) {
				// determine whether next page exists by the extracted total number of pages
				++pageCounter;

				if(pageCounter >= pageTotal)
					break;	// always cancel when maximum number of pages is reached
			}
			else if(this->queryPagingIsNextFrom) {
				// determine whether next page exists by boolean query on page content
				bool isNext = false;

				this->getBoolFromQuery(this->queryPagingIsNextFrom, isNext, queryWarnings);

				// log warnings if necessary
				this->log(Config::generalLoggingDefault, queryWarnings);

				if(!isNext)
					break;	// always cancel when query says that the last page is reached
			}
			else
				noLimit = true;

			// get ID(s) of next pages
			if(this->queryPagingNextFrom) {
				if(this->queryPagingNextFrom.resultMulti) {
					// get possibly multiple IDs by performing query on page content
					std::vector<std::string> pagesToAdd;

					this->getMultiFromQuery(this->queryPagingNextFrom, pagesToAdd, queryWarnings);

					// copy non-empty new ID(s) into page queue
					for(const auto& page : pagesToAdd)
						if(!page.empty())	// add only non-empty pages
							pageNames.push(page);
				}
				else {
					// get possibly one ID by performing query on page content
					std::string page;

					this->getSingleFromQuery(this->queryPagingNextFrom, page, queryWarnings);

					if(!page.empty())
						pageNames.push(page);
				}

				// log warnings if necessary
				this->log(Config::generalLoggingDefault, queryWarnings);


			}
			else if(this->config.pagingStep && noPageString && !noLimit)
				// get ID by incrementing old ID
				pageNum += this->config.pagingStep;

			// clear query target before continuing to next page
			this->clearQueryTarget();

			queryTargetSet = false;
		}

		// clear query target before continuing to next URL (or finish)
		if(queryTargetSet)
			this->clearQueryTarget();

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

				if(this->config.expectedErrorIfSmaller)
					throw Exception(expectedStrStr.str());
				else
					this->log(Config::generalLoggingDefault, "WARNING: " + expectedStrStr.str() + ".");
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
				if(this->config.expectedErrorIfLarger)
					throw Exception(expectedStrStr.str());
				else
					this->log(Config::generalLoggingDefault, "WARNING: " + expectedStrStr.str() + ".");
			}
			else {
				expectedStrStr	<< "number of extracted datasets ["
								<< extracted
								<< "] as expected ["
								<< expected
								<< "] ["
								<< this->urls.front().second
								<< "].";

				this->log(Config::generalLoggingVerbose, expectedStrStr.str());
			}
		}

		return extracted;
	}

	// get values of variables
	void Thread::extractingGetVariableValues(std::vector<StringString>& variables) {
		size_t parsedSource = 0;
		size_t queryCounter = 0;

		// loop over variables (and their aliases)
		for(auto i = this->config.variablesName.begin(); i != this->config.variablesName.end(); ++i) {
			const auto index = i - this->config.variablesName.begin();

			// get value for variable
			std::string value;

			switch(this->config.variablesSource.at(index)) {
			case Config::variablesSourcesParsed:
				this->database.getLatestParsedData(this->urls.front().first, parsedSource, value);

				++parsedSource;

				break;

			case Config::variablesSourcesContent:
				this->extractingGetValueFromContent(
						this->queriesVariables.at(queryCounter),
						value
				);

				++queryCounter;

				break;

			case Config::variablesSourcesUrl:
				this->extractingGetValueFromUrl(
						this->queriesVariables.at(queryCounter),
						value
				);

				++queryCounter;

				break;

			default:
				this->log(
						Config::generalLoggingDefault,
						"WARNING: Invalid source for value of variable \'"
						+ *i
						+ "\'."
				);
			}

			variables.emplace_back(*i, value);

			// get value for alias
			const auto& alias = this->config.variablesAlias.at(index);

			if(!alias.empty()) {
				const auto& aliasAdd = this->config.variablesAliasAdd.at(index);

				if(aliasAdd != 0) {
					// try to add value to variable
					std::string aliasValue;

					try {
						aliasValue = std::to_string(
								boost::lexical_cast<long>(value)
								+ aliasAdd
						);
					}
					catch(const boost::bad_lexical_cast& e) {
						this->log(
								Config::generalLoggingDefault,
								"WARNING: Could not create numeric alias \'"
								+ alias
								+ "\' for non-numeric variable \'"
								+ *i
								+ "\' [= \'"
								+ value
								+ "\']."
						);
					}

					// set variable alias to new value
					variables.emplace_back(alias, aliasValue);
				}
				else
					// set variable alias to same value
					variables.emplace_back(alias, value);
			}
		}
	}

	// get values of global tokens
	void Thread::extractingGetTokenValues(std::vector<StringString>& variables) {
		if(this->config.pagingVariable.empty()) {
			// copy headers
			std::vector<std::string> headers(this->config.variablesTokenHeaders);

			// replace already existing variables in headers
			for(auto& header : headers)
				for(const auto& variable : variables)
					Helper::Strings::replaceAll(header, variable.first, variable.second, true);

			// no paging variable: resolve all tokens
			for(auto i = this->config.variablesTokens.begin(); i != this->config.variablesTokens.end(); ++i) {
				const auto index = i - this->config.variablesTokens.begin();

				// copy source URL and cookies
				std::string source(this->config.variablesTokensSource.at(index));
				std::string cookies(this->config.variablesTokensCookies.at(index));

				// replace already existing variables in source URL and cookies
				for(const auto& variable : variables) {
					Helper::Strings::replaceAll(source, variable.first, variable.second, true);
					Helper::Strings::replaceAll(cookies, variable.first, variable.second, true);
				}

				// add token
				variables.emplace_back(
						*i,
						this->extractingGetTokenValue(
								*i,
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
				std::find_if(
						this->config.variablesTokenHeaders.begin(),
						this->config.variablesTokenHeaders.end(),
						[this](const auto& header) {
							return header.find(this->config.pagingVariable) != std::string::npos;
						}
				) == this->config.variablesTokenHeaders.end()
		) { /* if headers are page-dependent, all tokens are also dependent on the current page */
			// copy headers
			std::vector<std::string> headers(this->config.variablesTokenHeaders);

			// replace already existing variables in headers
			for(auto& header : headers)
				for(const auto& variable : variables)
					Helper::Strings::replaceAll(header, variable.first, variable.second, true);

			// paging variable exists: resolve only page-independent tokens
			for(auto i = this->config.variablesTokens.begin(); i != this->config.variablesTokens.end(); ++i) {
				const auto index = i - this->config.variablesTokens.begin();
				const std::string& sourceRef = this->config.variablesTokensSource.at(index);
				const std::string& cookiesRef = this->config.variablesTokensCookies.at(index);

				if(
						sourceRef.find(this->config.pagingVariable) == std::string::npos
						&& cookiesRef.find(this->config.pagingVariable) == std::string::npos
				) {
					// copy source URL and cookies
					std::string source(sourceRef);
					std::string cookies(cookiesRef);

					// replace already existing variables in source URL and cookies
					for(const auto& variable : variables) {
						Helper::Strings::replaceAll(source, variable.first, variable.second, true);
						Helper::Strings::replaceAll(cookies, variable.first, variable.second, true);
					}

					// get value of variable
					variables.emplace_back(
							*i,
							this->extractingGetTokenValue(
									*i,
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
	}

	// get values of page-specific tokens
	void Thread::extractingGetPageTokenValues(
			const std::string& page,
			std::vector<StringString>& tokens,
			const std::vector<StringString>& variables
	) {
		if(this->config.pagingVariable.empty())
			return;

		// copy headers
		std::vector<std::string> headers(this->config.variablesTokenHeaders);

		// replace variables in headers
		for(auto& header : headers) {
			for(const auto& variable : variables)
				Helper::Strings::replaceAll(header, variable.first, variable.second, true);

			Helper::Strings::replaceAll(header, this->config.pagingVariable, page, true);
		}

		// check whether all tokens are page-specific
		bool allTokens =
				std::find_if(
						headers.begin(),
						headers.end(),
						[this](const auto& header) {
							return header.find(this->config.pagingVariable) != std::string::npos;
						}
				) != headers.end();

		for(auto i = this->config.variablesTokens.begin(); i != this->config.variablesTokens.end(); ++i) {
			const auto index = i - this->config.variablesTokens.begin();
			const auto& sourceRef = this->config.variablesTokensSource.at(index);
			const auto& cookiesRef = this->config.variablesTokensCookies.at(index);

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
					Helper::Strings::replaceAll(source, variable.first, variable.second, true);
					Helper::Strings::replaceAll(cookies, variable.first, variable.second, true);
				}

				Helper::Strings::replaceAll(source, this->config.pagingVariable, page, true);
				Helper::Strings::replaceAll(cookies, this->config.pagingVariable, page, true);

				tokens.emplace_back(
						*i,
						this->extractingGetTokenValue(
								*i,
								source,
								cookies,
								headers,
								this->config.variablesTokensUsePost.at(index),
								this->queriesTokens.at(index)
						)
				);

				this->log(
						Config::generalLoggingVerbose,
						"got token: " + tokens.back().first + "=" + tokens.back().second
				);
			}
		}
	}

	// get value of token
	std::string Thread::extractingGetTokenValue(
			const std::string& name,
			const std::string& source,
			const std::string& cookies,
			const std::vector<std::string>& headers,
			bool usePost,
			const QueryStruct& query
	) {
		// ignore if invalid query is specified
		if(!query.resultBool && !query.resultSingle)
			return "";

		// get content for extracting token
		std::string sourceUrl(source);
		std::string content;
		std::string result;
		bool success = false;

		while(this->isRunning()) {
			try {
				// set local network configuration
				this->networking.setConfigCurrent(*this);

				// set custom headers if necessary
				if(!cookies.empty())
					this->networking.setCookies(cookies);

				if(!headers.empty())
					this->networking.setHeaders(headers);

				// get content
				this->networking.getContent(
						this->getProtocol() + sourceUrl,
						usePost,
						content,
						this->config.generalRetryHttp
				);

				// unset custom headers if necessary
				if(!cookies.empty())
					this->networking.unsetCookies();

				if(!headers.empty())
					this->networking.unsetHeaders();

				success = true;

				break;
			}
			catch(const CurlException& e) { // error while getting content
				// unset custom headers if necessary
				if(!cookies.empty())
					this->networking.unsetCookies();

				if(!headers.empty())
					this->networking.unsetHeaders();

				// check type of error i.e. last cURL code
				if(this->extractingCheckCurlCode(
						this->networking.getCurlCode(),
						sourceUrl
				))
					// reset connection and retry
					this->extractingReset(e.whatStr(), sourceUrl);
				else {
					this->log(
							Config::generalLoggingDefault,
							"WARNING: Could not get token \'"
							+ name
							+ "\' from "
							+ sourceUrl
							+ ": "
							+ e.whatStr()
					);

					break;
				}
			}
			catch(const Utf8Exception& e) {
				// unset custom headers if necessary
				if(!cookies.empty())
					this->networking.unsetCookies();

				if(!headers.empty())
					this->networking.unsetHeaders();

				// write UTF-8 error to log if neccessary
				this->log(Config::generalLoggingDefault, "WARNING: " + e.whatStr() + " [" + sourceUrl + "].");

				break;
			}
		}

		if(success) {
			std::queue<std::string> queryWarnings;

			// set token page content as target for subsequent query
			this->setQueryTarget(content, sourceUrl);

			// get token from content
			if(query.resultSingle)
				this->getSingleFromQuery(query, result, queryWarnings);
			else {
				bool booleanResult = false;

				if(this->getBoolFromQuery(query, booleanResult, queryWarnings))
					result = booleanResult ? "true" : "false";
			}

			// clear query target
			this->clearQueryTarget();

			// logging if necessary
			this->log(Config::generalLoggingDefault, queryWarnings);

			this->log(
					Config::generalLoggingExtended,
					"fetched token \'"
					+ name
					+ "\' from "
					+ sourceUrl
					+ " [= \'"
					+ result
					+ "\']."
			);
		}

		return result;
	}

	// get page content from URL
	void Thread::extractingPageContent(
			const std::string& url,
			const std::string& cookies,
			const std::vector<std::string>& headers,
			std::string& resultTo
	) {
		while(this->isRunning()) {
			try {
				// set local network configuration
				this->networking.setConfigCurrent(*this);

				// set custom headers if necessary
				if(!cookies.empty())
					this->networking.setCookies(cookies);

				if(!headers.empty())
					this->networking.setHeaders(headers);

				// get content
				this->networking.getContent(
						this->getProtocol() + url,
						this->config.sourceUsePost,
						resultTo,
						this->config.generalRetryHttp
				);

				// unset custom headers if necessary
				if(!cookies.empty())
					this->networking.unsetCookies();

				if(!headers.empty())
					this->networking.unsetHeaders();

				break;
			}
			catch(const CurlException& e) { // error while getting content
				// unset custom headers if necessary
				if(!cookies.empty())
					this->networking.unsetCookies();

				if(!headers.empty())
					this->networking.unsetHeaders();

				// error while getting content: check type of error i.e. last cURL code
				if(this->extractingCheckCurlCode(
						this->networking.getCurlCode(),
						url
				))
					// reset connection and retry
					this->extractingReset(e.whatStr(), url);
				else {
					this->log(
							Config::generalLoggingDefault,
							"WARNING: Could not extract data from "
							+ url
							+ ": "
							+ e.whatStr()
					);

					break;
				}
			}
			catch(const Utf8Exception& e) {
				// unset custom headers if necessary
				if(!cookies.empty())
					this->networking.unsetCookies();

				if(!headers.empty())
					this->networking.unsetHeaders();

				// write UTF-8 error to log if neccessary
				this->log(Config::generalLoggingDefault, "WARNING: " + e.whatStr() + " [" + url + "].");

				break;
			}
		}
	}

	// extract data from crawled content
	void Thread::extractingGetValueFromContent(const QueryStruct& query, std::string& resultTo) {
		// ignore if invalid query is specified
		if(!query.resultBool && !query.resultSingle)
			return;

		// get value by running query of any type on page content
		std::queue<std::string> queryWarnings;

		if(query.resultSingle)
			this->getSingleFromQuery(query, resultTo, queryWarnings);
		else {
			bool booleanResult = false;

			if(this->getBoolFromQuery(query, booleanResult, queryWarnings))
				resultTo = booleanResult ? "true" : "false";
		}

		// log warnings if necessary
		this->log(Config::generalLoggingDefault, queryWarnings);
	}

	// extract data from URL
	void Thread::extractingGetValueFromUrl(const QueryStruct& query, std::string& resultTo) {
		// ignore if invalid query is specified
		if(
				(!query.resultBool && !query.resultSingle)
				|| query.type != QueryStruct::typeRegEx
		)
			return;

		// get value by running RegEx query on URL
		std::queue<std::string> queryWarnings;

		if(query.resultSingle)
			this->getSingleFromRegEx(query, this->urls.front().second, resultTo, queryWarnings);
		else {
			bool booleanResult = false;

			if(this->getBoolFromRegEx(query, this->urls.front().second, booleanResult, queryWarnings))
				resultTo = booleanResult ? "true" : "false";
		}

		// log warnings if necessary
		this->log(Config::generalLoggingDefault, queryWarnings);
	}

	// check for an error in the page because of which the page needs to be retried
	bool Thread::extractingPageIsRetry(std::queue<std::string>& queryWarningsTo) {
		for(const auto& query : this->queriesErrorRetry) {
			bool error = false;

			if(this->getBoolFromQuery(query, error, queryWarningsTo) && error)
				return true;
		}

		return false;
	}

	// extract data by parsing page content, return number of extracted datasets
	size_t Thread::extractingPage(size_t contentId, const std::string& url) {
		std::queue<std::string> queryWarnings;

		// check for errors if necessary
		for(const auto& query : this->queriesErrorFail) {
			bool error = false;

			if(this->getBoolFromQuery(query, error, queryWarnings) && error) {
				std::string target;

				if(this->getTarget(target))
					throw Exception("Error in data: " +  target + " [" + url + "]");
				else
					throw Exception("Error in data from " + url);
			}
		}

		// get datasets
		for(const auto& query : this->queriesDatasets) {
			// get datasets by performing query of any type on page content
			this->setSubSetsFromQuery(query, queryWarnings);

			// log warnings if necessary
			this->log(Config::generalLoggingDefault, queryWarnings);

			// check whether datasets have been extracted
			if(this->getNumberOfSubSets())
				break;
		}

		// check whether no dataset has been extracted
		if(!(this->getNumberOfSubSets()))
			return 0;

		// save old number of results
		size_t before = this->results.size();

		// go through all datasets
		while(this->nextSubSet()) {
			DataEntry dataset(contentId);

			// extract IDs
			for(const auto& query : this->queriesId) {
				// get ID by performing query on current subset
				this->getSingleFromQueryOnSubSet(query, dataset.dataId, queryWarnings);

				// log warnings if necessary
				this->log(Config::generalLoggingDefault, queryWarnings);

				// check whether ID has been extracted
				if(!dataset.dataId.empty())
					break;
			}

			// check whether no ID has been extracted
			if(dataset.dataId.empty())
				continue;

			// check whether extracted ID ought to be ignored
			if(
					std::find(
							this->config.extractingIdIgnore.begin(),
							this->config.extractingIdIgnore.end(),
							dataset.dataId
					) != this->config.extractingIdIgnore.end()
			) {
				this->log(
						Config::generalLoggingExtended,
						"ignored parsed ID \'"
						+ dataset.dataId
						+ "\' ["
						+ url
						+ "]."
				);

				return false;
			}

			// extract date/time
			for(auto i = this->queriesDateTime.begin(); i != this->queriesDateTime.end(); ++i) {
				// extract date/time by performing query on current subset
				this->getSingleFromQueryOnSubSet(*i, dataset.dateTime, queryWarnings);

				// log warnings if necessary
				this->log(Config::generalLoggingDefault, queryWarnings);

				// check whether date/time has been extracted
				if(!dataset.dateTime.empty()) {
					const auto index = i - this->queriesDateTime.begin();

					// found date/time: try to convert it to SQL time stamp
					std::string format(this->config.extractingDateTimeFormats.at(index));

					// use "%F %T" as default date/time format
					if(format.empty())
						format = "%F %T";

					try {
						Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								dataset.dateTime,
								format,
								this->config.extractingDateTimeLocales.at(index)
						);
					}
					catch(const LocaleException& e) {
						this->log(
								Config::generalLoggingDefault,
								"WARNING: " + e.whatStr() + " - locale ignored."
						);

						try {
								Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
										dataset.dateTime,
										format
								);
						}
						catch(const DateTimeException& e) {
							this->log(
									Config::generalLoggingExtended,
									e.whatStr() + " - query skipped [" + url + "]."
							);

							dataset.dateTime.clear();
						}
					}
					catch(const DateTimeException& e) {
						this->log(
								Config::generalLoggingExtended,
								e.whatStr() + " - query skipped [" + url + "]."
						);

						dataset.dateTime.clear();
					}

					if(!dataset.dateTime.empty())
						break;
				}
			}

			// extract custom fields
			dataset.fields.reserve(this->queriesFields.size());

			for(auto i = this->queriesFields.begin(); i != this->queriesFields.end(); ++i) {
				const auto index = i - this->queriesFields.begin();
				const auto dateTimeFormat(this->config.extractingFieldDateTimeFormats.at(index));

				// determinate whether to get all or just the first match (as string or boolean value) from the query result
				if(i->resultMulti) {
					// extract multiple elements
					std::vector<std::string> extractedFieldValues;

					// extract field values by using query on content
					this->getMultiFromQueryOnSubSet(*i, extractedFieldValues, queryWarnings);

					// log warnings if necessary
					this->log(Config::generalLoggingDefault, queryWarnings);

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
									this->log(
											Config::generalLoggingDefault,
											"WARNING: "
											+ e.whatStr()
											+ " for field \'"
											+ this->config.extractingFieldNames.at(index)
											+ "\' ["
											+ url
											+ "]."
									);

									Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
											value,
											dateTimeFormat
									);
								}
								catch(const DateTimeException& e) {
									this->log(
											Config::generalLoggingDefault,
											"WARNING: "
											+ e.whatStr()
											+ " for field \'"
											+ this->config.extractingFieldNames.at(index)
											+ "\' ["
											+ url
											+ "]."
									);

									value.clear();
								}
							}
							catch(const DateTimeException& e) {
								this->log(
										Config::generalLoggingDefault,
										"WARNING: "
										+ e.whatStr()
										+ " for field \'"
										+ this->config.extractingFieldNames.at(index)
										+ "\' ["
										+ url
										+ "]."
								);

								value.clear();
							}
						}
					}

					// if necessary, check whether array or all values are empty
					if(this->config.extractingFieldWarningsEmpty.at(index)) {
						if(
								std::find_if(
										extractedFieldValues.begin(),
										extractedFieldValues.end(),
										[](auto const& value) {
											return !value.empty();
										}
								) == extractedFieldValues.end()
						)
							this->log(
									Config::generalLoggingDefault,
									"WARNING: \'"
									+ this->config.extractingFieldNames.at(index) + "\'"
									" is empty for "
									+ url
							);
					}

					// determine how to save result: JSON array or concatenate using delimiting character
					if(this->config.extractingFieldJSON.at(index)) {
						// if necessary, tidy texts
						if(this->config.extractingFieldTidyTexts.at(index))
							for(auto& value : extractedFieldValues)
								Helper::Strings::utfTidy(value);

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
						if(this->config.extractingFieldTidyTexts.at(index))
							Helper::Strings::utfTidy(result);

						dataset.fields.emplace_back(result);
					}
				}
				else if(i->resultSingle) {
					// extract first element only (as string)
					std::string extractedFieldValue;

					// extract single field value by using query on content
					this->getSingleFromQueryOnSubSet(*i, extractedFieldValue, queryWarnings);

					// log warnings if necessary
					this->log(Config::generalLoggingDefault, queryWarnings);

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
								this->log(
										Config::generalLoggingDefault,
										"WARNING: "
										+ e.whatStr()
										+ " for field \'"
										+ this->config.extractingFieldNames.at(index)
										+ "\' ["
										+ url
										+ "]."
								);

								Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
										extractedFieldValue,
										dateTimeFormat
								);
							}
							catch(const DateTimeException& e) {
								this->log(
										Config::generalLoggingDefault,
										"WARNING: "
										+ e.whatStr()
										+ " for field \'"
										+ this->config.extractingFieldNames.at(index)
										+ "\' ["
										+ url
										+ "]."
								);

								extractedFieldValue.clear();
							}
						}
						catch(const DateTimeException& e) {
							this->log(
									Config::generalLoggingDefault,
									"WARNING: "
									+ e.whatStr()
									+ " for field \'"
									+ this->config.extractingFieldNames.at(index)
									+ "\' ["
									+ url
									+ "]."
							);

							extractedFieldValue.clear();
						}
					}

					// if necessary, check whether value is empty
					if(
							this->config.extractingFieldWarningsEmpty.at(index)
							&& extractedFieldValue.empty()
					)
						this->log(
								Config::generalLoggingDefault,
								"WARNING: \'"
								+ this->config.extractingFieldNames.at(index) + "\'"
								" is empty for "
								+ url
						);

					// if necessary, tidy text
					if(this->config.extractingFieldTidyTexts.at(index))
						Helper::Strings::utfTidy(extractedFieldValue);

					// determine how to save result: JSON array or string as is
					if(this->config.extractingFieldJSON.at(index))
						// stringify and add extracted element as JSON array with one element
						dataset.fields.emplace_back(Helper::Json::stringify(extractedFieldValue));

					else
						// save as is
						dataset.fields.emplace_back(extractedFieldValue);
				}
				else if(i->resultBool) {
					// only save whether a match for the query exists
					bool booleanResult = false;

					// extract boolean field value by using query on content
					this->getBoolFromQueryOnSubSet(*i, booleanResult, queryWarnings);

					// log warnings if necessary
					this->log(Config::generalLoggingDefault, queryWarnings);

					// date/time conversion is not possible for boolean values
					if(!dateTimeFormat.empty())
						this->log(
								Config::generalLoggingDefault,
								"WARNING: Cannot convert boolean value for field \'"
								+ this->config.extractingFieldNames.at(index)
								+ "\' to date/time\' ["
								+ this->urls.front().second
								+ "]."
						);

					// determine how to save result: JSON array or boolean value as string
					if(this->config.extractingFieldJSON.at(index))
						// stringify and add extracted element as JSON array with one boolean value as string
						dataset.fields.emplace_back(
								Helper::Json::stringify(
										booleanResult ? std::string("true") : std::string("false")
								)
						);

					else
						// save boolean value as string
						dataset.fields.emplace_back(booleanResult ? "true" : "false");
				}
				else {
					if(i->type != QueryStruct::typeNone)
						this->log(
								Config::generalLoggingDefault,
								"WARNING: Ignored query for \'"
								+ this->config.extractingFieldNames.at(index)
								+ "\' without specified result type."
						);

					dataset.fields.emplace_back();
				}
			}

			// check for duplicate ID
			if(this->ids.insert(dataset.dataId).second)
				// add extracted dataset to results
				this->results.push(dataset);

			// recursive extracting
			for(const auto& query : this->queriesRecursive)
				if(this->addSubSetsFromQueryOnSubSet(query, queryWarnings))
					break;

			// log warnings if necessary
			this->log(Config::generalLoggingDefault, queryWarnings);
		}

		return this->results.size() - before;
	}

	// check cURL code and decide whether to retry or skip
	bool Thread::extractingCheckCurlCode(CURLcode curlCode, const std::string& url) {
		if(curlCode == CURLE_TOO_MANY_REDIRECTS) {
			// redirection error: skip URL
			this->log(Config::generalLoggingDefault, "redirection error at " + url + " - skips...");

			return false;
		}

		return true;
	}

	// check the HTTP response code for an error and decide whether to continue or skip
	bool Thread::extractingCheckResponseCode(const std::string& url, long responseCode) {
		if(responseCode >= 400 && responseCode < 600) {
			this->log(
					Config::generalLoggingDefault,
					"HTTP error "
					+ std::to_string(responseCode)
					+ " from "
					+ url
					+ " - skips..."
			);

			return false;
		}
		else if(responseCode != 200)
			this->log(
					Config::generalLoggingDefault,
					"WARNING: HTTP response code "
					+ std::to_string(responseCode)
					+ " from "
					+ url
					+ "."
			);

		return true;
	}

	// URL has been processed (skipped or used for extraction)
	void Thread::extractingUrlFinished() {
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

		// save URL ID as last processed URL
		this->lastUrl = this->urls.front().first;

		// delete current URL from cache
		this->urls.pop();
	}

	// save results to database
	void Thread::extractingSaveResults(bool warped) {
		// check whether there are no results
		if(this->results.empty()) {
			// set last URL
			if(!warped && this->lastUrl)
				this->setLast(this->lastUrl);

			// no results: done!
			return;
		}

		// timer
		Timer::Simple timer;

		// save status message
		const std::string status(this->getStatusMessage());

		this->setStatusMessage("Waiting for target table...");

		{ // lock target table
			DatabaseLock(
					this->database,
					"targetTable." + this->targetTable,
					std::bind(&Thread::isRunning, this)
			);

			// save results
			this->setStatusMessage("Saving results...");

			this->log(Config::generalLoggingExtended, "saves results...");

			// update or add entries in/to database
			this->database.updateOrAddEntries(this->results);

			// update target table
			this->database.updateTargetTable();
		} // target table unlocked

		// set last URL
		if(!warped)
			this->setLast(this->lastUrl);

		// set those URLs to finished whose URL lock is okay (still locked or re-lockable)
		this->database.setUrlsFinishedIfLockOk(this->finished);

		// update status
		this->setStatusMessage("Results saved. [" + status + "]");

		if(this->config.generalTiming)
			this->log(Config::generalLoggingDefault, "saved results in " + timer.tickStr());
	}

	// reset connection and retry
	void Thread::extractingReset(const std::string& error, const std::string& url) {
		// clear query target
		this->clearQueryTarget();

		// show error
		this->log(Config::generalLoggingDefault, error + " - retrying [" + url + "].");

		this->setStatusMessage("ERROR " + error + " - retrying [" + url + "]");

		// reset connection and retry (if still running)
		if(this->isRunning()) {
			this->log(Config::generalLoggingDefault, "resets connection...");

			this->extractingResetTor();

			this->networking.resetConnection(this->config.generalSleepError);

			this->log(
					Config::generalLoggingDefault,
					"new public IP: " + this->networking.getPublicIp()
			);
		}
	}

	// request a new TOR identity if necessary
	void Thread::extractingResetTor() {
		try {
			if(this->torControl && this->resetTor) {
				this->torControl.newIdentity();

				this->log(Config::generalLoggingDefault, "requested a new TOR identity.");
			}
		}
		catch(const TorControlException& e) {
			this->log(
					Config::generalLoggingDefault,
					"could not request new TOR identity - " + e.whatStr()
			);
		}
	}
} /* crawlservpp::Module::Extractor */
