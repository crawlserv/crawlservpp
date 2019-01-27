/*
 * Thread.cpp
 *
 * Implementation of the Thread interface for crawler threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.h"

namespace crawlservpp::Module::Crawler {

// constructor A: run previously interrupted crawler
Thread::Thread(crawlservpp::Main::Database& dbBase, unsigned long crawlerId,
		const std::string& crawlerStatus, bool crawlerPaused, const crawlservpp::Struct::ThreadOptions& threadOptions,
		unsigned long crawlerLast)
			: crawlservpp::Module::Thread(dbBase, crawlerId, "crawler", crawlerStatus, crawlerPaused, threadOptions, crawlerLast),
			  database(this->crawlservpp::Module::Thread::database) {
	// set default values
	this->parser = NULL;
	this->networkingArchives = NULL;
	this->tickCounter = 0;
	this->startPageId = 0;
	this->manualCounter = 0;
	this->startCrawled = false;
	this->manualOff = false;
	this->archiveRetry = true;
	this->retryCounter = 0;
	this->httpTime = std::chrono::steady_clock::time_point::min();
	this->startTime = std::chrono::steady_clock::time_point::min();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->idleTime = std::chrono::steady_clock::time_point::min();
}

// constructor B: start a new crawler
Thread::Thread(crawlservpp::Main::Database& dbBase,
		const crawlservpp::Struct::ThreadOptions& threadOptions)
			: crawlservpp::Module::Thread(dbBase, "crawler", threadOptions),
			  database(this->crawlservpp::Module::Thread::database) {
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
Thread::~Thread() {
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
bool Thread::onInit(bool resumed) {
	std::vector<std::string> configWarnings;
	bool verbose = false;

	// get configuration and show warnings if necessary
	if(!(this->config.loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings))) {
		this->log(this->config.getErrorMessage());
		return false;
	}
	if(this->config.crawlerLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);
	verbose = config.crawlerLogging == Config::crawlerLoggingVerbose;

	// check for link extraction queries
	if(this->config.crawlerQueriesLinks.empty()) {
		this->log("ERROR: No link extraction query specified.");
		return false;
	}

	// set database options
	if(verbose) this->log("sets database options...");
	this->database.setId(this->getId());
	this->database.setWebsiteNamespace(this->websiteNamespace);
	this->database.setUrlListNamespace(this->urlListNamespace);
	this->database.setRecrawl(this->config.crawlerReCrawl);
	this->database.setLogging(this->config.crawlerLogging);
	this->database.setVerbose(verbose);
	this->database.setSleepOnError(this->config.crawlerSleepMySql);

	// prepare SQL statements for crawler
	if(verbose) this->log("prepares SQL statements...");
	if(!(this->database.prepare())) {
		if(this->config.crawlerLogging) this->log(this->database.getModuleErrorMessage());
		return false;
	}

	// get domain
	if(verbose) this->log("gets website domain...");
	this->domain = this->database.getWebsiteDomain(this->getWebsite());

	// create URI parser
	if(verbose) this->log("creates URI parser...");
	if(!(this->parser)) this->parser = new crawlservpp::Parsing::URI;
	this->parser->setCurrentDomain(this->domain);

	// set network configuration
	if(verbose) this->log("sets network configuration...");
	configWarnings.clear();
	if(config.crawlerLogging == Config::crawlerLoggingVerbose)
		this->log("sets global network configuration...");
	if(!(this->networking.setConfigGlobal(this->config.network, false, &configWarnings))) {
		if(this->config.crawlerLogging) this->log(this->networking.getErrorMessage());
		return false;
	}
	if(this->config.crawlerLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);
	configWarnings.clear();

	// initialize custom URLs
	if(verbose) this->log("initializes custom URLs...");
	this->initCustomUrls();

	// initialize queries
	if(verbose) this->log("initializes queries...");
	this->initQueries();

	// initialize networking for archives if necessary
	if(this->config.crawlerArchives && !(this->networkingArchives)) {
		if(verbose) this->log("initializes networking for archives...");
		this->networkingArchives = new crawlservpp::Network::Curl();
		if(!(this->networkingArchives->setConfigGlobal(this->config.network, true, &configWarnings))) {
			if(this->config.crawlerLogging) this->log(this->networking.getErrorMessage());
			return false;
		}
		if(this->config.crawlerLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
			this->log("WARNING: " + *i);
	}

	// save start time and initialize counter
	this->startTime = std::chrono::steady_clock::now();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->tickCounter = 0;
	return true;
}

// crawler tick
bool Thread::onTick() {
	std::pair<unsigned long, std::string> url;
	crawlservpp::Timer::StartStop timerSelect;
	crawlservpp::Timer::StartStop timerArchives;
	crawlservpp::Timer::StartStop timerTotal;
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
		if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("crawls " + url.second + "...");

		// get content
		bool crawled = this->crawlingContent(url, checkedUrls, newUrls, timerString);

		// get archive (also when crawling failed!)
		if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("gets archives of " + url.second + "...");
		if(this->config.crawlerTiming) timerArchives.start();

		if(this->crawlingArchive(url, checkedUrlsArchive, newUrlsArchive) && crawled) {
			// stop timers
			if(this->config.crawlerTiming) {
				timerArchives.stop();
				timerTotal.stop();
			}

			// success!
			this->crawlingSuccess(url);
			if((this->config.crawlerLogging > Config::crawlerLoggingDefault)
					|| (this->config.crawlerTiming && this->config.crawlerLogging)) {
				std::ostringstream logStrStr;
				logStrStr.imbue(std::locale(""));
				logStrStr << "finished " << url.second;
				if(this->config.crawlerTiming) {
					logStrStr << " after ";
					logStrStr << timerTotal.totalStr() << " (select: " << timerSelect.totalStr() << ", " << timerString;
					if(this->config.crawlerArchives) logStrStr << ", archive: " << timerArchives.totalStr();
					logStrStr << ")";
				}
				logStrStr << " - checked " << checkedUrls;
				if(checkedUrlsArchive) logStrStr << " (+" << checkedUrlsArchive << " archived)";
				logStrStr << ", added " << newUrls;
				if(newUrlsArchive) logStrStr << " (+" << newUrlsArchive << " archived)";
				logStrStr << " URL(s).";
				this->log(logStrStr.str());
			}
		}

		// remove URL lock if necessary
		this->database.lockUrlList();
		if(this->database.checkUrlLock(url.first, this->lockTime)) this->database.unLockUrl(url.first);
		this->database.releaseLocks();
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
void Thread::onPause() {
	// save pause start time
	this->pauseTime = std::chrono::steady_clock::now();
}

// crawler unpaused
void Thread::onUnpause() {
	// add pause time to start or idle time to ignore pause
	if(this->idleTime > std::chrono::steady_clock::time_point::min())
		this->idleTime += std::chrono::steady_clock::now() - this->pauseTime;
	else this->startTime += std::chrono::steady_clock::now() - this->pauseTime;
	this->pauseTime = std::chrono::steady_clock::time_point::min();
}

// clear crawler
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
	this->queriesBlackListContent.clear();
	this->queriesBlackListTypes.clear();
	this->queriesBlackListUrls.clear();
	this->queriesLinks.clear();
	this->queriesWhiteListContent.clear();
	this->queriesWhiteListTypes.clear();
	this->queriesWhiteListUrls.clear();
	this->clearQueries();

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

// initialize function for custom URLs
void Thread::initCustomUrls() {
	std::vector<std::string> warnings;

	// lock URL list
	if(this->config.crawlerLogging == Config::crawlerLoggingVerbose)
		this->log("initializes start page and custom URLs...");
	this->database.lockUrlList();

	// get id for start page (and add it to URL list if necessary)
	if(this->database.isUrlExists(this->config.crawlerStart)) this->startPageId = this->database.getUrlId(this->config.crawlerStart);
	else this->startPageId = this->database.addUrl(this->config.crawlerStart, true);

	if(!(this->config.customCounters.empty())) {
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
		for(auto i = newUrls.begin(); i != newUrls.end(); ++i) this->customPages.push_back(std::pair<unsigned long, std::string>(0, *i));
	}
	else {
		// no counters: add all custom URLs as is
		this->customPages.reserve(this->config.customUrls.size());
		for(auto i = this->config.customUrls.begin(); i != this->config.customUrls.end(); ++i)
			this->customPages.push_back(std::pair<unsigned long, std::string>(0, *i));
	}

	// get ids for custom URLs (and add them to the URL list if necessary)
	for(auto i = this->customPages.begin(); i != this->customPages.end(); ++i) {
		// check URI
		if(this->parser->setCurrentSubUrl(i->second)) {
			if(this->database.isUrlExists(i->second)) i->first = this->database.getUrlId(i->second);
			else i->first = this->database.addUrl(i->second, true);
		}
		else if(this->config.crawlerLogging) warnings.push_back("WARNING: Skipped invalid custom URL " + i->second);
	}

	// unlock URL list
	this->database.releaseLocks();

	// log warnings
	for(auto i = warnings.begin(); i != warnings.end(); ++i) this->log(*i);
}

// use a counter to multiply a list of URLs ("global" counting)
void Thread::initDoGlobalCounting(std::vector<std::string>& urlList, const std::string& variable, long start, long end, long step) {
	std::vector<std::string> newUrlList;
	newUrlList.reserve(urlList.size());
	for(auto i = urlList.begin(); i != urlList.end(); ++i) {
		if(i->find(variable) != std::string::npos) {
			long counter = start;
			while((start > end && counter >= end) || (start < end && counter <= end) || (start == end)) {
				std::string newUrl = *i;
				std::ostringstream counterStrStr;
				counterStrStr << counter;
				crawlservpp::Helper::Strings::replaceAll(newUrl, variable, counterStrStr.str(), true);
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
std::vector<std::string> Thread::initDoLocalCounting(const std::string& url, const std::string& variable, long start, long end,
		long step) {
	std::vector<std::string> newUrlList;
	if(url.find(variable) != std::string::npos) {
		long counter = start;
		while((start > end && counter >= end) || (start < end && counter <= end) || (start == end)) {
			std::string newUrl = url;
			std::ostringstream counterStrStr;
			counterStrStr << counter;
			crawlservpp::Helper::Strings::replaceAll(newUrl, variable, counterStrStr.str(), true);
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
void Thread::initQueries() {
	std::string queryText;
	std::string queryType;
	bool queryResultBool = false;
	bool queryResultSingle = false;
	bool queryResultMulti = false;
	bool queryTextOnly = false;

	if(this->config.crawlerHTMLCanonicalCheck) {
		this->queryCanonicalCheck = this->addQuery("//link[contains(concat(' ', normalize-space(@rel), ' '), ' canonical ')]/@href", "xpath",
				false, true, false, true);
	}
	for(auto i = this->config.crawlerQueriesBlackListContent.begin(); i != this->config.crawlerQueriesBlackListContent.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesBlackListContent.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
				queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesBlackListTypes.begin(); i != this->config.crawlerQueriesBlackListTypes.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesBlackListTypes.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
					queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesBlackListUrls.begin(); i != this->config.crawlerQueriesBlackListUrls.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesBlackListUrls.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
					queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesLinks.begin(); i != this->config.crawlerQueriesLinks.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesLinks.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesWhiteListContent.begin(); i != this->config.crawlerQueriesWhiteListContent.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesWhiteListContent.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
				queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesWhiteListTypes.begin(); i != this->config.crawlerQueriesWhiteListTypes.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesWhiteListTypes.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
				queryResultMulti, queryTextOnly));
	}
	for(auto i = this->config.crawlerQueriesWhiteListUrls.begin(); i != this->config.crawlerQueriesWhiteListUrls.end(); ++i) {
		this->database.getQueryProperties(*i, queryText, queryType, queryResultBool, queryResultSingle, queryResultMulti,
				queryTextOnly);
		this->queriesWhiteListUrls.push_back(this->addQuery(queryText, queryType, queryResultBool, queryResultSingle,
				queryResultMulti, queryTextOnly));
	}
}

// crawling function for URL selection
bool Thread::crawlingUrlSelection(std::pair<unsigned long, std::string>& urlTo) {
	std::vector<std::string> logEntries;
	bool result = true;

	// lock URL list
	this->database.lockUrlList();

	// MANUAL CRAWLING MODE (get URL from configuration)
	if(!(this->getLast())) {
		if(this->manualUrl.first) {
			// re-try custom URL or start page if not locked
			this->database.lockUrlList();
			if(this->database.renewUrlLock(this->config.crawlerLock, this->manualUrl.first, this->lockTime)) {
				urlTo = this->manualUrl;
			}
			else {
				// skipped locked URL
				logEntries.push_back("URL lock active - " + this->manualUrl.second + " skipped.");
				this->manualUrl = std::pair<unsigned long, std::string>(0, "");
			}
		}

		if(!(this->manualUrl.first)) {
			// no retry: check custom URLs
			if(!(this->customPages.empty())) {
				if(!(this->manualCounter)) {
					// start manual crawling with custom URLs
					logEntries.push_back("starts crawling in non-recoverable MANUAL mode.");
				}

				// get next custom URL (that is lockable and maybe not crawled)
				while(this->manualCounter < this->customPages.size()) {
					this->manualUrl = this->customPages.at(this->manualCounter);

					if(!(this->config.customReCrawl)) {
						if(this->database.isUrlCrawled(this->manualUrl.first)) {
							this->manualCounter++;
							this->manualUrl = std::pair<unsigned long, std::string>(0, "");
							continue;
						}
					}

					if(this->database.isUrlLockable(this->manualUrl.first)) {
						this->lockTime = this->database.lockUrl(this->manualUrl.first, this->config.crawlerLock);
						urlTo = this->manualUrl;
						break;
					}

					// skip locked custom URL
					logEntries.push_back("URL lock active - " + this->manualUrl.second + " skipped.");
					this->manualCounter++;
					this->manualUrl = std::pair<unsigned long, std::string>(0, "");
				}
			}

			if(this->manualCounter == this->customPages.size()) {
				// no more custom URLs to go: get start page (if lockable)
				if(!(this->startCrawled)) {
					if(this->customPages.empty()) {
						// start manual crawling with start page
						logEntries.push_back("starts crawling in non-recoverable MANUAL mode.");
					}

					this->manualUrl = std::pair<unsigned long, std::string>(this->startPageId, this->config.crawlerStart);
					if((this->config.crawlerReCrawlStart || !(this->database.isUrlCrawled(this->startPageId)))
							&& this->database.isUrlLockable(this->startPageId)) {
						this->lockTime = this->database.lockUrl(this->manualUrl.first, this->config.crawlerLock);
						urlTo = this->manualUrl;
					}
					else {
						// skip locked start page
						logEntries.push_back("URL lock active - " + this->manualUrl.second + " skipped.");
						this->manualUrl = std::pair<unsigned long, std::string>(0, "");
						this->startCrawled = true;
					}
				}
			}
		}
	}

	// AUTOMATIC CRAWLING MODE (get URL directly from database)
	if(!(this->manualUrl.first)) {
		// check whether manual crawling mode was already set off
		if(!(this->manualOff)) {
			// start manual crawling with start page
			logEntries.push_back("switches to recoverable AUTOMATIC mode.");
			this->manualOff = true;
		}

		// check for re-try
		if(this->nextUrl.first && this->database.checkUrlLock(this->nextUrl.first, this->lockTime)) {
			this->lockTime = this->database.lockUrl(this->nextUrl.first, this->config.crawlerLock);
			logEntries.push_back("retries " + this->nextUrl.second + "...");
			urlTo = this->nextUrl;
		}
		else {
			if(this->nextUrl.first) logEntries.push_back("could not retry " + this->nextUrl.second + ", because it is locked.");
			while(true) {
				// get id and name of next URL
				this->nextUrl = this->database.getNextUrl(this->getLast());

				if(this->nextUrl.first) {
					// lock URL
					if(this->database.isUrlLockable(this->nextUrl.first)) {
						this->lockTime = this->database.lockUrl(this->nextUrl.first, this->config.crawlerLock);
						urlTo = this->nextUrl;
						// success!
						break;
					}
					else {
						// skip locked URL
						logEntries.push_back("skipped " + this->nextUrl.second + ", because it is locked.");
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
	this->database.releaseLocks();
	if(this->config.crawlerLogging) for(auto i = logEntries.begin(); i != logEntries.end(); ++i) this->log(*i);

	// set thread status
	if(result) this->setStatusMessage(urlTo.second);
	else {
		this->setStatusMessage("IDLE Waiting for new URLs to crawl.");
		this->setProgress(1L);
	}

	return result;
}

// crawl content
bool Thread::crawlingContent(const std::pair<unsigned long, std::string>& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo,
		std::string& timerStrTo) {
	crawlservpp::Timer::StartStop sleepTimer;
	crawlservpp::Timer::StartStop httpTimer;
	crawlservpp::Timer::StartStop parseTimer;
	crawlservpp::Timer::StartStop updateTimer;
	std::string content;
	std::string contentType;
	timerStrTo = "";

	// skip crawling if only archive needs to be retried
	if(this->config.crawlerArchives && this->archiveRetry) {
		if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("Re-trying archive only [" + url.second + "].");
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
	if(this->networking.setConfigCurrent(this->config.network)) {

		// get content
		if(this->config.crawlerTiming) httpTimer.start();
		if(this->config.crawlerSleepHttp) this->httpTime = std::chrono::steady_clock::now();
		if(this->networking.getContent("https://" + this->domain + url.second, content, this->config.crawlerRetryHttp)) {

			// check response code
			unsigned int responseCode = this->networking.getResponseCode();
			if(this->crawlingCheckResponseCode(url.second, responseCode)) {
				if(this->config.crawlerTiming) {
					httpTimer.stop();
					if(!timerStrTo.empty()) timerStrTo += ", ";
					timerStrTo += "http: " + httpTimer.totalStr();
					parseTimer.start();
				}

				// check content type
				std::string contentType = this->networking.getContentType();
				if(this->crawlingCheckContentType(url, contentType)) {

					// optional: simple HTML consistency check (not very reliable though, mostly for debugging purposes)
					if(this->config.crawlerHTMLConsistencyCheck) {
						unsigned long html = content.find_first_of("<html");
						if(html != std::string::npos) {
							if(content.find("<html", html + 5) != std::string::npos) {
								if(this->config.crawlerLogging) {
									this->log("ERROR: HTML consistency check failed for " + url.second);
									std::ofstream out("debug.txt");
									out << content;
									out.close();
									this->log("Wrote content to \"debug.txt\".");
								}
								this->crawlingSkip(url);
								return false;
							}
						}
					}

					// parse content
					crawlservpp::Parsing::XML doc;
					if(doc.parse(content)) {

						// optional: simple HTML canonical check (not very reliable though, mostly for debugging purposes)
						if(this->config.crawlerHTMLCanonicalCheck) {
							std::string canonical;
							if(this->getXPathQueryPtr(this->queryCanonicalCheck.index)->getFirst(doc, canonical) && !canonical.empty()) {
								if(!(canonical.length() == (8 + this->domain.length() + url.second.length())
											&& canonical.substr(0, 8) == "https://"
											&& canonical.substr(8, this->domain.length()) == this->domain
											&& canonical.substr(8 + this->domain.length()) == url.second)
										&& (!(canonical.length() == (7 + this->domain.length() + url.second.length())
											&& canonical.substr(0, 7) == "http://"
											&& canonical.substr(7, this->domain.length()) == this->domain
											&& canonical.substr(7 + this->domain.length()) == url.second))) {
									if(this->config.crawlerLogging) {
										this->log("ERROR: HTML canonical check failed for " + url.second + " [ != " + canonical + "].");
										std::ofstream out("debug.txt");
										out << content;
										out.close();
										this->log("Wrote content to \"debug.txt\".");
									}
									this->crawlingSkip(url);
									return false;
								}
							}
							else if(this->config.crawlerLogging)
								this->log("WARNING: Could not perform canonical check for " + url.second + ".");
						}

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
							if(!urls.empty()) {
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
						if(this->config.crawlerLogging) this->log(doc.getErrorMessage() + " [" + url.second + "].");
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
				if(this->config.crawlerLogging) this->log("redirection error at " + url.second + " - skips...");
				this->crawlingSkip(url);
			}
			else {
				// other error: reset connection and retry
				if(this->config.crawlerLogging) {
					this->log(this->networking.getErrorMessage() + " [" + url.second + "].");
					this->log("resets connection...");
				}
				this->setStatusMessage("ERROR " + this->networking.getErrorMessage() + " [" + url.second + "]");
				this->networking.resetConnection(this->config.crawlerSleepError);
				this->crawlingRetry(url, false);
			}
			return false;
		}
	}
	else {
		// error while setting up network
		if(this->config.crawlerLogging) {
			this->log(this->networking.getErrorMessage() + " [" + url.second + "].");
			this->log("resets connection...");
		}
		this->setStatusMessage("ERROR " + this->networking.getErrorMessage() + " [" + url.second + "]");
		this->networking.resetConnection(this->config.crawlerSleepError);
		this->crawlingRetry(url, false);
		return false;
	}

	return true;
}

// check whether URL should be added
bool Thread::crawlingCheckUrl(const std::string& url) {
	if(url.empty()) return false;

	if(!(this->queriesWhiteListUrls.empty())) {
		// whitelist: only allow URLs that fit a specified whitelist query
		bool whitelist = false;
		bool found = false;
		for(auto i = this->queriesWhiteListUrls.begin(); i != this->queriesWhiteListUrls.end(); ++i) {
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				whitelist = true;
				if(!(this->getRegExQueryPtr(i->index)->getBool(url, found)))
					if(this->config.crawlerLogging) this->log(this->getRegExQueryPtr(i->index)->getErrorMessage()
							+ " [" + url + "].");
				if(found) break;
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone) {
				whitelist = true;
				if(this->config.crawlerLogging) this->log("WARNING: Query on URL is not of type RegEx.");
			}
		}
		if(whitelist && !found) {
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log("skipped " + url + " (not whitelisted).");
			return false;
		}
	}

	if(!(this->queriesBlackListUrls.empty())) {
		// blacklist: do not allow URLs that fit a specified blacklist query
		for(auto i = this->queriesBlackListUrls.begin(); i != this->queriesBlackListUrls.end(); ++i) {
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				bool found = false;
				if(!(this->getRegExQueryPtr(i->index)->getBool(url, found)))
					if(this->config.crawlerLogging) this->log(this->getRegExQueryPtr(i->index)->getErrorMessage()
							+ " [" + url + "].");
				if(found) {
					if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
						this->log("skipped " + url + " (blacklisted).");
					return false;
				}
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
				this->log("WARNING: Query on URL is not of type RegEx.");
		}
	}

	return true;
}

// check the HTTP response code for an error and decide whether to continue or skip
bool Thread::crawlingCheckResponseCode(const std::string& url, long responseCode) {
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
bool Thread::crawlingCheckContentType(const std::pair<unsigned long, std::string>& url, const std::string& contentType) {
	// check whitelist for content types
	if(!(this->queriesWhiteListTypes.empty())) {
		bool found = false;
		for(auto i = this->queriesWhiteListTypes.begin(); i != this->queriesWhiteListTypes.end(); ++i) {
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				if(!(this->getRegExQueryPtr(i->index)->getBool(contentType, found)))
					if(this->config.crawlerLogging) this->log(this->getRegExQueryPtr(i->index)->getErrorMessage()
							+ " [" + url.second + "].");
				if(found) break;
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
				this->log("WARNING: Query on content type is not of type RegEx.");
		}
		if(!found) return false;
	}

	// check blacklist for content types
	if(!(this->queriesBlackListTypes.empty())) {
		bool found = false;
		for(auto i = this->queriesBlackListTypes.begin(); i != this->queriesBlackListTypes.end(); ++i) {
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				if(!(this->getRegExQueryPtr(i->index)->getBool(contentType, found)))
					if(this->config.crawlerLogging) this->log(this->getRegExQueryPtr(i->index)->getErrorMessage()
						+ " [" + url.second + "].");
				if(found) break;
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
				this->log("WARNING: Query on content type is not of type RegEx.");
		}
		if(found) return false;
	}

	return true;
}

// check whether specific content should be crawled
bool Thread::crawlingCheckContent(const std::pair<unsigned long, std::string>& url, const std::string& content,
		const crawlservpp::Parsing::XML& doc) {
	// check whitelist for content types
	if(!(this->queriesWhiteListContent.empty())) {
		bool found = false;
		for(auto i = this->queriesWhiteListContent.begin(); i != this->queriesWhiteListContent.end(); ++i) {
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				if(!(this->getRegExQueryPtr(i->index)->getBool(content, found)))
					if(this->config.crawlerLogging) this->log(this->getRegExQueryPtr(i->index)->getErrorMessage()
						+ " [" + url.second + "].");
				if(found) break;
			}
			else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
				if(!(this->getXPathQueryPtr(i->index)->getBool(doc, found)))
					if(this->config.crawlerLogging) this->log(this->getXPathQueryPtr(i->index)->getErrorMessage()
						+ " [" + url.second + "].");
				if(found) break;
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
				this->log("WARNING: Query on content type is not of type RegEx or XPath.");
		}
		if(!found) return false;
	}

	// check blacklist for content types
	if(!(this->queriesBlackListContent.empty())) {
		bool found = false;
		for(auto i = this->queriesBlackListContent.begin(); i != this->queriesBlackListContent.end(); ++i) {
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				if(!(this->getRegExQueryPtr(i->index)->getBool(content, found)))
					if(this->config.crawlerLogging) this->log(this->getRegExQueryPtr(i->index)->getErrorMessage()
						+ " [" + url.second + "].");
				if(found) break;
			}
			else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
				if(!(this->getXPathQueryPtr(i->index)->getBool(doc, found)))
					if(this->config.crawlerLogging) this->log(this->getXPathQueryPtr(i->index)->getErrorMessage()
						+ " [" + url.second + "].");
				if(found) break;
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
				this->log("WARNING: Query on content type is not of type RegEx or XPath.");
		}
		if(found) return false;
	}

	return true;
}

// save content to database
void Thread::crawlingSaveContent(const std::pair<unsigned long, std::string>& url, unsigned int response, const std::string& type,
		const std::string& content, const crawlservpp::Parsing::XML& doc) {
	if(this->config.crawlerXml) {
		std::string xmlContent;
		if(doc.getContent(xmlContent)) {
			this->database.saveContent(url.first, response, type, xmlContent);
			return;
		}
		else if(this->config.crawlerLogging) this->log("WARNING: Could not clean content [" + url.second + "].");
	}
	this->database.saveContent(url.first, response, type, content);
}

// extract URLs from content
std::vector<std::string> Thread::crawlingExtractUrls(const std::pair<unsigned long, std::string>& url, const std::string& content,
		const crawlservpp::Parsing::XML& doc) {
	std::vector<std::string> urls;
	for(auto i = this->queriesLinks.begin(); i != this->queriesLinks.end(); ++i) {
		if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
			if(i->resultMulti) {
				std::vector<std::string> results;
				if(this->getRegExQueryPtr(i->index)->getAll(content, results)) {
					urls.resize(urls.size() + results.size());
					urls.insert(urls.end(), results.begin(), results.end());
				}
				else if(this->config.crawlerLogging) this->log(this->getRegExQueryPtr(i->index)->getErrorMessage()
					+ " [" + url.second + "].");
			}
			else {
				std::string result;
				if(this->getRegExQueryPtr(i->index)->getFirst(content, result)) {
					urls.push_back(result);
				}
				else if(this->config.crawlerLogging) this->log(this->getRegExQueryPtr(i->index)->getErrorMessage()
					+ " [" + url.second + "].");
			}
		}
		else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
			if(i->resultMulti) {
				std::vector<std::string> results;
				if(this->getXPathQueryPtr(i->index)->getAll(doc, results)) {
					urls.resize(urls.size() + results.size());
					urls.insert(urls.end(), results.begin(), results.end());
				}
				else if(this->config.crawlerLogging) this->log(this->getXPathQueryPtr(i->index)->getErrorMessage()
					+ " [" + url.second + "].");
			}
			else {
				std::string result;
				if(this->getXPathQueryPtr(i->index)->getFirst(doc, result)) {
					urls.push_back(result);
				}
				else if(this->config.crawlerLogging) this->log(this->getXPathQueryPtr(i->index)->getErrorMessage()
					+ " [" + url.second + "].");
			}
		}
		else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
			this->log("WARNING: Query on content type is not of type RegEx or XPath.");
	}

	// remove duplicates
	std::sort(urls.begin(), urls.end());
	urls.erase(std::unique(urls.begin(), urls.end()), urls.end());
	return urls;
}

// parse URLs and add them as sub-links to the database if they do not already exist
void Thread::crawlingParseAndAddUrls(const std::pair<unsigned long, std::string>& url, std::vector<std::string>& urls,
		unsigned long& newUrlsTo, bool archived) {
	// set current URL
	if(!(this->parser->setCurrentSubUrl(url.second))) {
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
			if(pos) urls.at(n - 1) = crawlservpp::Parsing::URI::unescape(urls.at(n - 1).substr(pos), false);
			else urls.at(n - 1) = "";
		}

		if(!urls.at(n - 1).empty()) {
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
					if(!(this->config.crawlerParamsBlackList.empty())) {
						urls.at(n - 1) = this->parser->getSubUrl(this->config.crawlerParamsBlackList, false);

					}
					else urls.at(n - 1)
							= this->parser->getSubUrl(this->config.crawlerParamsWhiteList, true);
					if(!(this->crawlingCheckUrl(urls.at(n - 1)))) urls.at(n - 1) = "";

					if(!urls.at(n - 1).empty()) {
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
			if(locked) this->database.releaseLocks();

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
				if(this->config.crawlerLogging && this->config.crawlerWarningsFile && !(i->empty()) && i->back() != '/') {
					this->log("WARNING: Found file \'" + *i + "\'.");
				}

				linkUrlId = this->database.addUrl(*i, false);
				newUrlsTo++;
			}

			// add linkage information to database
			this->database.addLinkIfNotExists(url.first, linkUrlId, archived);
		}
	}

	// unlock URL list
	this->database.releaseLocks();

	// reset status
	this->setStatusMessage(statusMessage);

	if(longUrls && this->config.crawlerLogging) this->log("WARNING: URLs longer than 2000 Bytes ignored.");
}

// crawl archives
bool Thread::crawlingArchive(const std::pair<unsigned long, std::string>& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo) {
	if(this->config.crawlerArchives && this->networkingArchives) {
		bool success = true;
		bool skip = false;

		for(unsigned long n = 0; n < this->config.crawlerArchivesNames.size(); n++) {
			// skip empty URLs
			if((this->config.crawlerArchivesUrlsMemento.at(n).empty())
					|| (this->config.crawlerArchivesUrlsTimemap.at(n).empty()))
				continue;

			std::string archivedUrl = this->config.crawlerArchivesUrlsTimemap.at(n) + this->domain + url.second;
			std::string archivedContent;

			while(success && this->isRunning()) {
				// get memento content
				archivedContent = "";
				if(this->networkingArchives->getContent(archivedUrl, archivedContent, this->config.crawlerRetryHttp)) {

					// check response code
					if(this->crawlingCheckResponseCode(archivedUrl, this->networkingArchives->getResponseCode())) {

						// get memento content type
						std::string contentType = this->networkingArchives->getContentType();
						if(contentType == "application/link-format") {
							if(!archivedContent.empty()) {

								// parse memento response
								std::vector<Memento> mementos;
								std::vector<std::string> warnings;
								archivedUrl = Thread::parseMementos(archivedContent, warnings, mementos);
								if(this->config.crawlerLogging) {
									// just show warning, maybe mementos were partially parsed
									for(auto i = warnings.begin(); i != warnings.end(); ++i)
										this->log("Memento parsing WARNING: " + *i + " [" + url.second + "]");
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
									if(this->database.checkUrlLock(url.first, this->lockTime)) {
										this->lockTime = this->database.lockUrl(url.first, this->config.crawlerLock);

										while(this->isRunning()) { // loop over references if necessary

											// check whether archived content already exists in database
											if(!(this->database.isArchivedContentExists(url.first, timeStamp))) {

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
														if(crawlservpp::Helper::DateTime::convertSQLTimeStampToTimeStamp(
																timeStamp)) {
															unsigned long subUrlPos = i->url.find(timeStamp);
															if(subUrlPos != std::string::npos) {
																subUrlPos += timeStamp.length();
																timeStamp = archivedContent.substr(17, 14);
																i->url = this->config.crawlerArchivesUrlsMemento.at(n) + timeStamp
																		+ i->url.substr(subUrlPos);
																if(crawlservpp::Helper::DateTime::convertTimeStampToSQLTimeStamp(
																		timeStamp))	continue;
																else if(this->config.crawlerLogging)
																	this->log("WARNING: Invalid timestamp \'" + timeStamp
																			+ "\' from " + this->config.crawlerArchivesNames.at(n)
																			+ " [" + url.second + "].");
															}
															else if(this->config.crawlerLogging)
																this->log("WARNING: Could not find timestamp in " + i->url
																		+ " [" + url.second + "].");
														}
														else if(this->config.crawlerLogging)
															this->log("WARNING: Could not convert timestamp in " + i->url
																	+ " [" + url.second + "].");
													}
													else {
														// parse content
														crawlservpp::Parsing::XML doc;
														if(doc.parse(archivedContent)) {

															// add archived content to database
															this->database.saveArchivedContent(url.first, i->timeStamp,
															this->networkingArchives->getResponseCode(),
															this->networkingArchives->getContentType(), archivedContent);

															// extract URLs
															std::vector<std::string> urls = this->crawlingExtractUrls(url,
																	archivedContent, doc);
															if(!urls.empty()) {
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
					this->setStatusMessage("ERROR " + this->networkingArchives->getErrorMessage() + " [" + url.second + "]");
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
void Thread::crawlingSuccess(const std::pair<unsigned long, std::string>& url) {
	// update URL list if possible
	this->database.lockUrlList();
	if(this->database.checkUrlLock(url.first, this->lockTime)) this->database.setUrlFinished(url.first);
	this->database.releaseLocks();
	this->lockTime = "";

	if(this->manualUrl.first) {
		// manual mode: disable retry, check for custom URL or start page that has been crawled
		this->manualUrl = std::pair<unsigned long, std::string>(0, "");
		if(this->manualCounter < this->customPages.size()) this->manualCounter++;
		else this->startCrawled = true;
	}
	else {
		// automatic mode: update thread status
		this->setLast(url.first);
		this->setProgress((float) (this->database.getUrlPosition(url.first) + 1) / this->database.getNumberOfUrls());
	}

	// reset retry counter
	this->retryCounter = 0;

	// do not re-try
	this->nextUrl = std::pair<unsigned long, std::string>(0, "");
}

// skip URL after crawling problem
void Thread::crawlingSkip(const std::pair<unsigned long, std::string>& url) {
	if(this->manualUrl.first) {
		// manual mode: disable retry, check for custom URL or start page that has been crawled
		this->manualUrl = std::pair<unsigned long, std::string>(0, "");
		if(this->manualCounter < this->customPages.size()) this->manualCounter++;
		else this->startCrawled = true;
	}
	else {
		// automatic mode: update thread status
		this->setLast(url.first);
		this->setProgress((float) (this->database.getUrlPosition(url.first) + 1) / this->database.getNumberOfUrls());
	}

	// reset retry counter
	this->retryCounter = 0;

	// do not re-try
	this->nextUrl = std::pair<unsigned long, std::string>(0, "");
	this->archiveRetry = false;
}

// retry URL (completely) after crawling problem
void Thread::crawlingRetry(const std::pair<unsigned long, std::string>& url, bool archiveOnly) {
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

// parse memento reply, get mementos and link to next page if one exists (also convert timestamps to YYYYMMDD HH:MM:SS)
std::string Thread::parseMementos(std::string mementoContent, std::vector<std::string>& warningsTo,
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
				if(!newMemento.url.empty() && !newMemento.timeStamp.empty()) mementosTo.push_back(newMemento);
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
				if(!newMemento.url.empty() && !newMemento.timeStamp.empty()) mementosTo.push_back(newMemento);
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
					if(crawlservpp::Helper::DateTime::convertLongDateTimeToSQLTimeStamp(fieldValue))
						newMemento.timeStamp = fieldValue;
					else {
						warningStrStr << "Could not convert timestamp \'" << fieldValue << "\'  at " << pos << ".";
						warningsTo.push_back(warningStrStr.str());
						warningStrStr.clear();
						warningStrStr.str(std::string());
					}
				}
				else if(fieldName == "rel") {
					if(fieldValue == "timemap" && !newMemento.url.empty()) {
						nextPage = newMemento.url;
						newMemento.url = "";
					}
				}
				pos = end + 1;
			}
		}
	}

	if(mementoStarted && !newMemento.url.empty() && !newMemento.timeStamp.empty()) mementosTo.push_back(newMemento);
	return nextPage;
}

}
