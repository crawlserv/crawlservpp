/*
 * ThreadParser.cpp
 *
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "ThreadParser.h"

// constructor A: run previously interrupted parser
ThreadParser::ThreadParser(Database& dbBase, unsigned long parserId, const std::string& parserStatus, bool parserPaused,
		const ThreadOptions& threadOptions, unsigned long parserLast)
	: Thread(dbBase, parserId, "parser", parserStatus, parserPaused, threadOptions, parserLast), database(this->Thread::database) {
	// set default values
	this->tickCounter = 0;
	this->idFromUrl = false;
	this->startTime = std::chrono::steady_clock::time_point::min();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->idleTime = std::chrono::steady_clock::time_point::min();
}

// constructor B: start a new parser
ThreadParser::ThreadParser(Database& dbBase, const ThreadOptions& threadOptions)
	: Thread(dbBase, "parser", threadOptions), database(this->Thread::database) {
	// set default values
	this->tickCounter = 0;
	this->idFromUrl = false;
	this->startTime = std::chrono::steady_clock::time_point::min();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->idleTime = std::chrono::steady_clock::time_point::min();
}

// destructor stub
ThreadParser::~ThreadParser() {}

// initialize parser
bool ThreadParser::onInit(bool resumed) {
	std::vector<std::string> configWarnings;
	std::vector<std::string> fields;
	bool verbose = false;

	// get configuration and show warnings if necessary
	if(!(this->config.loadConfig(this->database.getConfigJson(this->getConfig()), configWarnings))) {
		this->log(this->config.getErrorMessage());
		return false;
	}
	if(this->config.parserLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);
	verbose = config.parserLogging == ConfigParser::parserLoggingVerbose;

	// set database configuration
	if(verbose) this->log("Set database configuration...");
	this->database.setSleepOnError(this->config.parserSleepMySql);

	// initialize table
	if(verbose) this->log("Initialiting target table...");
	this->database.initTargetTable(this->getWebsite(), this->getUrlList(), this->websiteNameSpace, this->urlListNameSpace,
			this->config.parserResultTable, &(this->config.parserFieldNames));

	// prepare SQL statements for parser
	if(verbose) this->log("Prepare SQL statements...");
	if(!(this->database.prepare(this->getId(), this->config.parserResultTable, this->config.parserReParse, verbose))) {
		if(this->config.parserLogging) this->log(this->database.getErrorMessage());
		return false;
	}

	// initialize queries
	if(verbose) this->log("Initialiting queries...");
	this->initQueries();

	// check whether id can be parsed from URL only
	if(verbose) this->log("Check for URL-only parsing of content IDs...");
	this->idFromUrl = true;
	for(std::vector<unsigned short>::const_iterator i = this->config.parserIdSources.begin(); i != this->config.parserIdSources.end();
			++i) {
		if(*i == ConfigParser::parserSourceContent) {
			this->idFromUrl = false;
			break;
		}
	}

	// save start time and initialize counter
	this->startTime = std::chrono::steady_clock::now();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->tickCounter = 0;
	return true;
}

// parser tick
bool ThreadParser::onTick() {
	TimerStartStop timerSelect;
	TimerStartStop timerTotal;
	unsigned long parsed = 0;

	// start timers
	if(this->config.parserTiming) {
		timerTotal.start();
		timerSelect.start();
	}

	// URL selection
	if(this->parsingUrlSelection()) {
		if(this->config.parserTiming) timerSelect.stop();
		if(this->idleTime > std::chrono::steady_clock::time_point::min()) {
			// idling stopped
			this->startTime += std::chrono::steady_clock::now() - this->idleTime;
			this->pauseTime = std::chrono::steady_clock::time_point::min();
			this->idleTime = std::chrono::steady_clock::time_point::min();
		}

		// increase tick counter
		this->tickCounter++;

		// start parsing
		if(this->config.parserLogging > ConfigParser::parserLoggingDefault) this->log("parses " + this->currentUrl.string + "...");

		// parse content(s)
		parsed = this->parsing();

		// stop timer
		if(this->config.parserTiming) timerTotal.stop();

		// update URL list if possible, release URL lock
		this->database.lockUrlList();
		if(this->database.checkUrlLock(this->currentUrl.id, this->lockTime)) this->database.setUrlFinished(this->currentUrl.id);
		this->database.unlockTables();
		this->lockTime = "";

		// update status
		this->setLast(this->currentUrl.id);
		this->setProgress((float) (this->database.getUrlPosition(this->currentUrl.id) + 1) / this->database.getNumberOfUrls());

		// write to log if necessary
		if((this->config.parserLogging > ConfigParser::parserLoggingDefault)
				|| (this->config.parserTiming && this->config.parserLogging)) {
			std::ostringstream logStrStr;
			logStrStr.imbue(std::locale(""));
			if(parsed > 1) logStrStr << "parsed " << parsed << " versions of ";
			else if(parsed == 1) logStrStr << "parsed ";
			else logStrStr << "skipped ";
			logStrStr << this->currentUrl.string;
			if(this->config.parserTiming) logStrStr << " in " << timerTotal.totalStr();
			this->log(logStrStr.str());
		}
		else if(this->config.parserLogging && !parsed) this->log("skipped " + this->currentUrl.string);

		// remove URL lock if necessary
		this->database.lockUrlList();
		if(this->database.checkUrlLock(this->currentUrl.id, this->lockTime)) this->database.unLockUrl(this->currentUrl.id);
		this->database.unlockTables();
		this->lockTime = "";
	}
	else {
		// no URLs left: set timer if just finished, sleep
		if(this->idleTime == std::chrono::steady_clock::time_point::min()) {
			this->idleTime = std::chrono::steady_clock::now();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(this->config.parserSleepIdle));
	}

	return true;
}

// parser paused
void ThreadParser::onPause() {
	// save pause start time
	this->pauseTime = std::chrono::steady_clock::now();
}

// parser unpaused
void ThreadParser::onUnpause() {
	// add pause time to start or idle time to ignore pause
	if(this->idleTime > std::chrono::steady_clock::time_point::min())
		this->idleTime += std::chrono::steady_clock::now() - this->pauseTime;
	else this->startTime += std::chrono::steady_clock::now() - this->pauseTime;
	this->pauseTime = std::chrono::steady_clock::time_point::min();
}

// clear parser
void ThreadParser::onClear(bool interrupted) {
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
	this->queriesDateTime.clear();
	this->queriesFields.clear();
	this->queriesId.clear();
	this->clearQueries();
}

// hide functions not to be used by thread
void ThreadParser::start() { throw(std::logic_error("Thread::start() not to be used by thread itself")); }
void ThreadParser::pause() { throw(std::logic_error("Thread::pause() not to be used by thread itself")); }
void ThreadParser::unpause() { throw(std::logic_error("Thread::unpause() not to be used by thread itself")); }
void ThreadParser::stop() { throw(std::logic_error("Thread::stop() not to be used by thread itself")); }
void ThreadParser::interrupt() { throw(std::logic_error("Thread::interrupt() not to be used by thread itself")); }

// initialize queries
void ThreadParser::initQueries() {
	std::string queryText;
	std::string queryType;
	bool queryResultBool = false;
	bool queryResultSingle = false;
	bool queryResultMulti = false;
	bool queryTextOnly = false;

	for(auto i = this->config.parserIdQueries.begin(); i != this->config.parserIdQueries.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
		this->queriesId.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly));
	}
	for(auto i = this->config.parserDateTimeQueries.begin(); i != this->config.parserDateTimeQueries.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
		this->queriesDateTime.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly));
	}
	for(auto i = this->config.parserFieldQueries.begin(); i != this->config.parserFieldQueries.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);

		this->queriesFields.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly));
	}
}

// parsing function for URL selection
bool ThreadParser::parsingUrlSelection() {
	std::vector<std::string> logEntries;
	bool result = true;

	// lock URL list
	this->database.lockUrlList();

	// get id and name of next URL (skip locked URLs)
	while(true) {
		this->currentUrl = this->database.getNextUrl(this->getLast());

		if(this->currentUrl.id) {
			// lock URL
			if(this->database.isUrlLockable(this->currentUrl.id)) {
				this->lockTime = this->database.lockUrl(this->currentUrl.id, this->config.parserLock);
				// success!
				break;
			}
			else {
				// skip locked URL
				logEntries.push_back("skipped " + this->currentUrl.string + ", because it is locked.");
			}
		}
		else {
			// no more URLs
			result = false;
			break;
		}
	}

	// unlock URL list and write to log if necessary
	this->database.unlockTables();
	if(this->config.parserLogging) for(auto i = logEntries.begin(); i != logEntries.end(); ++i) this->log(*i);

	// set thread status
	if(result) this->setStatusMessage(this->currentUrl.string);
	else {
		this->setStatusMessage("IDLE Waiting for new URLs to parse.");
		this->setProgress(1L);
	}

	return result;
}

// parse content(s) of current URL, return number of successfully parsed contents
unsigned long ThreadParser::parsing() {
	std::string parsedId;

	// parse id from URL if possible (using RegEx only)
	if(this->idFromUrl) {
		for(auto i = this->queriesId.begin(); i != this->queriesId.end(); ++i) {
			// check result type of query
			if(!(i->resultSingle) && this->config.parserLogging)
				this->log("WARNING: Invalid result type of ID query (not single).");

			// check query type
			if(i->type == QueryContainer::Query::typeRegEx) {
				// parse id by running RegEx query on URL
				if(this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.string, parsedId) && parsedId.length()) break;
			}
			else if(this->config.parserLogging) this->log("WARNING: ID query on URL is not of type RegEx.");
		}
		if(!parsedId.length()) return 0;
	}

	if(this->config.parserNewestOnly) {
		// parse newest content of URL
		unsigned long index = 0;
		while(true) {
			IdString latestContent;
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

		std::vector<IdString> contents = this->database.getAllContents(this->currentUrl.id);
		for(std::vector<IdString>::const_iterator i = contents.begin(); i != contents.end(); ++i) {
			if(this->parsingContent(*i, parsedId)) counter++;
		}

		return counter;
	}

	return 0;
}

// parse id-specific content, return whether parsing was successfull (i.e. an id could be parsed)
bool ThreadParser::parsingContent(const IdString& content, const std::string& parsedId) {
	// parse HTML
	XMLDocument parsedContent;
	if(!parsedContent.parse(content.string)) {
		if(this->config.parserLogging > ConfigParser::parserLoggingDefault) {
			std::ostringstream logStrStr;
			logStrStr << "Content #" << content.id << " [" << this->currentUrl.string << "] could not be parsed.";
			this->log(logStrStr.str());
		}
		return false;
	}

	// parse id (if still necessary)
	std::string id;
	if(this->idFromUrl) id = parsedId;
	else {
		unsigned long idQueryCounter = 0;

		for(auto i = this->queriesId.begin(); i != this->queriesId.end(); ++i) {
			// check result type of query
			if(!(i->resultSingle) && this->config.parserLogging)
				this->log("WARNING: Invalid result type of ID query (not single).");

			// check query source
			if(this->config.parserIdSources.at(idQueryCounter) == ConfigParser::parserSourceUrl) {
				// check query type
				if(i->type == QueryContainer::Query::typeRegEx) {
					// parse id by running RegEx query on URL
					if(this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.string, id) && id.length()) break;
				}
				else if(this->config.parserLogging) this->log("WARNING: ID query on URL is not of type RegEx.");
			}
			else {
				// check query type
				if(i->type == QueryContainer::Query::typeRegEx) {
					// parse id by running RegEx query on content string
					if(this->getRegExQueryPtr(i->index)->getFirst(content.string, id) && id.length()) break;
				}
				else if(i->type == QueryContainer::Query::typeXPath) {
					// parse if by running XPath query on parsed content
					if(this->getXPathQueryPtr(i->index)->getFirst(parsedContent, id) && id.length()) break;
				}
				else if(this->config.parserLogging) this->log("WARNING: ID query on content is not of type RegEx or XPath.");
			}

			// not successfull: check next query for parsing the id (if exists)
			idQueryCounter++;
		}
	}

	// check id
	if(!id.length()) return false;

	// parse date/time
	std::string parsedDateTime;
	unsigned long dateTimeQueryCounter = 0;

	for(auto i = this->queriesDateTime.begin(); i != this->queriesDateTime.end(); ++i) {
		bool querySuccess = false;

		// check result type of query
		if(!(i->resultSingle) && this->config.parserLogging)
			this->log("WARNING: Invalid result type of DateTime query (not single).");

		// check query source
		if(this->config.parserDateTimeSources.at(dateTimeQueryCounter) == ConfigParser::parserSourceUrl) {
			// check query type
			if(i->type == QueryContainer::Query::typeRegEx) {
				// parse date/time by running RegEx query on URL
				querySuccess = this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.string, parsedDateTime);
			}
			else if(this->config.parserLogging) this->log("WARNING: DateTime query on URL is not of type RegEx.");
		}
		else {
			// check query type
			if(i->type == QueryContainer::Query::typeRegEx) {
				// parse date/time by running RegEx query on content string
				querySuccess = this->getRegExQueryPtr(i->index)->getFirst(content.string, parsedDateTime);
			}
			else if(i->type == QueryContainer::Query::typeXPath) {
				// parse date/time by running XPath query on parsed content
				querySuccess = this->getXPathQueryPtr(i->index)->getFirst(parsedContent, parsedDateTime);
			}
			else if(this->config.parserLogging) this->log("WARNING: DateTime query on content is not of type RegEx or XPath.");
		}

		if(querySuccess && parsedDateTime.length()) {
			// found date/time: try to convert it to SQL time stamp
			bool conversionSuccess = false;
			std::string format = this->config.parserDateTimeFormats.at(dateTimeQueryCounter);
			std::string locale = this->config.parserDateTimeLocales.at(dateTimeQueryCounter);

			// use "%F %T" as default date/time format
			if(!format.length()) format = "%F %T";

			if(locale.length()) {
				try {
					conversionSuccess = DateTime::convertCustomDateTimeToSQLTimeStamp(parsedDateTime, format, std::locale(locale));
				}
				catch(const std::runtime_error& e) {
					if(this->config.parserLogging) this->log("WARNING: Unknown locale \'" + locale + "\' ignored.");
					conversionSuccess = DateTime::convertCustomDateTimeToSQLTimeStamp(parsedDateTime, format);
				}
			}
			else {
				conversionSuccess = DateTime::convertCustomDateTimeToSQLTimeStamp(parsedDateTime, format);
			}

			if(conversionSuccess && parsedDateTime.length()) break;
		}

		// not successfull: check next query for parsing the date/time (if exists)
		dateTimeQueryCounter++;
	}

	// parse custom fields
	std::vector<std::string> parsedFields;
	unsigned long fieldCounter = 0;

	for(auto i = this->queriesFields.begin(); i != this->queriesFields.end(); ++i) {
		// determinate whether to get all or just the first match (as string or as boolean value) from the query result
		if(i->resultMulti) {
			// parse multiple elements
			std::vector<std::string> parsedFieldValues;

			// check query source
			if(this->config.parserFieldSources.at(fieldCounter) == ConfigParser::parserSourceUrl) {
				// parse from URL: check query type
				if(i->type == QueryContainer::Query::typeRegEx) {
					// parse multiple field elements by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getAll(this->currentUrl.string, parsedFieldValues);
				}
				else if(this->config.parserLogging) this->log("WARNING: \'" + this->config.parserFieldNames.at(fieldCounter)
						+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == QueryContainer::Query::typeRegEx) {
					// parse multiple field elements by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getAll(content.string, parsedFieldValues);
				}
				else if(i->type == QueryContainer::Query::typeXPath) {
					// parse multiple field elements by running XPath query on parsed content
					this->getXPathQueryPtr(i->index)->getAll(parsedContent, parsedFieldValues);
				}
				else if(this->config.parserLogging) this->log("WARNING: \'" + this->config.parserFieldNames.at(fieldCounter)
						+ "\' query on content is not of type RegEx or XPath.");
			}

			// determine how to save result: JSON array or concatenate using delimiting character
			if(this->config.parserFieldJSON.at(fieldCounter)) {
				// stringify and add parsed elements as JSON array
				parsedFields.push_back(Json::stringify(parsedFieldValues));
			}
			else {
				// concatenate elements
				parsedFields.push_back(Strings::concat(parsedFieldValues, this->config.parserFieldDelimiters.at(fieldCounter),
						this->config.parserFieldIgnoreEmpty.at(fieldCounter)));
			}
		}
		else if(i->resultSingle) {
			// parse first element only (as string)
			std::string parsedFieldValue;

			// check query source
			if(this->config.parserFieldSources.at(fieldCounter) == ConfigParser::parserSourceUrl) {
				// parse from URL: check query type
				if(i->type == QueryContainer::Query::typeRegEx) {
					// parse single field element by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.string, parsedFieldValue);
				}
				else if(this->config.parserLogging) this->log("WARNING: \'" + this->config.parserFieldNames.at(fieldCounter)
						+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == QueryContainer::Query::typeRegEx) {
					// parse single field element by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getFirst(content.string, parsedFieldValue);
				}
				else if(i->type == QueryContainer::Query::typeXPath) {
					// parse single field element by running XPath query on parsed content
					this->getXPathQueryPtr(i->index)->getFirst(parsedContent, parsedFieldValue);
				}
				else if(this->config.parserLogging) this->log("WARNING: \'" + this->config.parserFieldNames.at(fieldCounter)
						+ "\' query on content is not of type RegEx or XPath.");
			}

			// determine how to save result: JSON array or string as is
			if(this->config.parserFieldJSON.at(fieldCounter)) {
				// stringify and add parsed element as JSON array with one element
				parsedFields.push_back(Json::stringify(parsedFieldValue));
			}
			else {
				// save as is
				parsedFields.push_back(parsedFieldValue);
			}
		}
		else if(i->resultBool) {
			// only save whether match exists
			bool parsedBool = false;

			// check query source
			if(this->config.parserFieldSources.at(fieldCounter) == ConfigParser::parserSourceUrl) {
				// parse from URL: check query type
				if(i->type == QueryContainer::Query::typeRegEx) {
					// parse boolean value by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getBool(this->currentUrl.string, parsedBool);
				}
				else if(this->config.parserLogging) this->log("WARNING: \'" + this->config.parserFieldNames.at(fieldCounter)
						+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == QueryContainer::Query::typeRegEx) {
					// parse boolean value by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getBool(content.string, parsedBool);
				}
				else if(i->type == QueryContainer::Query::typeXPath) {
					// parse boolean value by running XPath query on parsed content
					this->getXPathQueryPtr(i->index)->getBool(parsedContent, parsedBool);
				}
				else if(this->config.parserLogging) this->log("WARNING: \'" + this->config.parserFieldNames.at(fieldCounter)
						+ "\' query on content is not of type RegEx or XPath.");
			}

			// determine how to save result: JSON array or boolean value as string
			if(this->config.parserFieldJSON.at(fieldCounter)) {
				// stringify and add parsed element as JSON array with one boolean value as string
				parsedFields.push_back(Json::stringify(parsedBool ? "true" : "false"));
			}
			else {
				// save boolean value as string
				parsedFields.push_back(parsedBool ? "true" : "false");
			}
		}
		else if(this->config.parserLogging) this->log("WARNING: Ignored \'" + this->config.parserFieldNames.at(fieldCounter)
				+ "\' query without specified result type.");

		fieldCounter++;
	}

	// update or add parsed data to database
	this->database.updateOrAddEntry(content.id, parsedId, parsedDateTime, parsedFields);
	return true;
}
