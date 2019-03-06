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
		unsigned long crawlerLast) : crawlservpp::Module::Thread(dbBase, crawlerId, "crawler", crawlerStatus, crawlerPaused,
				threadOptions, crawlerLast), database(this->crawlservpp::Module::Thread::database),	manualCounter(0),
				startCrawled(false), manualOff(false), retryCounter(0), archiveRetry(false), tickCounter(0),
				startTime(std::chrono::steady_clock::time_point::min()), pauseTime(std::chrono::steady_clock::time_point::min()),
				idleTime(std::chrono::steady_clock::time_point::min()), httpTime(std::chrono::steady_clock::time_point::min()) {}

// constructor B: start a new crawler
Thread::Thread(crawlservpp::Main::Database& dbBase, const crawlservpp::Struct::ThreadOptions& threadOptions)
			: crawlservpp::Module::Thread(dbBase, "crawler", threadOptions), database(this->crawlservpp::Module::Thread::database),
			  manualCounter(0), startCrawled(false), manualOff(false), retryCounter(0), archiveRetry(false), tickCounter(0),
			  startTime(std::chrono::steady_clock::time_point::min()), pauseTime(std::chrono::steady_clock::time_point::min()),
			  idleTime(std::chrono::steady_clock::time_point::min()), httpTime(std::chrono::steady_clock::time_point::min()) {}

// destructor stub
Thread::~Thread() {}

// initialize crawler
void Thread::onInit(bool resumed) {
	std::vector<std::string> configWarnings;
	bool verbose = false;

	// get configuration and show warnings if necessary
	this->config.loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);
	if(this->config.crawlerLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
		this->log("WARNING: " + *i);
	verbose = config.crawlerLogging == Config::crawlerLoggingVerbose;

	// check for link extraction queries
	if(this->config.crawlerQueriesLinks.empty()) throw Exception("ERROR: No link extraction query specified.");

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
	this->database.prepare();

	// get domain
	if(verbose) this->log("gets website domain...");
	this->domain = this->database.getWebsiteDomain(this->getWebsite());

	// create URI parser
	if(verbose) this->log("creates URI parser...");
	this->parser = std::make_unique<crawlservpp::Parsing::URI>();
	this->parser->setCurrentDomain(this->domain);

	// set network configuration
	if(verbose) this->log("sets network configuration...");
	configWarnings.clear();
	if(config.crawlerLogging == Config::crawlerLoggingVerbose)
		this->log("sets global network configuration...");
	this->networking.setConfigGlobal(this->config.network, false, &configWarnings);
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
		this->networkingArchives = std::make_unique<crawlservpp::Network::Curl>();
		this->networkingArchives->setConfigGlobal(this->config.network, true, &configWarnings);
		if(this->config.crawlerLogging) for(auto i = configWarnings.begin(); i != configWarnings.end(); ++i)
			this->log("WARNING: " + *i);
	}

	// save start time and initialize counter
	this->startTime = std::chrono::steady_clock::now();
	this->pauseTime = std::chrono::steady_clock::time_point::min();
	this->tickCounter = 0;
}

// crawler tick
void Thread::onTick() {
	std::tuple<unsigned long, std::string, unsigned long> url;
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
			this->log("crawls " + std::get<1>(url) + "...");

		// get content
		bool crawled = this->crawlingContent(url, checkedUrls, newUrls, timerString);

		// get archive (also when crawling failed!)
		if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("gets archives of " + std::get<1>(url) + "...");
		if(this->config.crawlerTiming) timerArchives.start();

		if(this->crawlingArchive(url, checkedUrlsArchive, newUrlsArchive)) {
			if(crawled) {
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
					logStrStr << "finished " << std::get<1>(url);
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
		}
		else if(!crawled) this->archiveRetry = false; // if crawling and getting archives failed, retry both (not only archives)!
	}
	else {
		if(this->idleTime == std::chrono::steady_clock::time_point::min()) {
			this->idleTime = std::chrono::steady_clock::now();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(this->config.crawlerSleepIdle));
	}
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
}

// shadow pause function not to be used by thread
void Thread::pause() {
	this->pauseByThread();
}

// hide other functions not to be used by thread
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

