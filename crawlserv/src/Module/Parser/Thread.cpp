/*
 * Thread.cpp
 *
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.h"

// constructor A: run previously interrupted parser
crawlservpp::Module::Parser::Thread::Thread(crawlservpp::Global::Database& dbBase, unsigned long parserId,
		const std::string& parserStatus, bool parserPaused, const crawlservpp::Struct::ThreadOptions& threadOptions,
		unsigned long parserLast)
	: crawlservpp::Module::Thread(dbBase, parserId, "parser", parserStatus, parserPaused, threadOptions, parserLast),
	  	  database(this->crawlservpp::Module::Thread::database) {
	// set default values
	this->tickCounter = 0;
	this->idFromUrl = false;
	this->startTime = std::chrono::steady_clock::time_point::min();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->idleTime = std::chrono::steady_clock::time_point::min();
}

// constructor B: start a new parser
crawlservpp::Module::Parser::Thread::Thread(crawlservpp::Global::Database& dbBase,
		const crawlservpp::Struct::ThreadOptions& threadOptions)
	: crawlservpp::Module::Thread(dbBase, "parser", threadOptions), database(this->crawlservpp::Module::Thread::database) {
	// set default values
	this->tickCounter = 0;
	this->idFromUrl = false;
	this->startTime = std::chrono::steady_clock::time_point::min();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->idleTime = std::chrono::steady_clock::time_point::min();
}

// destructor stub
crawlservpp::Module::Parser::Thread::~Thread() {}

// initialize parser
bool crawlservpp::Module::Parser::Thread::onInit(bool resumed) {
	std::vector<std::string> configWarnings;
	std::vector<std::string> fields;
	bool verbose = false;

	// get configuration and show warnings if necessary
	if(!(this->config.loadConfig(this->database.getConfigJson(this->getConfig()), configWarnings))) {
		this->log(this->config.getErrorMessage());
		return false;
	}
	if(this->config.generalLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);
	verbose = config.generalLogging == crawlservpp::Module::Parser::Config::generalLoggingVerbose;

	// check configuration
	if(verbose) this->log("checks configuration...");
	if(!(this->config.generalResultTable.length())) {
		if(this->config.generalLogging) this->log("ERROR: No target table specified.");
		return false;
	}

	// set database configuration
	if(verbose) this->log("sets database configuration...");
	this->database.setSleepOnError(this->config.generalSleepMySql);

	// initialize target table
	if(verbose) this->log("initializes target table...");
	this->database.initTargetTable(this->getWebsite(), this->getUrlList(), this->websiteNameSpace, this->urlListNameSpace,
			this->config.generalResultTable, this->config.parsingFieldNames);

	// prepare SQL statements for parser
	if(verbose) this->log("prepares SQL statements...");
	if(!(this->database.prepare(this->getId(), this->getWebsite(), this->getUrlList(), this->config.generalResultTable,
			this->config.generalReParse, verbose))) {
		if(this->config.generalLogging) this->log(this->database.getModuleErrorMessage());
		return false;
	}

	// initialize queries
	if(verbose) this->log("initializes queries...");
	this->initQueries();

	// check whether ID can be parsed from URL only
	if(verbose) this->log("checks for URL-only parsing of content IDs...");
	this->idFromUrl = true;
	for(std::vector<unsigned short>::const_iterator i = this->config.parsingIdSources.begin();
			i != this->config.parsingIdSources.end(); ++i) {
		if(*i == crawlservpp::Module::Parser::Config::parsingSourceContent) {
			this->idFromUrl = false;
			break;
		}
	}

	// set current URL to last URL
	this->currentUrl.id = this->getLast();

	// save start time and initialize counter
	this->startTime = std::chrono::steady_clock::now();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->tickCounter = 0;
	return true;
}

// parser tick
bool crawlservpp::Module::Parser::Thread::onTick() {
	crawlservpp::Timer::StartStop timerSelect;
	crawlservpp::Timer::StartStop timerTotal;
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
		if(this->config.generalLogging > crawlservpp::Module::Parser::Config::generalLoggingDefault)
			this->log("parses " + this->currentUrl.string + "...");

		// parse content(s)
		parsed = this->parsing();

		// stop timer
		if(this->config.generalTiming) timerTotal.stop();

		// update URL list if possible, release URL lock
		this->database.lockUrlList();
		if(this->database.checkUrlLock(this->currentUrl.id, this->lockTime) && parsed)
			this->database.setUrlFinished(this->currentUrl.id);
		this->database.unlockTables();
		this->lockTime = "";

		// update status
		this->setLast(this->currentUrl.id);
		this->setProgress((float) (this->database.getUrlPosition(this->currentUrl.id) + 1) / this->database.getNumberOfUrls());

		// write to log if necessary
		if((this->config.generalLogging > crawlservpp::Module::Parser::Config::generalLoggingDefault)
				|| (this->config.generalTiming && this->config.generalLogging)) {
			std::ostringstream logStrStr;
			logStrStr.imbue(std::locale(""));
			if(parsed > 1) logStrStr << "parsed " << parsed << " versions of ";
			else if(parsed == 1) logStrStr << "parsed ";
			else logStrStr << "skipped ";
			logStrStr << this->currentUrl.string;
			if(this->config.generalTiming) logStrStr << " in " << timerTotal.totalStr();
			this->log(logStrStr.str());
		}
		else if(this->config.generalLogging && !parsed) this->log("skipped " + this->currentUrl.string);

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
		std::this_thread::sleep_for(std::chrono::milliseconds(this->config.generalSleepIdle));
	}

	return true;
}

// parser paused
void crawlservpp::Module::Parser::Thread::onPause() {
	// save pause start time
	this->pauseTime = std::chrono::steady_clock::now();
}

// parser unpaused
void crawlservpp::Module::Parser::Thread::onUnpause() {
	// add pause time to start or idle time to ignore pause
	if(this->idleTime > std::chrono::steady_clock::time_point::min())
		this->idleTime += std::chrono::steady_clock::now() - this->pauseTime;
	else this->startTime += std::chrono::steady_clock::now() - this->pauseTime;
	this->pauseTime = std::chrono::steady_clock::time_point::min();
}

// clear parser
void crawlservpp::Module::Parser::Thread::onClear(bool interrupted) {
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
void crawlservpp::Module::Parser::Thread::start() {
	throw(std::logic_error("crawlservpp::Module::Parser::Thread::start() not to be used by thread itself"));
}
void crawlservpp::Module::Parser::Thread::pause() {
	throw(std::logic_error("crawlservpp::Module::Parser::Thread::pause() not to be used by thread itself"));
}
void crawlservpp::Module::Parser::Thread::unpause() {
	throw(std::logic_error("crawlservpp::Module::Parser::Thread::unpause() not to be used by thread itself"));
}
void crawlservpp::Module::Parser::Thread::stop() {
	throw(std::logic_error("crawlservpp::Module::Parser::Thread::stop() not to be used by thread itself"));
}
void crawlservpp::Module::Parser::Thread::interrupt() {
	throw(std::logic_error("crawlservpp::Module::Parser::Thread::interrupt() not to be used by thread itself"));
}

// initialize queries
void crawlservpp::Module::Parser::Thread::initQueries() {
	std::string queryText;
	std::string queryType;
	bool queryResultBool = false;
	bool queryResultSingle = false;
	bool queryResultMulti = false;
	bool queryTextOnly = false;

	for(auto i = this->config.parsingIdQueries.begin(); i != this->config.parsingIdQueries.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesId.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly));
	}
	for(auto i = this->config.parsingDateTimeQueries.begin(); i != this->config.parsingDateTimeQueries.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesDateTime.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly));
	}
	for(auto i = this->config.parsingFieldQueries.begin(); i != this->config.parsingFieldQueries.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);

		this->queriesFields.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly));
	}
}

// parsing function for URL selection
bool crawlservpp::Module::Parser::Thread::parsingUrlSelection() {
	std::vector<std::string> logEntries;
	bool result = true;
	bool notIdle = this->currentUrl.id > 0;

	// lock URL list
	this->database.lockUrlList();

	// get id and name of next URL (skip locked URLs)
	while(true) {
		this->currentUrl = this->database.getNextUrl(this->getLast());

		if(this->currentUrl.id) {
			// lock URL
			if(this->database.isUrlLockable(this->currentUrl.id)) {
				this->lockTime = this->database.lockUrl(this->currentUrl.id, this->config.generalLock);
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
	if(this->config.generalLogging) for(auto i = logEntries.begin(); i != logEntries.end(); ++i) this->log(*i);

	// set thread status
	if(result) this->setStatusMessage(this->currentUrl.string);
	else {
		if(notIdle) {
			if(this->config.generalResetOnFinish) {
				if(this->config.generalLogging) this->log("finished, resetting parsing status.");
				this->database.resetParsingStatus(this->getUrlList());
			}
			else if(this->config.generalLogging) this->log("finished.");
		}
		this->setStatusMessage("IDLE Waiting for new URLs to parse.");
		this->setProgress(1L);
	}

	return result;
}

// parse content(s) of current URL, return number of successfully parsed contents
unsigned long crawlservpp::Module::Parser::Thread::parsing() {
	std::string parsedId;

	// parse ID from URL if possible (using RegEx only)
	if(this->idFromUrl) {
		for(auto i = this->queriesId.begin(); i != this->queriesId.end(); ++i) {
			// check result type of query
			if(!(i->resultSingle) && this->config.generalLogging)
				this->log("WARNING: Invalid result type of ID query (not single).");

			// check query type
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				// parse id by running RegEx query on URL
				if(this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.string, parsedId) && parsedId.length()) break;
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
				this->log("WARNING: ID query on URL is not of type RegEx.");
		}

		// check ID
		if(!parsedId.length()) return 0;
		if(this->config.parsingIdIgnore.size() && std::find(this->config.parsingIdIgnore.begin(),
				this->config.parsingIdIgnore.end(),	parsedId) != this->config.parsingIdIgnore.end())
			return 0;
	}

	if(this->config.generalNewestOnly) {
		// parse newest content of URL
		unsigned long index = 0;
		while(true) {
			crawlservpp::Struct::IdString latestContent;
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

		std::vector<crawlservpp::Struct::IdString> contents = this->database.getAllContents(this->currentUrl.id);
		for(std::vector<crawlservpp::Struct::IdString>::const_iterator i = contents.begin(); i != contents.end(); ++i) {
			if(this->parsingContent(*i, parsedId)) counter++;
		}

		return counter;
	}

	return 0;
}

// parse id-specific content, return whether parsing was successfull (i.e. an id could be parsed)
bool crawlservpp::Module::Parser::Thread::parsingContent(const crawlservpp::Struct::IdString& content,
		const std::string& parsedId) {
	// parse HTML
	crawlservpp::Parsing::XML parsedContent;
	if(!parsedContent.parse(content.string)) {
		if(this->config.generalLogging > crawlservpp::Module::Parser::Config::generalLoggingDefault) {
			std::ostringstream logStrStr;
			logStrStr << "Content #" << content.id << " [" << this->currentUrl.string << "] could not be parsed.";
			this->log(logStrStr.str());
		}
		return false;
	}

	// parse ID (if still necessary)
	std::string id;
	if(this->idFromUrl) id = parsedId;
	else {
		unsigned long idQueryCounter = 0;

		for(auto i = this->queriesId.begin(); i != this->queriesId.end(); ++i) {
			// check result type of query
			if(!(i->resultSingle) && this->config.generalLogging)
				this->log("WARNING: Invalid result type of ID query (not single).");

			// check query source
			if(this->config.parsingIdSources.at(idQueryCounter) == crawlservpp::Module::Parser::Config::parsingSourceUrl) {
				// check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse id by running RegEx query on URL
					if(this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.string, id) && id.length()) break;
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: ID query on URL is not of type RegEx.");
			}
			else {
				// check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse id by running RegEx query on content string
					if(this->getRegExQueryPtr(i->index)->getFirst(content.string, id) && id.length()) break;
				}
				else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
					// parse if by running XPath query on parsed content
					if(this->getXPathQueryPtr(i->index)->getFirst(parsedContent, id) && id.length()) break;
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: ID query on content is not of type RegEx or XPath.");
			}

			// not successfull: check next query for parsing the id (if exists)
			idQueryCounter++;
		}
	}

	// check ID
	if(!id.length()) return false;
	if(this->config.parsingIdIgnore.size() && std::find(this->config.parsingIdIgnore.begin(), this->config.parsingIdIgnore.end(),
		parsedId) != this->config.parsingIdIgnore.end()) return false;

	// parse date/time
	std::string parsedDateTime;
	unsigned long dateTimeQueryCounter = 0;
	bool dateTimeSuccess = false;

	for(auto i = this->queriesDateTime.begin(); i != this->queriesDateTime.end(); ++i) {
		bool querySuccess = false;

		// check result type of query
		if(!(i->resultSingle) && this->config.generalLogging)
			this->log("WARNING: Invalid result type of DateTime query (not single).");

		// check query source
		if(this->config.parsingDateTimeSources.at(dateTimeQueryCounter) == crawlservpp::Module::Parser::Config::parsingSourceUrl) {
			// check query type
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				// parse date/time by running RegEx query on URL
				querySuccess = this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.string, parsedDateTime);
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
				this->log("WARNING: DateTime query on URL is not of type RegEx.");
		}
		else {
			// check query type
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				// parse date/time by running RegEx query on content string
				querySuccess = this->getRegExQueryPtr(i->index)->getFirst(content.string, parsedDateTime);
			}
			else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
				// parse date/time by running XPath query on parsed content
				querySuccess = this->getXPathQueryPtr(i->index)->getFirst(parsedContent, parsedDateTime);
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
				this->log("WARNING: DateTime query on content is not of type RegEx or XPath.");
		}

		if(querySuccess && parsedDateTime.length()) {
			// found date/time: try to convert it to SQL time stamp
			std::string format = this->config.parsingDateTimeFormats.at(dateTimeQueryCounter);
			std::string locale = this->config.parsingDateTimeLocales.at(dateTimeQueryCounter);

			// use "%F %T" as default date/time format
			if(!format.length()) format = "%F %T";

			if(locale.length()) {
				// locale hack: The French abbreviation "avr." for April is not stringently supported
				if(locale.length() > 1 && tolower(locale.at(0) == 'f') && tolower(locale.at(1) == 'r'))
					crawlservpp::Helper::Strings::replaceAll(parsedDateTime, "avr.", "avril", true);

				try {
					dateTimeSuccess = crawlservpp::Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(parsedDateTime, format,
							std::locale(locale));
				}
				catch(const std::runtime_error& e) {
					if(this->config.generalLogging) this->log("WARNING: Unknown locale \'" + locale + "\' ignored.");
					dateTimeSuccess = crawlservpp::Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(parsedDateTime, format);
				}
			}
			else {
				dateTimeSuccess = crawlservpp::Helper::DateTime::convertCustomDateTimeToSQLTimeStamp(parsedDateTime, format);
			}

			if(dateTimeSuccess && parsedDateTime.length()) break;
		}

		// not successfull: check next query for parsing the date/time (if exists)
		dateTimeQueryCounter++;
	}

	// check whether date/time conversion was successful
	if(parsedDateTime.length() && !dateTimeSuccess) {
		if(this->config.generalLogging) this->log("ERROR: Could not parse date/time \'" + parsedDateTime + "\'!");
		parsedDateTime = "";
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
			if(this->config.parsingFieldSources.at(fieldCounter) == crawlservpp::Module::Parser::Config::parsingSourceUrl) {
				// parse from URL: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse multiple field elements by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getAll(this->currentUrl.string, parsedFieldValues);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse multiple field elements by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getAll(content.string, parsedFieldValues);
				}
				else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
					// parse multiple field elements by running XPath query on parsed content
					this->getXPathQueryPtr(i->index)->getAll(parsedContent, parsedFieldValues);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on content is not of type RegEx or XPath.");
			}

			// if necessary, check whether array or all values are empty
			if(this->config.generalLogging && this->config.parsingFieldWarningsEmpty.at(fieldCounter)) {
				bool empty = true;
				for(auto i = parsedFieldValues.begin(); i != parsedFieldValues.end(); ++i) {
					if(i->length()) {
						empty = false;
						break;
					}
				}
				if(empty) this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\' is empty for "
						+ this->currentUrl.string);
			}

			// determine how to save result: JSON array or concatenate using delimiting character
			if(this->config.parsingFieldJSON.at(fieldCounter)) {
				// if necessary, tidy texts
				if(this->config.parsingFieldTidyTexts.at(fieldCounter))
					for(auto i = parsedFieldValues.begin(); i != parsedFieldValues.end(); ++i)
						crawlservpp::Helper::Strings::utfTidy(*i);

				// stringify and add parsed elements as JSON array
				parsedFields.push_back(crawlservpp::Helper::Json::stringify(parsedFieldValues));
			}
			else {
				// concatenate elements
				std::string result = crawlservpp::Helper::Strings::concat(parsedFieldValues,
						this->config.parsingFieldDelimiters.at(fieldCounter),
						this->config.parsingFieldIgnoreEmpty.at(fieldCounter));

				// if necessary, tidy text
				if(this->config.parsingFieldTidyTexts.at(fieldCounter)) crawlservpp::Helper::Strings::utfTidy(result);

				parsedFields.push_back(result);
			}
		}
		else if(i->resultSingle) {
			// parse first element only (as string)
			std::string parsedFieldValue;

			// check query source
			if(this->config.parsingFieldSources.at(fieldCounter) == crawlservpp::Module::Parser::Config::parsingSourceUrl) {
				// parse from URL: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse single field element by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.string, parsedFieldValue);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse single field element by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getFirst(content.string, parsedFieldValue);
				}
				else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
					// parse single field element by running XPath query on parsed content
					this->getXPathQueryPtr(i->index)->getFirst(parsedContent, parsedFieldValue);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on content is not of type RegEx or XPath.");
			}

			// if necessary, check whether value is empty
			if(this->config.generalLogging && this->config.parsingFieldWarningsEmpty.at(fieldCounter) && !parsedFieldValue.length())
				this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\' is empty for "
										+ this->currentUrl.string);

			// if necessary, tidy text
			if(this->config.parsingFieldTidyTexts.at(fieldCounter)) crawlservpp::Helper::Strings::utfTidy(parsedFieldValue);

			// determine how to save result: JSON array or string as is
			if(this->config.parsingFieldJSON.at(fieldCounter)) {
				// stringify and add parsed element as JSON array with one element
				parsedFields.push_back(crawlservpp::Helper::Json::stringify(parsedFieldValue));
			}
			else {
				// save as is
				parsedFields.push_back(parsedFieldValue);
			}
		}
		else if(i->resultBool) {
			// only save whether a match for the query exists
			bool parsedBool = false;

			// check query source
			if(this->config.parsingFieldSources.at(fieldCounter) == crawlservpp::Module::Parser::Config::parsingSourceUrl) {
				// parse from URL: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse boolean value by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getBool(this->currentUrl.string, parsedBool);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse boolean value by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getBool(content.string, parsedBool);
				}
				else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
					// parse boolean value by running XPath query on parsed content
					this->getXPathQueryPtr(i->index)->getBool(parsedContent, parsedBool);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
						+ "\' query on content is not of type RegEx or XPath.");
			}

			// determine how to save result: JSON array or boolean value as string
			if(this->config.parsingFieldJSON.at(fieldCounter)) {
				// stringify and add parsed element as JSON array with one boolean value as string
				parsedFields.push_back(crawlservpp::Helper::Json::stringify(parsedBool ? "true" : "false"));
			}
			else {
				// save boolean value as string
				parsedFields.push_back(parsedBool ? "true" : "false");
			}
		}
		else {
			if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
				this->log("WARNING: Ignored \'" + this->config.parsingFieldNames.at(fieldCounter)
						+ "\' query without specified result type.");
			parsedFields.push_back("");
		}

		fieldCounter++;
	}

	// update or add parsed data to database
	this->database.updateOrAddEntry(content.id, parsedId, parsedDateTime, parsedFields);
	return true;
}
