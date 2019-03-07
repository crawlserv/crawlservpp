/*
 * Thread.cpp
 *
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.h"

namespace crawlservpp::Module::Parser {

// constructor A: run previously interrupted parser
Thread::Thread(Main::Database& dbBase, unsigned long parserId, const std::string& parserStatus, bool parserPaused,
		const ThreadOptions& threadOptions, unsigned long parserLast)
			: Module::Thread(dbBase, parserId, "parser", parserStatus, parserPaused, threadOptions, parserLast),
			  database(this->Module::Thread::database), idFromUrl(false), tickCounter(0),
			  startTime(std::chrono::steady_clock::time_point::min()), pauseTime(std::chrono::steady_clock::time_point::min()),
			  idleTime(std::chrono::steady_clock::time_point::min()) {}

// constructor B: start a new parser
Thread::Thread(Main::Database& dbBase, const ThreadOptions& threadOptions)
			: Module::Thread(dbBase, "parser", threadOptions), database(this->Module::Thread::database),
			  idFromUrl(false), tickCounter(0), startTime(std::chrono::steady_clock::time_point::min()),
			  pauseTime(std::chrono::steady_clock::time_point::min()), idleTime(std::chrono::steady_clock::time_point::min()) {}

// destructor stub
Thread::~Thread() {}

// initialize parser
void Thread::onInit(bool resumed) {
	std::queue<std::string> configWarnings;
	std::vector<std::string> fields;

	// get configuration
	this->config.loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);

	// show warnings if necessary
	if(this->config.generalLogging) {
		while(!configWarnings.empty()) {
			this->log("WARNING: " + configWarnings.front());
			configWarnings.pop();
		}
	}

	// check configuration
	bool verbose = config.generalLogging == Config::generalLoggingVerbose;
	if(this->config.generalResultTable.empty())
		throw std::runtime_error("ERROR: No target table specified.");

	// set database options
	this->setStatusMessage("Setting database options...");
	if(verbose) this->log("sets database options...");
	this->database.setId(this->getId());
	this->database.setWebsite(this->getWebsite());
	this->database.setWebsiteNamespace(this->websiteNamespace);
	this->database.setUrlList(this->getUrlList());
	this->database.setUrlListNamespace(this->urlListNamespace);
	this->database.setTargetTable(this->config.generalResultTable);
	this->database.setTargetFields(this->config.parsingFieldNames);
	this->database.setReparse(this->config.generalReParse);
	this->database.setParseCustom(this->config.generalParseCustom);
	this->database.setLogging(this->config.generalLogging);
	this->database.setVerbose(verbose);
	this->database.setSleepOnError(this->config.generalSleepMySql);
	this->database.setTimeoutTargetLock(this->config.generalTimeoutTargetLock);

	// create name of parsing table for locking
	this->parsingTable = "crawlserv_" + this->websiteNamespace + "_" + this->urlListNamespace + "_parsing";

	// initialize target table
	this->setStatusMessage("Initialiting target table...");
	if(verbose) this->log("initializes target table...");
	this->database.initTargetTable(std::bind(&Thread::isRunning, this)); // @suppress("Invalid arguments")

	// prepare SQL statements for parser
	this->setStatusMessage("Preparing SQL statements...");
	if(verbose) this->log("prepares SQL statements...");
	this->database.prepare();

	// initialize queries
	this->setStatusMessage("Initialiting custom queries...");
	if(verbose) this->log("initializes custom queries...");
	this->initQueries();

	// check whether ID can be parsed from URL only
	this->setStatusMessage("Checking for URL-only parsing...");
	if(verbose) this->log("checks for URL-only parsing...");
	this->idFromUrl = true;
	for(auto i = this->config.parsingIdSources.begin();
			i != this->config.parsingIdSources.end(); ++i) {
		if(*i == Config::parsingSourceContent) {
			this->idFromUrl = false;
			break;
		}
	}

	// set current URL to last URL
	this->setStatusMessage("Starting to parse...");
	this->currentUrl = UrlProperties(this->getLast());

	// save start time and initialize counter
	this->startTime = std::chrono::steady_clock::now();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->tickCounter = 0;
}

// parser tick
void Thread::onTick() {
	Timer::StartStop timerSelect;
	Timer::StartStop timerTotal;
	unsigned long parsed = 0;

	// start timers
	if(this->config.generalTiming) {
		timerTotal.start();
		timerSelect.start();
	}

	// URL selection
	if(this->parsingUrlSelection()) {
		if(this->config.generalTiming) timerSelect.stop();
		if(this->idleTime > std::chrono::steady_clock::time_point::min()) {
			// idling stopped
			this->startTime += std::chrono::steady_clock::now() - this->idleTime;
			this->pauseTime = std::chrono::steady_clock::time_point::min();
			this->idleTime = std::chrono::steady_clock::time_point::min();
		}

		// increase tick counter
		this->tickCounter++;

		// start parsing
		if(this->config.generalLogging > Config::generalLoggingDefault)
			this->log("parses " + this->currentUrl.url + "...");

		// parse content(s)
		parsed = this->parsing();

		// stop timer
		if(this->config.generalTiming) timerTotal.stop();

		{ // lock parsing table
			TableLock parsingTableLock(this->database, this->parsingTable);

			// update URL status and release URL lock if possible
			if(this->database.checkUrlLock(this->currentUrl.lockId, this->lockTime) && parsed)
				this->database.setUrlFinished(this->currentUrl.lockId);
			else this->database.unLockUrl(this->currentUrl.lockId);
		} // parsing table unlocked

		this->lockTime = "";

		// update status
		this->setLast(this->currentUrl.id);
		this->setProgress((float) (this->database.getUrlPosition(this->currentUrl.id) + 1) / this->database.getNumberOfUrls());

		// write to log if necessary
		if((this->config.generalLogging > Config::generalLoggingDefault)
				|| (this->config.generalTiming && this->config.generalLogging)) {
			std::ostringstream logStrStr;
			logStrStr.imbue(std::locale(""));
			if(parsed > 1) logStrStr << "parsed " << parsed << " versions of ";
			else if(parsed == 1) logStrStr << "parsed ";
			else logStrStr << "skipped ";
			logStrStr << this->currentUrl.url;
			if(this->config.generalTiming) logStrStr << " in " << timerTotal.totalStr()
					<< " (URL selection: " << timerSelect.totalStr() << ")";
			this->log(logStrStr.str());
		}
		else if(this->config.generalLogging > Config::generalLoggingDefault && !parsed)
			this->log("skipped " + this->currentUrl.url);

		// remove URL lock if necessary
		{ // lock parsing table
			TableLock parsingTableLock(this->database, this->parsingTable);

			if(this->database.checkUrlLock(this->currentUrl.lockId, this->lockTime))
				this->database.unLockUrl(this->currentUrl.lockId);
		} // parsing table unlocked

		this->lockTime = "";
	}
	else {
		// no URLs left: set timer if just finished, sleep
		if(this->idleTime == std::chrono::steady_clock::time_point::min()) {
			this->idleTime = std::chrono::steady_clock::now();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(this->config.generalSleepIdle));
	}
}

// parser paused
void Thread::onPause() {
	// save pause start time
	this->pauseTime = std::chrono::steady_clock::now();
}

// parser unpaused
void Thread::onUnpause() {
	// add pause time to start or idle time to ignore pause
	if(this->idleTime > std::chrono::steady_clock::time_point::min())
		this->idleTime += std::chrono::steady_clock::now() - this->pauseTime;
	else this->startTime += std::chrono::steady_clock::now() - this->pauseTime;
	this->pauseTime = std::chrono::steady_clock::time_point::min();
}

// clear parser
void Thread::onClear(bool interrupted) {
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
		long double tps = (long double) this->tickCounter /
				std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - this->startTime).count();
		tpsStrStr.imbue(std::locale(""));
		tpsStrStr << std::setprecision(2) << std::fixed << tps;
		this->log("average speed: " + tpsStrStr.str() + " ticks per second.");
	}

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

// parsing function for URL selection
bool Thread::parsingUrlSelection() {
	std::queue<std::string> logEntries;
	bool result = true;
	bool notIdle = this->currentUrl.id > 0;

	// get ID and name of next URL (skip locked URLs)
	bool skip = false;
	unsigned long skipped = 0;

	while(true) {
		if(skip) this->currentUrl = this->database.getNextUrl(skipped);
		else this->currentUrl = this->database.getNextUrl(this->getLast());

		if(this->currentUrl.id) {
			// check whether to skip URL
			skip = false;
			if(!(this->config.generalSkip.empty())) {
				for(auto i = this->queriesSkip.begin(); i != this->queriesSkip.end(); ++i) {
					// check result type of query
					if(!(i->resultBool) && this->config.generalLogging)
						this->log("WARNING: Invalid result type of skip query (not bool).");

					// check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse ID by running RegEx query on URL
						try {
							if(this->getRegExQueryPtr(i->index).getBool(this->currentUrl.url)) {
								skip = true;
								break;
							}
						}
						catch(const RegExException& e) {} // ignore query on error
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: ID query on URL is not of type RegEx.");
				}
			}

			if(skip) {
				// skip URL
				if(this->config.generalLogging) logEntries.emplace("skipped " + this->currentUrl.url);
				skipped = this->currentUrl.id;
			}
			else
			{ // lock parsing table
				TableLock parsingTableLock(this->database, this->parsingTable);

				// get current URL lock ID if none was received on URL selection
				this->database.getUrlLockId(this->currentUrl);

				// lock URL
				if(this->database.isUrlLockable(this->currentUrl.lockId)) {
					this->lockTime = this->database.lockUrl(this->currentUrl, this->config.generalLock);
					// success!
					break;
				}
				else if(this->config.generalLogging) {
					// skip locked URL
					logEntries.emplace("skipped " + this->currentUrl.url + ", because it is locked.");
					skip = true;
					skipped = this->currentUrl.id;
				}
			} // parsing table unlocked
		}
		else {
			// no more URLs
			result = false;
			break;
		}
	}

	// write to log if necessary
	if(this->config.generalLogging) {
		while(!logEntries.empty()) {
			this->log(logEntries.front());
			logEntries.pop();
		}
	}

	// set thread status
	if(result) this->setStatusMessage(this->currentUrl.url);
	else {
		if(notIdle) {
			if(this->config.generalResetOnFinish) {
				if(this->config.generalLogging) this->log("finished, resetting parsing status.");
				this->database.resetParsingStatus(this->getUrlList());
			}
			else if(this->config.generalLogging > Config::generalLoggingDefault) this->log("finished.");
		}
		this->setStatusMessage("IDLE Waiting for new URLs to parse.");
		this->setProgress(1L);
	}

	return result;
}

// parse content(s) of current URL, return number of successfully parsed contents
unsigned long Thread::parsing() {
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
					this->getRegExQueryPtr(i->index).getFirst(this->currentUrl.url, parsedId);
					if(!parsedId.empty()) break;
				}
				catch(const RegExException& e) {} // ignore query on error
			}
			else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
				this->log("WARNING: ID query on URL is not of type RegEx.");
		}

		// check ID
		if(parsedId.empty()) return 0;
		if(!(this->config.parsingIdIgnore.empty()) && std::find(this->config.parsingIdIgnore.begin(),
				this->config.parsingIdIgnore.end(),	parsedId) != this->config.parsingIdIgnore.end())
			return 0;
	}

	if(this->config.generalNewestOnly) {
		// parse newest content of URL
		unsigned long index = 0;
		while(true) {
			std::pair<unsigned long, std::string> latestContent;
			if(this->database.getLatestContent(this->currentUrl.id, index, latestContent)) {
				if(this->parsingContent(latestContent, parsedId)) return 1;
				index++;
			}
			else break;
		}
	}
	else {
		// parse all contents of URL
		unsigned long counter = 0;
		std::queue<IdString> contents = this->database.getAllContents(this->currentUrl.id);
		while(!contents.empty()) {
			if(this->parsingContent(contents.front(), parsedId)) counter++;
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
			logStrStr << "Content #" << content.first << " [" << this->currentUrl.url << "] could not be parsed: " << e.what();
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
						this->getRegExQueryPtr(i->index).getFirst(this->currentUrl.url, parsedData.parsedId);
						if(!parsedData.parsedId.empty()) break;
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
						if(!parsedData.parsedId.empty()) break;
					}
					catch(const RegExException& e) {} // ignore query on error
				}
				else if(i->type == QueryStruct::typeXPath) {
					// parse ID by running XPath query on parsed content
					try {
						this->getXPathQueryPtr(i->index).getFirst(parsedContent, parsedData.parsedId);
						if(!parsedData.parsedId.empty()) break;
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
	if(parsedData.parsedId.empty()) return false;

	// check whether parsed ID is ought to be ignored
	if(!(this->config.parsingIdIgnore.empty()) && std::find(this->config.parsingIdIgnore.begin(),
			this->config.parsingIdIgnore.end(), parsedData.parsedId) != this->config.parsingIdIgnore.end())
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
					this->getRegExQueryPtr(i->index).getFirst(this->currentUrl.url, parsedData.dateTime);
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
			if(format.empty()) format = "%F %T";

			if(!locale.empty()) {
				// locale hack: The French abbreviation "avr." for April is not stringently supported
				if(locale.length() > 1 && tolower(locale.at(0) == 'f') && tolower(locale.at(1) == 'r'))
					Helper::Strings::replaceAll(parsedData.dateTime, "avr.", "avril", true);

				try {
					dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
							parsedData.dateTime, format, std::locale(locale)
					);
				}
				catch(const std::runtime_error& e) {
					if(this->config.generalLogging) this->log("WARNING: Unknown locale \'" + locale + "\' ignored.");
					dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
							parsedData.dateTime, format
					);
				}
			}
			else
				dateTimeSuccess = Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(
						parsedData.dateTime, format
				);

			if(dateTimeSuccess && !parsedData.dateTime.empty()) break;
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
					if(i->type == QueryStruct::typeRegEx) {
						// parse multiple field elements by running RegEx query on URL
						this->getRegExQueryPtr(i->index).getAll(this->currentUrl.url, parsedFieldValues);
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
								+ "\' query on URL is not of type RegEx.");
				}
				else {
					// parse from content: check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse multiple field elements by running RegEx query on content string
						this->getRegExQueryPtr(i->index).getAll(content.second, parsedFieldValues);
					}
					else if(i->type == QueryStruct::typeXPath) {
						// parse multiple field elements by running XPath query on parsed content
						this->getXPathQueryPtr(i->index).getAll(parsedContent, parsedFieldValues);
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
								+ "\' query on content is not of type RegEx or XPath.");
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
					if(empty) this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\' is empty for "
							+ this->currentUrl.url);
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
					std::string result = Helper::Strings::concat(parsedFieldValues,
							this->config.parsingFieldDelimiters.at(fieldCounter),
							this->config.parsingFieldIgnoreEmpty.at(fieldCounter));

					// if necessary, tidy text
					if(this->config.parsingFieldTidyTexts.at(fieldCounter)) Helper::Strings::utfTidy(result);

					parsedData.fields.emplace_back(result);
				}
			}
			else if(i->resultSingle) {
				// parse first element only (as string)
				std::string parsedFieldValue;

				// check query source
				if(this->config.parsingFieldSources.at(fieldCounter) == Config::parsingSourceUrl) {
					// parse from URL: check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse single field element by running RegEx query on URL
						this->getRegExQueryPtr(i->index).getFirst(this->currentUrl.url, parsedFieldValue);
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
								+ "\' query on URL is not of type RegEx.");
				}
				else {
					// parse from content: check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse single field element by running RegEx query on content string
						this->getRegExQueryPtr(i->index).getFirst(content.second, parsedFieldValue);
					}
					else if(i->type == QueryStruct::typeXPath) {
						// parse single field element by running XPath query on parsed content
						this->getXPathQueryPtr(i->index).getFirst(parsedContent, parsedFieldValue);
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
								+ "\' query on content is not of type RegEx or XPath.");
				}

				// if necessary, check whether value is empty
				if(this->config.generalLogging && this->config.parsingFieldWarningsEmpty.at(fieldCounter) && parsedFieldValue.empty())
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\' is empty for "
											+ this->currentUrl.url);

				// if necessary, tidy text
				if(this->config.parsingFieldTidyTexts.at(fieldCounter)) Helper::Strings::utfTidy(parsedFieldValue);

				// determine how to save result: JSON array or string as is
				if(this->config.parsingFieldJSON.at(fieldCounter)) {
					// stringify and add parsed element as JSON array with one element
					parsedData.fields.emplace_back(Helper::Json::stringify(parsedFieldValue));
				}
				else {
					// save as is
					parsedData.fields.emplace_back(parsedFieldValue);
				}
			}
			else if(i->resultBool) {
				// only save whether a match for the query exists
				bool parsedBool = false;

				// check query source
				if(this->config.parsingFieldSources.at(fieldCounter) == Config::parsingSourceUrl) {
					// parse from URL: check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse boolean value by running RegEx query on URL
						parsedBool = this->getRegExQueryPtr(i->index).getBool(this->currentUrl.url);
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
								+ "\' query on URL is not of type RegEx.");
				}
				else {
					// parse from content: check query type
					if(i->type == QueryStruct::typeRegEx) {
						// parse boolean value by running RegEx query on content string
						parsedBool = this->getRegExQueryPtr(i->index).getBool(content.second);
					}
					else if(i->type == QueryStruct::typeXPath) {
						// parse boolean value by running XPath query on parsed content
						parsedBool = this->getXPathQueryPtr(i->index).getBool(parsedContent);
					}
					else if(i->type != QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on content is not of type RegEx or XPath.");
				}

				// determine how to save result: JSON array or boolean value as string
				if(this->config.parsingFieldJSON.at(fieldCounter)) {
					// stringify and add parsed element as JSON array with one boolean value as string
					parsedData.fields.emplace_back(Helper::Json::stringify(parsedBool ? "true" : "false"));
				}
				else {
					// save boolean value as string
					parsedData.fields.emplace_back(parsedBool ? "true" : "false");
				}
			}
			else {
				if(i->type != QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: Ignored \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query without specified result type.");
				parsedData.fields.emplace_back();
			}

			fieldCounter++;
		}
		catch(const RegExException& e) {} // ignore query on error
	}

	// update or add parsed data to database
	this->database.updateOrAddEntry(parsedData);
	return true;
}

}
