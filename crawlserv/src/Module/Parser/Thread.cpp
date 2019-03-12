/*
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
			unsigned long parserId,
			const std::string& parserStatus,
			bool parserPaused,
			const ThreadOptions& threadOptions,
			unsigned long parserLast
	)
				: Module::Thread(
						dbBase,
						parserId,
						"parser",
						parserStatus,
						parserPaused,
						threadOptions,
						parserLast
				  ),
				  targetTableAlias("a"),
				  database(this->Module::Thread::database, this->targetTableAlias),
				  idFromUrl(false),
				  tickCounter(0),
				  startTime(std::chrono::steady_clock::time_point::min()),
				  pauseTime(std::chrono::steady_clock::time_point::min()),
				  idleTime(std::chrono::steady_clock::time_point::min()),
				  idle(false),
				  idFirst(0),
				  idDist(0),
				  posFirstF(0.),
				  posDist(0),
				  total(0) {}

	// constructor B: start a new parser
	Thread::Thread(Main::Database& dbBase, const ThreadOptions& threadOptions)
				: Module::Thread(dbBase, "parser", threadOptions),
				  targetTableAlias("a"),
				  database(this->Module::Thread::database, this->targetTableAlias),
				  idFromUrl(false),
				  tickCounter(0),
				  startTime(std::chrono::steady_clock::time_point::min()),
				  pauseTime(std::chrono::steady_clock::time_point::min()),
				  idleTime(std::chrono::steady_clock::time_point::min()),
				  idle(false),
				  idFirst(0),
				  idDist(0),
				  posFirstF(0.),
				  posDist(0),
				  total(0) {}

	// destructor stub
	Thread::~Thread() {}

	// initialize parser
	void Thread::onInit(bool resumed) {
		std::queue<std::string> configWarnings;
		std::vector<std::string> fields;

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

		// set database options
		this->setStatusMessage("Setting database options...");

		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("sets database options...");

		this->database.setCacheSize(this->config.generalCacheSize);
		this->database.setReparse(this->config.generalReParse);
		this->database.setParseCustom(this->config.generalParseCustom);
		this->database.setLogging(this->config.generalLogging);
		this->database.setVerbose(config.generalLogging == Config::generalLoggingVerbose);
		this->database.setTargetTable(this->config.generalResultTable);
		this->database.setTargetFields(this->config.parsingFieldNames);
		this->database.setSleepOnError(this->config.generalSleepMySql);
		this->database.setTimeoutTargetLock(this->config.generalTimeoutTargetLock);

		// create table names for locking
		std::string urlListTable("crawlserv_" + this->websiteNamespace + "_" + this->urlListNamespace);

		this->parsingTable = urlListTable + "_parsing";
		this->targetTable = urlListTable + "_parsed_" + this->config.generalResultTable;

		// initialize target table
		this->setStatusMessage("Initialiting target table...");

		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("initializes target table...");

		this->database.initTargetTable(std::bind(&Thread::isRunning, this)); // @suppress("Invalid arguments")

		// prepare SQL statements for parser
		this->setStatusMessage("Preparing SQL statements...");

		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("prepares SQL statements...");

		this->database.prepare();

		// initialize queries
		this->setStatusMessage("Initialiting custom queries...");

		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("initializes custom queries...");

		this->initQueries();

		// check whether ID can be parsed from URL only
		this->setStatusMessage("Checking for URL-only parsing...");

		if(config.generalLogging == Config::generalLoggingVerbose)
			this->log("checks for URL-only parsing...");

		this->idFromUrl = true;

		for(auto i = this->config.parsingIdSources.begin();
				i != this->config.parsingIdSources.end(); ++i) {
			if(*i == Config::parsingSourceContent) {
				this->idFromUrl = false;
				break;
			}
		}

		// save start time and initialize counter
		this->startTime = std::chrono::steady_clock::now();
		this->pauseTime = std::chrono::steady_clock::time_point::min();
		this->tickCounter = 0;
	}

	// parser tick
	void Thread::onTick() {
		bool skip = false;
		unsigned long parsed = 0;

		// URL selection
		if(this->urls.empty())
			this->parsingUrlSelection();

		if(this->urls.empty()) {
			// no URLs left: set timer if just finished, sleep
			if(this->idleTime == std::chrono::steady_clock::time_point::min())
				this->idleTime = std::chrono::steady_clock::now();

			std::this_thread::sleep_for(std::chrono::milliseconds(this->config.generalSleepIdle));
			return;
		}

		// update timers
		if(this->idleTime > std::chrono::steady_clock::time_point::min()) {
			// idling stopped
			this->startTime += std::chrono::steady_clock::now() - this->idleTime;
			this->pauseTime = std::chrono::steady_clock::time_point::min();
			this->idleTime = std::chrono::steady_clock::time_point::min();
		}

		// increase tick counter
		this->tickCounter++;

		// write log entry if necessary
		if(this->config.generalLogging > Config::generalLoggingExtended)
			this->log("parses " + this->urls.front().url + "...");

		// check whether URL needs to be skipped because it is locked
		{ // lock parsing table
			TableLock parsingTableLock(this->database, TableLockProperties(this->parsingTable));

			// get current URL lock ID
			this->database.getLockId(this->urls.front());

			// try to lock URL
			this->lockTime = this->database.lockUrlIfOk(this->urls.front(), this->config.generalLock);
			skip = this->lockTime.empty();
		} // parsing table unlocked

		if(skip) {
			// skip locked URL
			if(this->config.generalLogging > Config::generalLoggingDefault)
				this->log("skip (locked) " + this->urls.front().url);
		}
		else {
			// set status
			this->setStatusMessage(this->urls.front().url);

			// approximate progress
			if(!(this->total))
				throw std::runtime_error("Parser::Thread::onTick(): Could not get URL list size");

			if(this->idDist) {
				float cacheProgress = static_cast<float>(this->urls.front().id - this->idFirst) / this->idDist;
					// cache progress = (current ID - first ID) / (last ID - first ID)
				float approxPosition = this->posFirstF + cacheProgress * this->posDist;
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
			if(this->config.generalTiming && this->config.generalLogging)
				timerStr = timer.tickStr();

			// save URL lock ID and expiration time if parsing was successful or unlock URL if parsing failed
			if(parsed)
				this->finished.emplace(this->urls.front().lockId, this->lockTime);
			else
				// unlock URL if necesssary
				this->database.unLockUrlIfOk(this->urls.front().lockId, this->lockTime);

			// reset lock time
			this->lockTime = "";

			// write to log if necessary
			if((this->config.generalLogging > Config::generalLoggingDefault)
					|| (this->config.generalTiming && this->config.generalLogging)) {
				std::ostringstream logStrStr;
				logStrStr.imbue(std::locale(""));

				if(parsed > 1)
					logStrStr << "parsed " << parsed << " versions of ";
				else if(parsed == 1)
					logStrStr << "parsed ";
				else
					logStrStr << "skipped ";

				logStrStr << this->urls.front().url;

				if(this->config.generalTiming)
					logStrStr << " in " << timerStr;

				this->log(logStrStr.str());
			}
		}

		// URL finished
		this->parsingUrlFinished();
	}

	// parser paused
	void Thread::onPause() {
		// save pause start time
		this->pauseTime = std::chrono::steady_clock::now();

		// save results if necessary
		this->parsingSaveResults();
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
	void Thread::onClear(bool interrupted) {
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

			long double tps =
					(long double) this->tickCounter /
					std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()
					- this->startTime).count();

			tpsStrStr.imbue(std::locale(""));
			tpsStrStr << std::setprecision(2) << std::fixed << tps;

			this->log("average speed: " + tpsStrStr.str() + " ticks per second.");
		}

		// save results if necessary
		this->parsingSaveResults();

		// delete queries
		this->queriesSkip.clear();
		this->queriesDateTime.clear();
		this->queriesFields.clear();
		this->queriesId.clear();
		this->clearQueries();
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

	// initialize queries
	void Thread::initQueries() {
		// reserve memory for query ids
		this->queriesId.reserve(this->config.parsingIdQueries.size());
		this->queriesDateTime.reserve(this->config.parsingDateTimeQueries.size());
		this->queriesFields.reserve(this->config.parsingFieldQueries.size());
		this->reserveForQueries(this->config.parsingIdQueries.size() + this->config.parsingDateTimeQueries.size()
				+ this->config.parsingFieldQueries.size());

		// create queries and get query ids
		for(auto i = this->config.generalSkip.begin(); i != this->config.generalSkip.end(); ++i) {
			QueryProperties properties;
			this->database.getQueryProperties(*i, properties);
			this->queriesSkip.emplace_back(this->addQuery(properties));
		}

		for(auto i = this->config.parsingIdQueries.begin(); i != this->config.parsingIdQueries.end(); ++i) {
			QueryProperties properties;
			this->database.getQueryProperties(*i, properties);
			this->queriesId.emplace_back(this->addQuery(properties));
		}

		for(auto i = this->config.parsingDateTimeQueries.begin(); i != this->config.parsingDateTimeQueries.end(); ++i) {
			QueryProperties properties;
			this->database.getQueryProperties(*i, properties);
			this->queriesDateTime.emplace_back(this->addQuery(properties));
		}

		for(auto i = this->config.parsingFieldQueries.begin(); i != this->config.parsingFieldQueries.end(); ++i) {
			QueryProperties properties;
			this->database.getQueryProperties(*i, properties);
			this->queriesFields.emplace_back(this->addQuery(properties));
		}
	}

	// parsing function for URL selection, return whether there are URLs left to parse
	void Thread::parsingUrlSelection() {
		Timer::Simple timer;

		// fill cache with next URLs
		this->setStatusMessage("Fetching URLs...");

		if(this->config.generalLogging > Config::generalLoggingDefault)
			this->log("fetches URLs...");

		this->parsingFetchUrls();

		if(this->config.generalTiming && this->config.generalLogging)
			this->log("fetched URLs in " + timer.tickStr());

		// check whether URLs have been fetched
		this->setStatusMessage("Checking URLs...");

		if(this->urls.empty()) {
			// no more URLs to parse
			if(!(this->idle)) {
				if(this->config.generalResetOnFinish) {
					if(this->config.generalLogging)
						this->log("finished, resetting parsing status...");

					this->database.resetParsingStatus(this->getUrlList());
				}
				else if(this->config.generalLogging > Config::generalLoggingDefault)
					this->log("finished.");

				this->setStatusMessage("IDLE Waiting for new URLs to parse.");
				this->setProgress(1.f);
			}

			return;
		}
		else // reset idling status
			this->idle = false;

		// check whether next URL(s) need to be skipped
		while(!(this->urls.empty()) && this->isRunning()) {
			// check whether URL needs to be skipped because of invalid ID
			if(!(this->urls.front().id)) {
				if(this->config.generalLogging)
					this->log("skip (INVALID ID) " + this->urls.front().url);

				this->parsingUrlFinished();
				continue;
			}

			// check whether URL needs to be skipped because of custom query
			bool skip = false;

			if(!(this->config.generalSkip.empty())) {
				// loop over custom queries
				for(auto i = this->queriesSkip.begin(); i != this->queriesSkip.end(); ++i) {
					// check result type of query
					if(!(i->resultBool) && this->config.generalLogging)
						this->log("WARNING: Invalid result type of skip query (not bool).");

					// check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse ID by running RegEx query on URL
						try {
							if(this->getRegExQueryPtr(i->index).getBool(this->urls.front().url)) {
								// skip URL
								skip = true;
								break; // exit loop over custom queries
							}
						}
						catch(const RegExException& e) {} // ignore query on error
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: ID query on URL is not of type RegEx.");
				}

				if(skip) {
					// skip URL because of query
					if(this->config.generalLogging > Config::generalLoggingDefault)
						this->log("skip (query) " + urls.front().url);

					this->parsingUrlFinished();
					continue;
				}
			}

			break; // found URL to process
		}

		// done
		if(this->config.generalTiming && this->config.generalLogging)
			this->log("checked URLs in " + timer.tickStr());

		this->setStatusMessage("URLs fetched.");
	}

	// fetch next URLs from database
	void Thread::parsingFetchUrls() {
		// fetch URLs from database to cache
		this->database.fetchUrls(this->getLast(), this->urls);

		// check whether URLs have been fetched
		if(this->urls.empty())
			return;

		// save properties of fetched URLs and URL list for progress calculation
		this->idFirst = this->urls.front().id;
		this->idDist = this->urls.back().id - this->idFirst;

		unsigned long posFirst = this->database.getUrlPosition(this->idFirst);
		this->posFirstF = static_cast<float>(posFirst);
		this->posDist = this->database.getUrlPosition(this->urls.back().id) - posFirst;
		this->total = this->database.getNumberOfUrls();
	}

	// parse content(s) of next URL, return number of successfully parsed contents
	unsigned long Thread::parsingNext() {
		std::string parsedId;

		// parse ID from URL if possible (using RegEx only)
		if(this->idFromUrl) {
			for(auto i = this->queriesId.begin(); i != this->queriesId.end(); ++i) {
				// check result type of query
				if(!(i->resultSingle) && (this->config.generalLogging))
					this->log("WARNING: Invalid result type of ID query (not single).");

				// check query type
				if(i->type == QueryStruct::typeRegEx) {
					// parse ID by running RegEx query on URL
					try {
						this->getRegExQueryPtr(i->index).getFirst(this->urls.front().url, parsedId);

						if(!parsedId.empty())
							break;
					}
					catch(const RegExException& e) {} // ignore query on error
				}
				else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: ID query on URL is not of type RegEx.");
			}

			// check ID
			if(parsedId.empty())
				return 0;

			if(!(this->config.parsingIdIgnore.empty()) && std::find(this->config.parsingIdIgnore.begin(),
					this->config.parsingIdIgnore.end(),	parsedId) != this->config.parsingIdIgnore.end())
				return 0;
		}

		if(this->config.generalNewestOnly) {
			// parse newest content of URL
			unsigned long index = 0;

			while(true) {
				std::pair<unsigned long, std::string> latestContent;

				if(this->database.getLatestContent(this->urls.front().id, index, latestContent)) {
					if(this->parsingContent(latestContent, parsedId))
						return 1;

					index++;
				}
				else
					break;
			}
		}
		else {
			// parse all contents of URL
			unsigned long counter = 0;

			std::queue<IdString> contents = this->database.getAllContents(this->urls.front().id);

			while(!contents.empty()) {
				if(this->parsingContent(contents.front(), parsedId))
					counter++;
				contents.pop();
			}

			return counter;
		}

		return 0;
	}

	// parse ID-specific content, return whether parsing was successfull (i.e. an ID could be parsed)
	bool Thread::parsingContent(const std::pair<unsigned long, std::string>& content, const std::string& parsedId) {
		ParsingEntry parsedData(content.first);
		Parsing::XML parsedContent;

		// parse HTML
		try {
			parsedContent.parse(content.second);
		}
		catch(const XMLException& e) {
			if(this->config.generalLogging > Config::generalLoggingDefault) {
				std::ostringstream logStrStr;

				logStrStr	<< "Content #" << content.first
							<< " [" << this->urls.front().url << "] could not be parsed: "
							<< e.what();

				this->log(logStrStr.str());
			}

			return false;
		}

		// parse ID (if still necessary)
		if(this->idFromUrl) parsedData.parsedId = parsedId;
		else {
			unsigned long idQueryCounter = 0;

			for(auto i = this->queriesId.begin(); i != this->queriesId.end(); ++i) {
				// check result type of query
				if(!(i->resultSingle) && this->config.generalLogging)
					this->log("WARNING: Invalid result type of ID query (not single).");

				// check query source
				if(this->config.parsingIdSources.at(idQueryCounter) == Config::parsingSourceUrl) {
					// check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse ID by running RegEx query on URL
						try {
							this->getRegExQueryPtr(i->index).getFirst(this->urls.front().url, parsedData.parsedId);

							if(!parsedData.parsedId.empty())
								break;
						}
						catch(const RegExException& e) {} // ignore query on error
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: ID query on URL is not of type RegEx.");
				}
				else {
					// check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse ID by running RegEx query on content string
						try {
							this->getRegExQueryPtr(i->index).getFirst(content.second, parsedData.parsedId);

							if(!parsedData.parsedId.empty())
								break;
						}
						catch(const RegExException& e) {} // ignore query on error
					}
					else if(i->type == QueryStruct::typeXPath) {
						// parse ID by running XPath query on parsed content
						try {
							this->getXPathQueryPtr(i->index).getFirst(parsedContent, parsedData.parsedId);

							if(!parsedData.parsedId.empty())
								break;
						}
						catch(const RegExException& e) {} // ignore query on error
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: ID query on content is not of type RegEx or XPath.");
				}

				// not successfull: check next query for parsing the ID (if it exists)
				idQueryCounter++;
			}
		}

		// check whether no ID has been parsed
		if(parsedData.parsedId.empty())
			return false;

		// check whether parsed ID is ought to be ignored
		if(!(this->config.parsingIdIgnore.empty())
				&& std::find(
						this->config.parsingIdIgnore.begin(),
						this->config.parsingIdIgnore.end(),
						parsedData.parsedId
				) != this->config.parsingIdIgnore.end())
			return false;

		// check whether parsed ID already exists and the current content ID differs from the one in the database
		unsigned long contentId = this->database.getContentIdFromParsedId(parsedData.parsedId);

		if(contentId && contentId != content.first) {
			if(this->config.generalLogging)
				this->log("WARNING: Content for parsed ID '" + parsedData.parsedId + "' already exists.");

			return false;
		}

		// parse date/time
		unsigned long dateTimeQueryCounter = 0;
		bool dateTimeSuccess = false;

		for(auto i = this->queriesDateTime.begin(); i != this->queriesDateTime.end(); ++i) {
			bool querySuccess = false;

			// check result type of query
			if(!(i->resultSingle) && this->config.generalLogging)
				this->log("WARNING: Invalid result type of DateTime query (not single).");

			// check query source
			if(this->config.parsingDateTimeSources.at(dateTimeQueryCounter) == Config::parsingSourceUrl) {
				// check query type
				if(i->type == QueryStruct::typeRegEx) {
					// parse date/time by running RegEx query on URL
					try {
						this->getRegExQueryPtr(i->index).getFirst(this->urls.front().url, parsedData.dateTime);

						querySuccess = true;
					}
					catch(const RegExException& e) {} // ignore query on error
				}
				else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: DateTime query on URL is not of type RegEx.");
			}
			else {
				// check query type
				if(i->type == QueryStruct::typeRegEx) {
					// parse date/time by running RegEx query on content string
					try {
						this->getRegExQueryPtr(i->index).getFirst(content.second, parsedData.dateTime);

						querySuccess = true;
					}
					catch(const RegExException& e) {} // ignore query on error
				}
				else if(i->type == QueryStruct::typeXPath) {
					// parse date/time by running XPath query on parsed content
					try {
						this->getXPathQueryPtr(i->index).getFirst(parsedContent, parsedData.dateTime);

						querySuccess = true;
					}
					catch(const RegExException& e) {} // ignore query on error
				}
				else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: DateTime query on content is not of type RegEx or XPath.");
			}

			if(querySuccess && !parsedData.dateTime.empty()) {
				// found date/time: try to convert it to SQL time stamp
				std::string format = this->config.parsingDateTimeFormats.at(dateTimeQueryCounter);
				std::string locale = this->config.parsingDateTimeLocales.at(dateTimeQueryCounter);

				// use "%F %T" as default date/time format
				if(format.empty())
					format = "%F %T";

				if(!locale.empty()) {
					// locale hack: The French abbreviation "avr." for April is not stringently supported
					if(locale.length() > 1
							&& tolower(locale.at(0) == 'f')
							&& tolower(locale.at(1) == 'r'))
						Helper::Strings::replaceAll(parsedData.dateTime, "avr.", "avril", true);

					try {
						dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								parsedData.dateTime, format, std::locale(locale)
						);
					}
					catch(const std::runtime_error& e) {
						if(this->config.generalLogging)
							this->log("WARNING: Unknown locale \'" + locale + "\' ignored.");

						dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
								parsedData.dateTime, format
						);
					}
				}
				else
					dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
							parsedData.dateTime, format
					);

				if(dateTimeSuccess && !parsedData.dateTime.empty())
					break;
			}

			// not successfull: check next query for parsing the date/time (if exists)
			dateTimeQueryCounter++;
		}

		// check whether date/time conversion was successful
		if(!parsedData.dateTime.empty() && !dateTimeSuccess) {
			if(this->config.generalLogging)
				this->log("ERROR: Could not parse date/time \'" + parsedData.dateTime + "\'!");

			parsedData.dateTime = "";
		}

		// parse custom fields
		unsigned long fieldCounter = 0;

		parsedData.fields.reserve(this->queriesFields.size());

		for(auto i = this->queriesFields.begin(); i != this->queriesFields.end(); ++i) {
			try {
				// determinate whether to get all or just the first match (as string or as boolean value) from the query result
				if(i->resultMulti) {
					// parse multiple elements
					std::vector<std::string> parsedFieldValues;

					// check query source
					if(this->config.parsingFieldSources.at(fieldCounter) == Config::parsingSourceUrl) {
						// parse from URL: check query type
						if(i->type == QueryStruct::typeRegEx)
							// parse multiple field elements by running RegEx query on URL
							this->getRegExQueryPtr(i->index).getAll(this->urls.front().url, parsedFieldValues);

						else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
							this->log(
									"WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)	+ "\'"
									" query on URL is not of type RegEx."
							);
					}
					else {
						// parse from content: check query type
						if(i->type == QueryStruct::typeRegEx)
							// parse multiple field elements by running RegEx query on content string
							this->getRegExQueryPtr(i->index).getAll(content.second, parsedFieldValues);

						else if(i->type == QueryStruct::typeXPath)
							// parse multiple field elements by running XPath query on parsed content
							this->getXPathQueryPtr(i->index).getAll(parsedContent, parsedFieldValues);

						else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
							this->log(
									"WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)	+ "\' query"
									" on content is not of type RegEx or XPath."
							);
					}

					// if necessary, check whether array or all values are empty
					if(this->config.generalLogging && this->config.parsingFieldWarningsEmpty.at(fieldCounter)) {
						bool empty = true;

						for(auto i = parsedFieldValues.begin(); i != parsedFieldValues.end(); ++i) {
							if(!(i->empty())) {
								empty = false;
								break;
							}
						}
						if(empty)
							this->log(
									"WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\'"
									" is empty for " + this->urls.front().url
							);
					}

					// determine how to save result: JSON array or concatenate using delimiting character
					if(this->config.parsingFieldJSON.at(fieldCounter)) {
						// if necessary, tidy texts
						if(this->config.parsingFieldTidyTexts.at(fieldCounter))
							for(auto i = parsedFieldValues.begin(); i != parsedFieldValues.end(); ++i)
								Helper::Strings::utfTidy(*i);

						// stringify and add parsed elements as JSON array
						parsedData.fields.emplace_back(Helper::Json::stringify(parsedFieldValues));
					}
					else {
						// concatenate elements
						std::string result = Helper::Strings::concat(
								parsedFieldValues,
								this->config.parsingFieldDelimiters.at(fieldCounter),
								this->config.parsingFieldIgnoreEmpty.at(fieldCounter)
						);

						// if necessary, tidy text
						if(this->config.parsingFieldTidyTexts.at(fieldCounter))
							Helper::Strings::utfTidy(result);

						parsedData.fields.emplace_back(result);
					}
				}
				else if(i->resultSingle) {
					// parse first element only (as string)
					std::string parsedFieldValue;

					// check query source
					if(this->config.parsingFieldSources.at(fieldCounter) == Config::parsingSourceUrl) {
						// parse from URL: check query type
						if(i->type == QueryStruct::typeRegEx)
							// parse single field element by running RegEx query on URL
							this->getRegExQueryPtr(i->index).getFirst(this->urls.front().url, parsedFieldValue);

						else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
							this->log(
									"WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\'"
									" query on URL is not of type RegEx."
							);
					}
					else {
						// parse from content: check query type
						if(i->type == QueryStruct::typeRegEx)
							// parse single field element by running RegEx query on content string
							this->getRegExQueryPtr(i->index).getFirst(content.second, parsedFieldValue);

						else if(i->type == QueryStruct::typeXPath)
							// parse single field element by running XPath query on parsed content
							this->getXPathQueryPtr(i->index).getFirst(parsedContent, parsedFieldValue);

						else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
							this->log(
									"WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\'"
									" query on content is not of type RegEx or XPath."
							);
					}

					// if necessary, check whether value is empty
					if(this->config.generalLogging
							&& this->config.parsingFieldWarningsEmpty.at(fieldCounter)
							&& parsedFieldValue.empty())
						this->log(
								"WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\'"
								" is empty for " + this->urls.front().url
						);

					// if necessary, tidy text
					if(this->config.parsingFieldTidyTexts.at(fieldCounter))
						Helper::Strings::utfTidy(parsedFieldValue);

					// determine how to save result: JSON array or string as is
					if(this->config.parsingFieldJSON.at(fieldCounter))
						// stringify and add parsed element as JSON array with one element
						parsedData.fields.emplace_back(Helper::Json::stringify(parsedFieldValue));

					else
						// save as is
						parsedData.fields.emplace_back(parsedFieldValue);
				}
				else if(i->resultBool) {
					// only save whether a match for the query exists
					bool parsedBool = false;

					// check query source
					if(this->config.parsingFieldSources.at(fieldCounter) == Config::parsingSourceUrl) {
						// parse from URL: check query type
						if(i->type == QueryStruct::typeRegEx)
							// parse boolean value by running RegEx query on URL
							parsedBool = this->getRegExQueryPtr(i->index).getBool(this->urls.front().url);

						else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
							this->log(
									"WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\'"
									" query on URL is not of type RegEx."
							);
					}
					else {
						// parse from content: check query type
						if(i->type == QueryStruct::typeRegEx)
							// parse boolean value by running RegEx query on content string
							parsedBool = this->getRegExQueryPtr(i->index).getBool(content.second);

						else if(i->type == QueryStruct::typeXPath)
							// parse boolean value by running XPath query on parsed content
							parsedBool = this->getXPathQueryPtr(i->index).getBool(parsedContent);

						else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
							this->log(
									"WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\'"
									" query on content is not of type RegEx or XPath."
							);
					}

					// determine how to save result: JSON array or boolean value as string
					if(this->config.parsingFieldJSON.at(fieldCounter))
						// stringify and add parsed element as JSON array with one boolean value as string
						parsedData.fields.emplace_back(Helper::Json::stringify(parsedBool ? "true" : "false"));

					else
						// save boolean value as string
						parsedData.fields.emplace_back(parsedBool ? "true" : "false");
				}
				else {
					if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log(
								"WARNING: Ignored \'" + this->config.parsingFieldNames.at(fieldCounter) + "\'"
								" query without specified result type."
						);

					parsedData.fields.emplace_back();
				}

				fieldCounter++;
			}
			catch(const RegExException& e) {} // ignore query on error
		}

		// add parsed data to results
		this->results.push(parsedData);

		return true;
	}

	// URL has been processed (skipped or parsed)
	void Thread::parsingUrlFinished() {
		// check whether the finished URL is the last URL in the cache
		if(this->urls.size() == 1) {
			// if yes, save results to database
			this->parsingSaveResults();

			// set current URL as last URL
			this->setLast(this->urls.back().id);

			// reset URL properties
			this->idFirst = 0;
			this->idDist = 0;
			this->posFirstF = 0;
			this->posDist = 0;
		}

		// delete current URL from cache
		this->urls.pop();
	}

	// save results to database
	void Thread::parsingSaveResults() {
		// check whether there are no results
		if(this->results.empty()) return;

		// timer
		Timer::Simple timer;

		// queue for log entries while table is locked
		std::queue<std::string> logEntries;

		// save results
		this->setStatusMessage("Saving results...");

		if(this->config.generalLogging > Config::generalLoggingDefault)
			this->log("saves results...");

		{ // lock target table
			TableLock targetTableLock(this->database, TableLockProperties(this->targetTable, this->targetTableAlias, 1000));

			// update or add entries in/to database
			this->database.updateOrAddEntries(this->results, logEntries);
		} // target and parsing tables unlocked

		// update parsing table
		this->database.updateTargetTable();

		{ // lock parsing table
			TableLock parsingTableLock(this->database, TableLockProperties(this->parsingTable));

			// set those URLs to finished whose URL lock is okay (still locked or re-lockable (and not parsed when not re-parsing)
			this->database.setUrlsFinishedIfLockOk(this->finished);
		} // parsing table unlocked

		// write log entries if necessary
		if(this->config.generalLogging) {
			while(!logEntries.empty()) {
				this->log(logEntries.front());
				logEntries.pop();
			}
		}

		// update status
		this->setStatusMessage("Results saved.");
		if(this->config.generalTiming && this->config.generalLogging)
			this->log("saved results in " + timer.tickStr());
	}

} /* crawlservpp::Module::Crawler */
