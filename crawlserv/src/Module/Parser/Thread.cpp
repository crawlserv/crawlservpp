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
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.hpp"

namespace crawlservpp::Module::Parser {

	// constructor A: run previously interrupted parser
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	)		:	Module::Thread(
						dbBase,
						threadOptions,
						threadStatus
				),
				database(this->Module::Thread::database),
				tickCounter(0),
				startTime(std::chrono::steady_clock::time_point::min()),
				pauseTime(std::chrono::steady_clock::time_point::min()),
				idleTime(std::chrono::steady_clock::time_point::min()),
				idle(false),
				idFromUrlOnly(false),
				lastUrl(0),
				idFirst(0),
				idDist(0),
				posFirstF(0.),
				posDist(0),
				total(0) {}

	// constructor B: start a new parser
	Thread::Thread(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions
	)		:	Module::Thread(
						dbBase,
						threadOptions
				),
				database(this->Module::Thread::database),
				tickCounter(0),
				startTime(std::chrono::steady_clock::time_point::min()),
				pauseTime(std::chrono::steady_clock::time_point::min()),
				idleTime(std::chrono::steady_clock::time_point::min()),
				idle(false),
				idFromUrlOnly(false),
				lastUrl(0),
				idFirst(0),
				idDist(0),
				posFirstF(0.),
				posDist(0),
				total(0) {}

	// destructor stub
	Thread::~Thread() {}

	// initialize parser
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

		// set query container options
		this->setRepairCData(this->config.parsingRepairCData);
		this->setTidyErrorsAndWarnings(this->config.parsingTidyErrors, this->config.parsingTidyWarnings);

		// set database options
		this->setStatusMessage("Setting database options...");

		this->database.setLogging(
				this->config.generalLogging,
				Config::generalLoggingDefault,
				Config::generalLoggingVerbose
		);

		this->log(Config::generalLoggingVerbose, "sets database options...");

		this->database.setCacheSize(this->config.generalCacheSize);
		this->database.setReparse(this->config.generalReParse);
		this->database.setParseCustom(this->config.generalParseCustom);
		this->database.setTargetTable(this->config.generalResultTable);
		this->database.setTargetFields(this->config.parsingFieldNames);
		this->database.setSleepOnError(this->config.generalSleepMySql);

		if(this->config.generalDbTimeOut)
			this->database.setTimeOut(this->config.generalDbTimeOut);

		// create table names for locking
		const std::string urlListTable(
				"crawlserv_"
				+ this->websiteNamespace
				+ "_"
				+ this->urlListNamespace
		);

		this->parsingTable = urlListTable + "_parsing";
		this->targetTable = urlListTable + "_parsed_" + this->config.generalResultTable;

		// initialize target table
		this->setStatusMessage("Initializing target table...");

		this->log(Config::generalLoggingVerbose, "initializes target table...");

		this->database.initTargetTable();

		// prepare SQL statements for parser
		this->setStatusMessage("Preparing SQL statements...");

		this->log(Config::generalLoggingVerbose, "prepares SQL statements...");

		this->database.prepare();

		// initialize queries
		this->setStatusMessage("Initializing custom queries...");

		this->log(Config::generalLoggingVerbose, "initializes custom queries...");

		this->initQueries();

		{
			// wait for parsing table lock
			this->setStatusMessage("Waiting for parsing table...");

			this->log(Config::generalLoggingVerbose, "waits for parsing table...");

			DatabaseLock(
					this->database,
					"parsingTable." + this->parsingTable,
					std::bind(&Thread::isRunning, this)
			);

			if(!(this->isRunning()))
				return;

			// check parsing table
			this->setStatusMessage("Checking parsing table...");

			this->log(Config::generalLoggingVerbose, "checks parsing table...");

			const unsigned int deleted = this->database.checkParsingTable();

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

		// parser is ready
		this->log(Config::generalLoggingExtended, "is ready.");
	}

	// parser tick, throws Thread::Exception
	void Thread::onTick() {
		bool skip = false;
		size_t parsed = 0;

		// check for jump in last ID ("time travel")
		long warpedOver = this->getWarpedOverAndReset();

		if(warpedOver != 0) {
			// save cached results
			this->parsingSaveResults(true);

			// unlock and discard old URLs
			this->database.unLockUrlsIfOk(this->urls, this->cacheLockTime);

			// overwrite last URL ID
			this->lastUrl = this->getLast();

			// adjust tick counter
			this->tickCounter += warpedOver;
		}

		// URL selection if no URLs are left to parse
		if(this->urls.empty())
			this->parsingUrlSelection();

		if(this->urls.empty()) {
			// no URLs left in database: set timer if just finished, sleep
			if(this->idleTime == std::chrono::steady_clock::time_point::min())
				this->idleTime = std::chrono::steady_clock::now();

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
		if(this->urls.empty())
			return;

		// write log entry if necessary
		this->log(Config::generalLoggingExtended, "parses " + this->urls.front().second + "...");

		// try to renew URL lock
		this->lockTime = this->database.renewUrlLockIfOk(
				this->urls.front().first,
				this->cacheLockTime,
				this->config.generalLock
		);

		skip = this->lockTime.empty();

		if(skip) {
			// skip locked URL
			this->log(Config::generalLoggingExtended, "skips (locked) " + this->urls.front().second);
		}
		else {
			// set status
			this->setStatusMessage(this->urls.front().second);

			// approximate progress
			if(!(this->total))
				throw Exception("Parser::Thread::onTick(): Could not get URL list size");

			if(this->idDist) {
				const float cacheProgress =
						static_cast<float>(this->urls.front().first - this->idFirst) / this->idDist;
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

			// parse content(s)
			parsed = this->parsingNext();

			// stop timer
			if(this->config.generalTiming)
				timerStr = timer.tickStr();

			// save expiration time of URL lock if parsing was successful or unlock URL if parsing failed
			if(parsed)
				this->finished.emplace(this->urls.front().first, this->lockTime);
			else
				// unlock URL if necesssary
				this->database.unLockUrlIfOk(this->urls.front().first, this->lockTime);

			// reset lock time
			this->lockTime = "";

			// write to log if necessary
			std::ostringstream logStrStr;

			logStrStr.imbue(std::locale(""));

			if(parsed > 1)
				logStrStr << "parsed " << parsed << " versions of ";
			else if(parsed == 1)
				logStrStr << "parsed ";
			else
				logStrStr << "skipped ";

			logStrStr << this->urls.front().second;

			if(this->config.generalTiming)
				logStrStr << " in " << timerStr;

			this->log(
					this->config.generalTiming ? Config::generalLoggingDefault : Config::generalLoggingExtended,
					logStrStr.str()
			);
		}

		// URL finished
		this->parsingUrlFinished();
	}

	// parser paused
	void Thread::onPause() {
		// save pause start time
		this->pauseTime = std::chrono::steady_clock::now();

		// save results if necessary
		this->parsingSaveResults(false);
	}

	// parser unpaused
	void Thread::onUnpause() {
		// add pause time to start or idle time to ignore pause
		if(this->idleTime > std::chrono::steady_clock::time_point::min())
			this->idleTime += std::chrono::steady_clock::now() - this->pauseTime;
		else
			this->startTime += std::chrono::steady_clock::now() - this->pauseTime;

		this->pauseTime = std::chrono::steady_clock::time_point::min();
	}

	// clear parser
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
							std::chrono::steady_clock::now() - this->startTime
					).count();

			tpsStrStr.imbue(std::locale(""));

			tpsStrStr << std::setprecision(2) << std::fixed << tps;

			this->log(Config::generalLoggingDefault, "average speed: " + tpsStrStr.str() + " ticks per second.");
		}

		// save results if necessary
		this->parsingSaveResults(false);

		// save status message
		const std::string oldStatus(this->getStatusMessage());

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
		this->queriesSkip.reserve(this->config.generalSkip.size());
		this->queriesContentIgnore.reserve(this->config.parsingContentIgnoreQueries.size());
		this->queriesId.reserve(this->config.parsingIdQueries.size());
		this->queriesDateTime.reserve(this->config.parsingDateTimeQueries.size());
		this->queriesFields.reserve(this->config.parsingFieldQueries.size());

		try {
			// create queries and get query properties
			for(const auto& query : this->config.generalSkip) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesSkip.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.parsingContentIgnoreQueries) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesContentIgnore.emplace_back(this->addQuery(properties));
				}
			}

			// NOTE: The following queries need to be added even if they are of type 'none'
			//			as their index needs to correspond to other options
			for(const auto& query : this->config.parsingIdQueries) {
				QueryProperties properties;

				if(query)
					this->database.getQueryProperties(query, properties);

				this->queriesId.emplace_back(this->addQuery(properties));
			}

			for(const auto& query : this->config.parsingDateTimeQueries) {
				QueryProperties properties;

				if(query)
					this->database.getQueryProperties(query, properties);

				this->queriesDateTime.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.parsingFieldQueries.begin();
					i != this->config.parsingFieldQueries.end();
					++i
			) {
				QueryProperties properties;

				if(*i)
					this->database.getQueryProperties(*i, properties);
				else {
					const auto& name =
							this->config.parsingFieldNames.at(
									i - this->config.parsingFieldQueries.begin()
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
		}
		catch(const QueryException& e) {
			throw Exception("Parser::Thread::initQueries(): " + e.whatStr());
		}

		// check whether the ID is exclusively parsed from the URL
		this->idFromUrlOnly = std::find(
				this->config.parsingIdSources.begin(),
				this->config.parsingIdSources.end(),
				static_cast<unsigned char>(Config::parsingSourceContent)
		) == this->config.parsingIdSources.end();
	}

	// URL selection
	void Thread::parsingUrlSelection() {
		Timer::Simple timer;

		// get number of URLs
		this->total = this->database.getNumberOfUrls();

		this->setStatusMessage("Fetching URLs...");

		// fill cache with next URLs
		this->log(Config::generalLoggingExtended, "fetches URLs...");

		// get next URL(s)
		this->parsingFetchUrls();

		// write to log if necessary
		if(this->config.generalTiming)
			this->log(Config::generalLoggingDefault, "fetched URLs in " + timer.tickStr());

		// update status
		this->setStatusMessage("Checking URLs...");

		// check whether URLs have been fetched
		if(this->urls.empty()) {
			// no more URLs to parse
			if(!(this->idle)) {
				this->log(Config::generalLoggingExtended, "finished.");

				this->setStatusMessage("IDLE Waiting for new URLs to parse.");
				this->setProgress(1.f);
			}

			return;
		}
		else // reset idling status
			this->idle = false;
	}

	// fetch next URLs from database
	void Thread::parsingFetchUrls() {
		// fetch URLs from database to cache
		this->cacheLockTime =
				this->database.fetchUrls(this->getLast(), this->urls, this->config.generalLock);

		// check whether URLs have been fetched
		if(this->urls.empty())
			return;

		// save properties of fetched URLs and URL list for progress calculation
		this->idFirst = this->urls.front().first;
		this->idDist = this->urls.back().first - this->idFirst;

		const size_t posFirst = this->database.getUrlPosition(this->idFirst);

		this->posFirstF = static_cast<float>(posFirst);
		this->posDist = this->database.getUrlPosition(this->urls.back().first) - posFirst;
	}

	// check whether next URL(s) ought to be skipped
	void Thread::parsingCheckUrls() {
		std::queue<std::string> queryWarnings;

		// loop over next URLs in cache
		while(!(this->urls.empty()) && this->isRunning()) {
			// check whether URL needs to be skipped because of invalid ID
			if(!(this->urls.front().first)) {
				this->log(Config::generalLoggingDefault, "skips (INVALID ID) " + this->urls.front().second);

				// unlock URL if necessary
				this->database.unLockUrlIfOk(this->urls.front().first, this->cacheLockTime);

				// finish skipped URL
				this->parsingUrlFinished();

				continue;
			}

			// check whether URL needs to be skipped because of query
			bool skip = false;

			if(!(this->config.generalSkip.empty())) {
				// loop over skip queries
				for(const auto& query : this->queriesSkip)
					if(this->getBoolFromRegEx(query, urls.front().second, skip, queryWarnings) && skip)
						break;

				// log warnings if necessary
				this->log(Config::generalLoggingDefault, queryWarnings);

				if(skip) {
					// skip URL because of query
					this->log(Config::generalLoggingExtended, "skips (query) " + this->urls.front().second);

					// unlock URL if necessary
					this->database.unLockUrlIfOk(this->urls.front().first, this->cacheLockTime);

					// finish skipped URL
					this->parsingUrlFinished();

					continue;
				}
			}

			break; // found URL to process
		} // end of loop over URLs in cache

		// log warnings if necessary
		this->log(Config::generalLoggingDefault, queryWarnings);
	}

	// parse URL and content(s) of next URL, return number of successfully parsed contents
	size_t Thread::parsingNext() {
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
				)
					// ID has been found
					break;
			}

			// write warnings to log if necessary
			this->log(Config::generalLoggingDefault, queryWarnings);

			// check ID
			if(parsedId.empty())
				return 0;

			if(
					!(this->config.parsingIdIgnore.empty())
					&& std::find(
							this->config.parsingIdIgnore.begin(),
							this->config.parsingIdIgnore.end(),
							parsedId
					) != this->config.parsingIdIgnore.end()
			)
				return 0;
		}

		if(this->config.generalNewestOnly) {
			// parse newest content of URL
			size_t numberOfContents = 0;
			size_t index = 0;
			bool changedStatus = false;

			while(this->isRunning()) {
				IdString latestContent;

				if(
						this->database.getLatestContent(
								this->urls.front().first,
								index,
								latestContent
						)
				) {
					if(this->parsingContent(latestContent, parsedId)) {
						if(changedStatus)
							this->setStatusMessage(this->urls.front().second);

						return 1;
					}

					++index;

					if(index % 100 == 0) {
						if(!numberOfContents)
							numberOfContents = this->database.getNumberOfContents(
									this->urls.front().first
							);

						std::ostringstream statusStrStr;

						statusStrStr.imbue(std::locale(""));

						statusStrStr	<< "["
										<< index
										<< "/"
										<< numberOfContents
										<< "] "
										<< this->urls.front().second;

						this->setStatusMessage(statusStrStr.str());

						changedStatus = true;
					}
				}
				else
					break;
			}

			if(changedStatus)
				this->setStatusMessage(this->urls.front().second);
		}
		else {
			// parse all contents of URL
			size_t counter = 0;

			std::queue<IdString> contents(
					this->database.getAllContents(
							this->urls.front().first
					)
			);

			while(!contents.empty()) {
				if(this->parsingContent(contents.front(), parsedId))
					++counter;

				contents.pop();
			}

			return counter;
		}

		return 0;
	}

	// parse ID-specific content, return whether parsing was successfull (i.e. an ID could be parsed)
	bool Thread::parsingContent(const IdString& content, const std::string& parsedId) {
		DataEntry parsedData(content.first);
		std::queue<std::string> queryWarnings;

		// set content as target for subsequent queries
		this->setQueryTarget(content.second, this->urls.front().second);

		// parse ID (if still necessary)
		if(!(this->idFromUrlOnly)) {
			for(auto i = this->queriesId.begin(); i != this->queriesId.end(); ++i) {
				// check query source
				switch(this->config.parsingIdSources.at(i - this->queriesId.begin())) {
				case Config::parsingSourceUrl:
					// parse ID by running RegEx query on URL
					this->getSingleFromRegEx(
							*i,
							this->urls.front().second,
							parsedData.dataId,
							queryWarnings
					);

					break;

				case Config::parsingSourceContent:
					// parse ID by running query on content
					this->getSingleFromQuery(*i, parsedData.dataId, queryWarnings);

					break;

				default:
					queryWarnings.emplace("WARNING: Invalid source for ID.");
				}

				if(!parsedData.dataId.empty())
					break; // ID successfully parsed
			}

			// log warnings if necessary
			this->log(Config::generalLoggingDefault, queryWarnings);
		}
		else
			parsedData.dataId = parsedId;

		// check whether no ID has been parsed
		if(parsedData.dataId.empty()) {
			// clear query target before continuing to next URL (or finishing up)
			this->clearQueryTarget();

			return false;
		}

		// check whether parsed ID is ought to be ignored
		if(
				std::find(
						this->config.parsingIdIgnore.begin(),
						this->config.parsingIdIgnore.end(),
						parsedData.dataId
				) != this->config.parsingIdIgnore.end()
		) {
			this->log(
					Config::generalLoggingExtended,
					"ignored parsed ID \'"
					+ parsedData.dataId
					+ "\' ["
					+ this->urls.front().second
					+ "]."
			);

			// clear query target before continuing to next URL (or finishing up)
			this->clearQueryTarget();

			return false;
		}

		// check whether content is ought to be ignored
		for(const auto& query : this->queriesContentIgnore) {
			bool ignore = false;

			this->getBoolFromQuery(query, ignore, queryWarnings);

			if(ignore) {
				this->log(
						Config::generalLoggingExtended,
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
		const size_t contentId = this->database.getContentIdFromParsedId(parsedData.dataId);
		bool duplicateInCache = false;

		if(!contentId) {
			// check cached results
			auto cachedResults(this->results);

			while(!cachedResults.empty()) {
				if(cachedResults.front().dataId == parsedData.dataId) {
					duplicateInCache = true;

					break;
				}

				cachedResults.pop();
			}
		}

		if((contentId && contentId != content.first) || duplicateInCache) {
			this->log(
					Config::generalLoggingDefault,
					"skipped content with already existing ID \'"
					+ parsedData.dataId
					+ "\' ["
					+ this->urls.front().second
					+ "]."
			);

			// clear query target before continuing to next URL (or finishing up)
			this->clearQueryTarget();

			return false;
		}

		// parse date/time
		for(auto i = this->queriesDateTime.begin(); i != this->queriesDateTime.end(); ++i) {
			const auto index = i - this->queriesDateTime.begin();

			// check query source
			switch(this->config.parsingDateTimeSources.at(index)) {
			case Config::parsingSourceUrl:
				// parse date/time by running RegEx query on URL
				this->getSingleFromRegEx(*i, this->urls.front().second, parsedData.dateTime, queryWarnings);

				break;

			case Config::parsingSourceContent:
				// parse date/time by running query on content
				this->getSingleFromQuery(*i, parsedData.dateTime, queryWarnings);

				break;
			}

			// log warnings if necessary
			this->log(Config::generalLoggingDefault, queryWarnings);

			if(!parsedData.dateTime.empty()) {
				// found date/time: try to convert it to SQL time stamp
				std::string format(this->config.parsingDateTimeFormats.at(index));

				// use "%F %T" as default date/time format
				if(format.empty())
					format = "%F %T";

				try {
					Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
							parsedData.dateTime,
							format,
							this->config.parsingDateTimeLocales.at(index)
					);
				}
				catch(const LocaleException& e) {
					this->log(Config::generalLoggingDefault, "WARNING: " + e.whatStr() + " - locale ignored.");

					try {
						Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								parsedData.dateTime,
								format
						);
					}
					catch(const DateTimeException& e) {
						this->log(
								Config::generalLoggingExtended,
								e.whatStr()
								+ " - query skipped ["
								+ this->urls.front().second
								+ "]."
						);

						parsedData.dateTime.clear();
					}
				}
				catch(const DateTimeException & e) {
					this->log(
							Config::generalLoggingExtended,
							e.whatStr()
							+ " - query skipped ["
							+ this->urls.front().second
							+ "]."
					);

					parsedData.dateTime.clear();
				}

				if(!parsedData.dateTime.empty())
					break; // date/time successfully parsed and converted
			}
		}

		// warn about empty date/time if necessary
		if(
				this->config.parsingDateTimeWarningEmpty
				&& parsedData.dateTime.empty()
				&& !(this->queriesDateTime.empty())

		)
			this->log(Config::generalLoggingDefault, "WARNING: date/time is empty for " + this->urls.front().second);

		// parse custom fields
		parsedData.fields.reserve(this->queriesFields.size());

		for(auto i = this->queriesFields.begin(); i != this->queriesFields.end(); ++i) {
			const auto index = i - this->queriesFields.begin();
			const auto dateTimeFormat(this->config.parsingFieldDateTimeFormats.at(index));

			// determinate whether to get all or just the first match (as string or boolean value) from the query result
			if(i->resultMulti) {
				// parse multiple elements
				std::vector<std::string> parsedFieldValues;

				// check query source
				switch(this->config.parsingFieldSources.at(index)) {
				case Config::parsingSourceUrl:
					// parse field values by using RegEx query on URL
					this->getMultiFromRegEx(*i, this->urls.front().second, parsedFieldValues, queryWarnings);

					break;

				case Config::parsingSourceContent:
					// parse field values by using query on content
					this->getMultiFromQuery(*i, parsedFieldValues, queryWarnings);

					break;
				}

				// log warnings if necessary
				this->log(Config::generalLoggingDefault, queryWarnings);

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
								this->log(
										Config::generalLoggingDefault,
										"WARNING: "
										+ e.whatStr()
										+ " for field \'"
										+ this->config.parsingFieldNames.at(index)
										+ "\' ["
										+ this->urls.front().second
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
										+ this->config.parsingFieldNames.at(index)
										+ "\' ["
										+ this->urls.front().second
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
									+ this->config.parsingFieldNames.at(index)
									+ "\' ["
									+ this->urls.front().second
									+ "]."
							);

							value.clear();
						}
					}
				}

				// if necessary, check whether array or all values are empty
				if(this->config.parsingFieldWarningsEmpty.at(index)) {
					if(
							std::find_if(
									parsedFieldValues.begin(),
									parsedFieldValues.end(),
									[](auto const& value) {
										return !value.empty();
									}
							) == parsedFieldValues.end()
					)
						this->log(
								Config::generalLoggingDefault,
								"WARNING: \'"
								+ this->config.parsingFieldNames.at(index) + "\'"
								" is empty for "
								+ this->urls.front().second
						);
				}

				// determine how to save result: JSON array or concatenate using delimiting character
				if(this->config.parsingFieldJSON.at(index)) {
					// if necessary, tidy texts
					if(dateTimeFormat.empty() && this->config.parsingFieldTidyTexts.at(index))
						for(auto& value : parsedFieldValues)
							Helper::Strings::utfTidy(value);

					// stringify and add parsed elements as JSON array
					parsedData.fields.emplace_back(Helper::Json::stringify(parsedFieldValues));
				}
				else {
					// concatenate elements
					std::string result(
							Helper::Strings::join(
									parsedFieldValues,
									this->config.parsingFieldDelimiters.at(index),
									this->config.parsingFieldIgnoreEmpty.at(index)
							)
						);

					// if necessary, tidy text
					if(dateTimeFormat.empty() && this->config.parsingFieldTidyTexts.at(index))
						Helper::Strings::utfTidy(result);

					parsedData.fields.emplace_back(result);
				}
			}
			else if(i->resultSingle) {
				// parse first element only (as string)
				std::string parsedFieldValue;

				// check query source
				switch(this->config.parsingFieldSources.at(index)) {
				case Config::parsingSourceUrl:
					// parse single field value by using RegEx query on URL
					this->getSingleFromRegEx(*i, this->urls.front().second, parsedFieldValue, queryWarnings);

					break;

				case Config::parsingSourceContent:
					// parse single field value by using query on content
					this->getSingleFromQuery(*i, parsedFieldValue, queryWarnings);

					break;
				}

				// log warnings if necessary
				this->log(Config::generalLoggingDefault, queryWarnings);

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
							this->log(
									Config::generalLoggingDefault,
									"WARNING: "
									+ e.whatStr()
									+ " for field \'"
									+ this->config.parsingFieldNames.at(index)
									+ "\' ["
									+ this->urls.front().second
									+ "]."
							);

							Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
									parsedFieldValue,
									dateTimeFormat
							);
						}
						catch(const DateTimeException& e) {
							this->log(
									Config::generalLoggingDefault,
									"WARNING: "
									+ e.whatStr()
									+ " for field \'"
									+ this->config.parsingFieldNames.at(index)
									+ "\' ["
									+ this->urls.front().second
									+ "]."
							);

							parsedFieldValue.clear();
						}
					}
					catch(const DateTimeException& e) {
						this->log(
								Config::generalLoggingDefault,
								"WARNING: "
								+ e.whatStr()
								+ " for field \'"
								+ this->config.parsingFieldNames.at(index)
								+ "\' ["
								+ this->urls.front().second
								+ "]."
						);

						parsedFieldValue.clear();
					}
				}

				// if necessary, check whether value is empty
				if(
						this->config.parsingFieldWarningsEmpty.at(index)
						&& parsedFieldValue.empty()
				)
					this->log(
							Config::generalLoggingDefault,
							"WARNING: \'"
							+ this->config.parsingFieldNames.at(index) + "\'"
							" is empty for "
							+ this->urls.front().second
					);

				// if necessary, tidy text
				if(dateTimeFormat.empty() && this->config.parsingFieldTidyTexts.at(index))
					Helper::Strings::utfTidy(parsedFieldValue);

				// determine how to save result: JSON array or string as is
				if(this->config.parsingFieldJSON.at(index))
					// stringify and add parsed element as JSON array with one element
					parsedData.fields.emplace_back(Helper::Json::stringify(parsedFieldValue));

				else
					// save as is
					parsedData.fields.emplace_back(parsedFieldValue);
			}
			else if(i->resultBool) {
				// only save whether a match for the query exists
				bool booleanResult = false;

				// check query source
				switch(this->config.parsingFieldSources.at(index)) {
				case Config::parsingSourceUrl:
					// parse boolean field value by using RegEx query on URL
					this->getBoolFromRegEx(*i, this->urls.front().second, booleanResult, queryWarnings);

					break;

				case Config::parsingSourceContent:
					// parse boolean field value by using query on content
					this->getBoolFromQuery(*i, booleanResult, queryWarnings);

					break;
				}

				// log warnings if necessary
				this->log(Config::generalLoggingDefault, queryWarnings);

				// date/time conversion is not possible for boolean values
				if(!dateTimeFormat.empty())
					this->log(
							Config::generalLoggingDefault,
							"WARNING: Cannot convert boolean value for field \'"
							+ this->config.parsingFieldNames.at(index)
							+ "\' to date/time\' ["
							+ this->urls.front().second
							+ "]."
					);

				// determine how to save result: JSON array or boolean value as string
				if(this->config.parsingFieldJSON.at(index))
					// stringify and add parsed element as JSON array with one boolean value as string
					parsedData.fields.emplace_back(
							Helper::Json::stringify(
									booleanResult ? std::string("true") : std::string("false")
							)
					);

				else
					// save boolean value as string
					parsedData.fields.emplace_back(booleanResult ? "true" : "false");
			}
			else {
				if(i->type != QueryStruct::typeNone)
					this->log(
							Config::generalLoggingDefault,
							"WARNING: Ignored query for \'"
							+ this->config.parsingFieldNames.at(index)
							+ "\' without specified result type."
					);

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
	void Thread::parsingUrlFinished() {
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

} /* crawlservpp::Module::Parser */
