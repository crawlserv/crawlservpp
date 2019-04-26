/*
 * Thread.cpp
 *
 * Implementation of the Thread interface for crawler threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#include "Thread.hpp"

namespace crawlservpp::Module::Crawler {

	// constructor A: run previously interrupted crawler
	Thread::Thread(
			Main::Database& dbBase,
			unsigned long crawlerId,
			const std::string& crawlerStatus,
			bool crawlerPaused,
			const ThreadOptions& threadOptions,
			unsigned long crawlerLast
	)
				: Module::Thread(
						dbBase,
						crawlerId,
						"crawler",
						crawlerStatus,
						crawlerPaused,
						threadOptions,
						crawlerLast
				  ),
				  database(this->Module::Thread::database),
				  manualCounter(0),
				  startCrawled(false),
				  manualOff(false),
				  retryCounter(0),
				  archiveRetry(false),
				  xmlParsed(false),
				  jsonParsedRapid(false),
				  jsonParsedCons(false),
				  tickCounter(0),
				  startTime(std::chrono::steady_clock::time_point::min()),
				  pauseTime(std::chrono::steady_clock::time_point::min()),
				  idleTime(std::chrono::steady_clock::time_point::min()),
				  httpTime(std::chrono::steady_clock::time_point::min()) {}

	// constructor B: start a new crawler
	Thread::Thread(Main::Database& dbBase, const ThreadOptions& threadOptions)
				: Module::Thread(dbBase, "crawler", threadOptions),
				  database(this->Module::Thread::database),
				  manualCounter(0),
				  startCrawled(false),
				  manualOff(false),
				  retryCounter(0),
				  archiveRetry(false),
				  xmlParsed(false),
				  jsonParsedRapid(false),
				  jsonParsedCons(false),
				  tickCounter(0),
				  startTime(std::chrono::steady_clock::time_point::min()),
				  pauseTime(std::chrono::steady_clock::time_point::min()),
				  idleTime(std::chrono::steady_clock::time_point::min()),
				  httpTime(std::chrono::steady_clock::time_point::min()) {}

	// destructor stub
	Thread::~Thread() {}

	// initialize crawler
	void Thread::onInit() {
		std::queue<std::string> configWarnings;

		// set ID, website and URL list namespace
		this->database.setId(this->getId());
		this->database.setNamespaces(this->websiteNamespace, this->urlListNamespace);

		// load configuration
		this->setCrossDomain(this->database.getWebsiteDomain(this->getWebsite()).empty());
		this->loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);

		// show warnings if necessary
		while(!configWarnings.empty()) {
			this->log("WARNING: " + configWarnings.front());
			configWarnings.pop();
		}

		// check configuration
		bool verbose = this->config.crawlerLogging == Config::crawlerLoggingVerbose;

		if(this->config.crawlerQueriesLinks.empty())
			throw Exception("Crawler::Thread::onInit(): No link extraction query specified");

		// set database options
		this->setStatusMessage("Setting database options...");

		if(verbose)
			this->log("sets database options...");

		this->database.setRecrawl(this->config.crawlerReCrawl);
		this->database.setLogging(this->config.crawlerLogging);
		this->database.setVerbose(verbose);
		this->database.setUrlCaseSensitive(this->config.crawlerUrlCaseSensitive);
		this->database.setUrlDebug(this->config.crawlerUrlDebug);
		this->database.setUrlStartupCheck(this->config.crawlerUrlStartupCheck);
		this->database.setSleepOnError(this->config.crawlerSleepMySql);

		// create table names for table locking
		this->urlListTable = "crawlserv_" + this->websiteNamespace + "_" + this->urlListNamespace;
		this->crawlingTable = this->urlListTable + "_crawling";

		// prepare SQL statements for crawler
		this->setStatusMessage("Preparing SQL statements...");

		if(verbose)
			this->log("prepares SQL statements...");

		this->database.prepare();

		{ // lock URL list
			this->setStatusMessage("Waiting for URL list...");

			DatabaseLock urlListLock(
					this->database,
					"urlList." + this->websiteNamespace + "_" + this->urlListNamespace,
					std::bind(&Thread::isRunning, this)
			);

			// check URL list
			this->setStatusMessage("Checking URL list...");

			if(verbose)
				this->log("checks URL list...");

			// check hashs of URLs
			this->database.urlHashCheck();

			// optional startup checks
			if(this->config.crawlerUrlStartupCheck) {
				this->database.urlDuplicationCheck();
				this->database.urlEmptyCheck(std::vector<std::string>());
			}
		}

		// get domain
		this->setStatusMessage("Getting website domain...");

		if(verbose)
			this->log("gets website domain...");

		this->domain = this->database.getWebsiteDomain(this->getWebsite());

		// create URI parser
		this->setStatusMessage("Creating URI parser...");

		if(verbose)
			this->log("creates URI parser...");

		this->parser = std::make_unique<Parsing::URI>();
		this->parser->setCurrentDomain(this->domain);

		// set network configuration
		this->setStatusMessage("Setting network configuration...");

		if(config.crawlerLogging == Config::crawlerLoggingVerbose)
			this->log("sets global network configuration...");

		this->networking.setConfigGlobal(*this, false, &configWarnings);

		while(!configWarnings.empty()) {
			this->log("WARNING: " + configWarnings.front());
			configWarnings.pop();
		}

		// initialize custom URLs
		this->setStatusMessage("Initializing custom URLs...");

		if(verbose)
			this->log("initializes custom URLs...");

		this->initCustomUrls();

		// initialize queries
		this->setStatusMessage("Initializing custom queries...");

		if(verbose)
			this->log("initializes custom queries...");

		this->initQueries();

		// initialize networking for archives if necessary
		if(this->config.crawlerArchives && !(this->networkingArchives)) {
			this->setStatusMessage("Initializing networking for archives...");

			if(verbose)
				this->log("initializes networking for archives...");

			this->networkingArchives = std::make_unique<Network::Curl>();
			this->networkingArchives->setConfigGlobal(*this, true, &configWarnings);

			// log warnings if necessary
			if(this->config.crawlerLogging)
				while(!configWarnings.empty()) {
					this->log("WARNING: " + configWarnings.front());
					configWarnings.pop();
				}
		}

		// save start time and initialize counter
		this->startTime = std::chrono::steady_clock::now();
		this->pauseTime = std::chrono::steady_clock::time_point::min();
		this->tickCounter = 0;
	}

	// crawler tick
	void Thread::onTick() {
		IdString url;

		Timer::StartStop timerSelect;
		Timer::StartStop timerArchives;
		Timer::StartStop timerTotal;

		std::string timerString;

		unsigned long checkedUrls = 0;
		unsigned long newUrls = 0;
		unsigned long checkedUrlsArchive = 0;
		unsigned long newUrlsArchive = 0;

		bool usePost = false;

		// reset parsing state
		this->resetParsingState();

		// check for jump in last ID ("time travel")
		long warpedOver = this->getWarpedOverAndReset();

		if(warpedOver) {
			// unlock last URL if necessary
			if(this->manualUrl.first && !(this->lockTime.empty()))
				this->database.unLockUrlIfOk(this->manualUrl.first, this->lockTime);
			else if(this->nextUrl.first && !(this->lockTime.empty()))
				this->database.unLockUrlIfOk(this->nextUrl.first, this->lockTime);

			// no retry
			this->manualUrl = IdString();
			this->nextUrl = IdString();

			// adjust tick counter
			this->tickCounter += warpedOver;
		}

		// start timers
		if(this->config.crawlerTiming) {
			timerTotal.start();
			timerSelect.start();
		}

		// URL selection
		if(this->crawlingUrlSelection(url, usePost)) {
			if(this->config.crawlerTiming)
				timerSelect.stop();

			if(this->idleTime > std::chrono::steady_clock::time_point::min()) {
				// idling stopped
				this->startTime += std::chrono::steady_clock::now() - this->idleTime;
				this->pauseTime = std::chrono::steady_clock::time_point::min();
				this->idleTime = std::chrono::steady_clock::time_point::min();
			}

			// increase tick counter
			++(this->tickCounter);

			// start crawling
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log("crawls " + url.second + "...");

			// crawl content
			bool crawled = this->crawlingContent(url, usePost, checkedUrls, newUrls, timerString);

			// handle parsing errors
			this->logParsingErrors(url.second);

			// get archive (also when crawling failed!)
			if(this->config.crawlerTiming)
				timerArchives.start();

			if(this->crawlingArchive(url, checkedUrlsArchive, newUrlsArchive, crawled)) {
				if(crawled) {
					// stop timers
					if(this->config.crawlerTiming) {
						timerArchives.stop();
						timerTotal.stop();
					}

					// success!
					this->crawlingSuccess(url);

					// log if necessary
					if((this->config.crawlerLogging > Config::crawlerLoggingDefault)
							|| (this->config.crawlerTiming && this->config.crawlerLogging)) {
						std::ostringstream logStrStr;
						logStrStr.imbue(std::locale(""));

						logStrStr << "finished " << url.second;

						if(this->config.crawlerTiming) {
							logStrStr	<< " after " << timerTotal.totalStr()
										<< " (select: " << timerSelect.totalStr() << ", "
										<< timerString;

							if(this->config.crawlerArchives)
								logStrStr << ", archive: " << timerArchives.totalStr();

							logStrStr << ")";
						}

						logStrStr << " - checked " << checkedUrls;

						if(checkedUrlsArchive)
							logStrStr << " (+" << checkedUrlsArchive << " archived)";

						logStrStr << ", added " << newUrls;

						if(newUrlsArchive)
							logStrStr << " (+" << newUrlsArchive << " archived)";

						logStrStr << " URL(s).";

						this->log(logStrStr.str());
					}
				}
			}
			else if(!crawled)
				// if crawling and getting archives failed, retry both (not only archives)
				this->archiveRetry = false;
		}
		else {
			// no URLs to crawl: set idle timer and sleep
			if(this->idleTime == std::chrono::steady_clock::time_point::min())
				this->idleTime = std::chrono::steady_clock::now();

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
		else
			this->startTime += std::chrono::steady_clock::now() - this->pauseTime;

		this->pauseTime = std::chrono::steady_clock::time_point::min();
	}

	// clear crawler
	void Thread::onClear() {
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

			long double tps = (long double) this->tickCounter
					/ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()
					- this->startTime).count();

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

				for(unsigned long n = 0; n < this->config.customCounters.size(); ++n)
					this->initDoGlobalCounting(
							newUrls,
							this->config.customCounters.at(n),
							this->config.customCountersAlias.at(n),
							this->config.customCountersStart.at(n),
							this->config.customCountersEnd.at(n),
							this->config.customCountersStep.at(n),
							this->config.customCountersAliasAdd.at(n)
					);
			}
			else {
				// run each counter over one URL
				for(
						unsigned long n = 0;
						n < std::min(
								this->config.customCounters.size(),
								this->config.customUrls.size()
						);

						++n
				) {
					std::vector<std::string> temp = this->initDoLocalCounting(
							this->config.customUrls.at(n),
							this->config.customCounters.at(n),
							this->config.customCountersAlias.at(n),
							this->config.customCountersStart.at(n),
							this->config.customCountersEnd.at(n),
							this->config.customCountersStep.at(n),
							this->config.customCountersAliasAdd.at(n)
					);

					newUrls.reserve(newUrls.size() + temp.size());
					newUrls.insert(newUrls.end(), temp.begin(), temp.end());
				}
			}

			this->customPages.reserve(newUrls.size());

			for(auto i = newUrls.begin(); i != newUrls.end(); ++i)
				this->customPages.emplace_back(0, *i);
		}
		else {
			// no counters: add all custom URLs as is
			this->customPages.reserve(this->config.customUrls.size());

			for(
					auto i = this->config.customUrls.begin();
					i != this->config.customUrls.end();
					++i
			)
				this->customPages.emplace_back(0, *i);
		}

		// queue for log entries while URL list is locked
		std::queue<std::string> warnings;

		if(!(this->config.crawlerStart.empty())) {
			// set URL of start page
			this->startPage.second = this->config.crawlerStart;

			// add start page to database (if it does not exists already)
			this->database.addUrlIfNotExists(this->startPage.second, true);

			// check for duplicates if URL debugging is active
			if(this->config.crawlerUrlDebug)
				this->database.urlDuplicationCheck();

			// get the ID of the start page URL
			this->startPage.first = this->database.getUrlId(this->startPage.second);
		}

		// get IDs and lock IDs for custom URLs (and add them to the URL list if necessary)
		for(auto i = this->customPages.begin(); i != this->customPages.end(); ++i) {
			try {
				// check URI
				this->parser->setCurrentUrl(i->second);

				// add URL (if it does not exist)
				this->database.addUrlIfNotExists(i->second, true);

				// check for duplicates if URL debugging is active
				if(this->config.crawlerUrlDebug)
					this->database.urlDuplicationCheck();

				// get the ID of the custom URL (and of its URL lock if one already exists)
				i->first = this->database.getUrlId(i->second);
			}
			catch(const URIException& e) {
				if(this->config.crawlerLogging) {
					warnings.emplace("URI Parser error: " + e.whatStr());
					warnings.emplace("Skipped invalid custom URL " + i->second);
				}
			}
		}

		// log warnings if necessary
		if(this->config.crawlerLogging) {
			while(!warnings.empty()) {
				this->log(warnings.front());

				warnings.pop();
			}
		}
	}

	// use a counter to multiply a list of URLs ("global" counting)
	void Thread::initDoGlobalCounting(
			std::vector<std::string>& urlList,
			const std::string& variable,
			const std::string& alias,
			long start,
			long end,
			long step,
			long aliasAdd
	) {
		std::vector<std::string> newUrlList;

		newUrlList.reserve(urlList.size());

		for(auto i = urlList.begin(); i != urlList.end(); ++i) {
			if(i->find(variable) != std::string::npos) {
				long counter = start;

				while(
						this->isRunning()
						&& (
								(start > end && counter >= end)
								|| (start < end && counter <= end)
								|| (start == end)
						)
				) {
					std::string newUrl = *i;
					std::ostringstream counterStrStr;

					counterStrStr << counter;

					Helper::Strings::replaceAll(newUrl, variable, counterStrStr.str(), true);

					if(!alias.empty()) {
						std::ostringstream aliasStrStr;

						aliasStrStr << counter + aliasAdd;

						Helper::Strings::replaceAll(newUrl, alias, aliasStrStr.str(), true);
					}

					newUrlList.emplace_back(newUrl);

					if(start == end)
						break;

					counter += step;
				}

				// sort and remove duplicates
				Helper::Strings::sortAndRemoveDuplicates(
						newUrlList,
						this->config.crawlerUrlCaseSensitive
				);
			}
			else
				newUrlList.emplace_back(*i); // variable not in URL
		}

		urlList.swap(newUrlList);
	}

	// use a counter to multiply a single URL ("local" counting)
	std::vector<std::string> Thread::initDoLocalCounting(
			const std::string& url,
			const std::string& variable,
			const std::string& alias,
			long start,
			long end,
			long step,
			long aliasAdd
	) {
		std::vector<std::string> newUrlList;

		if(url.find(variable) != std::string::npos) {
			long counter = start;

			while(
					this->isRunning()
					&& (
							(start > end && counter >= end)
							|| (start < end && counter <= end)
							|| (start == end)
					)
			) {
				std::string newUrl = url;
				std::ostringstream counterStrStr;

				counterStrStr << counter;

				Helper::Strings::replaceAll(newUrl, variable, counterStrStr.str(), true);

				if(!alias.empty()) {
					std::ostringstream aliasStrStr;

					aliasStrStr << counter + aliasAdd;

					Helper::Strings::replaceAll(newUrl, alias, aliasStrStr.str(), true);
				}

				newUrlList.emplace_back(newUrl);

				if(start == end)
					break;

				counter += step;
			}

			// sort and remove duplicates
			Helper::Strings::sortAndRemoveDuplicates(
					newUrlList,
					this->config.crawlerUrlCaseSensitive
			);
		}
		else // variable not in URL
			newUrlList.emplace_back(url);

		return newUrlList;
	}

	// initialize queries
	void Thread::initQueries() {
		try {
			for(
					auto i = this->config.crawlerQueriesBlackListContent.begin();
					i != this->config.crawlerQueriesBlackListContent.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesBlackListContent.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesBlackListTypes.begin();
					i != this->config.crawlerQueriesBlackListTypes.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesBlackListTypes.emplace_back(this->addQuery(properties));

			}

			for(
					auto i = this->config.crawlerQueriesBlackListUrls.begin();
					i != this->config.crawlerQueriesBlackListUrls.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesBlackListUrls.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesLinks.begin();
					i != this->config.crawlerQueriesLinks.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesLinks.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesLinksBlackListContent.begin();
					i != this->config.crawlerQueriesLinksBlackListContent.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesLinksBlackListContent.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesLinksBlackListTypes.begin();
					i != this->config.crawlerQueriesLinksBlackListTypes.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesLinksBlackListTypes.emplace_back(this->addQuery(properties));

			}

			for(
					auto i = this->config.crawlerQueriesLinksBlackListUrls.begin();
					i != this->config.crawlerQueriesLinksBlackListUrls.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesLinksBlackListUrls.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesLinksWhiteListContent.begin();
					i != this->config.crawlerQueriesLinksWhiteListContent.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesLinksWhiteListContent.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesLinksWhiteListTypes.begin();
					i != this->config.crawlerQueriesLinksWhiteListTypes.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesLinksWhiteListTypes.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesLinksWhiteListUrls.begin();
					i != this->config.crawlerQueriesLinksWhiteListUrls.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesLinksWhiteListUrls.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesWhiteListContent.begin();
					i != this->config.crawlerQueriesWhiteListContent.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesWhiteListContent.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesWhiteListTypes.begin();
					i != this->config.crawlerQueriesWhiteListTypes.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesWhiteListTypes.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.crawlerQueriesWhiteListUrls.begin();
					i != this->config.crawlerQueriesWhiteListUrls.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesWhiteListUrls.emplace_back(this->addQuery(properties));
			}

			for(
					auto i = this->config.customTokensQuery.begin();
					i != this->config.customTokensQuery.end();
					++i
			) {
				QueryProperties properties;

				this->database.getQueryProperties(*i, properties);

				this->queriesTokens.emplace_back(this->addQuery(properties));
			}

			if(this->config.expectedQuery) {
				QueryProperties properties;

				this->database.getQueryProperties(this->config.expectedQuery, properties);

				this->queryExpected = this->addQuery(properties);
			}
		}
		catch(const RegExException& e) {
			throw Exception("Crawler::Thread::initQueries(): [RegEx] " + e.whatStr());
		}
		catch(const XPathException& e) {
			throw Exception("Crawler::Thread::initQueries(): [XPath] " + e.whatStr());
		}
		catch(const JsonPointerException &e) {
			throw Exception("Crawler::Thread::initQueries(): [JSONPointer] " + e.whatStr());
		}
		catch(const JsonPathException &e) {
			throw Exception("Crawler::Thread::initQueries(): [JSONPath] " + e.whatStr());
		}
	}

	// crawling function for URL selection (includes locking the URL)
	bool Thread::crawlingUrlSelection(IdString& urlTo, bool& usePostTo) {
		bool result = true;

		// use GET by default
		usePostTo = false;

		// MANUAL CRAWLING MODE (get URL from configuration)
		if(!(this->getLast())) {
			if(this->manualUrl.first) {
				// renew URL lock on manual URL (custom URL or start page) for retry
				this->lockTime = this->database.lockUrlIfOk(
						this->manualUrl.first,
						this->lockTime,
						this->config.crawlerLock
				);

				if(this->lockTime.empty()) {
					// skip locked URL
					if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
						this->log(
								"URL lock active - " +
								this->manualUrl.second + " skipped."
						);

					this->manualUrl = IdString();
				}
				else {
					// use custom URL
					urlTo = this->crawlingReplaceTokens(this->manualUrl);
					usePostTo = this->config.customUsePost;
				}
			}

			if(!this->manualUrl.first) {
				// no retry: check custom URLs
				if(!(this->customPages.empty())) {
					if(!(this->manualCounter) && this->config.crawlerLogging)
						// start manual crawling with custom URLs
						this->log("starts crawling in non-recoverable MANUAL mode.");

					// check for custom URLs to crawl
					if(this->manualCounter < this->customPages.size()) {
						while(this->manualCounter < this->customPages.size()) {
							// check whether custom URL was already crawled (except when recrawling is enabled)
							if(!(this->config.customReCrawl)
									&& this->database.isUrlCrawled(this->customPages.at(this->manualCounter).first)) {
								// skip custom URL
								++(this->manualCounter);

								continue;
							}

							// set current manual URL to custom URL
							this->manualUrl = this->customPages.at(this->manualCounter);

							// lock custom URL if possible
							this->lockTime = this->database.lockUrlIfOk(
									this->manualUrl.first,
									this->lockTime,
									this->config.crawlerLock
							);

							if(this->lockTime.empty()) {
								// skip locked custom URL
								if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
									this->log(
											"URL lock active - " +
											this->manualUrl.second + " skipped."
									);

								++(this->manualCounter);
								this->manualUrl = IdString();
							}
							else {
								// use custom URL
								urlTo = this->crawlingReplaceTokens(this->manualUrl);
								usePostTo = this->config.customUsePost;

								break;
							}


						}
					}
				}

				if(this->manualCounter == this->customPages.size()) {
					// no more custom URLs to go: get start page (if not crawled, not ignored and lockable)
					if(!(this->config.crawlerStartIgnore) && !(this->startCrawled)) {
						if(this->customPages.empty() && this->config.crawlerLogging) {
							// start manual crawling with start page
							this->log("starts crawling in non-recoverable MANUAL mode.");
						}

						// check whether start page was already crawled (or needs to be re-crawled anyway)
						if(this->config.crawlerReCrawlStart	|| !(this->database.isUrlCrawled(this->startPage.first))) {
							// check whether start page is lockable
							this->lockTime = this->database.lockUrlIfOk(
									this->startPage.first,
									this->lockTime,
									this->config.crawlerLock
							);

							if(this->lockTime.empty()) {
								// start page is locked: write skipping of entry to log if enabled
								if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
									this->log(
											"URL lock active - " +
											this->startPage.second + " skipped."
									);

								// start page is done
								this->startCrawled = true;
							}
							else {
								// select start page
								urlTo = this->startPage;

								this->manualUrl = this->startPage;
							}
						}
						else {
							// start page is done
							this->startCrawled = true;
						}

						// reset manual URL if start page has been skipped
						if(this->startCrawled)
							this->manualUrl = IdString();
					}
				}
			}
		}

		// AUTOMATIC CRAWLING MODE (get URL directly from database)
		if(!(this->manualUrl.first)) {
			// check whether manual crawling mode was already set off
			if(!(this->manualOff)) {
				// start manual crawling with start page
				if(this->config.crawlerLogging)
					this->log("switches to recoverable AUTOMATIC mode.");

				this->manualOff = true;
			}

			// check for retry
			bool retry = false;
			if(this->nextUrl.first) {
				// try to renew URL lock on automatic URL for retry
				this->lockTime = this->database.lockUrlIfOk(
						this->nextUrl.first,
						this->lockTime,
						this->config.crawlerLock
				);

				if(!(this->lockTime.empty())) {
					// log retry
					if(this->config.crawlerLogging)
						this->log("retries " + this->nextUrl.second + "...");

					// set URL to last URL
					urlTo = this->nextUrl;

					// do retry
					retry = true;
				}
			}

			if(!retry) {
				// log failed retry if necessary
				if(this->nextUrl.first && this->config.crawlerLogging > Config::crawlerLoggingDefault)
					this->log(
							"could not retry " + this->nextUrl.second + ","
							" because it is locked."
					);

				while(true) {
					// get next URL
					this->nextUrl = this->database.getNextUrl(this->getLast());

					if(this->nextUrl.first) {
						// try to lock next URL
						this->lockTime = this->database.lockUrlIfOk(
										this->nextUrl.first,
										this->lockTime,
										this->config.crawlerLock
						);

						if(this->lockTime.empty()) {
							// skip locked URL
							if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
								this->log("skipped " + this->nextUrl.second + ", because it is locked.");
						}
						else {
							urlTo = this->nextUrl;
							break;
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

		// set thread status
		if(result)
			this->setStatusMessage(urlTo.second);
		else {
			this->setStatusMessage("IDLE Waiting for new URLs to crawl.");
			this->setProgress(1.f);
		}

		return result;
	}

	// replace token variables in custom URL
	Thread::IdString Thread::crawlingReplaceTokens(const IdString& url) {
		if(this->config.customTokens.empty())
			return url;

		IdString result(url);

		for(unsigned long n = 0; n < this->config.customTokens.size(); ++n) {
			if(result.second.find(this->config.customTokens.at(n))) {
				// get content for extracting token
				std::string content;
				std::string token;
				bool success = false;

				while(this->isRunning()) {
					try {
						this->networking.getContent(
								"https://" + this->config.customTokensSource.at(n),
								this->config.customUsePost,
								content,
								this->config.crawlerRetryHttp
						);

						success = true;

						break;
					}
					catch(const CurlException& e) {
						// error while getting content: check type of error i.e. last cURL code
						if(this->crawlingCheckCurlCode(
								this->networking.getCurlCode(),
								url.second
						)) {
							// reset connection and retry
							if(this->config.crawlerLogging) {
								this->log(e.whatStr() + " [" + this->config.customTokensSource.at(n) + "].");
								this->log("resets connection...");
							}

							this->setStatusMessage("ERROR " + e.whatStr() + " [" + this->config.customTokensSource.at(n) + "]");

							this->networking.resetConnection(this->config.crawlerSleepError);
						}
						else {
							if(this->config.crawlerLogging)
								this->log(
										"Could not get token from "
										+ this->config.customTokensSource.at(n)
										+ ": "
										+ e.whatStr()
										+ " - skips "
										+ result.second
										+ "..."
								);

							break;
						}
					}
					catch(const Utf8Exception& e) {
						// write to log if neccessary
						if(this->config.crawlerLogging)
							this->log("WARNING: " + e.whatStr() + " [" + url.second + "].");

						break;
					}
				}

				if(success) {
					// check query result type
					if(!(this->queriesTokens.at(n).resultSingle)) {
						if(this->config.crawlerLogging)
							this->log("WARNING: Query for getting token has wrong result type.");

						continue;
					}

					// get token from content
					Parsing::XML xmlDoc;
					rapidjson::Document jsonDoc;
					jsoncons::json json;

					switch(this->queriesTokens.at(n).type) {
					case QueryStruct::typeRegEx:
						// get token from content
						try {
							this->getRegExQuery(this->queriesTokens.at(n).index).getFirst(content, token);
						}
						catch(const RegExException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: RegEx error - "
										+ e.whatStr()
										+ " ["
										+ this->config.customTokensSource.at(n)
										+ "]."
								);
						}

						break;

					case QueryStruct::typeXPath:
						// parse XML content
						try {
							xmlDoc.parse(content, this->config.crawlerRepairCData);
						}
						catch(const XMLException& e) {
							// error while parsing content
							if(this->config.crawlerLogging)
								this->log(
										"XML error: "
										+ e.whatStr()
										+ " ["
										+ this->config.customTokensSource.at(n)
										+ "]."
								);

							break;
						}

						// get token from XML
						try {
							this->getXPathQuery(this->queriesTokens.at(n).index).getFirst(xmlDoc, token);
						}
						catch(const XPathException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: XPath error - "
										+ e.whatStr()
										+ " ["
										+ this->config.customTokensSource.at(n)
										+ "]."
								);
						}

						break;

					case QueryStruct::typeJsonPointer:
						// parse JSON using RapidJSON
						try {
							jsonDoc = Helper::Json::parseRapid(content);
						}
						catch(const JsonException& e) {
							// error while parsing content
							if(this->config.crawlerLogging)
								this->log(
										"JSON parsing error: "
										+ e.whatStr()
										+ " ["
										+ this->config.customTokensSource.at(n)
										+ "]."
								);

							break;
						}

						// get token from JSON
						try {
							this->getJsonPointerQuery(this->queriesTokens.at(n).index).getFirst(jsonDoc, token);
						}
						catch(const JsonPointerException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: JSONPointer error - "
										+ e.whatStr() + " ["
										+ this->config.customTokensSource.at(n)
										+ "]."
								);
						}

						break;

					case QueryStruct::typeJsonPath:
						// parse JSON using jsoncons
						try {
							json = Helper::Json::parseCons(content);
						}
						catch(const JsonException& e) {
							// error while parsing content
							if(this->config.crawlerLogging)
								this->log(
										"JSON parsing error: "
										+ e.whatStr()
										+ " ["
										+ this->config.customTokensSource.at(n)
										+ "]."
								);

							break;
						}

						// get token from JSON
						try {
							this->getJsonPathQuery(this->queriesTokens.at(n).index).getFirst(json, token);
						}
						catch(const JsonPathException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: JSONPointer error - "
										+ e.whatStr() + " ["
										+ this->config.customTokensSource.at(n)
										+ "]."
								);
						}

						break;

					case QueryStruct::typeNone:
						break;

					default:
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: Invalid type of Query for getting token."
							);
					}
				}

				// replace variable(s) with token
				Helper::Strings::replaceAll(result.second, this->config.customTokens.at(n), token, true);
			}
		}

		return result;
	}

	// crawl content
	bool Thread::crawlingContent(
			const IdString& url,
			bool usePost,
			unsigned long& checkedUrlsTo,
			unsigned long& newUrlsTo,
			std::string& timerStrTo
	) {
		Timer::StartStop sleepTimer;
		Timer::StartStop httpTimer;
		Timer::StartStop parseTimer;
		Timer::StartStop updateTimer;
		std::string content;

		timerStrTo = "";

		// check arguments
		if(!url.first)
			throw Exception("Crawler::Thread::crawlingContent(): No URL ID specified");

		if(url.second.empty())
			throw Exception("Crawler::Thread::crawlingContent(): No URL specified");

		// skip crawling if only archive needs to be retried
		if(this->config.crawlerArchives && this->archiveRetry) {
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log("Retrying archive only [" + url.second + "].");

			return true;
		}

		// check HTTP sleeping time
		if(this->config.crawlerSleepHttp) {
			// calculate elapsed time since last HTTP request and sleep if necessary
			unsigned long httpElapsed =
					std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::steady_clock::now() - this->httpTime
					).count();

			if(httpElapsed < this->config.crawlerSleepHttp) {
				this->idleTime = std::chrono::steady_clock::now();

				if(this->config.crawlerTiming)
					sleepTimer.start();

				std::this_thread::sleep_for(
						std::chrono::milliseconds(this->config.crawlerSleepHttp - httpElapsed)
				);

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
			this->networking.setConfigCurrent(*this);
		}
		catch(const CurlException& e) {
			// error while setting up network
			if(this->config.crawlerLogging) {
				this->log(e.whatStr() + " [" + url.second+ "].");
				this->log("resets connection...");
			}

			this->setStatusMessage("ERROR " + e.whatStr() + " [" + url.second + "]");

			this->networking.resetConnection(this->config.crawlerSleepError);

			this->crawlingRetry(url, false);

			return false;
		}

		// start HTTP timer(s)
		if(this->config.crawlerTiming)
			httpTimer.start();
		if(this->config.crawlerSleepHttp)
			this->httpTime = std::chrono::steady_clock::now();

		try {
			// get content
			this->networking.getContent(
					"https://" + this->domain + url.second,
					usePost,
					content,
					this->config.crawlerRetryHttp
			);
		}
		catch(const CurlException& e) {
			// error while getting content: check type of error i.e. last cURL code
			if(this->crawlingCheckCurlCode(
					this->networking.getCurlCode(),
					url.second
			)) {
				// reset connection and retry
				if(this->config.crawlerLogging) {
					this->log(e.whatStr() + " [" + url.second + "].");
					this->log("resets connection...");
				}

				this->setStatusMessage("ERROR " + e.whatStr() + " [" + url.second + "]");

				this->networking.resetConnection(this->config.crawlerSleepError);

				this->crawlingRetry(url, false);
			}
			else {
				// skip URL
				this->crawlingSkip(url, !(this->config.crawlerArchives));
			}
			return false;
		}
		catch(const Utf8Exception& e) {
			// write to log if neccessary
			if(this->config.crawlerLogging)
				this->log("WARNING: " + e.whatStr() + " [" + url.second + "].");

			// skip URL
			this->crawlingSkip(url, !(this->config.crawlerArchives));
		}

		// check response code
		unsigned int responseCode = this->networking.getResponseCode();

		if(!(this->crawlingCheckResponseCode(url.second, responseCode))) {
			// skip because of response code
			this->crawlingSkip(url, !(this->config.crawlerArchives));
			return false;
		}

		// update timer if necessary
		if(this->config.crawlerTiming) {
			httpTimer.stop();

			if(!timerStrTo.empty())
				timerStrTo += ", ";

			timerStrTo += "http: " + httpTimer.totalStr();

			parseTimer.start();
		}

		// check content type
		std::string contentType = this->networking.getContentType();

		if(!(this->crawlingCheckContentType(url.second, contentType))) {
			// skip because of content type (on blacklist or not on whitelist)
			this->crawlingSkip(url, !(this->config.crawlerArchives));

			return false;
		}

		// check content
		if(!(this->crawlingCheckContent(url.second, content))) {
			// skip because of content (on blacklist or not on whitelist)
			this->crawlingSkip(url, !(this->config.crawlerArchives));

			return false;
		}

		if(this->config.crawlerTiming) {
			parseTimer.stop();

			updateTimer.start();
		}

		// save content
		this->crawlingSaveContent(url, responseCode, contentType, content);

		if(this->config.crawlerTiming) {
			updateTimer.stop();

			parseTimer.start();
		}

		// extract URLs
		std::vector<std::string> urls = this->crawlingExtractUrls(url.second, contentType, content);

		if(!urls.empty()) {
			// update timer if necessary
			if(this->config.crawlerTiming) {
				parseTimer.stop();

				updateTimer.start();
			}

			// parse and add URLs
			checkedUrlsTo += urls.size();

			this->crawlingParseAndAddUrls(url.second, urls, newUrlsTo, false);

			// update timer if necessary
			if(this->config.crawlerTiming) {
				updateTimer.stop();

				timerStrTo +=
						", parse: " + parseTimer.totalStr() +
						", update: " + updateTimer.totalStr();
			}
		}

		return true;
	}

	// check whether URL should be added
	bool Thread::crawlingCheckUrl(const std::string& url, const std::string& from) {
		// check argument
		if(url.empty())
			return false;

		// check for whitelisted URLs
		if(!(this->queriesWhiteListUrls.empty())) {
			// whitelist: only allow URLs that fit a specified whitelist query
			bool whitelist = false;
			bool found = false;

			for(
					auto i = this->queriesWhiteListUrls.begin();
					i != this->queriesWhiteListUrls.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					whitelist = true;

					try {
						found = this->getRegExQuery(i->index).getBool(url);

						if(found)
							break;
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error"
									" - " + e.whatStr() + " [" + url + " from " + from + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					whitelist = true;

					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Query on URL"
								" is not of type RegEx."
					);
				}
			}

			if(whitelist && !found) {
				if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
					this->log("skipped " + url + " (not whitelisted).");

				return false;
			}
		}

		// check for blacklisted URLs
		if(!(this->queriesBlackListUrls.empty())) {
			// blacklist: do not allow URLs that fit a specified blacklist query
			for(
					auto i = this->queriesBlackListUrls.begin();
					i != this->queriesBlackListUrls.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					try {
						if(this->getRegExQuery(i->index).getBool(url)) {
							if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
								this->log("skipped " + url + " (blacklisted).");

							return false;
						}
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - " +
									e.whatStr() + " [" + url + " from " + from + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Query on URL"
								" is not of type RegEx."
						);
				}
			}
		}

		return true;
	}

	// check whether links should be extracted from URL
	bool Thread::crawlingCheckUrlForLinkExtraction(const std::string& url) {
		// check argument
		if(url.empty())
			return false;

		// check for whitelisted URLs
		if(!(this->queriesLinksWhiteListUrls.empty())) {
			// whitelist: only allow URLs that fit a specified whitelist query
			bool whitelist = false;
			bool found = false;

			for(
					auto i = this->queriesLinksWhiteListUrls.begin();
					i != this->queriesLinksWhiteListUrls.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					whitelist = true;

					try {
						found = this->getRegExQuery(i->index).getBool(url);

						if(found)
							break;
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error"
									" - " + e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					whitelist = true;

					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Query on URL is not of type RegEx."
						);
				}
			}

			if(whitelist && !found) {
				if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
					this->log("skipped " + url + " (not whitelisted).");

				return false;
			}
		}

		// check for blacklisted URLs
		if(!(this->queriesLinksBlackListUrls.empty())) {
			// blacklist: do not allow URLs that fit a specified blacklist query
			for(
					auto i = this->queriesLinksBlackListUrls.begin();
					i != this->queriesLinksBlackListUrls.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					try {
						if(this->getRegExQuery(i->index).getBool(url)) {
							if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
								this->log("skipped " + url + " (blacklisted).");

							return false;
						}
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Query on URL"
								" is not of type RegEx."
						);
				}
			}
		}

		return true;
	}

	// check CURL code and decide whether to retry or skip
	bool Thread::crawlingCheckCurlCode(CURLcode curlCode, const std::string& url) {
		if(curlCode == CURLE_TOO_MANY_REDIRECTS) {
			// redirection error: skip URL
			if(this->config.crawlerLogging)
				this->log("redirection error at " + url + " - skips...");

			return false;
		}

		return true;
	}

	// check the HTTP response code for an error and decide whether to continue or skip
	bool Thread::crawlingCheckResponseCode(const std::string& url, long responseCode) {
		if(responseCode >= 400 && responseCode < 600) {
			if(this->config.crawlerLogging) {
				std::ostringstream logStrStr;

				logStrStr	<< "HTTP error " << responseCode
							<< " from " << url << " - skips...";

				this->log(logStrStr.str());
			}

			return false;
		}
		else if(responseCode != 200 && this->config.crawlerLogging) {
			if(this->config.crawlerLogging) {
				std::ostringstream logStrStr;

				logStrStr	<< "WARNING: HTTP response code " << responseCode
							<< " from " << url << ".";

				this->log(logStrStr.str());
			}
		}

		return true;
	}

	// check whether specific content type should be crawled
	bool Thread::crawlingCheckContentType(const std::string& url, const std::string& contentType) {
		// check whitelist for content types
		if(!(this->queriesWhiteListTypes.empty())) {
			bool found = false;

			for(
					auto i = this->queriesWhiteListTypes.begin();
					i != this->queriesWhiteListTypes.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					try {
						found = this->getRegExQuery(i->index).getBool(contentType);

						if(found)
							break;
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Query on content type"
								" is not of type RegEx."
						);
				}
			}

			if(!found)
				return false;
		}

		// check blacklist for content types
		if(!(this->queriesBlackListTypes.empty())) {
			bool found = false;

			for(
					auto i = this->queriesBlackListTypes.begin();
					i != this->queriesBlackListTypes.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					try {
						found = this->getRegExQuery(i->index).getBool(contentType);

						if(found)
							break;
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Query on content type"
								" is not of type RegEx."
						);
				}
			}

			if(found)
				return false;
		}

		return true;
	}

	// check whether specific content type should be used for link extraction
	bool Thread::crawlingCheckContentTypeForLinkExtraction(const std::string& url, const std::string& contentType) {
		// check whitelist for content types
		if(!(this->queriesLinksWhiteListTypes.empty())) {
			bool found = false;

			for(
					auto i = this->queriesLinksWhiteListTypes.begin();
					i != this->queriesLinksWhiteListTypes.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					// run query to determine whether to extract links
					try {
						found = this->getRegExQuery(i->index).getBool(contentType);

						if(found)
							break;
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Query on content type"
								" is not of type RegEx."
						);
				}
			}

			if(!found)
				return false;
		}

		// check blacklist for content types
		if(!(this->queriesLinksBlackListTypes.empty())) {
			bool found = false;

			for(
					auto i = this->queriesLinksBlackListTypes.begin();
					i != this->queriesLinksBlackListTypes.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					// run query to determine whether to extract links
					try {
						found = this->getRegExQuery(i->index).getBool(contentType);

						if(found)
							break;
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;
				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Query on content type"
								" is not of type RegEx."
						);
				}
			}

			if(found)
				return false;
		}

		return true;
	}

	// check whether specific content should be crawled
	bool Thread::crawlingCheckContent(const std::string& url, const std::string& content) {
		// check argument
		if(url.empty())
			throw Exception("Crawler::Thread::crawlingCheckContent(): No URL specified");

		// check whitelist for content types
		if(!(this->queriesWhiteListContent.empty())) {
			bool found = false;

			for(
					auto i = this->queriesWhiteListContent.begin();
					i != this->queriesWhiteListContent.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					// run query to determine whether to crawl content
					try {
						found = this->getRegExQuery(i->index).getBool(content);

						if(found)
							break;
					}
					catch(const RegExException&e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeXPath:
					// parse XML/HTML if still necessary
					if(!(this->parseXml(content)))
						return false;

					// run query to determine whether to crawl content
					try {
						found = this->getXPathQuery(i->index).getBool(this->parsedXML);

						if(found)
							break;
					}
					catch(const XPathException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: XPath error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeJsonPointer:
					// parse JSON using RapidJSON if still necessary
					if(!(this->parseJsonRapid(content)))
						return false;

					// run query to determine whether to crawl content
					try {
						found = this->getJsonPointerQuery(i->index).getBool(this->parsedJsonRapid);

						if(found)
							break;
					}
					catch(const JsonPointerException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: JSONPointer error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeJsonPath:
					// parse JSON using jsoncons if still necessary
					if(!(this->parseJsonCons(content)))
						return false;

					// run query to determine whether to crawl content
					try {
						found = this->getJsonPathQuery(i->index).getBool(this->parsedJsonCons);

						if(found)
							break;
					}
					catch(const JsonPathException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: JSONPath error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Invalid type of query on content type."
						);
				}
			}

			if(!found)
				return false;
		}

		// check blacklist for content types
		if(!(this->queriesBlackListContent.empty())) {
			bool found = false;

			for(
					auto i = this->queriesBlackListContent.begin();
					i != this->queriesBlackListContent.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					// run query to determine whether to crawl content
					try {
						found = this->getRegExQuery(i->index).getBool(content);

						if(found)
							break;
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - "
									+ e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeXPath:
					// parse XML/HTML if still necessary
					if(this->parseXml(content)) {
						// run query to determine whether to crawl content
						try {
							found = this->getXPathQuery(i->index).getBool(this->parsedXML);

							if(found)
								break;
						}
						catch(const XPathException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: XPath error - " +
										e.whatStr() + " [" + url + "]."
								);
						}
					}

					break;

				case QueryStruct::typeJsonPointer:
					// parse JSON using RapidJSON if still necessary
					if(this->parseJsonRapid(content)) {
						// run query to determine whether to crawl content
						try {
							found = this->getJsonPointerQuery(i->index).getBool(this->parsedJsonRapid);

							if(found)
								break;
						}
						catch(const JsonPointerException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: JSONPointer error - " +
										e.whatStr() + " [" + url + "]."
								);
						}
					}

					break;

				case QueryStruct::typeJsonPath:
					// parse JSON using jsoncons if still necessary
					if(this->parseJsonCons(content)) {
						// run query to determine whether to crawl content
						try {
							found = this->getJsonPathQuery(i->index).getBool(this->parsedJsonCons);

							if(found)
								break;
						}
						catch(const JsonPathException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: JSONPath error - " +
										e.whatStr() + " [" + url + "]."
								);
						}
					}

					break;

				case QueryStruct::typeNone:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Invalid type of query on content type."
						);
				}
			}

			if(found)
				return false;
		}

		return true;
	}

	// check whether specific content should be used for link extraction
	bool Thread::crawlingCheckContentForLinkExtraction(const std::string& url, const std::string& content) {
		// check argument
		if(url.empty())
			throw Exception("Crawler::Thread::crawlingCheckContent(): No URL specified");

		// check whitelist for content types
		if(!(this->queriesLinksWhiteListContent.empty())) {
			bool found = false;

			for(
					auto i = this->queriesLinksWhiteListContent.begin();
					i != this->queriesLinksWhiteListContent.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					// run query to determine whether to extract links
					try {
						found = this->getRegExQuery(i->index).getBool(content);

						if(found)
							break;
					}
					catch(const RegExException&e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeXPath:
					// parse XML/HTML if still necessary
					if(!(this->parseXml(content)))
						return false;

					// run query to determine whether to extract links
					try {
						found = this->getXPathQuery(i->index).getBool(this->parsedXML);

						if(found)
							break;
					}
					catch(const XPathException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: XPath error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeJsonPointer:
					// parse JSON using RapidJSON if still necessary
					if(!(this->parseJsonRapid(content)))
						return false;

					// run query to determine whether to extract links
					try {
						found = this->getJsonPointerQuery(i->index).getBool(this->parsedJsonRapid);

						if(found)
							break;
					}
					catch(const JsonPointerException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: JSONPointer error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeJsonPath:
					// parse JSON using jsoncons if still necessary
					if(!(this->parseJsonCons(content)))
						return false;

					// run query to determine whether to extract links
					try {
						found = this->getJsonPathQuery(i->index).getBool(this->parsedJsonCons);

						if(found)
							break;
					}
					catch(const JsonPathException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: JSONPath error - " +
									e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Unknown type of query on content type"
						);
				}
			}

			if(!found)
				return false;
		}

		// check blacklist for content types
		if(!(this->queriesLinksBlackListContent.empty())) {
			bool found = false;

			for(
					auto i = this->queriesLinksBlackListContent.begin();
					i != this->queriesLinksBlackListContent.end();
					++i
			) {
				switch(i->type) {
				case QueryStruct::typeRegEx:
					// run query to determine whether to extract links
					try {
						found = this->getRegExQuery(i->index).getBool(content);

						if(found)
							break;
					}
					catch(const RegExException& e) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: RegEx error - "
									+ e.whatStr() + " [" + url + "]."
							);
					}

					break;

				case QueryStruct::typeXPath:
					// parse XML/HTML if still necessary
					if(this->parseXml(content)) {
						// run query to determine whether to extract links
						try {
							found = this->getXPathQuery(i->index).getBool(this->parsedXML);

							if(found)
								break;
						}
						catch(const XPathException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: XPath error - " +
										e.whatStr() + " [" + url + "]."
								);
						}
					}

					break;

				case QueryStruct::typeJsonPointer:
					// parse JSON using RapidJSON if still necessary
					if(this->parseJsonRapid(content)) {
						// run query to determine whether to extract links
						try {
							found = this->getJsonPointerQuery(i->index).getBool(this->parsedJsonRapid);

							if(found)
								break;
						}
						catch(const JsonPointerException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: JSONPointer error - " +
										e.whatStr() + " [" + url + "]."
								);
						}
					}

					break;

				case QueryStruct::typeJsonPath:
					// parse JSON using jsoncons if still necessary
					if(this->parseJsonCons(content)) {
						// run query to determine whether to extract links
						try {
							found = this->getJsonPathQuery(i->index).getBool(this->parsedJsonCons);

							if(found)
								break;
						}
						catch(const JsonPathException& e) {
							if(this->config.crawlerLogging)
								this->log(
										"WARNING: JSONPath error - " +
										e.whatStr() + " [" + url + "]."
								);
						}
					}

					break;

				case QueryStruct::typeNone:
					break;

				default:
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Invalid type of query on content type."
						);
				}
			}

			if(found)
				return false;
		}

		return true;
	}

	// save content to database
	void Thread::crawlingSaveContent(
			const IdString& url,
			unsigned int response,
			const std::string& type,
			const std::string& content
	) {
		// check arguments
		if(!url.first)
			throw Exception("Crawler::Thread::crawlingSaveContent(): No URL ID specified");

		if(url.second.empty())
			throw Exception("Crawler::Thread::crawlingSaveContent(): No URL specified");

		if(this->config.crawlerXml) {
			// parse XML/HTML if still necessary
			if(!(this->xmlParsed) && this->xmlParsingError.empty()) {
				try {
					this->parsedXML.parse(content, this->config.crawlerRepairCData);

					this->xmlParsed = true;
				}
				catch(const XMLException& e) {
					this->xmlParsingError = e.whatStr();
				}
			}

			if(this->xmlParsed) {
				std::string xmlContent;

				try {
					this->parsedXML.getContent(xmlContent);

					this->database.saveContent(url.first, response, type, xmlContent);

					return;
				}
				catch(const XMLException& e) {
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: Could not clean content"
								" [" + url.second + "]: " + e.whatStr()
						);
				}
			}
		}

		this->database.saveContent(url.first, response, type, content);
	}

	// extract URLs from content
	std::vector<std::string> Thread::crawlingExtractUrls(
			const std::string& url,
			const std::string& type,
			const std::string& content
	) {
		// check argument
		if(url.empty())
			throw Exception("Crawler::Thread::crawlingExtractUrls(): No URL specified");

		// check queries
		if(this->queriesLinks.empty())
			return std::vector<std::string>();

		// check whether to extract URLs
		if(
				!(this->crawlingCheckUrlForLinkExtraction(url))
				|| !(this->crawlingCheckContentTypeForLinkExtraction(url, type))
				|| !(this->crawlingCheckContentForLinkExtraction(url, content))
		)
			return std::vector<std::string>();

		std::vector<std::string> urls;

		for(
				auto i = this->queriesLinks.begin();
				i != this->queriesLinks.end();
				++i
		) {
			switch(i->type) {
			case QueryStruct::typeRegEx:
				try {
					// run query to extract URLs
					if(i->resultMulti) {
						std::vector<std::string> results;

						this->getRegExQuery(i->index).getAll(content, results);

						urls.reserve(urls.size() + results.size());
						urls.insert(urls.end(), results.begin(), results.end());
					}
					else {
						std::string result;

						this->getRegExQuery(i->index).getFirst(content, result);

						urls.emplace_back(result);
					}
				}
				catch(const RegExException& e) {
					if(this->config.crawlerLogging)
						this->log("WARNING: RegEx error - " + e.whatStr() + " [" + url + "].");
				}

				break;

			case QueryStruct::typeXPath:
				// parse XML/HTML if still necessary
				if(this->parseXml(content)) {
					// run query to extract URLs
					try {
						if(i->resultMulti) {
							std::vector<std::string> results;

							this->getXPathQuery(i->index).getAll(this->parsedXML, results);

							urls.reserve(urls.size() + results.size());
							urls.insert(urls.end(), results.begin(), results.end());
						}
						else {
							std::string result;

							this->getXPathQuery(i->index).getFirst(this->parsedXML, result);

							urls.emplace_back(result);
						}
					}
					catch(const XPathException& e) {
						if(this->config.crawlerLogging)
							this->log("WARNING: XPath error - " + e.whatStr() + " [" + url + "].");
					}
				}

				break;

			case QueryStruct::typeJsonPointer:
				// parse JSON using RapidJSON if still necessary
				if(this->parseJsonRapid(content)) {
					// run query to extract URLs
					try {
						if(i->resultMulti) {
							std::vector<std::string> results;

							this->getJsonPointerQuery(i->index).getAll(this->parsedJsonRapid, results);

							urls.reserve(urls.size() + results.size());
							urls.insert(urls.end(), results.begin(), results.end());
						}
						else {
							std::string result;

							this->getJsonPointerQuery(i->index).getFirst(this->parsedJsonRapid, result);

							urls.emplace_back(result);
						}
					}
					catch(const JsonPointerException& e) {
						if(this->config.crawlerLogging)
							this->log("WARNING: JSONPointer error - " + e.whatStr() + " [" + url + "].");
					}
				}

				break;

			case QueryStruct::typeJsonPath:
				// parse JSON using jsoncons if still necessary
				if(this->parseJsonCons(content)) {
					// run query to extract URLs
					try {
						if(i->resultMulti) {
							std::vector<std::string> results;

							this->getJsonPathQuery(i->index).getAll(this->parsedJsonCons, results);

							urls.reserve(urls.size() + results.size());
							urls.insert(urls.end(), results.begin(), results.end());
						}
						else {
							std::string result;

							this->getJsonPathQuery(i->index).getFirst(this->parsedJsonCons, result);

							urls.emplace_back(result);
						}
					}
					catch(const JsonPathException& e) {
						if(this->config.crawlerLogging)
							this->log("WARNING: JSONPath error - " + e.whatStr() + " [" + url + "].");
					}
				}

				break;

			case QueryStruct::typeNone:
				break;

			default:
				if(this->config.crawlerLogging)
					this->log("WARNING: Invalid type of query on content type.");
			}
		}

		// check number of extracted URLs if necessary
		std::string expectedStr;

		switch(this->queryExpected.type) {
		case QueryStruct::typeRegEx:
			if(this->queryExpected.resultSingle) {
				try {
					this->getRegExQuery(this->queryExpected.index).getFirst(content, expectedStr);
				}
				catch(const RegExException& e) {
					if(this->config.crawlerLogging)
						this->log("WARNING: RegEx error - " + e.whatStr() + " [" + url + "].");
				}
			}
			else if(this->config.crawlerLogging)
				this->log("WARNING: Invalid result type of query for expected number of results.");

			break;

		case QueryStruct::typeXPath:
			if(this->queryExpected.resultSingle) {
				if(this->parseXml(content)) {
					try {
						this->getXPathQuery(this->queryExpected.index).getFirst(this->parsedXML, expectedStr);
					}
					catch(const XPathException& e) {
						if(this->config.crawlerLogging)
							this->log("WARNING: XPath error - " + e.whatStr() + " [" + url + "].");
					}
				}
			}
			else if(this->config.crawlerLogging)
				this->log("WARNING: Invalid result type of query for expected number of results.");
			break;

		case QueryStruct::typeJsonPointer:
			if(this->queryExpected.resultSingle) {
				if(this->parseJsonRapid(content)) {
					try {
						this->getJsonPointerQuery(this->queryExpected.index).getFirst(this->parsedJsonRapid, expectedStr);
					}
					catch(const JsonPointerException& e) {
						if(this->config.crawlerLogging)
							this->log("WARNING: JSONPointer error - " + e.whatStr() + " [" + url + "].");
					}
				}
			}
			else if(this->config.crawlerLogging)
				this->log("WARNING: Invalid result type of query for expected number of results.");
			break;

		case QueryStruct::typeJsonPath:
			if(this->queryExpected.resultSingle) {
				if(this->parseJsonCons(content)) {
					try {
						this->getJsonPathQuery(this->queryExpected.index).getFirst(this->parsedJsonCons, expectedStr);
					}
					catch(const JsonPathException& e) {
						if(this->config.crawlerLogging)
							this->log("WARNING: JSONPath error - " + e.whatStr() + " [" + url + "].");
					}
				}
			}
			else if(this->config.crawlerLogging)
				this->log("WARNING: Invalid result type of query for expected number of results.");
			break;

		case QueryStruct::typeNone:
			break;

		default:
			if(this->config.crawlerLogging)
				this->log("WARNING: Invalid type of query for expected number of results.");
		}

		if(!expectedStr.empty()) {
			unsigned long expected = std::stoul(expectedStr);
			std::ostringstream errorStrStr;

			errorStrStr.imbue(std::locale(""));

			if(urls.size() < expected) {
				// number of URLs is smaller than expected
				errorStrStr	<< "Number of extracted URLs ["
							<< urls.size()
							<< "] is smaller than expected ["
							<< expected
							<< " ["
							<< url
							<< "]";

				if(this->config.expectedErrorIfSmaller)
					throw Exception(errorStrStr.str());
				else if(this->config.crawlerLogging)
					this->log("WARNING: " + errorStrStr.str() + ".");
			}
			else if(urls.size() > expected) {
				// number of URLs is larger than expected
				errorStrStr	<< "Number of extracted URLs ["
							<< urls.size()
							<< "] is larger than expected ["
							<< expected
							<< " ["
							<< url
							<< "]";

				// number of URLs is smaller than expected
				if(this->config.expectedErrorIfLarger)
					throw Exception(errorStrStr.str());
				else if(this->config.crawlerLogging)
					this->log("WARNING: " + errorStrStr.str() + ".");
			}
		}

		// sort and remove duplicates
		Helper::Strings::sortAndRemoveDuplicates(urls, this->config.crawlerUrlCaseSensitive);

		return urls;
	}

	// parse URLs and add them as sub-links (or links if website is cross-domain) to the database if they do not already exist
	void Thread::crawlingParseAndAddUrls(
			const std::string& url,
			std::vector<std::string>& urls,
			unsigned long& newUrlsTo,
			bool archived
	) {
		// check argument
		if(url.empty())
			throw Exception("Crawler::Thread::crawlingParseAndAddUrls(): No URL specified");

		// set current URL
		try {
			this->parser->setCurrentUrl(url);
		}
		catch(const URIException& e) {
			throw Exception(
					"Crawler::Thread::crawlingParseAndAddUrls():"
					" Could not set current sub-url"
					" because of URI Parser error: " + e.whatStr() +
					" [" + url + "]"
			);
		}

		// parse URLs
		newUrlsTo = 0;

		for(unsigned long n = 1; n <= urls.size(); ++n) {
			// parse archive URLs (only absolute links behind archive links!)
			if(archived) {
				unsigned long pos = 0;
				unsigned long pos1 = urls.at(n - 1).find("https://", 1);
				unsigned long pos2 = urls.at(n - 1).find("http://", 1);

				if(pos1 != std::string::npos && pos2 != std::string::npos) {
					if(pos1 < pos2)
						pos = pos2;
					else
						pos = pos1;
				}
				else if(pos1 != std::string::npos)
					pos = pos1;
				else if(pos2 != std::string::npos)
					pos = pos2;

				if(pos)
					urls.at(n - 1) = Parsing::URI::unescape(urls.at(n - 1).substr(pos), false);
				else
					urls.at(n - 1) = "";
			}

			if(!urls.at(n - 1).empty()) {
				// replace &amp; with &
				Helper::Strings::replaceAll(urls.at(n - 1), "&amp;", "&", true);

				// parse link
				try {
					if(this->parser->parseLink(urls.at(n - 1))) {
						if(this->parser->isSameDomain()) {
							if(!(this->config.crawlerParamsBlackList.empty())) {
								urls.at(n - 1) = this->parser->getSubUrl(
										this->config.crawlerParamsBlackList,
										false
								);
							}
							else
								urls.at(n - 1) = this->parser->getSubUrl(
										this->config.crawlerParamsWhiteList,
										true
								);

							if(!(this->crawlingCheckUrl(urls.at(n - 1), url)))
								urls.at(n - 1) = "";

							if(!urls.at(n - 1).empty()) {
								if(this->domain.empty()) {
									// check URL (has to contain at least one slash)
									if(urls.at(n - 1).find('/') == std::string::npos)
										urls.at(n - 1).append(1, '/');
								}
								else {
									// check sub-URL (needs to start with slash)
									if(urls.at(n - 1).at(0) != '/')
										throw Exception(
												"Crawler::Thread::crawlingParseAndAddUrls():"
												" " + urls.at(n - 1) + " is no sub-URL!"
												" [" + url + "]"
										);
								}

								if(
										this->config.crawlerLogging
										&& urls.at(n - 1).length() > 1
										&& urls.at(n - 1).at(1) == '#'
								)
									this->log("WARNING: Found anchor \'" + urls.at(n - 1) + "\'. [" + url + "]");

								continue;
							}
						}
					}
				}
				catch(const URIException& e) {
					if(this->config.crawlerLogging)
						this->log("WARNING: URI Parser error - " + e.whatStr() + " [" + url + "]");
				}
			}

			// delete out-of-domain or empty URL
			--n;

			urls.erase(urls.begin() + n);
		}

		// sort and remove duplicates
		Helper::Strings::sortAndRemoveDuplicates(urls, this->config.crawlerUrlCaseSensitive);

		// remove URLs longer than maximum number of characters
		const auto tmpSize = urls.size();

		urls.erase(std::remove_if(urls.begin(), urls.end(),
				[&maxLength = this->config.crawlerUrlMaxLength](const auto& url) {
					return url.length() > maxLength;
				}
		), urls.end());

		if(this->config.crawlerLogging && urls.size() < tmpSize)
			this->log("WARNING: URLs longer than 2,000 Bytes ignored [" + url + "]");

		// if necessary, check for file endings and show warnings
		if(this->config.crawlerLogging && this->config.crawlerWarningsFile)
			for(auto i = urls.begin(); i != urls.end(); ++i)
				if(i->back() != '/'){
					auto lastSlash = i->rfind('/');

					if(lastSlash == std::string::npos) {
						if(i->find('.') != std::string::npos)
							this->log("WARNING: Found file \'" + *i + "\' [" + url + "]");
					}
					else if(i->find('.', lastSlash + 1) != std::string::npos)
						this->log("WARNING: Found file \'" + *i + "\' [" + url + "]");
				}

		// save status message
		std::string statusMessage = this->getStatusMessage();

		// add URLs that do not exist already in chunks of config-defined size
		unsigned long pos = 0;
		unsigned long chunkSize = 0;

		// check for infinite chunk size
		if(this->config.crawlerUrlChunks)
			chunkSize = this->config.crawlerUrlChunks;
		else
			chunkSize = urls.size();

		while(pos < urls.size() && this->isRunning()) {
			std::vector<std::string>::const_iterator begin = urls.begin() + pos;
			std::vector<std::string>::const_iterator end =
					urls.begin() + pos + std::min(chunkSize, urls.size() - pos);
			std::queue<std::string, std::deque<std::string>> chunk(std::deque<std::string>(begin, end));

			pos += this->config.crawlerUrlChunks;

			// add URLs that do not exist already
			newUrlsTo += this->database.addUrlsIfNotExist(chunk);

			// check for duplicates if URL debugging is active
			if(this->config.crawlerUrlDebug)
				this->database.urlDuplicationCheck();

			// update status
			if(urls.size() > this->config.crawlerUrlChunks) {
				std::ostringstream statusStrStr;
				statusStrStr.imbue(std::locale(""));

				statusStrStr << "[URLs: " << pos << "/" << urls.size() << "] " << statusMessage;

				this->setStatusMessage(statusStrStr.str());
			}
		}

		// reset status message
		this->setStatusMessage(statusMessage);
	}

	// crawl archives
	bool Thread::crawlingArchive(
			IdString& url,
			unsigned long& checkedUrlsTo,
			unsigned long& newUrlsTo,
			bool unlockUrl
	) {
		// check arguments
		if(!url.first)
			throw Exception("Crawler::Thread::crawlingArchive(): No URL ID specified");

		if(url.second.empty())
			throw Exception("Crawler::Thread::crawlingArchive(): No URL specified");

		if(this->config.crawlerArchives && this->networkingArchives) {
			bool success = true;
			bool skip = false;

			// write to log if necessary
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("gets archives of " + url.second + "...");

			// loop over different archives
			for(unsigned long n = 0; n < this->config.crawlerArchivesNames.size(); ++n) {
				// skip empty archive and timemap URLs
				if((this->config.crawlerArchivesUrlsMemento.at(n).empty())
						|| (this->config.crawlerArchivesUrlsTimemap.at(n).empty()))
					continue;

				std::string archivedUrl =
						this->config.crawlerArchivesUrlsTimemap.at(n) + this->domain + url.second;
				std::string archivedContent;

				// loop over memento pages
				// [while also checking whether getting mementos was successfull and thread is still running]
				while(success && this->isRunning()) {
					// get memento content
					archivedContent = "";
					try {
						this->networkingArchives->getContent(
								archivedUrl,
								false,
								archivedContent,
								this->config.crawlerRetryHttp
						);

						// check response code
						if(this->crawlingCheckResponseCode(
								archivedUrl,
								this->networkingArchives->getResponseCode()
						)) {
							// check content type
							std::string contentType = this->networkingArchives->getContentType();

							if(contentType == "application/link-format") {
								if(!archivedContent.empty()) {
									// parse memento response and get next memento page if available
									std::queue<Memento> mementos;
									std::queue<std::string> warnings;

									archivedUrl = Thread::parseMementos(archivedContent, warnings, mementos);

									if(this->config.crawlerLogging) {
										// if there are warnings, just log them (maybe mementos were partially parsed)
										while(!warnings.empty()) {
											this->log(
													"Memento parsing WARNING: " +
													warnings.front() +
													" [" + url.second + "]"
											);

											warnings.pop();
										}
									}

									// get status
									std::string statusMessage = this->getStatusMessage();

									// go through all mementos
									unsigned long counter = 0, total = mementos.size();

									while(!mementos.empty() && this->isRunning()) {
										std::string timeStamp = mementos.front().timeStamp;

										// set status
										++counter;

										std::ostringstream statusStrStr;
										statusStrStr.imbue(std::locale(""));

										statusStrStr << "[" + this->config.crawlerArchivesNames.at(n) + ": "
													 << counter << "/" << total << "] " << statusMessage;

										this->setStatusMessage(statusStrStr.str());

										// lock URL if possible
										this->lockTime = this->database.lockUrlIfOk(
												url.first,
												this->lockTime,
												this->config.crawlerLock
										);

										if(this->lockTime.empty()) {
											success = false;
											skip = true;

											break;
										}

										// loop over references / memento retries
										// [while checking whether thread is still running]
										while(this->isRunning()) {
											// check whether archived content already exists in database
											if(!(this->database.isArchivedContentExists(
													url.first,
													timeStamp)))
											{
												// check whether thread is till running
												if(!(this->isRunning()))
													break;

												// get archived content
												archivedContent = "";

												try {
													this->networkingArchives->getContent(
															mementos.front().url,
															false,
															archivedContent,
															this->config.crawlerRetryHttp
													);

													// check response code
													if(!(this->crawlingCheckResponseCode(
															mementos.front().url,
															this->networkingArchives->getResponseCode()
													)))
														break;

													// check whether thread is still running
													if(!(this->isRunning()))
														break;

													// check archived content
													if(archivedContent.substr(0, 17) == "found capture at ") {

														// found a reference string: get timestamp
														if(Helper::DateTime::convertSQLTimeStampToTimeStamp(timeStamp)) {
															unsigned long subUrlPos = mementos.front().url.find(timeStamp);

															if(subUrlPos != std::string::npos) {
																subUrlPos += timeStamp.length();
																timeStamp = archivedContent.substr(17, 14);

																// get URL and validate timestamp
																mementos.front().url =
																		this->config.crawlerArchivesUrlsMemento.at(n)
																		+ timeStamp
																		+ mementos.front().url.substr(subUrlPos);

																if(Helper::DateTime::convertTimeStampToSQLTimeStamp(timeStamp))
																	// follow reference
																	continue;

																else if(this->config.crawlerLogging)
																	// log warning (and ignore reference)
																	this->log(
																			"WARNING: Invalid timestamp"
																			" \'" + timeStamp + "\'"
																			" from " + this->config.crawlerArchivesNames.at(n) +
																			" [" + url.second + "]."
																	);
															}
															else if(this->config.crawlerLogging)
																// log warning (and ignore reference)
																this->log(
																		"WARNING: Could not find timestamp"
																		" in " + mementos.front().url +
																		" [" + url.second + "].");
														}
														else if(this->config.crawlerLogging)
															// log warning (and ignore reference)
															this->log("WARNING: Could not convert timestamp"
																	" in " + mementos.front().url +
																	" [" + url.second + "].");
													}
													else {
														// reset parsing status
														this->resetParsingState();

														// get content type
														std::string contentType = this->networkingArchives->getContentType();

														// add archived content to database
														this->database.saveArchivedContent(
																url.first,
																mementos.front().timeStamp,
																this->networkingArchives->getResponseCode(),
																contentType,
																archivedContent
														);

														// extract URLs
														std::vector<std::string> urls = this->crawlingExtractUrls(
																url.second,
																contentType,
																archivedContent
														);

														// handle parsing errors
														this->logParsingErrors(mementos.front().url);

														if(!urls.empty()) {
															// parse and add URLs
															checkedUrlsTo += urls.size();

															this->crawlingParseAndAddUrls(
																	url.second,
																	urls,
																	newUrlsTo,
																	true
															);
														}
													}
												}
												catch(const CurlException& e) {
													if(this->config.crawlerRetryArchive) {
														// error while getting content:
														//  check type of error i.e. last cURL code
														if(this->crawlingCheckCurlCode(
																this->networkingArchives->getCurlCode(),
																mementos.front().url
														)) {
															// log error
															if(this->config.crawlerLogging) {
																this->log(
																		e.whatStr() +
																		" [" + mementos.front().url + "]."
																);

																this->log(
																		"resets connection"
																		" to " + this->config.crawlerArchivesNames.at(n) +
																		"..."
																);
															}

															// reset connection
															this->setStatusMessage(
																	"ERROR " + e.whatStr() +
																	" [" + mementos.front().url + "]"
															);

															this->networkingArchives->resetConnection(
																	this->config.crawlerSleepError
															);

															// retry
															this->crawlingRetry(url, true);
															return false;
														}
													}
													// log cURL error and skip
													else if(this->config.crawlerLogging)
														this->log(
																e.whatStr() + " - skips..."
																+ " [" + mementos.front().url + "]"
														);
												}
												catch(const Utf8Exception& e) {
													// log UTF-8 error and skip
													if(this->config.crawlerLogging)
														this->log(
																"WARNING: " + e.whatStr() + " - skips..."
																+ " [" + mementos.front().url + "]"
														);
												}
											}

											// exit loop over references/memento retries
											break;

										} // [end of loop over references/memento retries]

										// check whether thread is till running
										if(!(this->isRunning()))
											break;

										// remove memento from queue
										mementos.pop();
									} // [end of loop over mementos]

									// check whether thread is till running
									if(!(this->isRunning()))
										break;

									// reset status message
									this->setStatusMessage(statusMessage);

									// check for next memento page
									if(archivedUrl.empty())
										break;
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
						if(this->crawlingCheckCurlCode(
								this->networkingArchives->getCurlCode(),
								archivedUrl
						)) {
							// reset connection and retry
							if(this->config.crawlerLogging) {
								this->log(e.whatStr() + " [" + archivedUrl + "].");

								this->log(
										"resets connection"
										" to " + this->config.crawlerArchivesNames.at(n) + "..."
								);
							}

							this->setStatusMessage("ERROR " + e.whatStr() + " [" + archivedUrl + "]");

							this->networkingArchives->resetConnection(this->config.crawlerSleepError);

							success = false;
						}
					}
					catch(const Utf8Exception& e) {
						if(this->config.crawlerLogging)
							this->log("WARNING: " + e.whatStr() + " [" + archivedUrl + "]");

						success = false;
						skip = true;
					}

					if(!success && this->config.crawlerRetryArchive) {
						if(skip)
							this->crawlingSkip(url, unlockUrl);
						else
							this->crawlingRetry(url, true);

						return false;
					}
					else if(!success)
						this->crawlingSkip(url, unlockUrl);
				} // [end of loop over memento pages]

			} // [end of loop over archives]

			if(success || !(this->config.crawlerRetryArchive))
				this->archiveRetry = false;
		}

		return this->isRunning();
	}

	// crawling successfull
	void Thread::crawlingSuccess(const IdString& url) {
		// check argument
		if(!url.first)
			throw Exception("Crawler::Thread::crawlingSkip(): No URL ID specified");

		if(url.second.empty())
			throw Exception("Crawler::Thread::crawlingSkip(): No URL specified");

		// set URL to finished if URL lock is okay
		try {
			this->database.setUrlFinishedIfOk(url.first, this->lockTime);
		}
		catch(Main::Exception& e) {
			e.append("[" + url.second + "]");
			throw e;
		}

		// reset lock time
		this->lockTime = "";

		if(this->manualUrl.first) {
			// manual mode: disable retry, check for custom URL or start page that has been crawled
			this->manualUrl = IdString();

			if(this->manualCounter < this->customPages.size())
				++(this->manualCounter);
			else
				this->startCrawled = true;
		}
		else {
			// automatic mode: update thread status
			this->setLast(url.first);

			unsigned long total = this->database.getNumberOfUrls();

			if(total)
				this->setProgress(
						static_cast<float>(this->database.getUrlPosition(url.first) + 1)
						/ total
				);
			else
				this->setProgress(1.f);
		}

		// reset retry counter
		this->retryCounter = 0;

		// do not retry (only archive if necessary)
		this->nextUrl = IdString();
	}

	// skip URL after crawling problem
	void Thread::crawlingSkip(const IdString& url, bool unlockUrl) {
		// check argument
		if(!url.first)
			throw Exception("Crawler::Thread::crawlingSkip(): No URL ID specified");

		if(url.second.empty())
			throw Exception("Crawler::Thread::crawlingSkip(): No URL specified");

		// reset retry counter
		this->retryCounter = 0;

		if(this->manualUrl.first) {
			// manual mode: disable retry, check for custom URL or start page that has been crawled
			this->manualUrl = IdString();

			if(this->manualCounter < this->customPages.size())
				++(this->manualCounter);
			else
				this->startCrawled = true;
		}
		else {
			// automatic mode: update thread status
			this->setLast(url.first);

			this->setProgress(
					static_cast<float>(this->database.getUrlPosition(url.first) + 1)
					/ this->database.getNumberOfUrls()
			);
		}

		if(unlockUrl) {
			// unlock URL if it has not been locked by anyone else
			this->database.unLockUrlIfOk(url.first, this->lockTime);
			this->lockTime = "";
		}

		// do not retry
		this->nextUrl = IdString();
		this->archiveRetry = false;
	}

	// retry URL (completely or achives-only) after crawling problem
	//  NOTE: leaves the URL lock active for retry
	void Thread::crawlingRetry(const IdString& url, bool archiveOnly) {
		// check argument
		if(!url.first)
			throw Exception("Crawler::Thread::crawlingRetry(): No URL ID specified");

		if(url.second.empty())
			throw Exception("Crawler::Thread::crawlingRetry(): No URL specified");

		if(this->config.crawlerReTries > -1) {
			// increment and check retry counter
			++(this->retryCounter);

			if(this->retryCounter > (unsigned long) this->config.crawlerReTries) {
				// do not retry, but skip
				this->crawlingSkip(url, true);

				return;
			}
		}

		if(archiveOnly)
			this->archiveRetry = true;
	}

	// parse XML/HTML if still necessary, return false if parsing failed
	bool Thread::parseXml(const std::string& content) {
		if(!(this->xmlParsed) && this->xmlParsingError.empty()) {
			try {
				this->parsedXML.parse(content, this->config.crawlerRepairCData);

				this->xmlParsed = true;
			}
			catch(const XMLException& e) {
				this->xmlParsingError = e.whatStr();
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
	void Thread::logParsingErrors(const std::string& url) {
		if(!(this->xmlParsingError.empty())) {
			if(this->config.crawlerLogging)
				this->log("WARNING: " + this->xmlParsingError + " [" + url + "].");

			this->xmlParsingError.clear();
		}

		if(!(this->jsonParsingError.empty())) {
			if(this->config.crawlerLogging)
				this->log("WARNING: " + this->jsonParsingError + " [" + url + "].");

			this->jsonParsingError.clear();
		}
	}

	// parse memento reply, get mementos and link to next page if one exists
	//  (also convert timestamps to YYYYMMDD HH:MM:SS)
	std::string Thread::parseMementos(
			std::string mementoContent,
			std::queue<std::string>& warningsTo,
			std::queue<Memento>& mementosTo
	) {
		Memento newMemento;				// object for new memento
		bool mementoStarted = false;	// memento has been started
		std::string nextPage;			// link to next page
		unsigned long pos = 0;			// position of memento
		unsigned long end = 0;			// end of memento
		bool newField = true;			// not in the middle of a field

		while(pos < mementoContent.length()) {
			// skip wildchars
			if(
					mementoContent.at(pos) == ' '
					|| mementoContent.at(pos) == '\r'
					|| mementoContent.at(pos) == '\n'
					|| mementoContent.at(pos) == '\t'
			) {
				++pos;

				continue;
			}

			if(mementoContent.at(pos) == '<') {
				// parse link
				end = mementoContent.find('>', pos + 1);

				if(end == std::string::npos) {
					std::ostringstream warningStrStr;

					warningStrStr << "No '>' after '<' for link at " << pos << ".";
					warningsTo.emplace(warningStrStr.str());

					break;
				}

				if(mementoStarted) {
					// memento not finished -> warning
					if(!newMemento.url.empty() && !newMemento.timeStamp.empty())
						mementosTo.emplace(newMemento);

					std::ostringstream warningStrStr;

					warningStrStr <<	"New memento started without finishing the old one"
										" at " << pos << ".";

					warningsTo.emplace(warningStrStr.str());
				}

				mementoStarted = true;

				newMemento.url = mementoContent.substr(pos + 1, end - pos - 1);
				newMemento.timeStamp = "";

				pos = end + 1;
			}
			else if(mementoContent.at(pos) == ';') {
				// parse field separator
				newField = true;

				++pos;
			}
			else if(mementoContent.at(pos) == ',') {
				// parse end of memento
				if(mementoStarted) {
					if(!newMemento.url.empty() && !newMemento.timeStamp.empty())
						mementosTo.emplace(newMemento);

					mementoStarted  = false;
				}

				++pos;
			}
			else {
				if(newField)
					newField = false;
				else {
					std::ostringstream warningStrStr;

					warningStrStr << 	"Field seperator missing for new field"
										" at " << pos << ".";

					warningsTo.emplace(warningStrStr.str());
				}

				end = mementoContent.find('=', pos + 1);

				if(end == std::string::npos) {
					end = mementoContent.find_first_of(",;");

					if(end == std::string::npos) {
						std::ostringstream warningStrStr;

						warningStrStr <<	"Cannot find end of field"
											" at " << pos << ".";

						warningsTo.emplace(warningStrStr.str());

						break;
					}

					pos = end;
				}
				else {
					std::string fieldName = mementoContent.substr(pos, end - pos);
					unsigned long oldPos = pos;

					pos = mementoContent.find_first_of("\"\'", pos + 1);

					if(pos == std::string::npos) {
						std::ostringstream warningStrStr;

						warningStrStr <<	"Cannot find begin of value"
											" at " << oldPos << ".";

						warningsTo.emplace(warningStrStr.str());

						++pos;

						continue;
					}

					end = mementoContent.find_first_of("\"\'", pos + 1);

					if(end == std::string::npos) {
						std::ostringstream warningStrStr;

						warningStrStr <<	"Cannot find end of value"
											" at " << pos << ".";

						warningsTo.emplace(warningStrStr.str());

						break;
					}

					std::string fieldValue = mementoContent.substr(pos + 1, end - pos - 1);

					if(fieldName == "datetime") {
						// parse timestamp
						if(Helper::DateTime::convertLongDateTimeToSQLTimeStamp(fieldValue))
							newMemento.timeStamp = fieldValue;
						else {
							std::ostringstream warningStrStr;

							warningStrStr <<	"Could not convert timestamp \'" << fieldValue << "\'"
												"  at " << pos << ".";

							warningsTo.emplace(warningStrStr.str());
						}
					}
					else if(fieldName == "rel") {
						// parse link to next page
						if(fieldValue == "timemap" && !newMemento.url.empty()) {
							nextPage = newMemento.url;
							newMemento.url = "";
						}
					}

					pos = end + 1;
				}
			}
		}

		// finish final memento
		if(
				mementoStarted
				&& !newMemento.url.empty()
				&& !newMemento.timeStamp.empty()
		)
			mementosTo.emplace(newMemento);

		// return next page
		return nextPage;
	}

} /* crawlservpp::Module::Crawler */