// initialize custom URLs
void Thread::initCustomUrls() {
	if(this->config.crawlerLogging == Config::crawlerLoggingVerbose)
		this->log("initializes start page and custom URLs...");

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
		for(auto i = newUrls.begin(); i != newUrls.end(); ++i)
			this->customPages.push_back(std::make_tuple(0, *i, 0));
	}
	else {
		// no counters: add all custom URLs as is
		this->customPages.reserve(this->config.customUrls.size());
		for(auto i = this->config.customUrls.begin(); i != this->config.customUrls.end(); ++i)
			this->customPages.push_back(std::make_tuple(0, *i, 0));
	}

	// queue for log entries while URL list is locked
	std::queue<std::string> warnings;

	// lock URL list and crawling table
	this->database.lockUrlListAndCrawlingTable();

	try {
		// get id for start page (and add it to URL list if necessary)
		if(this->database.isUrlExists(this->config.crawlerStart)) {
			std::get<1>(this->startPage) = this->config.crawlerStart;
			this->database.getUrlIdLockId(this->startPage);
		}
		else std::get<0>(this->startPage) = this->database.addUrlGetId(this->config.crawlerStart, true);

		// get IDs and lock IDs for custom URLs (and add them to the URL list if necessary)
		for(auto i = this->customPages.begin(); i != this->customPages.end(); ++i) {
			// check URI
			try {
				this->parser->setCurrentSubUrl(std::get<1>(*i));
				if(this->database.isUrlExists(std::get<1>(*i)))
					this->database.getUrlIdLockId(*i);
				else std::get<0>(*i) = this->database.addUrlGetId(std::get<1>(*i), true);
			}
			catch(const URIException& e) {
				if(this->config.crawlerLogging) {
					warnings.push("URI Parser error: " + e.whatStr());
					warnings.push("Skipped invalid custom URL " + std::get<1>(*i));
				}
			}
		}
	}
	catch(...) {
		// any exception: try to release table locks and re-throw
		try { this->database.releaseLocks(); }
		catch(...) {}
		throw;
	}

	// unlock URL list and crawling table
	this->database.releaseLocks();

	// log warnings if necessary
	if(this->config.crawlerLogging) {
		while(!warnings.empty()) {
			this->log(warnings.front());
			warnings.pop();
		}
	}
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
	if(this->config.crawlerHTMLCanonicalCheck) {
		this->queryCanonicalCheck = this->addQuery(crawlservpp::Struct::QueryProperties(
				"//link[contains(concat(' ', normalize-space(@rel), ' '), ' canonical ')]/@href", "xpath",
				false, true, false, true));
	}
	for(auto i = this->config.crawlerQueriesBlackListContent.begin(); i != this->config.crawlerQueriesBlackListContent.end(); ++i) {
		crawlservpp::Struct::QueryProperties properties;
		this->database.getQueryProperties(*i, properties);
		this->queriesBlackListContent.push_back(this->addQuery(properties));
	}
	for(auto i = this->config.crawlerQueriesBlackListTypes.begin(); i != this->config.crawlerQueriesBlackListTypes.end(); ++i) {
		crawlservpp::Struct::QueryProperties properties;
		this->database.getQueryProperties(*i, properties);
		this->queriesBlackListTypes.push_back(this->addQuery(properties));
	}
	for(auto i = this->config.crawlerQueriesBlackListUrls.begin(); i != this->config.crawlerQueriesBlackListUrls.end(); ++i) {
		crawlservpp::Struct::QueryProperties properties;
		this->database.getQueryProperties(*i, properties);
		this->queriesBlackListUrls.push_back(this->addQuery(properties));
	}
	for(auto i = this->config.crawlerQueriesLinks.begin(); i != this->config.crawlerQueriesLinks.end(); ++i) {
		crawlservpp::Struct::QueryProperties properties;
		this->database.getQueryProperties(*i, properties);
		this->queriesLinks.push_back(this->addQuery(properties));
	}
	for(auto i = this->config.crawlerQueriesWhiteListContent.begin(); i != this->config.crawlerQueriesWhiteListContent.end(); ++i) {
		crawlservpp::Struct::QueryProperties properties;
		this->database.getQueryProperties(*i, properties);
		this->queriesWhiteListContent.push_back(this->addQuery(properties));
	}
	for(auto i = this->config.crawlerQueriesWhiteListTypes.begin(); i != this->config.crawlerQueriesWhiteListTypes.end(); ++i) {
		crawlservpp::Struct::QueryProperties properties;
		this->database.getQueryProperties(*i, properties);
		this->queriesWhiteListTypes.push_back(this->addQuery(properties));
	}
	for(auto i = this->config.crawlerQueriesWhiteListUrls.begin(); i != this->config.crawlerQueriesWhiteListUrls.end(); ++i) {
		crawlservpp::Struct::QueryProperties properties;
		this->database.getQueryProperties(*i, properties);
		this->queriesWhiteListUrls.push_back(this->addQuery(properties));
	}
}

