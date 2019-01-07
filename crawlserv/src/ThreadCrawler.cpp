/*
 * ThreadCrawler.cpp
 *
 * Implementation of the Thread interface for crawler threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "ThreadCrawler.h"

// constructor A: run previously interrupted crawler
ThreadCrawler::ThreadCrawler(Database& dbBase, unsigned long crawlerId, const std::string& crawlerStatus, bool crawlerPaused,
		const ThreadOptions& threadOptions, unsigned long crawlerLast)
	: Thread(dbBase, crawlerId, "crawler", crawlerStatus, crawlerPaused, threadOptions, crawlerLast), database(this->Thread::database) {
	// set default values
	this->parser = NULL;
	this->networkingArchives = NULL;
	this->tickCounter = 0;
	this->startPageId = 0;
	this->manualCounter = 0;
	this->startCrawled = false;
	this->manualOff = false;
	this->archiveRetry = false;
	this->retryCounter = 0;
	this->httpTime = std::chrono::steady_clock::time_point::min();
	this->startTime = std::chrono::steady_clock::time_point::min();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->idleTime = std::chrono::steady_clock::time_point::min();
}

// constructor B: start a new crawler
ThreadCrawler::ThreadCrawler(Database& dbBase, const ThreadOptions& threadOptions)
	: Thread(dbBase, "crawler", threadOptions), database(this->Thread::database) {
	// set default values
	this->parser = NULL;
	this->networkingArchives = NULL;
	this->tickCounter = 0;
	this->startPageId = 0;
	this->manualCounter = 0;
	this->startCrawled = false;
	this->manualOff = false;
	this->archiveRetry = false;
	this->retryCounter = 0;
	this->httpTime = std::chrono::steady_clock::time_point::min();
	this->startTime = std::chrono::steady_clock::time_point::min();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->idleTime = std::chrono::steady_clock::time_point::min();
}

// destructor: destroy objects that somehow did not get destroyed (should not happen)
ThreadCrawler::~ThreadCrawler() {
	if(this->parser) {
		delete this->parser;
		this->parser = NULL;
	}
	if(this->networkingArchives) {
		delete this->networkingArchives;
		this->networkingArchives = NULL;
	}
}

// initialize crawler
bool ThreadCrawler::onInit(bool resumed) {
	std::vector<std::string> configWarnings;

	// get configuration and show warnings if necessary
	if(!(this->config.loadConfig(this->database.getConfigJson(this->getConfig()), configWarnings))) {
		this->log(this->config.getErrorMessage());
		return false;
	}
	if(this->config.crawlerLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);

	// prepare SQL statements for crawler
	if(!(this->database.prepare(this->getId(), this->websiteNameSpace, this->urlListNameSpace, this->config.crawlerReCrawl,
			config.crawlerLogging == ConfigCrawler::crawlerLoggingVerbose))) {
		if(this->config.crawlerLogging) this->log(this->database.getErrorMessage());
		return false;
	}

	// get domain
	this->domain = this->database.getWebsiteDomain(this->getWebsite());

	// create parser
	if(!this->parser) this->parser = new URIParser;
	this->parser->setCurrentDomain(this->domain);

	// set network configuration
	configWarnings.clear();
	if(config.crawlerLogging == ConfigCrawler::crawlerLoggingVerbose) this->log("sets global network configuration...");
	if(!(this->networking.setCrawlingConfigGlobal(this->config, false, &configWarnings))) {
		if(this->config.crawlerLogging) this->log(this->networking.getErrorMessage());
		return false;
	}
	if(this->config.crawlerLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);
	configWarnings.clear();

	this->database.setSleepOnError(this->config.crawlerSleepMySql);

	// initialize custom URLs
	this->initCustomUrls();

	// initialize queries
	this->initQueries();

	// save start time and initialize counter
	this->startTime = std::chrono::steady_clock::now();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->tickCounter = 0;

	// initialize networking for archives if necessary
	if(this->config.crawlerArchives && !(this->networkingArchives)) {
		this->networkingArchives = new Networking();
		if(!(this->networkingArchives->setCrawlingConfigGlobal(this->config, true, &configWarnings))) {
			if(this->config.crawlerLogging) this->log(this->networking.getErrorMessage());
			return false;
		}
		if(this->config.crawlerLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
			this->log("WARNING: " + *i);
	}

	return true;
}

// crawler tick
bool ThreadCrawler::onTick() {
	IdString url;
	TimerStartStop timerSelect;
	TimerStartStop timerArchives;
	TimerStartStop timerTotal;
	std::string timerString;
	unsigned long checkedUrls = 0;
	unsigned long newUrls = 0;
	unsigned long checkedUrlsArchive = 0;
	unsigned long newUrlsArchive = 0;

	// start timers
	if(this->config.crawlerTiming) {
		timerTotal.start();
		timerSelect.start();
	}

	// URL selection
	if(this->crawlingUrlSelection(url)) {
		if(this->config.crawlerTiming) timerSelect.stop();
		if(this->idleTime > std::chrono::steady_clock::time_point::min()) {
			// idling stopped
			this->startTime += std::chrono::steady_clock::now() - this->idleTime;
			this->pauseTime = std::chrono::steady_clock::time_point::min();
			this->idleTime = std::chrono::steady_clock::time_point::min();
		}

		// increase tick counter
		this->tickCounter++;

		// start crawling
		if(this->config.crawlerLogging > ConfigCrawler::crawlerLoggingDefault) this->log("crawls " + url.string + "...");

		// get content
		bool crawled = this->crawlingContent(url, checkedUrls, newUrls, timerString);

		// get archive (also when crawling failed!)
		if(this->config.crawlerLogging > ConfigCrawler::crawlerLoggingDefault)
			this->log("gets archives of " + url.string + "...");
		if(this->config.crawlerTiming) timerArchives.start();

		if(this->crawlingArchive(url, checkedUrlsArchive, newUrlsArchive) && crawled) {
			// stop timers
			if(this->config.crawlerTiming) {
				timerArchives.stop();
				timerTotal.stop();
			}

			// success!
			this->crawlingSuccess(url);
			if((this->config.crawlerLogging > ConfigCrawler::crawlerLoggingDefault)
					|| (this->config.crawlerTiming && this->config.crawlerLogging)) {
				std::ostringstream logStrStr;
				logStrStr.imbue(std::locale(""));
				logStrStr << "finished " << url.string << " after ";
				logStrStr << timerTotal.totalStr() << " (select: " << timerSelect.totalStr() << ", " << timerString;
				if(this->config.crawlerArchives) logStrStr << ", archive: " << timerArchives.totalStr();
				logStrStr << ") - checked " << checkedUrls;
				if(checkedUrlsArchive) logStrStr << " (+" << checkedUrlsArchive << " archived)";
				logStrStr << ", added " << newUrls;
				if(newUrlsArchive) logStrStr << " (+" << newUrlsArchive << " archived)";
				logStrStr << " URL(s).";
				this->log(logStrStr.str());
			}
		}

		// remove URL lock if necessary
		this->database.lockUrlList();
		if(this->database.checkUrlLock(url.id, this->lockTime)) this->database.unLockUrl(url.id);
		this->database.unlockTables();
		this->lockTime = "";
	}
	else {
		if(this->idleTime == std::chrono::steady_clock::time_point::min()) {
			this->idleTime = std::chrono::steady_clock::now();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(this->config.crawlerSleepIdle));
	}

	return true;
}

// crawler paused
void ThreadCrawler::onPause() {
	// save pause start time
	this->pauseTime = std::chrono::steady_clock::now();
}

// crawler unpaused
void ThreadCrawler::onUnpause() {
	// add pause time to start or idle time to ignore pause
	if(this->idleTime > std::chrono::steady_clock::time_point::min())
		this->idleTime += std::chrono::steady_clock::now() - this->pauseTime;
	else this->startTime += std::chrono::steady_clock::now() - this->pauseTime;
	this->pauseTime = std::chrono::steady_clock::time_point::min();
}

// clear crawler
void ThreadCrawler::onClear(bool interrupted) {
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
	this->queriesBlackListContent.clear();
	this->queriesBlackListTypes.clear();
	this->queriesBlackListUrls.clear();
	this->queriesLinks.clear();
	this->queriesWhiteListContent.clear();
	this->queriesWhiteListTypes.clear();
	this->queriesWhiteListUrls.clear();
	for(auto i = this->queriesXPath.begin(); i != this->queriesXPath.end(); ++i) {
		if(*i) {
			delete *i;
			*i = NULL;
		}
	}
	this->queriesXPath.clear();
	for(auto i = this->queriesRegEx.begin(); i != this->queriesRegEx.end(); ++i) {
		if(*i) {
			delete *i;
			*i = NULL;
		}
	}
	this->queriesRegEx.clear();

	// destroy parser
	if(this->parser) {
		delete this->parser;
		this->parser = NULL;
	}

	// destroy networking for archives
	if(this->networkingArchives) {
		delete this->networkingArchives;
		this->networkingArchives = NULL;
	}
}

// hide functions not to be used by thread
void ThreadCrawler::start() { throw(std::logic_error("Thread::start() not to be used by thread itself")); }
void ThreadCrawler::pause() { throw(std::logic_error("Thread::pause() not to be used by thread itself")); }
void ThreadCrawler::unpause() { throw(std::logic_error("Thread::unpause() not to be used by thread itself")); }
void ThreadCrawler::stop() { throw(std::logic_error("Thread::stop() not to be used by thread itself")); }
void ThreadCrawler::interrupt() { throw(std::logic_error("Thread::interrupt() not to be used by thread itself")); }

// initialize function for custom URLs
void ThreadCrawler::initCustomUrls() {
	// lock URL list
	if(this->config.crawlerLogging == ConfigCrawler::crawlerLoggingVerbose) this->log("initializes start page and custom URLs...");
	this->database.lockUrlList();

	// get id for start page (and add it to URL list if necessary)
	if(this->database.isUrlExists(this->config.crawlerStart)) this->startPageId = this->database.getUrlId(this->config.crawlerStart);
	else this->startPageId = this->database.addUrl(this->config.crawlerStart, true);

	if(this->config.customCounters.size()) {
		// run custom counters
		std::vector<std::string> newUrls;
		newUrls.reserve(this->config.customCounters.size());

		if(this->config.customCountersGlobal) {
			// run each counter over every URL
			newUrls = this->config.customUrls;
			for(unsigned long n = 0; n < this->config.customCounters.size(); n++) {
				this->initDoGlobalCounting(newUrls, this->config.customCounters.at(n), this->config.customCountersStart.at(n),
						this->config.customCountersEnd.at(n), this->config.customCountersStep.at(n));
			}
		}
		else {
			// run each counter over one URL
			for(unsigned long n = 0; n < std::min(this->config.customCounters.size(), this->config.customUrls.size()); n++) {
				std::vector<std::string> temp = this->initDoLocalCounting(this->config.customUrls.at(n),
						this->config.customCounters.at(n), this->config.customCountersStart.at(n),
						this->config.customCountersEnd.at(n), this->config.customCountersStep.at(n));
				newUrls.reserve(newUrls.size() + temp.size());
				newUrls.insert(newUrls.end(), temp.begin(), temp.end());
			}
		}

		this->customPages.reserve(newUrls.size());
		for(auto i = newUrls.begin(); i != newUrls.end(); ++i) this->customPages.push_back(IdString(0, *i));
	}
	else {
		// no counters: add all custom URLs as is
		this->customPages.reserve(this->config.customUrls.size());
		for(auto i = this->config.customUrls.begin(); i != this->config.customUrls.end(); ++i)
			this->customPages.push_back(IdString(0, *i));
	}

	// get ids for custom URLs (and add them to the URL list if necessary)
	for(auto i = this->customPages.begin(); i != this->customPages.end(); ++i) {
		if(this->database.isUrlExists(i->string)) i->id = this->database.getUrlId(i->string);
		else i->id = this->database.addUrl(i->string, true);
	}

	// unlock URL list
	this->database.unlockTables();
}

// use a counter to multiply a list of URLs ("global" counting
void ThreadCrawler::initDoGlobalCounting(std::vector<std::string>& urlList, const std::string& variable, long start, long end, long step) {
	std::vector<std::string> newUrlList;
	for(auto i = urlList.begin(); i != urlList.end(); ++i) {
		if(i->find(variable) != std::string::npos) {
			long counter = start;
			while((start > end && counter >= end) || (start < end && counter <= end) || (start == end)) {
				std::string newUrl = *i;
				std::ostringstream counterStrStr;
				counterStrStr << counter;
				Strings::replaceAll(newUrl, variable, counterStrStr.str());
				newUrlList.push_back(newUrl);
				if(start == end) break;
				counter += step;
			}

			// remove duplicates
			std::sort(newUrlList.begin(), newUrlList.end());
			newUrlList.erase(std::unique(newUrlList.begin(), newUrlList.end()), newUrlList.end());
		}
		else newUrlList.push_back(*i); // variable not in URL
	}
	urlList.swap(newUrlList);
}

// use a counter to multiply a single URL ("local" counting)
std::vector<std::string> ThreadCrawler::initDoLocalCounting(const std::string& url, const std::string& variable, long start, long end,
		long step) {
	std::vector<std::string> newUrlList;
	if(url.find(variable) != std::string::npos) {
		long counter = start;
		while((start > end && counter >= end) || (start < end && counter <= end) || (start == end)) {
			std::string newUrl = url;
			std::ostringstream counterStrStr;
			counterStrStr << counter;
			Strings::replaceAll(newUrl, variable, counterStrStr.str());
			newUrlList.push_back(newUrl);
			if(start == end) break;
			counter += step;
		}

		// remove duplicates
		std::sort(newUrlList.begin(), newUrlList.end());
		newUrlList.erase(std::unique(newUrlList.begin(), newUrlList.end()), newUrlList.end());
	}
	else newUrlList.push_back(url); // variable not in URL
	return newUrlList;
}

// initialize queries
void ThreadCrawler::initQueries() {
	std::string queryText;
	std::string queryType;
	bool queryResultBool = false;
	bool queryResultSingle = false;
	bool queryResultMulti = false;
	bool queryTextOnly = false;

	for(auto i = this->config.crawlerQueriesBlackListContent.begin(); i != this->config.crawlerQueriesBlackListContent.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
		this->queriesBlackListContent.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
				queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesBlackListTypes.begin(); i != this->config.crawlerQueriesBlackListTypes.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
			this->queriesBlackListTypes.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
					queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesBlackListUrls.begin(); i != this->config.crawlerQueriesBlackListUrls.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
			this->queriesBlackListUrls.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
					queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesLinks.begin(); i != this->config.crawlerQueriesLinks.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
			this->queriesLinks.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
					queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesWhiteListContent.begin(); i != this->config.crawlerQueriesWhiteListContent.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
			this->queriesWhiteListContent.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
					queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesWhiteListTypes.begin(); i != this->config.crawlerQueriesWhiteListTypes.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
			this->queriesWhiteListTypes.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
					queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesWhiteListUrls.begin(); i != this->config.crawlerQueriesWhiteListUrls.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti, queryTextOnly);
			this->queriesWhiteListUrls.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
					queryResultMulti, queryTextOnly));
	}
}

// add a query to the crawler
ThreadCrawler::Query ThreadCrawler::addQuery(const std::string& queryText, const std::string& queryType, bool queryResultBool,
		bool queryResultSingle, bool queryResultMulti, bool queryTextOnly) {
	ThreadCrawler::Query newQuery;
	newQuery.resultBool = queryResultBool;
	newQuery.resultSingle = queryResultSingle;
	newQuery.resultMulti = queryResultMulti;

	if(queryType == "regex") {
		RegEx * regex = new RegEx;
		regex->compile(queryText, queryResultBool, queryResultMulti);
		newQuery.index = this->queriesRegEx.size();
		newQuery.type = ThreadCrawler::Query::typeRegEx;
		this->queriesRegEx.push_back(regex);
	}
	else if(queryType == "xpath") {
		XPath * xpath = new XPath;
		xpath->compile(queryText, queryTextOnly);
		newQuery.index = this->queriesXPath.size();
		newQuery.type = ThreadCrawler::Query::typeXPath;
		this->queriesXPath.push_back(xpath);
	}
	else throw std::runtime_error("Unknown query type \'" + queryType + "\'");

	return newQuery;
}

// crawling function for URL selection
bool ThreadCrawler::crawlingUrlSelection(IdString& urlTo) {
	std::vector<std::string> logEntries;
	bool result = true;

	// lock URL list
	this->database.lockUrlList();

	// MANUAL CRAWLING MODE (get URL from configuration)
	if(!(this->getLast())) {
		if(this->manualUrl.id) {
			// re-try custom URL or start page if not locked
			this->database.lockUrlList();
			if(this->database.renewUrlLock(this->config.crawlerLock, this->manualUrl.id, this->lockTime)) {
				urlTo = this->manualUrl;
			}
			else {
				// skipped locked URL
				logEntries.push_back("URL lock active - " + this->manualUrl.string + " skipped.");
				this->manualUrl = IdString(0, "");
			}
		}

		if(!(this->manualUrl.id)) {
			// no retry: check custom URLs
			if(this->customPages.size()) {
				if(!(this->manualCounter)) {
					// start manual crawling with custom URLs
					logEntries.push_back("starts crawling in non-recoverable MANUAL mode.");
				}

				// get next custom URL (that is lockable and maybe not crawled)
				while(this->manualCounter < this->customPages.size()) {
					this->manualUrl = this->customPages.at(this->manualCounter);

					if(!(this->config.customReCrawl)) {
						if(this->database.isUrlCrawled(this->manualUrl.id)) {
							this->manualCounter++;
							this->manualUrl = IdString(0, "");
							continue;
						}
					}

					if(this->database.isUrlLockable(this->manualUrl.id)) {
						this->lockTime = this->database.lockUrl(this->manualUrl.id, this->config.crawlerLock);
						urlTo = this->manualUrl;
						break;
					}

					// skip locked custom URL
					logEntries.push_back("URL lock active - " + this->manualUrl.string + " skipped.");
					this->manualCounter++;
					this->manualUrl = IdString(0, "");
				}
			}

			if(this->manualCounter == this->customPages.size()) {
				// no more custom URLs to go: get start page (if lockable)
				if(!(this->startCrawled)) {
					if(!(this->customPages.size())) {
						// start manual crawling with start page
						logEntries.push_back("starts crawling in non-recoverable MANUAL mode.");
					}

					this->manualUrl = IdString(this->startPageId, this->config.crawlerStart);
					if((this->config.crawlerReCrawlStart || !(this->database.isUrlCrawled(this->startPageId)))
							&& this->database.isUrlLockable(this->startPageId)) {
						this->lockTime = this->database.lockUrl(this->manualUrl.id, this->config.crawlerLock);
						urlTo = this->manualUrl;
					}
					else {
						// skip locked start page
						logEntries.push_back("URL lock active - " + this->manualUrl.string + " skipped.");
						this->manualUrl = IdString(0, "");
						this->startCrawled = true;
					}
				}
			}
		}
	}

	// AUTOMATIC CRAWLING MODE (get URL directly from database)
	if(!(this->manualUrl.id)) {
		// check whether manual crawling mode was already set off
		if(!(this->manualOff)) {
			// start manual crawling with start page
			logEntries.push_back("switches to recoverable AUTOMATIC mode.");
			this->manualOff = true;
		}

		// check for re-try
		if(this->nextUrl.id && this->database.checkUrlLock(this->nextUrl.id, this->lockTime)) {
			this->lockTime = this->database.lockUrl(this->nextUrl.id, this->config.crawlerLock);
			logEntries.push_back("retries " + this->nextUrl.string + "...");
			urlTo = this->nextUrl;
		}
		else {
			if(this->nextUrl.id) logEntries.push_back("could not retry " + this->nextUrl.string + ", because it is locked.");
			while(true) {
				// get id and name of next URL
				this->nextUrl = this->database.getNextUrl(this->getLast());

				if(this->nextUrl.id) {
					// lock URL
					if(this->database.isUrlLockable(this->nextUrl.id)) {
						this->lockTime = this->database.lockUrl(this->nextUrl.id, this->config.crawlerLock);
						urlTo = this->nextUrl;
						// success!
						break;
					}
					else {
						// skip locked URL
						logEntries.push_back("skipped " + this->nextUrl.string + ", because it is locked.");
					}
				}
				else {
					// no more URLs
					result = false;
					break;
				}
			}
		}
	}

	// unlock URL list and write to log if necessary
	this->database.unlockTables();
	if(this->config.crawlerLogging) for(auto i = logEntries.begin(); i != logEntries.end(); ++i) this->log(*i);

	// set thread status
	if(result) this->setStatusMessage(urlTo.string);
	else {
		this->setStatusMessage("IDLE Waiting for new URLs to crawl.");
		this->setProgress(1L);
	}

	return result;
}

// crawl content
bool ThreadCrawler::crawlingContent(const IdString& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo,
		std::string& timerStrTo) {
	TimerStartStop sleepTimer;
	TimerStartStop httpTimer;
	TimerStartStop parseTimer;
	TimerStartStop updateTimer;
	std::string content;
	std::string contentType;
	timerStrTo = "";

	// skip crawling if only archive needs to be retried
	if(this->config.crawlerArchives && this->archiveRetry) {
		if(this->config.crawlerLogging > ConfigCrawler::crawlerLoggingDefault) this->log("Re-trying archive only [" + url.string + "].");
		return true;
	}

	// check HTTP sleeping time
	if(this->config.crawlerSleepHttp) {
		// calculate elapsed time since last HTTP request and sleep if necessary
		unsigned long httpElapsed =
				std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - this->httpTime).count();
		if(httpElapsed < this->config.crawlerSleepHttp) {
			this->idleTime = std::chrono::steady_clock::now();
			if(this->config.crawlerTiming) sleepTimer.start();
			std::this_thread::sleep_for(std::chrono::milliseconds(this->config.crawlerSleepHttp - httpElapsed));
			if(this->config.crawlerTiming) {
				sleepTimer.stop();
				timerStrTo = "sleep: " + sleepTimer.totalStr();
			}
			this->startTime += std::chrono::steady_clock::now() - this->idleTime;
			this->idleTime = std::chrono::steady_clock::time_point::min();
		}
	}

	// set local networking options
	if(this->networking.setCrawlingConfigCurrent(this->config)) {

		// get content
		if(this->config.crawlerTiming) httpTimer.start();
		if(this->config.crawlerSleepHttp) this->httpTime = std::chrono::steady_clock::now();
		if(this->networking.getContent("https://" + this->domain + url.string, content, this->config.crawlerRetryHttp)) {

			// check response code
			unsigned int responseCode = this->networking.getResponseCode();
			if(this->crawlingCheckResponseCode(url.string, responseCode)) {
				if(this->config.crawlerTiming) {
					httpTimer.stop();
					if(timerStrTo.length()) timerStrTo += ", ";
					timerStrTo += "http: " + httpTimer.totalStr();
					parseTimer.start();
				}

				// check content type
				std::string contentType = this->networking.getContentType();
				if(this->crawlingCheckContentType(url, contentType)) {

					// parse content
					XMLDocument doc;
					if(doc.parse(content)) {

						// check content
						if(this->crawlingCheckContent(url, content, doc)) {
							if(this->config.crawlerTiming) {
								parseTimer.stop();
								updateTimer.start();
							}

							// save content
							this->crawlingSaveContent(url, responseCode, contentType, content, doc);

							if(this->config.crawlerTiming) {
								updateTimer.stop();
								parseTimer.start();
							}

							// extract URLs
							std::vector<std::string> urls = this->crawlingExtractUrls(url, content, doc);
							if(urls.size()) {
								if(this->config.crawlerTiming) {
									parseTimer.stop();
									updateTimer.start();
								}

								// parse and add URLs
								checkedUrlsTo += urls.size();
								this->crawlingParseAndAddUrls(url, urls, newUrlsTo, false);

								if(this->config.crawlerTiming) {
									updateTimer.stop();
									timerStrTo += ", parse: " + parseTimer.totalStr() + ", update: " + updateTimer.totalStr();
								}
							}
						}
						else {
							// skip content (not on whitelist or on blacklist)
							this->crawlingSkip(url);
							return false;
						}
					}
					else {
						// error while parsing content
						if(this->config.crawlerLogging) this->log(doc.getErrorMessage() + " [" + url.string + "].");
						this->crawlingSkip(url);
						return false;
					}
				}
				else {
					// skip content type (not on whitelist or on blacklist)
					this->crawlingSkip(url);
					return false;
				}
			}
			else {
				// response code: skip
				this->crawlingSkip(url);
				return false;
			}
		}
		else {
			// error while getting content: check type of error i.e. last cURL code
			CURLcode curlCode = this->networking.getCurlCode();
			if(curlCode == CURLE_TOO_MANY_REDIRECTS) {
				// redirection error: skip URL
				if(this->config.crawlerLogging) this->log("redirection error at " + url.string + " - skips...");
				this->crawlingSkip(url);
			}
			else {
				// other error: reset connection and retry
				if(this->config.crawlerLogging) {
					this->log(this->networking.getErrorMessage() + " [" + url.string + "].");
					this->log("resets connection...");
				}
				this->setStatusMessage("ERROR " + this->networking.getErrorMessage() + " [" + url.string + "]");
				this->networking.resetConnection(this->config.crawlerSleepError);
				this->crawlingRetry(url, false);
			}
			return false;
		}
	}
	else {
		// error while setting up network
		if(this->config.crawlerLogging) {
			this->log(this->networking.getErrorMessage() + " [" + url.string + "].");
			this->log("resets connection...");
		}
		this->setStatusMessage("ERROR " + this->networking.getErrorMessage() + " [" + url.string + "]");
		this->networking.resetConnection(this->config.crawlerSleepError);
		this->crawlingRetry(url, false);
		return false;
	}

	return true;
}

// check whether URL should be added
bool ThreadCrawler::crawlingCheckUrl(const std::string& url) {
	if(!url.length()) return false;

	if(this->queriesWhiteListUrls.size()) {
		// whitelist: only allow URLs that fit a specified whitelist query
		bool found = false;
		for(auto i = this->queriesWhiteListUrls.begin(); i != this->queriesWhiteListUrls.end(); ++i) {
			if(i->type == ThreadCrawler::Query::typeRegEx) {
				if(!(this->queriesRegEx.at(i->index)->getBool(url, found)))
					if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
							+ " [" + url + "].");
				if(found) break;
			}
			else if(this->config.crawlerLogging) this->log("WARNING: Query on URL is not of type RegEx.");
		}
		if(!found) {
			if(this->config.crawlerLogging > ConfigCrawler::crawlerLoggingDefault) this->log("skipped " + url + " (not whitelisted).");
			return false;
		}
	}

	if(this->queriesBlackListUrls.size()) {
		// blacklist: do not allow URLs that fit a specified blacklist query
		for(auto i = this->queriesBlackListUrls.begin(); i != this->queriesBlackListUrls.end(); ++i) {
			if(i->type == ThreadCrawler::Query::typeRegEx) {
				bool found = false;
				if(!(this->queriesRegEx.at(i->index)->getBool(url, found)))
					if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
							+ " [" + url + "].");
				if(found) {
					if(this->config.crawlerLogging > ConfigCrawler::crawlerLoggingDefault) this->log("skipped " + url + " (blacklisted).");
					return false;
				}
			}
			else if(this->config.crawlerLogging) this->log("WARNING: Query on URL is not of type RegEx.");
		}
	}

	return true;
}

// check the HTTP response code for an error and decide whether to continue or skip
bool ThreadCrawler::crawlingCheckResponseCode(const std::string& url, long responseCode) {
	if(responseCode >= 400 && responseCode < 600) {
		if(this->config.crawlerLogging) {
			std::ostringstream logStrStr;
			logStrStr << "HTTP error " << responseCode << " from " << url << " - skips...";
			this->log(logStrStr.str());
		}
		return false;
	}
	else if(responseCode != 200 && this->config.crawlerLogging) {
		if(this->config.crawlerLogging) {
			std::ostringstream logStrStr;
			logStrStr << "WARNING: HTTP response code " << responseCode << " from " << url << ".";
			this->log(logStrStr.str());
		}
	}

	return true;
}

// check whether specific content type should be crawled
bool ThreadCrawler::crawlingCheckContentType(const IdString& url, const std::string& contentType) {
	// check whitelist for content types
	if(this->queriesWhiteListTypes.size()) {
		bool found = false;
		for(auto i = this->queriesWhiteListTypes.begin(); i != this->queriesWhiteListTypes.end(); ++i) {
			if(i->type == ThreadCrawler::Query::typeRegEx) {
				if(!(this->queriesRegEx.at(i->index)->getBool(contentType, found)))
					if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
							+ " [" + url.string + "].");
				if(found) break;
			}
			else if(this->config.crawlerLogging) this->log("WARNING: Query on content type is not of type RegEx.");
		}
		if(!found) return false;
	}

	// check blacklist for content types
	if(this->queriesBlackListTypes.size()) {
		bool found = false;
		for(auto i = this->queriesBlackListTypes.begin(); i != this->queriesBlackListTypes.end(); ++i) {
			if(i->type == ThreadCrawler::Query::typeRegEx) {
				if(!(this->queriesRegEx.at(i->index)->getBool(contentType, found)))
					if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
						+ " [" + url.string + "].");
				if(found) break;
			}
			else if(this->config.crawlerLogging) this->log("WARNING: Query on content type is not of type RegEx.");
		}
		if(found) return false;
	}

	return true;
}

// check whether specific content should be crawled
bool ThreadCrawler::crawlingCheckContent(const IdString& url, const std::string& content, const XMLDocument& doc) {
	// check whitelist for content types
	if(this->queriesWhiteListContent.size()) {
		bool found = false;
		for(auto i = this->queriesWhiteListContent.begin(); i != this->queriesWhiteListContent.end(); ++i) {
			if(i->type == ThreadCrawler::Query::typeRegEx) {
				if(!(this->queriesRegEx.at(i->index)->getBool(content, found)))
					if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
						+ " [" + url.string + "].");
				if(found) break;
			}
			else if(i->type == ThreadCrawler::Query::typeXPath) {
				if(!(this->queriesXPath.at(i->index)->getBool(doc, found)))
					if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
						+ " [" + url.string + "].");
				if(found) break;
			}
			else if(this->config.crawlerLogging) this->log("WARNING: Query on content type is not of type RegEx or XPath.");
		}
		if(!found) return false;
	}

	// check blacklist for content types
	if(this->queriesBlackListContent.size()) {
		bool found = false;
		for(auto i = this->queriesBlackListContent.begin(); i != this->queriesBlackListContent.end(); ++i) {
			if(i->type == ThreadCrawler::Query::typeRegEx) {
				if(!(this->queriesRegEx.at(i->index)->getBool(content, found)))
					if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
						+ " [" + url.string + "].");
				if(found) break;
			}
			else if(i->type == ThreadCrawler::Query::typeXPath) {
				if(!(this->queriesXPath.at(i->index)->getBool(doc, found)))
					if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
						+ " [" + url.string + "].");
				if(found) break;
			}
			else if(this->config.crawlerLogging) this->log("WARNING: Query on content type is not of type RegEx or XPath.");
		}
		if(found) return false;
	}

	return true;
}

// save content to database
void ThreadCrawler::crawlingSaveContent(const IdString& url, unsigned int response, const std::string& type, const std::string& content,
		const XMLDocument& doc) {
	if(this->config.crawlerXml) {
		std::string xmlContent;
		if(doc.getContent(xmlContent)) {
			this->database.saveContent(url.id, response, type, xmlContent);
			return;
		}
		else if(this->config.crawlerLogging) this->log("WARNING: Could not clean content [" + url.string + "].");
	}
	this->database.saveContent(url.id, response, type, content);
}

// extract URLs from content
std::vector<std::string> ThreadCrawler::crawlingExtractUrls(const IdString& url, const std::string& content, const XMLDocument& doc) {
	std::vector<std::string> urls;
	for(auto i = this->queriesLinks.begin(); i != this->queriesLinks.end(); ++i) {
		if(i->type == ThreadCrawler::Query::typeRegEx) {
			if(i->resultMulti) {
				std::vector<std::string> results;
				if((this->queriesRegEx.at(i->index)->getAll(content, results))) {
					urls.resize(urls.size() + results.size());
					urls.insert(urls.end(), results.begin(), results.end());
				}
				else if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
					+ " [" + url.string + "].");
			}
			else {
				std::string result;
				if((this->queriesRegEx.at(i->index)->getFirst(content, result))) {
					urls.push_back(result);
				}
				else if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
					+ " [" + url.string + "].");
			}
		}
		else if(i->type == ThreadCrawler::Query::typeXPath) {
			if(i->resultMulti) {
				std::vector<std::string> results;
				if(this->queriesXPath.at(i->index)->getAll(doc, results)) {
					urls.resize(urls.size() + results.size());
					urls.insert(urls.end(), results.begin(), results.end());
				}
				else if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
					+ " [" + url.string + "].");
			}
			else {
				std::string result;
				if(this->queriesXPath.at(i->index)->getFirst(doc, result)) {
					urls.push_back(result);
				}
				else if(this->config.crawlerLogging) this->log(this->queriesRegEx.at(i->index)->getErrorMessage()
					+ " [" + url.string + "].");
			}
		}
		else if(this->config.crawlerLogging) this->log("WARNING: Query on content type is not of type RegEx or XPath.");
	}

	// remove duplicates
	std::sort(urls.begin(), urls.end());
	urls.erase(std::unique(urls.begin(), urls.end()), urls.end());
	return urls;
}

// parse URLs and add them as sub-links to the database if they do not already exist
void ThreadCrawler::crawlingParseAndAddUrls(const IdString& url, std::vector<std::string>& urls, unsigned long& newUrlsTo, bool archived) {
	// set current URL
	if(!(this->parser->setCurrentSubUrl(url.string))) {
		if(this->config.crawlerLogging) this->log(this->parser->getErrorMessage());
		throw std::runtime_error("Could not set current sub-url!");
	}

	// parse URLs
	for(unsigned long n = 1; n <= urls.size(); n++) {
		// parse archive URLs (only absolute links behind archive links!)
		if(archived) {
			unsigned long pos = 0;
			unsigned long pos1 = urls.at(n - 1).find("https://", 1);
			unsigned long pos2 = urls.at(n - 1).find("http://", 1);
			if(pos1 != std::string::npos && pos2 != std::string::npos) {
				if(pos1 < pos2) pos = pos2;
				else pos = pos1;
			}
			else if(pos1 != std::string::npos) pos = pos1;
			else if(pos2 != std::string::npos) pos = pos2;
			if(pos) urls.at(n - 1) = URIParser::unescape(urls.at(n - 1).substr(pos), false);
			else urls.at(n - 1) = "";
		}

		if(urls.at(n - 1).length()) {
			// replace &amp; with &
			unsigned long pos = 0;
			std::string processed;
			while(pos < urls.at(n - 1).length()) {
				unsigned long end = urls.at(n - 1).find("&amp;", pos);
				if(end == std::string::npos) {
					if(pos) processed += urls.at(n - 1).substr(pos);
					else processed = urls.at(n - 1);
					break;
				}
				else {
					processed += urls.at(n - 1).substr(pos, end + 1);
					pos += 5;
				}
			}
			urls.at(n - 1) = processed;

			// parse link
			if(this->parser->parseLink(urls.at(n - 1))) {
				if(this->parser->isSameDomain()) {
					if(this->config.crawlerParamsBlackList.size()) {
						urls.at(n - 1) = this->parser->getSubUrl(this->config.crawlerParamsBlackList, false);

					}
					else urls.at(n - 1)
							= this->parser->getSubUrl(this->config.crawlerParamsWhiteList, true);
					if(!(this->crawlingCheckUrl(urls.at(n - 1)))) urls.at(n - 1) = "";

					if(urls.at(n - 1).length()) {
						if(urls.at(n - 1).at(0) != '/') {
							throw std::runtime_error(urls.at(n - 1) + " is no sub-URL!");
						}
						if(this->config.crawlerLogging && urls.at(n - 1).length() > 1 && urls.at(n - 1).at(1) == '#') {
							this->log("WARNING: Found anchor \'" + urls.at(n - 1) + "\'.");
						}
						continue;
					}
				}
			}
			else if(this->config.crawlerLogging && this->parser->getErrorMessage().length())
				this->log("WARNING: " + this->parser->getErrorMessage());
		}

		// delete out-of-domain and empty URLs
		urls.erase(urls.begin() + (n - 1));
		n--;
	}

	// remove duplicates
	std::sort(urls.begin(), urls.end());
	urls.erase(std::unique(urls.begin(), urls.end()), urls.end());

	// get status message
	std::string statusMessage = this->getStatusMessage();

	// lock URL list and add non-existing URLs
	bool locked = false;
	bool longUrls = false;

	unsigned long counter = 0;
	unsigned long linkUrlId = 0;
	for(auto i = urls.begin(); i != urls.end(); ++i) {
		counter++;
		if(counter % 500 == 0) {
			// unlock tables
			if(locked) this->database.unlockTables();

			// set status
			std::ostringstream statusStrStr;
			statusStrStr.imbue(std::locale(""));
			statusStrStr << "[URLs: " << counter << "/" << urls.size() << "] " << statusMessage;
			this->setStatusMessage(statusStrStr.str());

			// lock tables
			this->database.lockUrlList();
			locked = true;
		}

		if(i->size() > 2000) longUrls = true;
		else {
			if(this->database.isUrlExists(*i)) {
				linkUrlId = this->database.getUrlId(*i);
			}
			else {
				if(this->config.crawlerLogging && this->config.crawlerWarningsFile && i->length() && i->back() != '/') {
					this->log("WARNING: Found file \'" + *i + "\'.");
				}

				linkUrlId = this->database.addUrl(*i, false);
				newUrlsTo++;
			}

			// add linkage information to database
			this->database.addLinkIfNotExists(url.id, linkUrlId, archived);
		}
	}

	// unlock URL list
	this->database.unlockTables();

	// reset status
	this->setStatusMessage(statusMessage);

	if(longUrls && this->config.crawlerLogging) this->log("WARNING: URLs longer than 2000 Bytes ignored.");
}

// crawl archives
bool ThreadCrawler::crawlingArchive(const IdString& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo) {
	if(this->config.crawlerArchives && this->networkingArchives) {
		bool success = true;
		bool skip = false;

		for(unsigned long n = 0; n < this->config.crawlerArchivesNames.size(); n++) {
			// skip empty URLs
			if(!(this->config.crawlerArchivesUrlsMemento.at(n).length())
					|| !(this->config.crawlerArchivesUrlsTimemap.at(n).length()))
				continue;

			std::string archivedUrl = this->config.crawlerArchivesUrlsTimemap.at(n) + this->domain + url.string;
			std::string archivedContent;

			while(success && this->isRunning()) {
				// get Memento content
				archivedContent = "";
				if(this->networkingArchives->getContent(archivedUrl, archivedContent, this->config.crawlerRetryHttp)) {

					// check response code
					if(this->crawlingCheckResponseCode(archivedUrl, this->networkingArchives->getResponseCode())) {

						// get Memento content type
						std::string contentType = this->networkingArchives->getContentType();
						if(contentType == "application/link-format") {
							if(archivedContent.length()) {

								// parse Memento response
								std::vector<Memento> mementos;
								std::vector<std::string> warnings;
								archivedUrl = ThreadCrawler::parseMementos(archivedContent, warnings, mementos);
								if(this->config.crawlerLogging) {
									// just show warning, maybe mementos were partially parsed
									for(auto i = warnings.begin(); i != warnings.end(); ++i)
										this->log("Memento parsing WARNING: " + *i + " [" + url.string + "]");
								}


								// get status
								std::string statusMessage = this->getStatusMessage();

								// go through all mementos
								unsigned long counter = 0;
								for(auto i = mementos.begin(); i != mementos.end(); ++i) {
									std::string timeStamp = i->timeStamp;

									// set status
									counter++;
									std::ostringstream statusStrStr;
									statusStrStr.imbue(std::locale(""));
									statusStrStr << "[" + this->config.crawlerArchivesNames.at(n) + ": " << counter << "/"
											<< mementos.size() << "] " << statusMessage;
									this->setStatusMessage(statusStrStr.str());

									// re-new URL lock to avoid duplicate archived content
									if(this->database.checkUrlLock(url.id, this->lockTime)) {
										this->lockTime = this->database.lockUrl(url.id, this->config.crawlerLock);

										while(this->isRunning()) { // loop over references if necessary

											// check whether archived content already exists in database
											if(!(this->database.isArchivedContentExists(url.id, timeStamp))) {

												// check whether thread is till running
												if(!(this->isRunning())) break;

												// get archived content
												archivedContent = "";
												if(this->networkingArchives->getContent(i->url, archivedContent,
														this->config.crawlerRetryHttp)) {

													// check response code
													if(!(this->crawlingCheckResponseCode(i->url,
															this->networkingArchives->getResponseCode()))) break;

													// check whether thread is still running
													if(!(this->isRunning())) break;

													// check archived content
													if(archivedContent.substr(0, 17) == "found capture at ") {

														// found a reference string: get and validate timestamp
														if(DateTime::convertSQLTimeStampToTimeStamp(timeStamp)) {
															unsigned long subUrlPos = i->url.find(timeStamp);
															if(subUrlPos != std::string::npos) {
																subUrlPos += timeStamp.length();
																timeStamp = archivedContent.substr(17, 14);
																i->url = this->config.crawlerArchivesUrlsMemento.at(n) + timeStamp
																		+ i->url.substr(subUrlPos);
																if(DateTime::convertTimeStampToSQLTimeStamp(timeStamp))
																	continue;
																else if(this->config.crawlerLogging)
																	this->log("WARNING: Invalid timestamp \'" + timeStamp + "\' from "
																			+ this->config.crawlerArchivesNames.at(n) + " ["
																			+ url.string + "].");
															}
															else if(this->config.crawlerLogging)
																this->log("WARNING: Could not find timestamp in " + i->url
																		+ " [" + url.string + "].");
														}
														else if(this->config.crawlerLogging)
															this->log("WARNING: Could not convert timestamp in " + i->url
																	+ " [" + url.string + "].");
													}
													else {
														// parse content
														XMLDocument doc;
														if(doc.parse(archivedContent)) {

															// add archived content to database
															this->database.saveArchivedContent(url.id, i->timeStamp,
															this->networkingArchives->getResponseCode(),
															this->networkingArchives->getContentType(), archivedContent);

															// extract URLs
															std::vector<std::string> urls = this->crawlingExtractUrls(url,
																	archivedContent, doc);
															if(urls.size()) {
																// parse and add URLs
																checkedUrlsTo += urls.size();
																this->crawlingParseAndAddUrls(url, urls, newUrlsTo, true);
															}
														}
													}
												}
												else if(this->config.crawlerRetryArchive) success = false;
											}

											// exit loop over all references
											break;
										}
									}

									// check whether thread has been cancelled
									if(!(this->isRunning())) break;
								}

								// reset status message
								if(success) this->setStatusMessage(statusMessage);
								if(archivedUrl.empty()) break;
							}
							else break;
						}
						else break;
					}
					else {
						success = false;
						skip = true;
					}
				}
				else {
					if(this->config.crawlerLogging) {
						this->log(this->networkingArchives->getErrorMessage() + " [" + archivedUrl + "].");
						this->log("resets connection to " + this->config.crawlerArchivesNames.at(n) + "...");
					}
					this->setStatusMessage("ERROR " + this->networkingArchives->getErrorMessage() + " [" + url.string + "]");
					this->networkingArchives->resetConnection(this->config.crawlerSleepError);
					success = false;
				}

				if(!success && this->config.crawlerRetryArchive) {
					if(skip) this->crawlingSkip(url);
					else this->crawlingRetry(url, true);
					return false;
				}
				else if(!success) this->crawlingSkip(url);
			}
		}

		if(success || !(this->config.crawlerRetryArchive)) this->archiveRetry = false;
	}

	return this->isRunning();
}

// crawling successfull
void ThreadCrawler::crawlingSuccess(const IdString& url) {
	// update URL list if possible
	this->database.lockUrlList();
	if(this->database.checkUrlLock(url.id, this->lockTime)) this->database.setUrlFinished(url.id);
	this->database.unlockTables();
	this->lockTime = "";

	if(this->manualUrl.id) {
		// manual mode: disable retry, check for custom URL or start page that has been crawled
		this->manualUrl = IdString(0, "");
		if(this->manualCounter < this->customPages.size()) this->manualCounter++;
		else this->startCrawled = true;
	}
	else {
		// automatic mode: update thread status
		this->setLast(url.id);
		this->setProgress((float) (this->database.getUrlPosition(url.id) + 1) / this->database.getNumberOfUrls());
	}

	// reset retry counter
	this->retryCounter = 0;

	// do not re-try
	this->nextUrl = IdString(0, "");
}

// skip URL after crawling problem
void ThreadCrawler::crawlingSkip(const IdString& url) {
	if(this->manualUrl.id) {
		// manual mode: disable retry, check for custom URL or start page that has been crawled
		this->manualUrl = IdString(0, "");
		if(this->manualCounter < this->customPages.size()) this->manualCounter++;
		else this->startCrawled = true;
	}
	else {
		// automatic mode: update thread status
		this->setLast(url.id);
		this->setProgress((float) (this->database.getUrlPosition(url.id) + 1) / this->database.getNumberOfUrls());
	}

	// reset retry counter
	this->retryCounter = 0;

	// do not re-try
	this->nextUrl = IdString(0, "");
	this->archiveRetry = false;
}

// retry URL (completely) after crawling problem
void ThreadCrawler::crawlingRetry(const IdString& url, bool archiveOnly) {
	if(this->config.crawlerReTries > -1) {
		// increment and check retry counter
		this->retryCounter++;
		if(this->retryCounter > (unsigned long) this->config.crawlerReTries) {
			this->crawlingSkip(url);
			return;
		}
	}
	if(archiveOnly) this->archiveRetry = true;
}

// parse Memento reply, get mementos and link to next page if one exists (also convert timestamps to YYYYMMDD HH:MM:SS)
std::string ThreadCrawler::parseMementos(std::string mementoContent, std::vector<std::string>& warningsTo,
		std::vector<Memento>& mementosTo) {
	std::string nextPage;
	Memento newMemento;
	std::ostringstream warningStrStr;
	unsigned long pos = 0;
	unsigned long end = 0;
	bool mementoStarted = false;
	bool newField = true;

	while(pos < mementoContent.length()) {
		// skip wildchars
		if(mementoContent.at(pos) == ' ' || mementoContent.at(pos) == '\r' || mementoContent.at(pos) == '\n'
				|| mementoContent.at(pos) == '\t') {
			pos++;
		}
		// parse link
		else if(mementoContent.at(pos) == '<') {
			end = mementoContent.find('>', pos + 1);
			if(end == std::string::npos) {
				warningStrStr << "No '>' after '<' for link at " << pos << ".";
				warningsTo.push_back(warningStrStr.str());
				warningStrStr.clear();
				warningStrStr.str(std::string());
				break;
			}
			if(mementoStarted) {
				// memento not finished -> warning
				if(newMemento.url.length() && newMemento.timeStamp.length()) mementosTo.push_back(newMemento);
				warningStrStr << "New memento started without finishing the old one at " << pos << ".";
				warningsTo.push_back(warningStrStr.str());
				warningStrStr.clear();
				warningStrStr.str(std::string());
			}
			mementoStarted = true;
			newMemento.url = mementoContent.substr(pos + 1, end - pos - 1);
			newMemento.timeStamp = "";
			pos = end + 1;
		}
		// parse field separator
		else if(mementoContent.at(pos) == ';') {
			newField = true;
			pos++;
		}
		// parse end of memento
		else if(mementoContent.at(pos) == ',') {
			if(mementoStarted) {
				if(newMemento.url.size() && newMemento.timeStamp.size()) mementosTo.push_back(newMemento);
				mementoStarted  = false;
			}
			pos++;
		}
		else {
			if(!newField) {
				warningStrStr << "Field seperator missing for new field at " << pos << ".";
				warningsTo.push_back(warningStrStr.str());
				warningStrStr.clear();
				warningStrStr.str(std::string());
			}
			else newField = false;
			end = mementoContent.find('=', pos + 1);
			if(end == std::string::npos) {
				end = mementoContent.find_first_of(",;");
				if(end == std::string::npos) {
					warningStrStr << "Cannot find end of field at " << pos << ".";
					warningsTo.push_back(warningStrStr.str());
					warningStrStr.clear();
					warningStrStr.str(std::string());
					break;
				}
				pos = end;
			}
			else {
				std::string fieldName = mementoContent.substr(pos, end - pos);
				unsigned long oldPos = pos;
				pos = mementoContent.find_first_of("\"\'", pos + 1);
				if(pos == std::string::npos) {
					warningStrStr << "Cannot find begin of value at " << oldPos << ".";
					warningsTo.push_back(warningStrStr.str());
					warningStrStr.clear();
					warningStrStr.str(std::string());
					pos++;
					continue;
				}
				end = mementoContent.find_first_of("\"\'", pos + 1);
				if(end == std::string::npos) {
					warningStrStr << "Cannot find end of value at " << pos << ".";
					warningsTo.push_back(warningStrStr.str());
					warningStrStr.clear();
					warningStrStr.str(std::string());
					break;
				}
				std::string fieldValue = mementoContent.substr(pos + 1, end - pos - 1);

				if(fieldName == "datetime") {
					// parse timestamp
					if(DateTime::convertLongDateToSQLTimeStamp(fieldValue)) newMemento.timeStamp = fieldValue;
					else {
						warningStrStr << "Could not convert timestamp \'" << fieldValue << "\'  at " << pos << ".";
						warningsTo.push_back(warningStrStr.str());
						warningStrStr.clear();
						warningStrStr.str(std::string());
					}
				}
				else if(fieldName == "rel") {
					if(fieldValue == "timemap" && newMemento.url.size()) {
						nextPage = newMemento.url;
						newMemento.url = "";
					}
				}
				pos = end + 1;
			}
		}
	}

	if(mementoStarted && newMemento.url.size() && newMemento.timeStamp.size()) mementosTo.push_back(newMemento);
	return nextPage;
}
