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
Thread::Thread(crawlservpp::Main::Database& dbBase, unsigned long parserId,
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
Thread::Thread(crawlservpp::Main::Database& dbBase,
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
Thread::~Thread() {}

// initialize parser
bool Thread::onInit(bool resumed) {
	std::vector<std::string> configWarnings;
	std::vector<std::string> fields;
	bool verbose = false;

	// get configuration and show warnings if necessary
	if(!(this->config.loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings))) {
		this->log(this->config.getErrorMessage());
		return false;
	}
	if(this->config.generalLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);
	verbose = config.generalLogging == Config::generalLoggingVerbose;

	// check configuration
	if(verbose) this->log("checks configuration...");
	if(this->config.generalResultTable.empty()) {
		if(this->config.generalLogging) this->log("ERROR: No target table specified.");
		return false;
	}

	// set database options
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

	// initialize target table
	if(verbose) this->log("initializes target table...");
	this->database.initTargetTable();

	// prepare SQL statements for parser
	if(verbose) this->log("prepares SQL statements...");
	if(!(this->database.prepare())) {
		if(this->config.generalLogging) this->log(this->database.getModuleErrorMessage());
		return false;
	}

	// initialize queries
	if(verbose) this->log("initializes queries...");
	this->initQueries();

	// check whether ID can be parsed from URL only
	if(verbose) this->log("checks for URL-only parsing of content IDs...");
	this->idFromUrl = true;
	for(auto i = this->config.parsingIdSources.begin();
			i != this->config.parsingIdSources.end(); ++i) {
		if(*i == Config::parsingSourceContent) {
			this->idFromUrl = false;
			break;
		}
	}

	// set current URL to last URL
	this->currentUrl.first = this->getLast();

	// save start time and initialize counter
	this->startTime = std::chrono::steady_clock::now();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->tickCounter = 0;
	return true;
}

// parser tick
bool Thread::onTick() {
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
		if(this->config.generalLogging > Config::generalLoggingDefault)
			this->log("parses " + this->currentUrl.second + "...");

		// parse content(s)
		parsed = this->parsing();

		// stop timer
		if(this->config.generalTiming) timerTotal.stop();

		// update URL list if possible, release URL lock
		this->database.lockUrlList();
		if(this->database.checkUrlLock(this->currentUrl.first, this->lockTime) && parsed)
			this->database.setUrlFinished(this->currentUrl.first);
		this->database.releaseLocks();
		this->lockTime = "";

		// update status
		this->setLast(this->currentUrl.first);
		this->setProgress((float) (this->database.getUrlPosition(this->currentUrl.first) + 1) / this->database.getNumberOfUrls());

		// write to log if necessary
		if((this->config.generalLogging > Config::generalLoggingDefault)
				|| (this->config.generalTiming && this->config.generalLogging)) {
			std::ostringstream logStrStr;
			logStrStr.imbue(std::locale(""));
			if(parsed > 1) logStrStr << "parsed " << parsed << " versions of ";
			else if(parsed == 1) logStrStr << "parsed ";
			else logStrStr << "skipped ";
			logStrStr << this->currentUrl.second;
			if(this->config.generalTiming) logStrStr << " in " << timerTotal.totalStr();
			this->log(logStrStr.str());
		}
		else if(this->config.generalLogging && !parsed) this->log("skipped " + this->currentUrl.second);

		// remove URL lock if necessary
		this->database.lockUrlList();
		if(this->database.checkUrlLock(this->currentUrl.first, this->lockTime)) this->database.unLockUrl(this->currentUrl.first);
		this->database.releaseLocks();
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

// hide functions not to be used by thread
void Thread::start() {
	throw(std::logic_error("Thread::start() not to be used by thread itself"));
}
void Thread::pause() {
	throw(std::logic_error("Thread::pause() not to be used by thread itself"));
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
	std::string queryText;
	std::string queryType;
	bool queryResultBool = false;
	bool queryResultSingle = false;
	bool queryResultMulti = false;
	bool queryTextOnly = false;

	// reserve memory for query ids
	this->queriesId.reserve(this->config.parsingIdQueries.size());
	this->queriesDateTime.reserve(this->config.parsingDateTimeQueries.size());
	this->queriesFields.reserve(this->config.parsingFieldQueries.size());
	this->reserveQueries(this->config.parsingIdQueries.size() + this->config.parsingDateTimeQueries.size()
			+ this->config.parsingFieldQueries.size());

	// create queries and get query ids
	for(auto i = this->config.generalSkip.begin(); i != this->config.generalSkip.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesSkip.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly));
	}
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
bool Thread::parsingUrlSelection() {
	std::vector<std::string> logEntries;
	bool result = true;
	bool notIdle = this->currentUrl.first > 0;

	// lock URL list and crawling table
	this->database.lockUrlListAndCrawledTable();

	// get id and name of next URL (skip locked URLs)
	bool skip = false;
	unsigned long skipped = 0;

	while(true) {
		if(skip) this->currentUrl = this->database.getNextUrl(skipped);
		else this->currentUrl = this->database.getNextUrl(this->getLast());

		if(this->currentUrl.first) {
			// check whether to skip URL
			skip = false;
			if(!(this->config.generalSkip.empty())) {
				for(auto i = this->queriesSkip.begin(); i != this->queriesSkip.end(); ++i) {
					// check result type of query
					if(!(i->resultBool) && this->config.generalLogging)
						this->log("WARNING: Invalid result type of skip query (not bool).");

					// check query type
					if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
						// parse id by running RegEx query on URL
						bool queryResult = false;
						if(this->getRegExQueryPtr(i->index)->getBool(this->currentUrl.second, queryResult)) {
							if(queryResult) {
								skip = true;
							}
							break;
						}
					}
					else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
						this->log("WARNING: ID query on URL is not of type RegEx.");
				}
			}

			if(skip) {
				// skip URL
				if(this->config.generalLogging) logEntries.push_back("skipped " + this->currentUrl.second);
				skipped = this->currentUrl.first;
			}
			else {
				// lock URL
				if(this->database.isUrlLockable(this->currentUrl.first)) {
					this->lockTime = this->database.lockUrl(this->currentUrl.first, this->config.generalLock);
					// success!
					break;
				}
				else if(this->config.generalLogging) {
					// skip locked URL
					logEntries.push_back("skipped " + this->currentUrl.second + ", because it is locked.");
					skip = true;
					skipped = this->currentUrl.first;
				}
			}
		}
		else {
			// no more URLs
			result = false;
			break;
		}
	}

	// unlock URL list and write to log if necessary
	this->database.releaseLocks();
	if(this->config.generalLogging) for(auto i = logEntries.begin(); i != logEntries.end(); ++i) this->log(*i);

	// set thread status
	if(result) this->setStatusMessage(this->currentUrl.second);
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
unsigned long Thread::parsing() {
	std::string parsedId;

	// parse ID from URL if possible (using RegEx only)
	if(this->idFromUrl) {
		for(auto i = this->queriesId.begin(); i != this->queriesId.end(); ++i) {
			// check result type of query
			if(!(i->resultSingle) && (this->config.generalLogging))
				this->log("WARNING: Invalid result type of ID query (not single).");

			// check query type
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				// parse id by running RegEx query on URL
				if(this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.second, parsedId) && !parsedId.empty()) break;
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
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
			if(this->database.getLatestContent(this->currentUrl.first, index, latestContent)) {
				if(this->parsingContent(latestContent, parsedId)) return 1;
				index++;
			}
			else break;
		}
	}
	else {
		// parse all contents of URL
		unsigned long counter = 0;

		std::vector<std::pair<unsigned long, std::string>> contents = this->database.getAllContents(this->currentUrl.first);
		for(auto i = contents.begin(); i != contents.end(); ++i) if(this->parsingContent(*i, parsedId)) counter++;

		return counter;
	}

	return 0;
}