// crawling function for URL selection
bool Thread::crawlingUrlSelection(std::tuple<unsigned long, std::string, unsigned long>& urlTo) {
	bool result = true;

	// queue for log entries while crawling table is locked
	std::queue<std::string> logEntries;

	// MANUAL CRAWLING MODE (get URL from configuration)
	if(!(this->getLast())) {
		if(std::get<0>(this->manualUrl)) {
			// get current lock ID for custom URL or start page if none was received yet
			this->database.getUrlLockId(this->manualUrl);

			// renew URL lock on manual URL (custom URL or start page) for retry
			if(this->database.renewUrlLock(this->config.crawlerLock, this->manualUrl, this->lockTime)) {
				urlTo = this->manualUrl;
			}
			else {
				// skipped locked URL
				logEntries.push("URL lock active - " + std::get<1>(this->manualUrl) + " skipped.");
				this->manualUrl = std::make_tuple(0, "", 0);
			}
		}

		if(!std::get<0>(this->manualUrl)) {
			// no retry: check custom URLs
			if(!(this->customPages.empty())) {
				if(!(this->manualCounter)) {
					// start manual crawling with custom URLs
					logEntries.push("starts crawling in non-recoverable MANUAL mode.");
				}

				// check for custom URLs to crawl
				if(this->manualCounter < this->customPages.size()) {
					// lock crawling table
					this->database.lockCrawlingTable();

					try {
						while(this->manualCounter < this->customPages.size()) {
							// get current lock ID for custom URL if none was received yet
							this->database.getUrlLockId(this->customPages.at(this->manualCounter));

							// set current manual URL to custom URL
							this->manualUrl = this->customPages.at(this->manualCounter);

							// check whether custom URL was already crawled
							if(!(this->config.customReCrawl)) {
								if(this->database.isUrlCrawled(std::get<2>(this->manualUrl))) {
									this->manualCounter++;
									this->manualUrl = std::make_tuple(0, "", 0);
									continue;
								}
							}

							// check whether custom URL is lockable
							if(this->database.isUrlLockable(std::get<2>(this->manualUrl))) {
								// lock custom URL
								this->lockTime = this->database.lockUrl(this->manualUrl, this->config.crawlerLock);
								urlTo = this->manualUrl;
								break;
							}

							// skip locked custom URL
							logEntries.push("URL lock active - " + std::get<1>(this->manualUrl) + " skipped.");
							this->manualCounter++;
							this->manualUrl = std::make_tuple(0, "", 0);
						}
					}
					// any exception: try to release table lock and re-throw
					catch(...) {
						try { this->database.releaseLocks(); }
						catch(...) {}
						throw;
					}

					// unlock crawling table
					this->database.releaseLocks();
				}
			}

			if(this->manualCounter == this->customPages.size()) {
				// no more custom URLs to go: get start page (if lockable)
				if(!(this->startCrawled)) {
					if(this->customPages.empty()) {
						// start manual crawling with start page
						logEntries.push("starts crawling in non-recoverable MANUAL mode.");
					}

					// lock crawling table
					this->database.lockCrawlingTable();

					try {
						// get current lock ID for start page if none was received yet
						this->database.getUrlLockId(this->startPage);

						// check whether start page was already crawled (or needs to be re-crawled anyway)
						if(this->config.crawlerReCrawlStart || !(this->database.isUrlCrawled(std::get<2>(this->startPage)))) {
							// check whether start page is lockable
							if(this->database.isUrlLockable(std::get<2>(this->startPage))) {
								// lock start page
								this->lockTime = this->database.lockUrl(this->startPage, this->config.crawlerLock);

								// select start page
								urlTo = this->startPage;
								this->manualUrl = this->startPage;
							}
							else {
								// start page is locked: write skip entry to log (if logging is enabled)
								logEntries.push("URL lock active - " + std::get<1>(this->startPage) + " skipped.");

								// start page is done
								this->startCrawled = true;
							}
						}
						else {
							// start page is done
							this->startCrawled = true;
						}
					}
					// any exception: try to release table lock and re-throw
					catch(...) {
						try { this->database.releaseLocks(); }
						catch(...) {}
						throw;
					}

					// unlock crawling table
					this->database.releaseLocks();

					// reset manual URL if start page has been skipped
					if(this->startCrawled) this->manualUrl = std::make_tuple(0, "", 0);
				}
			}

			// unlock crawling table
			this->database.releaseLocks();
		}
	}

	// AUTOMATIC CRAWLING MODE (get URL directly from database)
	if(!(std::get<0>(this->manualUrl))) {
		// check whether manual crawling mode was already set off
		if(!(this->manualOff)) {
			// start manual crawling with start page
			logEntries.push("switches to recoverable AUTOMATIC mode.");
			this->manualOff = true;
		}

		// check for retry
		bool retry = false;
		if(std::get<0>(this->nextUrl)) {
			// renew URL lock on automatic URL for retry
			if(this->database.renewUrlLock(this->config.crawlerLock, this->nextUrl, this->lockTime)) {
				// log retry
				logEntries.push("retries " + std::get<1>(this->nextUrl) + "...");

				// set URL to last URL
				urlTo = this->nextUrl;

				// do retry
				retry = true;
			}
		}

		if(!retry) {
			// log failed retry if necessary
			if(std::get<0>(this->nextUrl))
				logEntries.push("could not retry " + std::get<1>(this->nextUrl) + ", because it is locked.");

			while(true) {
				// get next URL
				this->nextUrl = this->database.getNextUrl(this->getLast());

				if(std::get<0>(this->nextUrl)) {
					bool success = false;

					// lock crawling table
					this->database.lockCrawlingTable();

					try {
						// get current lock ID for next URL if none has been received yet
						this->database.getUrlLockId(this->nextUrl);

						// check whether next URL is lockable
						if(this->database.isUrlLockable(std::get<2>(this->nextUrl))) {
							// lock next URL
							this->lockTime = this->database.lockUrl(this->nextUrl, this->config.crawlerLock);
							urlTo = this->nextUrl;
							success = true;
						}
						else {
							// skip locked URL
							logEntries.push("skipped " + std::get<1>(this->nextUrl) + ", because it is locked.");
						}
					}
					// any exception: try to release table lock and re-throw
					catch(...) {
						try { this->database.releaseLocks(); }
						catch(...) {}
						throw;
					}

					// unlock crawling table
					this->database.releaseLocks();

					// exit loop on success
					if(success) break;
				}
				else {
					// no more URLs
					result = false;
					break;
				}
			}
		}
	}

	// write to log if necessary
	if(this->config.crawlerLogging) {
		while(!logEntries.empty()) {
			this->log(logEntries.front());
			logEntries.pop();
		}
	}

	// set thread status
	if(result) this->setStatusMessage(std::get<1>(urlTo));
	else {
		this->setStatusMessage("IDLE Waiting for new URLs to crawl.");
		this->setProgress(1L);
	}

	return result;
}

