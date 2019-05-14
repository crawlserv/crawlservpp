/*
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
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	)
				: Module::Thread(
						dbBase,
						threadOptions,
						threadStatus
				  ),
				  database(this->Module::Thread::database),
				  lastUrl(0),
				  tickCounter(0),
				  startTime(std::chrono::steady_clock::time_point::min()),
				  pauseTime(std::chrono::steady_clock::time_point::min()),
				  idleTime(std::chrono::steady_clock::time_point::min()),
				  idle(false),
				  xmlParsed(false),
				  jsonParsedRapid(false),
				  jsonParsedCons(false),
				  idFirst(0),
				  idDist(0),
				  posFirstF(0.),
				  posDist(0),
				  total(0) {}

	// constructor B: start a new extractor
	Thread::Thread(Main::Database& dbBase, const ThreadOptions& threadOptions)
				: Module::Thread(
						dbBase,
						threadOptions
				  ),
				  database(this->Module::Thread::database),
				  lastUrl(0),
				  tickCounter(0),
				  startTime(std::chrono::steady_clock::time_point::min()),
				  pauseTime(std::chrono::steady_clock::time_point::min()),
				  idleTime(std::chrono::steady_clock::time_point::min()),
				  idle(false),
				  xmlParsed(false),
				  jsonParsedRapid(false),
				  jsonParsedCons(false),
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
		this->loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);

		// show warnings if necessary
		if(this->config.generalLogging) {
			while(!configWarnings.empty()) {
				this->log("WARNING: " + configWarnings.front());

				configWarnings.pop();
			}
		}

		// set database options
		this->setStatusMessage("Setting database options...");

		const bool verbose = this->config.generalLogging == Config::generalLoggingVerbose;

		if(verbose)
			this->log("sets database options...");

		this->database.setLogging(this->config.generalLogging, verbose);
		this->database.setCacheSize(this->config.generalCacheSize);
		this->database.setReextract(this->config.generalReExtract);
		this->database.setExtractCustom(this->config.generalExtractCustom);
		this->database.setTargetTable(this->config.generalResultTable);
		this->database.setTargetFields(this->config.extractingFieldNames);
		this->database.setSleepOnError(this->config.generalSleepMySql);

		// set sources
		this->database.setRawContentIsSource(
				std::find(
						this->config.variablesSource.begin(),
						this->config.variablesSource.end(),
						Config::variablesSourcesContent
				) != this->config.variablesSource.end()
		);

		std::queue<StringString> sources;

		for(unsigned long n = 0; n < this->config.variablesName.size(); ++n)
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

		if(verbose)
			this->log("initializes target table...");

		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("initializes target table...");

		this->database.initTargetTable();

		// prepare SQL statements for extractor
		this->setStatusMessage("Preparing SQL statements...");

		if(verbose)
			this->log("prepares SQL statements...");

		this->database.prepare();

		// initialize queries
		this->setStatusMessage("Initializing custom queries...");

		if(verbose)
			this->log("initializes custom queries...");

		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("initializes custom queries...");

		this->initQueries();

		{
			// wait for extracting table lock
			this->setStatusMessage("Waiting for extracting table...");

			if(verbose)
				this->log("waits for extracting table...");

			DatabaseLock(
					this->database,
					"extractingTable." + this->extractingTable,
					std::bind(&Thread::isRunning, this)
			);

			if(!(this->isRunning()))
				return;

			// check extracting table
			this->setStatusMessage("Checking extracting table...");

			if(verbose)
					this->log("checks extracting table...");

			const unsigned int deleted = this->database.checkExtractingTable();

			if(this->config.generalLogging) {
				switch(deleted) {
				case 0:
					break;

				case 1:
					this->log("WARNING: Deleted a duplicate URL lock.");

					break;

				default:
					std::ostringstream logStrStr;

					logStrStr.imbue(std::locale(""));

					logStrStr << "WARNING: Deleted " << deleted << " duplicate URL locks!";

					this->log(logStrStr.str());
				}
			}
		}

		// save start time and initialize counter
		this->startTime = std::chrono::steady_clock::now();
		this->pauseTime = std::chrono::steady_clock::time_point::min();

		this->tickCounter = 0;
	}

	// extractor tick, throws Thread::Exception
	void Thread::onTick() {
		// TODO: Not fully implemented yet
		throw Exception("Extractor is not implemented yet.");

		bool skip = false;

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
			// no URLs left in database: set timer if just finished, sleep
			if(this->idleTime == std::chrono::steady_clock::time_point::min())
				this->idleTime = std::chrono::steady_clock::now();

			std::this_thread::sleep_for(std::chrono::milliseconds(this->config.generalSleepIdle));

			return;
		}

		// check whether next URL(s) are ought not to be skipped
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
		if(this->config.generalLogging > Config::generalLoggingExtended)
			this->log("extracts data for " + this->urls.front().second + "...");

		// try to renew URL lock
		this->lockTime = this->database.renewUrlLockIfOk(
				this->urls.front().first,
				this->cacheLockTime,
				this->config.generalLock
		);

		skip = this->lockTime.empty();

		if(skip) {
			// skip locked URL
			if(this->config.generalLogging > Config::generalLoggingDefault)
				this->log("skips (locked) " + this->urls.front().second);
		}
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
			const unsigned long extracted = this->extractingNext();

			// save expiration time of URL lock if extracting was successful or unlock URL if extracting failed
			if(extracted)
				this->finished.emplace(this->urls.front().first, this->lockTime);
			else
				// unlock URL if necesssary
				this->database.unLockUrlIfOk(this->urls.front().first, this->lockTime);

			// stop timer
			if(this->config.generalTiming && this->config.generalLogging)
				timerStr = timer.tickStr();

			// reset lock time
			this->lockTime = "";

			// write to log if necessary
			if(
					(this->config.generalLogging > Config::generalLoggingDefault)
					|| (this->config.generalTiming && this->config.generalLogging)
			) {
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

				this->log(logStrStr.str());
			}
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
					(long double) this->tickCounter /
					std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()
					- this->startTime).count();

			tpsStrStr.imbue(std::locale(""));

			tpsStrStr << std::setprecision(2) << std::fixed << tps;

			this->log("average speed: " + tpsStrStr.str() + " ticks per second.");
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

	// initialize queries, throws Thread::Exception
	void Thread::initQueries() {
		// reserve memory for queries
		this->queriesId.reserve(this->config.extractingIdQueries.size());
		this->queriesDateTime.reserve(this->config.extractingDateTimeQueries.size());
		this->queriesFields.reserve(this->config.extractingFieldQueries.size());
		this->queriesVariables.reserve(this->config.variablesQuery.size());
		this->queriesTokens.reserve(this->config.variablesTokensQuery.size());

		try {
			// create queries and get query IDs
			for(const auto& query : this->config.extractingIdQueries) {
				QueryProperties properties;

				this->database.getQueryProperties(query, properties);

				this->queriesId.emplace_back(this->addQuery(properties));
			}

			for(const auto& query : this->config.extractingDateTimeQueries) {
				QueryProperties properties;

				this->database.getQueryProperties(query, properties);

				this->queriesDateTime.emplace_back(this->addQuery(properties));
			}

			for(const auto& query : this->config.extractingFieldQueries) {
				QueryProperties properties;

				this->database.getQueryProperties(query, properties);

				this->queriesFields.emplace_back(this->addQuery(properties));
			}

			for(const auto& query : this->config.variablesQuery) {
				QueryProperties properties;

				this->database.getQueryProperties(query, properties);

				this->queriesVariables.emplace_back(this->addQuery(properties));
			}

			for(const auto& query : this->config.variablesTokensQuery) {
				QueryProperties properties;

				this->database.getQueryProperties(query, properties);

				this->queriesTokens.emplace_back(this->addQuery(properties));
			}

			QueryStruct queryPagingIsNextFrom;
			QueryStruct queryPagingNextFrom;
			QueryStruct queryPagingNumberFrom;
			QueryStruct queryExpected;

			QueryProperties properties;

			this->database.getQueryProperties(this->config.pagingIsNextFrom, properties);

			this->queryPagingIsNextFrom = this->addQuery(properties);

			this->database.getQueryProperties(this->config.pagingNextFrom, properties);

			this->queryPagingNextFrom = this->addQuery(properties);

			this->database.getQueryProperties(this->config.pagingNumberFrom, properties);

			this->queryPagingNumberFrom = this->addQuery(properties);

			this->database.getQueryProperties(this->config.expectedQuery, properties);

			this->queryExpected = this->addQuery(properties);
		}
		catch(const RegExException& e) {
			throw Exception("Extractor::Thread::initQueries(): [RegEx] " + e.whatStr());
		}
		catch(const XPathException& e) {
			throw Exception("Extractor::Thread::initQueries(): [XPath] " + e.whatStr());
		}
		catch(const JsonPointerException& e) {
			throw Exception("Extractor::Thread::initQueries(): [JSONPointer] " + e.whatStr());
		}
		catch(const JsonPathException& e) {
			throw Exception("Extractor::Thread::initQueries(): [JSONPath] " + e.whatStr());
		}
	}

	// URL selection
	void Thread::extractingUrlSelection() {
		Timer::Simple timer;

		// get number of URLs
		this->total = this->database.getNumberOfUrls();

		this->setStatusMessage("Fetching URLs...");

		// fill cache with next URLs
		if(this->config.generalLogging > Config::generalLoggingDefault)
			this->log("fetches URLs...");

		// get next URL(s)
		this->extractingFetchUrls();

		// write to log if necessary
		if(this->config.generalTiming && this->config.generalLogging)
			this->log("fetched URLs in " + timer.tickStr());

		// update status
		this->setStatusMessage("Checking URLs...");

		// check whether URLs have been fetched
		if(this->urls.empty()) {
			// no more URLs to extract data from
			if(!(this->idle)) {
				if(this->config.generalLogging > Config::generalLoggingDefault)
					this->log("finished.");

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

		const unsigned long posFirst = this->database.getUrlPosition(this->idFirst);

		this->posFirstF = static_cast<float>(posFirst);
		this->posDist = this->database.getUrlPosition(this->urls.back().first) - posFirst;
	}

	// check whether next URL(s) ought to be skipped
	void Thread::extractingCheckUrls() {
		// loop over next URLs in cache
		while(!(this->urls.empty()) && this->isRunning()) {
			// check whether URL needs to be skipped because of invalid ID
			if(!(this->urls.front().first)) {
				if(this->config.generalLogging)
					this->log("skips (INVALID ID) " + this->urls.front().second);

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
	unsigned long Thread::extractingNext() {
		unsigned long extracted = 0;
		std::vector<StringString> globalTokens;

		// loop over variables
		for(auto i = this->config.variablesName.begin(); i != this->config.variablesName.end(); ++i) {
			const auto source =
					this->config.variablesSource.begin()
					+ (i - this->config.variablesName.begin());

			switch(*source) {
			case Config::variablesSourcesParsed:
				// TODO
				break;

			case Config::variablesSourcesContent:
				// TODO
				break;

			case Config::variablesSourcesUrl:
				// TODO
				break;
			}
		}

		// loop over global tokens
		if(!(this->config.pagingVariable.empty()))
			for(unsigned long n = 0; n < this->config.variablesTokens.size(); ++n) {
				if(
						this->config.variablesTokensSource.at(n).find(
								this->config.pagingVariable
						) == std::string::npos
				) {
					// get global token
					// TODO
				}
			}

		// loop over pages
		long pageNum = this->config.pagingFirst;
		std::string pageName(this->config.pagingFirstString);
		bool pageFirst = true;

		while(this->isRunning()) {
			// loop over page-specific tokens

			// resolve paging variable

			// get page
		}

		return false;
	}

	// extract data by parsing content, return number of extracted datasets
	unsigned long Thread::extractingParse(unsigned long contentId, const std::string& content) {
		DataEntry extractedData(contentId);

		// reset parsing state
		this->resetParsingState();

		// extract IDs
		unsigned long idQueryCounter = 0;

		for(const auto& query : this->queriesId) {
			// check result type of query
			if(query.resultMulti)
				// check query type
				switch(query.type) {
				case QueryStruct::typeRegEx:
					// extract ID by running RegEx query on content
					try {
						this->getRegExQuery(query.index).getFirst(content, extractedData.dataId);

						if(!extractedData.dataId.empty())
							break;
					}
					catch(const RegExException& e) {} // ignore query on error

					break;

				case QueryStruct::typeXPath:
					// parse HTML/XML if still necessary
					if(this->parseXml(content)) {
						// extract ID by running XPath query on content
						try {
							this->getXPathQuery(query.index).getFirst(this->parsedXML, extractedData.dataId);

							if(!extractedData.dataId.empty())
								break;
						}
						catch(const XPathException& e) {} // ignore query on error
					}

					break;

				case QueryStruct::typeJsonPointer:
					// parse JSON using RapidJSON if still necessary
					if(this->parseJsonRapid(content)) {
						// extract ID by running JSONPointer query on content
						try {
							this->getJsonPointerQuery(query.index).getFirst(this->parsedJsonRapid, extractedData.dataId);

							if(!extractedData.dataId.empty())
								break;
						}
						catch(const JsonPointerException& e) {} // ignore query on error
					}

					break;

				case QueryStruct::typeJsonPath:
					// parse JSON using jsoncons if still necessary
					if(this->parseJsonCons(content)) {
						// extract ID by running JSONPath query on content
						try {
							this->getJsonPathQuery(query.index).getFirst(this->parsedJsonCons, extractedData.dataId);

							if(!extractedData.dataId.empty())
								break;
						}
						catch(const JsonPathException& e) {} // ignore query on error
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.generalLogging)
						this->log("WARNING: Unknown type of ID query on content.");
				}
			else if(this->config.generalLogging)
				this->log("WARNING: Invalid result type of ID query (not single).");

			if(extractedData.dataId.empty())
				// not successfull: check next query for parsing the ID (if it exists)
				++idQueryCounter;
			else
				break;
		}

		// check whether no ID has been extracted
		if(extractedData.dataId.empty()) {
			this->logParsingErrors(contentId);

			return false;
		}

		// extract date/time
		unsigned long dateTimeQueryCounter = 0;
		bool dateTimeSuccess = false;

		for(const auto& query : this->queriesDateTime) {
			bool querySuccess = false;

			// check result type of query
			if(!query.resultSingle) {
				if(this->config.generalLogging)
					this->log("WARNING: Invalid result type of DateTime query (not single).");

				continue;
			}

			// check query type
			switch(query.type) {
			case QueryStruct::typeRegEx:
				// parse date/time by running RegEx query on content
				try {
					this->getRegExQuery(query.index).getFirst(content, extractedData.dateTime);

					querySuccess = true;
				}
				catch(const RegExException& e) {} // ignore query on error

				break;

			case QueryStruct::typeXPath:
				// parse XML/HTML if still necessary
				if(this->parseXml(content)) {
					// parse date/time by running XPath query on parsed content
					try {
						this->getXPathQuery(query.index).getFirst(this->parsedXML, extractedData.dateTime);

						querySuccess = true;
					}
					catch(const XPathException& e) {} // ignore query on error
				}

				break;

			case QueryStruct::typeJsonPointer:
				// parse JSON using RapidJSON if still necessary
				if(this->parseJsonRapid(content)) {
					// parse date/time by running JSONPointer query on parsed content
					try {
						this->getJsonPointerQuery(query.index).getFirst(this->parsedJsonRapid, extractedData.dateTime);

						querySuccess = true;
					}
					catch(const JsonPointerException& e) {} // ignore query on error
				}

				break;

			case QueryStruct::typeJsonPath:
				// parse JSON using jsoncons if still necessary
				if(this->parseJsonCons(content)) {
					// parse date/time by running JSONPointer query on parsed content
					try {
						this->getJsonPathQuery(query.index).getFirst(this->parsedJsonCons, extractedData.dateTime);

						querySuccess = true;
					}
					catch(const JsonPathException& e) {} // ignore query on error
				}

				break;

			case QueryStruct::typeNone:
				break;

			default:
				if(this->config.generalLogging)
					this->log("WARNING: Unknown type of DateTime query on content.");
			}

			if(querySuccess && !extractedData.dateTime.empty()) {
				// found date/time: try to convert it to SQL time stamp
				std::string format(this->config.extractingDateTimeFormats.at(dateTimeQueryCounter));
				const std::string locale(this->config.extractingDateTimeLocales.at(dateTimeQueryCounter));

				// use "%F %T" as default date/time format
				if(format.empty())
					format = "%F %T";

				if(!locale.empty()) {
					// locale hack: The French abbreviation "avr." for April is not stringently supported
					if(locale.length() > 1
							&& ::tolower(locale.at(0) == 'f')
							&& ::tolower(locale.at(1) == 'r'))
						Helper::Strings::replaceAll(extractedData.dateTime, "avr.", "avril", true);

					try {
						dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								extractedData.dateTime, format, std::locale(locale)
						);
					}
					catch(const std::runtime_error& e) {
						if(this->config.generalLogging)
							this->log("WARNING: Unknown locale \'" + locale + "\' ignored.");

						dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								extractedData.dateTime, format
						);
					}
				}
				else
					dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
							extractedData.dateTime, format
					);

				if(dateTimeSuccess && !extractedData.dateTime.empty())
					break;
			}

			// not successfull: check next query for parsing the date/time (if exists)
			++dateTimeQueryCounter;
		}

		// check whether date/time conversion was successful
		if(!extractedData.dateTime.empty() && !dateTimeSuccess) {
			if(this->config.generalLogging)
				this->log(
						"ERROR: Could not extract date/time \'"
						+ extractedData.dateTime
						+ "\' from "
						+ this->urls.front().second
				);

			extractedData.dateTime = "";
		}

		// parse custom fields
		unsigned long fieldCounter = 0;

		extractedData.fields.reserve(this->queriesFields.size());

		for(const auto& query : this->queriesFields) {
			try {
				// determinate whether to get the match as string or as boolean value
				if(query.resultSingle) {
					// parse first element only (as string)
					std::string parsedFieldValue;

					// check query type
					switch(query.type) {
					case QueryStruct::typeRegEx:
						// parse single field element by running RegEx query on content string
						this->getRegExQuery(query.index).getFirst(content, parsedFieldValue);

						break;

					case QueryStruct::typeXPath:
						// parse XML/HTML if still necessary
						if(this->parseXml(content))
							// parse single field element by running XPath query on parsed content
							this->getXPathQuery(query.index).getFirst(this->parsedXML, parsedFieldValue);

						break;

					case QueryStruct::typeJsonPointer:
						// parse JSON using RapidJSON if still necessary
						if(this->parseJsonRapid(content))
							// parse single field element by running JSONPointer query on content string
							this->getJsonPointerQuery(query.index).getFirst(this->parsedJsonRapid, parsedFieldValue);

						break;

					case QueryStruct::typeJsonPath:
						// parse JSON using jsoncons if still necessary
						if(this->parseJsonCons(content))
							// parse single field element by running JSONPath query on content string
							this->getJsonPathQuery(query.index).getFirst(this->parsedJsonCons, parsedFieldValue);

						break;

					case QueryStruct::typeNone:
						break;

					default:
						if(this->config.generalLogging)
							this->log(
									"WARNING: \'" + this->config.extractingFieldNames.at(fieldCounter) + "\'"
									" query on content is of unknown type."
							);
					}

					// if necessary, check whether value is empty
					if(
							this->config.extractingFieldWarningsEmpty.at(fieldCounter)
							&& parsedFieldValue.empty()
							&& this->config.generalLogging
					)
						this->log(
								"WARNING: \'" + this->config.extractingFieldNames.at(fieldCounter) + "\'"
								" is empty for " + this->urls.front().second
						);

					// if necessary, tidy text
					if(this->config.extractingFieldTidyTexts.at(fieldCounter))
						Helper::Strings::utfTidy(parsedFieldValue);

					// stringify and add parsed element as JSON array with one element
					extractedData.fields.emplace_back(Helper::Json::stringify(parsedFieldValue));
				}
				else if(query.resultBool) {
					// only save whether a match for the query exists
					bool parsedBool = false;

					// check query type
					switch(query.type) {
					case QueryStruct::typeRegEx:
						// parse boolean value by running RegEx query on content string
						parsedBool = this->getRegExQuery(query.index).getBool(content);

						break;

					case QueryStruct::typeXPath:
						// parse XML/HTML if still necessary
						if(this->parseXml(content))
							// parse boolean value by running XPath query on parsed content
							parsedBool = this->getXPathQuery(query.index).getBool(this->parsedXML);

						break;

					case QueryStruct::typeJsonPointer:
						// parse JSON using RapidJSON if still necessary
						if(this->parseJsonRapid(content))
							// parse boolean value by running JSONPointer query on content string
							parsedBool = this->getJsonPointerQuery(query.index).getBool(this->parsedJsonRapid);

						break;

					case QueryStruct::typeJsonPath:
						// parse JSON using jsoncons if still necessary
						if(this->parseJsonCons(content))
							// parse boolean value by running JSONPath query on content string
							parsedBool = this->getJsonPathQuery(query.index).getBool(this->parsedJsonCons);

						break;

					case QueryStruct::typeNone:
						break;

					default:
						if(this->config.generalLogging)
							this->log(
									"WARNING: \'" + this->config.extractingFieldNames.at(fieldCounter) + "\'"
									" query on content is of unknown type."
							);
					}

					// stringify and add parsed element as JSON array with one boolean value as string
					extractedData.fields.emplace_back(Helper::Json::stringify(parsedBool ? "true" : "false"));
				}
				else {
					if(query.type != QueryStruct::typeNone && this->config.generalLogging)
						this->log(
								"WARNING: Ignored \'" + this->config.extractingFieldNames.at(fieldCounter) + "\'"
								" query without specified result type."
						);

					extractedData.fields.emplace_back();
				}

				++fieldCounter;
			}
			catch(const RegExException& e) {} // ignore query on RegEx error
			catch(const XPathException& e) {} // ignore query on XPath error
			catch(const JsonPointerException& e) {} // ignore query on JSONPointer error
			catch(const JsonPathException& e) {} // ignore query on JSONPath error
		}

		// show parsing errors if necessary
		this->logParsingErrors(contentId);

		// add parsed data to results
		this->results.push(extractedData);

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

			if(this->config.generalLogging > Config::generalLoggingDefault)
				this->log("saves results...");

			// update or add entries in/to database
			this->database.updateOrAddEntries(this->results);

			// update parsing table
			this->database.updateTargetTable();
		} // target table unlocked

		// set last URL
		if(!warped)
			this->setLast(this->lastUrl);

		// set those URLs to finished whose URL lock is okay (still locked or re-lockable (and not parsed when not re-parsing)
		this->database.setUrlsFinishedIfLockOk(this->finished);

		// update status
		this->setStatusMessage("Results saved. [" + status + "]");

		if(this->config.generalTiming && this->config.generalLogging)
			this->log("saved results in " + timer.tickStr());
	}

	// parse XML/HTML if still necessary, return false if parsing failed
	bool Thread::parseXml(const std::string& content) {
		if(!(this->xmlParsed) && this->xmlParsingError.empty()) {
			std::queue<std::string> warnings;

			try {
				this->parsedXML.parse(content, warnings, this->config.extractingRepairCData);

				this->xmlParsed = true;
			}
			catch(const XMLException& e) {
				this->xmlParsingError = e.whatStr();
			}

			if(this->config.generalLogging)
				while(!warnings.empty()) {
					this->log(warnings.front());

					warnings.pop();
				}
		}

		return this->xmlParsed;
	}

	// parse JSON using RapidJSON if still necessary, return false if parsing failed
	bool Thread::parseJsonRapid(const std::string& content) {
		if(!(this->jsonParsedRapid) && this->jsonParsingError.empty()) {
			try {
				this->parsedJsonRapid = Helper::Json::parseRapid(content);

				this->jsonParsedRapid = true;
			}
			catch(const JsonException& e) {
				this->jsonParsingError = e.whatStr();
			}
		}

		return this->jsonParsedRapid;
	}

	// parse JSON using jsoncons if still necessary, return false if parsing failed
	bool Thread::parseJsonCons(const std::string& content) {
		if(!(this->jsonParsedCons) && this->jsonParsingError.empty()) {
			try {
				this->parsedJsonCons = Helper::Json::parseCons(content);

				this->jsonParsedCons = true;
			}
			catch(const JsonException& e) {
				this->jsonParsingError = e.whatStr();
			}
		}

		return this->jsonParsedCons;
	}

	// reset parsing state
	void Thread::resetParsingState() {
		this->xmlParsed = false;
		this->jsonParsedRapid = false;
		this->jsonParsedCons = false;

		this->xmlParsingError.clear();
		this->jsonParsingError.clear();
	}

	// write parsing errors as warnings to log if necessary
	void Thread::logParsingErrors(unsigned long contentId) {
		if(this->config.generalLogging) {
			if(!(this->xmlParsingError.empty())) {
				std::ostringstream logStrStr;

				logStrStr	<< "WARNING: "
							<< this->xmlParsingError
							<< " [content #"
							<< contentId
							<< " - "
							<< this->urls.front().second
							<< "].";

				this->log(logStrStr.str());
			}

			if(!(this->jsonParsingError.empty())) {
				std::ostringstream logStrStr;

				logStrStr	<< "WARNING: "
							<< this->jsonParsingError
							<< " [content #"
							<< contentId
							<< " - "
							<< this->urls.front().second
							<< "].";

				this->log(logStrStr.str());
			}
		}
	}

} /* crawlservpp::Module::Extractor */