// parse id-specific content, return whether parsing was successfull (i.e. an id could be parsed)
bool Thread::parsingContent(const std::pair<unsigned long, std::string>& content,
		const std::string& parsedId) {
	// parse HTML
	crawlservpp::Parsing::XML parsedContent;
	if(!parsedContent.parse(content.second)) {
		if(this->config.generalLogging > Config::generalLoggingDefault) {
			std::ostringstream logStrStr;
			logStrStr << "Content #" << content.first << " [" << this->currentUrl.second << "] could not be parsed.";
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
			if(this->config.parsingIdSources.at(idQueryCounter) == Config::parsingSourceUrl) {
				// check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse id by running RegEx query on URL
					if(this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.second, id) && !id.empty()) break;
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: ID query on URL is not of type RegEx.");
			}
			else {
				// check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse id by running RegEx query on content string
					if(this->getRegExQueryPtr(i->index)->getFirst(content.second, id) && !id.empty()) break;
				}
				else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
					// parse if by running XPath query on parsed content
					if(this->getXPathQueryPtr(i->index)->getFirst(parsedContent, id) && !id.empty()) break;
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: ID query on content is not of type RegEx or XPath.");
			}

			// not successfull: check next query for parsing the id (if exists)
			idQueryCounter++;
		}
	}

	// check ID
	if(id.empty()) return false;
	if(!(this->config.parsingIdIgnore.empty()) && std::find(this->config.parsingIdIgnore.begin(), this->config.parsingIdIgnore.end(),
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
		if(this->config.parsingDateTimeSources.at(dateTimeQueryCounter) == Config::parsingSourceUrl) {
			// check query type
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				// parse date/time by running RegEx query on URL
				querySuccess = this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.second, parsedDateTime);
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
				this->log("WARNING: DateTime query on URL is not of type RegEx.");
		}
		else {
			// check query type
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				// parse date/time by running RegEx query on content string
				querySuccess = this->getRegExQueryPtr(i->index)->getFirst(content.second, parsedDateTime);
			}
			else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
				// parse date/time by running XPath query on parsed content
				querySuccess = this->getXPathQueryPtr(i->index)->getFirst(parsedContent, parsedDateTime);
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
				this->log("WARNING: DateTime query on content is not of type RegEx or XPath.");
		}

		if(querySuccess && !parsedDateTime.empty()) {
			// found date/time: try to convert it to SQL time stamp
			std::string format = this->config.parsingDateTimeFormats.at(dateTimeQueryCounter);
			std::string locale = this->config.parsingDateTimeLocales.at(dateTimeQueryCounter);

			// use "%F %T" as default date/time format
			if(format.empty()) format = "%F %T";

			if(!locale.empty()) {
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

			if(dateTimeSuccess && !parsedDateTime.empty()) break;
		}

		// not successfull: check next query for parsing the date/time (if exists)
		dateTimeQueryCounter++;
	}

	// check whether date/time conversion was successful
	if(!parsedDateTime.empty() && !dateTimeSuccess) {
		if(this->config.generalLogging) this->log("ERROR: Could not parse date/time \'" + parsedDateTime + "\'!");
		parsedDateTime = "";
	}

	// parse custom fields
	std::vector<std::string> parsedFields;
	unsigned long fieldCounter = 0;
	parsedFields.reserve(this->queriesFields.size());

	for(auto i = this->queriesFields.begin(); i != this->queriesFields.end(); ++i) {
		// determinate whether to get all or just the first match (as string or as boolean value) from the query result
		if(i->resultMulti) {
			// parse multiple elements
			std::vector<std::string> parsedFieldValues;

			// check query source
			if(this->config.parsingFieldSources.at(fieldCounter) == Config::parsingSourceUrl) {
				// parse from URL: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse multiple field elements by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getAll(this->currentUrl.second, parsedFieldValues);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse multiple field elements by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getAll(content.second, parsedFieldValues);
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
					if(!(i->empty())) {
						empty = false;
						break;
					}
				}
				if(empty) this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\' is empty for "
						+ this->currentUrl.second);
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
			if(this->config.parsingFieldSources.at(fieldCounter) == Config::parsingSourceUrl) {
				// parse from URL: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse single field element by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getFirst(this->currentUrl.second, parsedFieldValue);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse single field element by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getFirst(content.second, parsedFieldValue);
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
			if(this->config.generalLogging && this->config.parsingFieldWarningsEmpty.at(fieldCounter) && parsedFieldValue.empty())
				this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter) + "\' is empty for "
										+ this->currentUrl.second);

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
			if(this->config.parsingFieldSources.at(fieldCounter) == Config::parsingSourceUrl) {
				// parse from URL: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse boolean value by running RegEx query on URL
					this->getRegExQueryPtr(i->index)->getBool(this->currentUrl.second, parsedBool);
				}
				else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.generalLogging)
					this->log("WARNING: \'" + this->config.parsingFieldNames.at(fieldCounter)
							+ "\' query on URL is not of type RegEx.");
			}
			else {
				// parse from content: check query type
				if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
					// parse boolean value by running RegEx query on content string
					this->getRegExQueryPtr(i->index)->getBool(content.second, parsedBool);
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
	this->database.updateOrAddEntry(content.first, parsedId, parsedDateTime, parsedFields);
	return true;
}

}