// crawl content
bool Thread::crawlingContent(const std::tuple<unsigned long, std::string, unsigned long>& urlData, unsigned long& checkedUrlsTo,
		unsigned long& newUrlsTo,
		std::string& timerStrTo) {
	crawlservpp::Timer::StartStop sleepTimer;
	crawlservpp::Timer::StartStop httpTimer;
	crawlservpp::Timer::StartStop parseTimer;
	crawlservpp::Timer::StartStop updateTimer;
	std::string content;
	timerStrTo = "";

	// skip crawling if only archive needs to be retried
	if(this->config.crawlerArchives && this->archiveRetry) {
		if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("Retrying archive only [" + std::get<1>(urlData) + "].");
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

	try {
		// set local networking options
		this->networking.setConfigCurrent(this->config.network);
	}
	catch(const CurlException& e) {
		// error while setting up network
		if(this->config.crawlerLogging) {
			this->log(e.whatStr() + " [" + std::get<1>(urlData) + "].");
			this->log("resets connection...");
		}
		this->setStatusMessage("ERROR " + e.whatStr() + " [" + std::get<1>(urlData) + "]");
		this->networking.resetConnection(this->config.crawlerSleepError);
		this->crawlingRetry(urlData, false);
		return false;
	}

	// start HTTP timer
	if(this->config.crawlerTiming) httpTimer.start();
	if(this->config.crawlerSleepHttp) this->httpTime = std::chrono::steady_clock::now();

	try {
		// get content
		this->networking.getContent("https://" + this->domain + std::get<1>(urlData), content, this->config.crawlerRetryHttp);
	}
	catch(const CurlException& e) {
		// error while getting content: check type of error i.e. last cURL code
		if(this->crawlingCheckCurlCode(this->networking.getCurlCode(), std::get<1>(urlData))) {
			// reset connection and retry
			if(this->config.crawlerLogging) {
				this->log(e.whatStr() + " [" + std::get<1>(urlData) + "].");
				this->log("resets connection...");
			}
			this->setStatusMessage("ERROR " + e.whatStr() + " [" + std::get<1>(urlData) + "]");
			this->networking.resetConnection(this->config.crawlerSleepError);
			this->crawlingRetry(urlData, false);
		}
		else {
			// skip URL
			this->crawlingSkip(urlData);
		}
		return false;
	}

	// check response code
	unsigned int responseCode = this->networking.getResponseCode();
	if(!(this->crawlingCheckResponseCode(std::get<1>(urlData), responseCode))) {
		// skip because of response code
		this->crawlingSkip(urlData);
		return false;
	}

	if(this->config.crawlerTiming) {
		httpTimer.stop();
		if(!timerStrTo.empty()) timerStrTo += ", ";
		timerStrTo += "http: " + httpTimer.totalStr();
		parseTimer.start();
	}

	// check content type
	std::string contentType = this->networking.getContentType();
	if(!(this->crawlingCheckContentType(urlData, contentType))) {
		// skip because of content type (not on whitelist or on blacklist)
		this->crawlingSkip(urlData);
		return false;
	}

	// optional: simple HTML consistency check (not very reliable though, mostly for debugging purposes)
	if(!(this->crawlingCheckConsistency(std::get<1>(urlData), content))) {
		// skip because of HTML inconsistency
		this->crawlingSkip(urlData);
		return false;
	}

	// parse content
	crawlservpp::Parsing::XML doc;
	try {
		doc.parse(content);
	}
	catch(const XMLException& e) {
		// error while parsing content
		if(this->config.crawlerLogging) this->log("XML error: " + e.whatStr() + " [" + std::get<1>(urlData) + "].");
		this->crawlingSkip(urlData);
		return false;
	}

	// optional: simple HTML canonical check (not very reliable though, mostly for debugging purposes)
	if(!(this->crawlingCheckCanonical(std::get<1>(urlData), doc))) {
		this->crawlingSkip(urlData);
		return false;
	}

	// check content
	if(!(this->crawlingCheckContent(std::get<1>(urlData), content, doc))) {
		// skip because of content (not on whitelist or on blacklist)
		this->crawlingSkip(urlData);
		return false;
	}

	if(this->config.crawlerTiming) {
		parseTimer.stop();
		updateTimer.start();
	}

	// save content
	this->crawlingSaveContent(urlData, responseCode, contentType, content, doc);

	if(this->config.crawlerTiming) {
		updateTimer.stop();
		parseTimer.start();
	}

	// extract URLs
	std::vector<std::string> urls = this->crawlingExtractUrls(urlData, content, doc);
	if(!urls.empty()) {
		if(this->config.crawlerTiming) {
			parseTimer.stop();
			updateTimer.start();
		}

		// parse and add URLs
		checkedUrlsTo += urls.size();
		this->crawlingParseAndAddUrls(std::get<1>(urlData), urls, newUrlsTo, false);

		if(this->config.crawlerTiming) {
			updateTimer.stop();
			timerStrTo += ", parse: " + parseTimer.totalStr() + ", update: " + updateTimer.totalStr();
		}
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
				try {
					found = this->getRegExQueryPtr(i->index).getBool(url);
					if(found) break;
				}
				catch(const RegExException& e) {
					if(this->config.crawlerLogging) this->log("WARNING: RegEx error - " + e.whatStr() + " [" + url + "].");
				}
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
				try {
					if(this->getRegExQueryPtr(i->index).getBool(url)) {
						if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
							this->log("skipped " + url + " (blacklisted).");
						return false;
					}
				}
				catch(const RegExException& e) {
					if(this->config.crawlerLogging) this->log("WARNING: RegEx error - " + e.whatStr() + " [" + url + "].");
				}
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
				this->log("WARNING: Query on URL is not of type RegEx.");
		}
	}

	return true;
}

// check CURL code and decide whether to retry or skip
bool Thread::crawlingCheckCurlCode(CURLcode curlCode, const std::string& url) {
	if(curlCode == CURLE_TOO_MANY_REDIRECTS) {
		// redirection error: skip URL
		if(this->config.crawlerLogging) this->log("redirection error at " + url + " - skips...");
		return false;
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
bool Thread::crawlingCheckContentType(const std::tuple<unsigned long, std::string, unsigned long>& urlData,
		const std::string& contentType) {
	// check whitelist for content types
	if(!(this->queriesWhiteListTypes.empty())) {
		bool found = false;
		for(auto i = this->queriesWhiteListTypes.begin(); i != this->queriesWhiteListTypes.end(); ++i) {
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				try {
					found = this->getRegExQueryPtr(i->index).getBool(contentType);
					if(found) break;
				}
				catch(const RegExException& e) {
					if(this->config.crawlerLogging)
						this->log("WARNING: RegEx error - " + e.whatStr() + " [" + std::get<1>(urlData) + "].");
				}
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
				try {
					found = this->getRegExQueryPtr(i->index).getBool(contentType);
					if(found) break;
				}
				catch(const RegExException& e) {
					if(this->config.crawlerLogging)
						this->log("WARNING: RegEx error - " + e.whatStr() + " [" + std::get<1>(urlData) + "].");
				}
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
				this->log("WARNING: Query on content type is not of type RegEx.");
		}
		if(found) return false;
	}

	return true;
}

// optional HTML consistency check (check whether there is only one opening HTML tag)
bool Thread::crawlingCheckConsistency(const std::string& url, const std::string& content) {
	if(this->config.crawlerHTMLConsistencyCheck) {
		unsigned long html = content.find_first_of("<html");
		if(html != std::string::npos) {
			if(content.find("<html", html + 5) != std::string::npos) {
				if(this->config.crawlerLogging)	this->log("ERROR: HTML consistency check failed for " + url);
				return false;
			}
		}
	}

	return true;
}

// optional canonical check (validate URL against canonical URL)
bool Thread::crawlingCheckCanonical(const std::string& url, const crawlservpp::Parsing::XML& doc) {
	if(this->config.crawlerHTMLCanonicalCheck) {
		std::string canonical;
		try {
			this->getXPathQueryPtr(this->queryCanonicalCheck.index).getFirst(doc, canonical);
			if(!canonical.empty()
					&& !(canonical.length() == (8 + this->domain.length() + url.length())
						&& canonical.substr(0, 8) == "https://"
						&& canonical.substr(8, this->domain.length()) == this->domain
						&& canonical.substr(8 + this->domain.length()) == url)
					&& (!(canonical.length() == (7 + this->domain.length() + url.length())
						&& canonical.substr(0, 7) == "http://"
						&& canonical.substr(7, this->domain.length()) == this->domain
						&& canonical.substr(7 + this->domain.length()) == url))) {
				if(this->config.crawlerLogging)
					this->log("ERROR: HTML canonical check failed for " + url + " [ != " + canonical + "].");
				return false;
			}
		}
		catch(const XPathException& e) {
			if(this->config.crawlerLogging)
				this->log("WARNING: Could not perform canonical check for " + url + ": " + e.whatStr());
		}
	}

	return true;
}

// check whether specific content should be crawled
bool Thread::crawlingCheckContent(const std::string& url, const std::string& content,
		const crawlservpp::Parsing::XML& doc) {
	// check whitelist for content types
	if(!(this->queriesWhiteListContent.empty())) {
		bool found = false;
		for(auto i = this->queriesWhiteListContent.begin(); i != this->queriesWhiteListContent.end(); ++i) {
			if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
				try {
					found = this->getRegExQueryPtr(i->index).getBool(content);
					if(found) break;
				}
				catch(const RegExException&e) {
					if(this->config.crawlerLogging) this->log("WARNING: RegEx error - " + e.whatStr() + " [" + url + "].");
				}
			}
			else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
				try {
					found = this->getXPathQueryPtr(i->index).getBool(doc);
					if(found) break;
				}
				catch(const XPathException& e) {
					if(this->config.crawlerLogging) this->log("WARNING: XPath error - " + e.whatStr() + " [" + url + "].");
				}
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
				try {
					found = this->getRegExQueryPtr(i->index).getBool(content);
					if(found) break;
				}
				catch(const RegExException& e) {
					if(this->config.crawlerLogging) this->log("WARNING: RegEx error - " + e.whatStr() + " [" + url + "].");
				}
			}
			else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
				try {
					found = this->getXPathQueryPtr(i->index).getBool(doc);
					if(found) break;
				}
				catch(const XPathException& e) {
					if(this->config.crawlerLogging) this->log("WARNING: XPath error - " + e.whatStr() + " [" + url + "].");
				}
			}
			else if(i->type != crawlservpp::Query::Container::QueryStruct::typeNone && this->config.crawlerLogging)
				this->log("WARNING: Query on content type is not of type RegEx or XPath.");
		}
		if(found) return false;
	}

	return true;
}

// save content to database
void Thread::crawlingSaveContent(const std::tuple<unsigned long, std::string, unsigned long>& urlData, unsigned int response,
		const std::string& type, const std::string& content, const crawlservpp::Parsing::XML& doc) {
	if(this->config.crawlerXml) {
		std::string xmlContent;
		try {
			doc.getContent(xmlContent);
			this->database.saveContent(std::get<0>(urlData), response, type, xmlContent);
			return;
		}
		catch(const XMLException& e) {
			this->log("WARNING: Could not clean content [" + std::get<1>(urlData) + "]: " + e.whatStr());
		}
	}
	this->database.saveContent(std::get<0>(urlData), response, type, content);
}

// extract URLs from content
std::vector<std::string> Thread::crawlingExtractUrls(const std::tuple<unsigned long, std::string, unsigned long>& urlData,
		const std::string& content,	const crawlservpp::Parsing::XML& doc) {
	std::vector<std::string> urls;
	for(auto i = this->queriesLinks.begin(); i != this->queriesLinks.end(); ++i) {
		if(i->type == crawlservpp::Query::Container::QueryStruct::typeRegEx) {
			try {
				if(i->resultMulti) {
					std::vector<std::string> results;
					this->getRegExQueryPtr(i->index).getAll(content, results);
					urls.reserve(urls.size() + results.size());
					urls.insert(urls.end(), results.begin(), results.end());
				}
				else {
					std::string result;
					this->getRegExQueryPtr(i->index).getFirst(content, result);
					urls.push_back(result);
				}
			}
			catch(const RegExException& e) {
				if(this->config.crawlerLogging)
					this->log("WARNING: RegEx error - " + e.whatStr() + " [" + std::get<1>(urlData) + "].");
			}
		}
		else if(i->type == crawlservpp::Query::Container::QueryStruct::typeXPath) {
			try {
				if(i->resultMulti) {
					std::vector<std::string> results;
					this->getXPathQueryPtr(i->index).getAll(doc, results);

					urls.reserve(urls.size() + results.size());
					urls.insert(urls.end(), results.begin(), results.end());
				}
				else {
					std::string result;
					this->getXPathQueryPtr(i->index).getFirst(doc, result);
					urls.push_back(result);
				}
			}
			catch(const XPathException& e) {
				if(this->config.crawlerLogging)
					this->log("WARNING: XPath error - " + e.whatStr() + " [" + std::get<1>(urlData) + "].");
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
void Thread::crawlingParseAndAddUrls(const std::string& url, std::vector<std::string>& urls,
		unsigned long& newUrlsTo, bool archived) {
	// check argument
	if(url.empty()) {
		throw Exception("Thread::crawlingParseAndAddUrls(): No URL specified");
	}

	// set current URL
	try {
		this->parser->setCurrentSubUrl(url);
	}
	catch(const URIException& e) {
		throw Exception("Could not set current sub-url because of URI Parser error: " + e.whatStr());
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
			crawlservpp::Helper::Strings::replaceAll(urls.at(n - 1), "&amp;", "&", true);

			// parse link
			try {
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
								throw Exception(urls.at(n - 1) + " is no sub-URL!");
							}
							if(this->config.crawlerLogging && urls.at(n - 1).length() > 1 && urls.at(n - 1).at(1) == '#') {
								this->log("WARNING: Found anchor \'" + urls.at(n - 1) + "\'.");
							}
							continue;
						}
					}
				}
			}
			catch(const URIException& e) {
				if(this->config.crawlerLogging) this->log("WARNING: URI Parser error - " + e.whatStr());
			}
		}

		// delete out-of-domain and empty URLs
		urls.erase(urls.begin() + (n - 1));
		n--;
	}

	// remove duplicates
	std::sort(urls.begin(), urls.end());
	urls.erase(std::unique(urls.begin(), urls.end()), urls.end());

	// remove URLs longer than 2000 characters
	const auto tmpSize = urls.size();
	urls.erase(std::remove_if(urls.begin(), urls.end(), [](const std::string& url) {
		return url.length() > 2000;
	}), urls.end());
	if(this->config.crawlerLogging && urls.size() < tmpSize)
		this->log("WARNING: URLs longer than 2000 Bytes ignored.");

	// if necessary, check for file (i.e. non-slash) endings and show warnings
	if(this->config.crawlerLogging && this->config.crawlerWarningsFile)
		for(auto i = urls.begin(); i != urls.end(); i++)
			if(i->back() != '/') this->log("WARNING: Found file \'" + *i + "\'.");

	// lock URL list
	this->database.lockUrlList();

	try {
		// remove already existing URLs
		urls.erase(std::remove_if(urls.begin(), urls.end(), [&](const std::string& url) {
			return this->database.isUrlExists(url);
		}), urls.end());

		this->database.addUrls(urls);
	}
	catch(...) {
		// any exception: try to release table lock and re-throw
		try { this->database.releaseLocks(); }
		catch(...) {}
		throw;
	}

	// unlock URL list
	this->database.releaseLocks();

	// save number of added URLs
	newUrlsTo = urls.size();
}

// crawl archives
bool Thread::crawlingArchive(std::tuple<unsigned long, std::string, unsigned long>& urlData,
		unsigned long& checkedUrlsTo, unsigned long& newUrlsTo) {
	if(this->config.crawlerArchives && this->networkingArchives) {
		bool success = true;
		bool skip = false;

		// loop over different archives
		for(unsigned long n = 0; n < this->config.crawlerArchivesNames.size(); n++) {
			// skip empty URLs
			if((this->config.crawlerArchivesUrlsMemento.at(n).empty())
					|| (this->config.crawlerArchivesUrlsTimemap.at(n).empty()))
				continue;

			std::string archivedUrl = this->config.crawlerArchivesUrlsTimemap.at(n) + this->domain + std::get<1>(urlData);
			std::string archivedContent;

			// loop over memento pages
			// [while also checking whether getting mementos was successfull and thread is still running]
			while(success && this->isRunning()) {
				// get memento content
				archivedContent = "";
				try {
					this->networkingArchives->getContent(archivedUrl, archivedContent, this->config.crawlerRetryHttp);

					// check response code
					if(this->crawlingCheckResponseCode(archivedUrl, this->networkingArchives->getResponseCode())) {

						// check content type
						std::string contentType = this->networkingArchives->getContentType();
						if(contentType == "application/link-format") {
							if(!archivedContent.empty()) {

								// parse memento response and get next memento page if available
								std::vector<Memento> mementos;
								std::vector<std::string> warnings;
								archivedUrl = Thread::parseMementos(archivedContent, warnings, mementos);

								if(this->config.crawlerLogging) {
									// if there are warnings, just log them (maybe mementos were partially parsed)
									for(auto i = warnings.begin(); i != warnings.end(); ++i)
										this->log("Memento parsing WARNING: " + *i + " [" + std::get<1>(urlData) + "]");
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

									// renew URL lock to avoid duplicate archived content
									if(this->database.renewUrlLock(this->config.crawlerLock, urlData, this->lockTime)) {
										// loop over references / memento retries
										// [while checking whether thread is still running]
										while(this->isRunning()) {

											// check whether archived content already exists in database
											if(!(this->database.isArchivedContentExists(std::get<0>(urlData), timeStamp))) {

												// check whether thread is till running
												if(!(this->isRunning())) break;

												// get archived content
												archivedContent = "";
												try {
													this->networkingArchives->getContent(i->url, archivedContent,
														this->config.crawlerRetryHttp);

													// check response code
													if(!(this->crawlingCheckResponseCode(i->url,
															this->networkingArchives->getResponseCode()))) break;

													// check whether thread is still running
													if(!(this->isRunning())) break;

													// check archived content
													if(archivedContent.substr(0, 17) == "found capture at ") {

														// found a reference string: get timestamp
														if(crawlservpp::Helper::DateTime::convertSQLTimeStampToTimeStamp(timeStamp)) {
															unsigned long subUrlPos = i->url.find(timeStamp);
															if(subUrlPos != std::string::npos) {
																subUrlPos += timeStamp.length();
																timeStamp = archivedContent.substr(17, 14);

																// get URL and validate timestamp
																i->url = this->config.crawlerArchivesUrlsMemento.at(n) + timeStamp
																		+ i->url.substr(subUrlPos);
																if(crawlservpp::Helper::DateTime::convertTimeStampToSQLTimeStamp(
																		timeStamp))
																	// follow reference
																	continue;
																else if(this->config.crawlerLogging)
																	// log warning (and ignore reference)
																	this->log("WARNING: Invalid timestamp \'" + timeStamp
																			+ "\' from " + this->config.crawlerArchivesNames.at(n)
																			+ " [" + std::get<1>(urlData) + "].");
															}
															else if(this->config.crawlerLogging)
																// log warning (and ignore reference)
																this->log("WARNING: Could not find timestamp in " + i->url
																		+ " [" + std::get<1>(urlData) + "].");
														}
														else if(this->config.crawlerLogging)
															// log warning (and ignore reference)
															this->log("WARNING: Could not convert timestamp in " + i->url
																	+ " [" + std::get<1>(urlData) + "].");
													}
													else {
														// parse content
														crawlservpp::Parsing::XML doc;
														try {
															doc.parse(archivedContent);

															// add archived content to database
															this->database.saveArchivedContent(std::get<0>(urlData), i->timeStamp,
															this->networkingArchives->getResponseCode(),
															this->networkingArchives->getContentType(), archivedContent);

															// extract URLs
															std::vector<std::string> urls
																	= this->crawlingExtractUrls(urlData, archivedContent, doc);
															if(!urls.empty()) {
																// parse and add URLs
																checkedUrlsTo += urls.size();
																this->crawlingParseAndAddUrls(
																		std::get<1>(urlData), urls, newUrlsTo, true);
															}
														}
														catch(const XMLException& e) {} // ignore parsing errors
													}
												}
												catch(const CurlException& e) {
													if(this->config.crawlerRetryArchive) {
														// error while getting content: check type of error i.e. last cURL code
														if(this->crawlingCheckCurlCode(
																this->networkingArchives->getCurlCode(), i->url)) {
															// log error
															if(this->config.crawlerLogging) {
																this->log(e.whatStr() + " [" + i->url + "].");
																this->log("resets connection to "
																		+ this->config.crawlerArchivesNames.at(n) + "...");
															}

															// reset connection
															this->setStatusMessage("ERROR " + e.whatStr() + " [" + i->url + "]");
															this->networkingArchives->resetConnection(this->config.crawlerSleepError);

															// retry
															continue;
														}
													}
													else if(this->config.crawlerLogging)
														// log error and skip
														this->log(e.whatStr() + " - skips...");
												}
											}

											// exit loop over references/memento retries
											break;

										} // [end of loop over references/memento retries]

										// check whether thread is till running
										if(!(this->isRunning())) break;
									} // [end of check whether URL lock could be renewed]
								} // [end of loop over mementos]

								// check whether thread is till running
								if(!(this->isRunning())) break;

								// reset status message
								this->setStatusMessage(statusMessage);

								// check for next memento page
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
				catch(const CurlException& e) {
					// error while getting content: check type of error i.e. last cURL code
					if(this->crawlingCheckCurlCode(this->networkingArchives->getCurlCode(), archivedUrl)) {
						// reset connection and retry
						if(this->config.crawlerLogging) {
							this->log(e.whatStr() + " [" + archivedUrl + "].");
							this->log("resets connection to " + this->config.crawlerArchivesNames.at(n) + "...");
						}

						this->setStatusMessage("ERROR " + e.whatStr() + " [" + archivedUrl + "]");
						this->networkingArchives->resetConnection(this->config.crawlerSleepError);
						success = false;
					}
				}

				if(!success && this->config.crawlerRetryArchive) {
					if(skip) this->crawlingSkip(urlData);
					else this->crawlingRetry(urlData, true);
					return false;
				}
				else if(!success) this->crawlingSkip(urlData);
			} // [end of loop over memento pages]
		} // [end of loop over archives]

		if(success || !(this->config.crawlerRetryArchive)) this->archiveRetry = false;
	}

	return this->isRunning();
}

// crawling successfull
void Thread::crawlingSuccess(const std::tuple<unsigned long, std::string, unsigned long>& urlData) {
	// lock crawling table
	this->database.lockCrawlingTable();

	try {
		// check URL lock
		if(this->database.checkUrlLock(std::get<2>(urlData), this->lockTime))
			// set URL to finished
			this->database.setUrlFinished(std::get<2>(urlData));
	}
	catch(...) {
		// any exception: try to release table locks and re-throw
		try { this->database.releaseLocks(); }
		catch(...) {}
		throw;
	}

	// unlock crawling table
	this->database.releaseLocks();

	// reset lock time
	this->lockTime = "";

	if(std::get<0>(this->manualUrl)) {
		// manual mode: disable retry, check for custom URL or start page that has been crawled
		this->manualUrl = std::make_tuple(0, "", 0);
		if(this->manualCounter < this->customPages.size()) this->manualCounter++;
		else this->startCrawled = true;
	}
	else {
		// automatic mode: update thread status
		this->setLast(std::get<0>(urlData));
		this->setProgress((float) (this->database.getUrlPosition(std::get<0>(urlData)) + 1) / this->database.getNumberOfUrls());
	}

	// reset retry counter
	this->retryCounter = 0;

	// do not retry
	this->nextUrl = std::make_tuple(0, "", 0);
}

// skip URL after crawling problem
void Thread::crawlingSkip(const std::tuple<unsigned long, std::string, unsigned long>& urlData) {
	// lock crawling table
	this->database.lockCrawlingTable();

	try {
		// check URL lock
		if(this->database.checkUrlLock(std::get<2>(urlData), this->lockTime))
			// remove URL lock
			this->database.unLockUrl(std::get<2>(urlData));
	}
	catch(...) {
		// any exception: try to release table locks and re-throw
		try { this->database.releaseLocks(); }
		catch(...) {}
		throw;
	}

	// unlock crawling table
	this->database.releaseLocks();

	// reset lock time
	this->lockTime = "";

	if(std::get<0>(this->manualUrl)) {
		// manual mode: disable retry, check for custom URL or start page that has been crawled
		this->manualUrl = std::make_tuple(0, "", 0);
		if(this->manualCounter < this->customPages.size()) this->manualCounter++;
		else this->startCrawled = true;
	}
	else {
		// automatic mode: update thread status
		this->setLast(std::get<0>(urlData));
		this->setProgress((float) (this->database.getUrlPosition(std::get<0>(urlData)) + 1) / this->database.getNumberOfUrls());
	}

	// reset retry counter
	this->retryCounter = 0;

	// do not retry
	this->nextUrl = std::make_tuple(0, "", 0);
	this->archiveRetry = false;
}

// retry URL (completely or achives-only) after crawling problem
//  NOTE: leaves the URL lock active for retry
void Thread::crawlingRetry(const std::tuple<unsigned long, std::string, unsigned long>& urlData, bool archiveOnly) {
	if(this->config.crawlerReTries > -1) {
		// increment and check retry counter
		this->retryCounter++;
		if(this->retryCounter > (unsigned long) this->config.crawlerReTries) {
			// do not retry, but skip
			this->crawlingSkip(urlData);
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
