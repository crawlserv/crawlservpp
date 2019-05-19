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
			const std::string& cookieDirectory,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	)
				: Module::Thread(
						dbBase,
						threadOptions,
						threadStatus
				  ),
				  database(this->Module::Thread::database),
				  networking(cookieDirectory),
				  cookieDir(cookieDirectory),
				  manualCounter(0),
				  startCrawled(false),
				  manualOff(false),
				  retryCounter(0),
				  archiveRetry(false),
				  tickCounter(0),
				  startTime(std::chrono::steady_clock::time_point::min()),
				  pauseTime(std::chrono::steady_clock::time_point::min()),
				  idleTime(std::chrono::steady_clock::time_point::min()),
				  httpTime(std::chrono::steady_clock::time_point::min()) {}

	// constructor B: start a new crawler
	Thread::Thread(
			Main::Database& dbBase,
			const std::string& cookieDirectory,
			const ThreadOptions& threadOptions)
				: Module::Thread(
						dbBase,
						threadOptions
				),
				  database(this->Module::Thread::database),
				  networking(cookieDirectory),
				  cookieDir(cookieDirectory),
				  manualCounter(0),
				  startCrawled(false),
				  manualOff(false),
				  retryCounter(0),
				  archiveRetry(false),
				  tickCounter(0),
				  startTime(std::chrono::steady_clock::time_point::min()),
				  pauseTime(std::chrono::steady_clock::time_point::min()),
				  idleTime(std::chrono::steady_clock::time_point::min()),
				  httpTime(std::chrono::steady_clock::time_point::min()) {}

	// destructor stub
	Thread::~Thread() {}

	// initialize crawler, throws Thread::Exception
	void Thread::onInit() {
		std::queue<std::string> configWarnings;

		// load configuration
		this->setStatusMessage("Loading configuration...");

		this->setCrossDomain(this->database.getWebsiteDomain(this->getWebsite()).empty());

		this->loadConfig(this->database.getConfiguration(this->getConfig()), configWarnings);

		// show warnings if necessary
		while(!configWarnings.empty()) {
			this->log("WARNING: " + configWarnings.front());

			configWarnings.pop();
		}

		// check required query
		if(this->config.crawlerQueriesLinks.empty())
			throw Exception("Crawler::Thread::onInit(): No link extraction query specified");

		// set query container options
		this->setRepairCData(this->config.crawlerRepairCData);
		this->setTidyErrorsAndWarnings(this->config.crawlerTidyErrors, this->config.crawlerTidyWarnings);

		// set database options
		this->setStatusMessage("Setting database options...");

		const bool verbose = this->config.crawlerLogging == Config::crawlerLoggingVerbose;

		if(verbose)
			this->log("sets database options...");

		this->database.setLogging(this->config.crawlerLogging, verbose);
		this->database.setRecrawl(this->config.crawlerReCrawl);
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

			if(verbose)
				this->log("waits for URL list...");

			DatabaseLock urlListLock(
					this->database,
					"urlList." + this->websiteNamespace + "_" + this->urlListNamespace,
					std::bind(&Thread::isRunning, this)
			);

			if(!(this->isRunning()))
				return;

			// check URL list
			this->setStatusMessage("Checking URL list...");

			if(verbose)
				this->log("checks URL list...");

			// check hashs of URLs
			this->database.urlHashCheck();

			// optional startup checks
			if(this->config.crawlerUrlStartupCheck) {
				this->database.urlDuplicationCheck();
				this->database.urlEmptyCheck();
				this->database.urlHashCheck();
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
			this->log("sets network configuration...");

		this->networking.setConfigGlobal(*this, false, configWarnings);

		while(!configWarnings.empty()) {
			this->log("WARNING: " + configWarnings.front());
			configWarnings.pop();
		}

		// initialize custom URLs
		this->setStatusMessage("Generating custom URLs...");

		if(verbose)
			this->log("generates custom URLs...");

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

			this->networkingArchives = std::make_unique<Network::Curl>(this->cookieDir);

			this->networkingArchives->setConfigGlobal(*this, true, configWarnings);

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

		std::string customCookies;
		std::vector<std::string> customHeaders;
		std::string timerString;

		unsigned long checkedUrls = 0;
		unsigned long newUrls = 0;
		unsigned long checkedUrlsArchive = 0;
		unsigned long newUrlsArchive = 0;

		bool usePost = false;

		// check for jump in last ID ("time travel")
		const long warpedOver = this->getWarpedOverAndReset();

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

			// dynamic redirect on URL if necessary
			this->crawlingDynamicRedirectUrl(url.second, customCookies, customHeaders, usePost);

			// add parameters to URL if necessary
			this->crawlingUrlParams(url.second);

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
			const bool crawled = this->crawlingContent(
					url,
					customCookies,
					customHeaders,
					usePost,
					checkedUrls,
					newUrls,
					timerString
			);

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

			const long double tps =
					static_cast<long double>(this->tickCounter)
					/ std::chrono::duration_cast<std::chrono::seconds>(
							std::chrono::steady_clock::now()
							- this->startTime
					).count();

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
		throw std::logic_error("Thread::start() not to be used by thread itself");
	}
	void Thread::unpause() {
		throw std::logic_error("Thread::unpause() not to be used by thread itself");
	}
	void Thread::stop() {
		throw std::logic_error("Thread::stop() not to be used by thread itself");
	}
	void Thread::interrupt() {
		throw std::logic_error("Thread::interrupt() not to be used by thread itself");
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
					const std::vector<std::string> temp(
							this->initDoLocalCounting(
									this->config.customUrls.at(n),
									this->config.customCounters.at(n),
									this->config.customCountersAlias.at(n),
									this->config.customCountersStart.at(n),
									this->config.customCountersEnd.at(n),
									this->config.customCountersStep.at(n),
									this->config.customCountersAliasAdd.at(n)
							)
					);

					newUrls.reserve(newUrls.size() + temp.size());

					newUrls.insert(newUrls.end(), temp.begin(), temp.end());
				}
			}

			this->customPages.reserve(newUrls.size());

			for(const auto& newUrl : newUrls)
				this->customPages.emplace_back(0, newUrl);
		}
		else {
			// no counters: add all custom URLs as is
			this->customPages.reserve(this->config.customUrls.size());

			for(const auto& url : this->config.customUrls)
				this->customPages.emplace_back(0, url);
		}

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

		// check whether to extract URLs from 'robots.txt'
		if(this->config.customRobots)
			// add sitemap(s) from 'robots.txt' as custom URLs
			this->initRobotsTxt();

		// get IDs and lock IDs for custom URLs (and add them to the URL list if necessary)
		for(auto& customPage : this->customPages) {
			try {
				// check URI
				this->parser->setCurrentUrl(customPage.second);

				// add URL (if it does not exist)
				this->database.addUrlIfNotExists(customPage.second, true);

				// check for duplicates if URL debugging is active
				if(this->config.crawlerUrlDebug)
					this->database.urlDuplicationCheck();

				// get the ID of the custom URL (and of its URL lock if one already exists)
				customPage.first = this->database.getUrlId(customPage.second);
			}
			catch(const URIException& e) {
				if(this->config.crawlerLogging) {
					this->log("URI Parser error: " + e.whatStr());
					this->log(" skipped invalid custom URL " + customPage.second);
				}
			}
		}
	}

	// add sitemap(s) from 'robots.txt' as custom URLs
	void Thread::initRobotsTxt() {
		// check for cross-domain website
		if(this->domain.empty()) {
			if(this->config.crawlerLogging)
				this->log("WARNING: Cannot get \'robots.txt\' for cross-domain website.");

			return;
		}

		// get content for extracting sitemap(s)
		std::string content;
		const std::string url("https://" + this->domain + "/robots.txt");
		bool success = false;

		if(this->config.crawlerLogging == Config::crawlerLoggingVerbose)
			this->log("fetches \'robots.txt\'...");

		// get robots.txt
		while(this->isRunning()) {
			try {
				this->networking.getContent(
						url,
						false,
						content,
						this->config.crawlerRetryHttp
				);

				success = this->crawlingCheckResponseCode(url, this->networking.getResponseCode());

				break;
			}
			catch(const CurlException& e) {
				// error while getting content: check type of error i.e. last cURL code
				if(this->crawlingCheckCurlCode(
						this->networking.getCurlCode(),
						url
				)) {
					// reset connection and retry
					if(this->config.crawlerLogging) {
						this->log(e.whatStr() + " [" + url + "].");
						this->log("resets connection...");
					}

					this->setStatusMessage("ERROR " + e.whatStr() + " [" + url + "]");

					this->networking.resetConnection(this->config.crawlerSleepError);
				}
				else {
					if(this->config.crawlerLogging)
						this->log("WARNING: " + e.whatStr() + " [" + url + "]");

					break;
				}
			}
			catch(const Utf8Exception& e) {
				// write UTF-8 error to log if neccessary
				if(this->config.crawlerLogging)
					this->log("WARNING: " + e.whatStr() + " [" + url + "].");

				break;
			}
		}

		if(!success)
			return;

		std::istringstream in(content);
		std::string line;

		// go through all lines in 'robots.txt'
		while(std::getline(in, line)) {
			// check length of line
			if(line.size() < 9)
				continue;

			// convert first 7 characters to lower case
			std::transform(line.begin(), line.begin() + 7, line.begin(), ::tolower);

			// check for sitemap
			if(line.substr(0, 8) == "sitemap:") {
				// get sitemap
				std::string sitemap(line.substr(8));

				// trim sitemap (removing optional space at the beginning)
				Helper::Strings::trim(sitemap);

				// check length of sitemap
				if(sitemap.empty())
					continue;

				// parse sitemap URL to sub-URL of domain
				try {
					Parsing::URI uriParser;

					uriParser.setCurrentDomain(this->domain);
					uriParser.setCurrentUrl("/robots.txt");

					uriParser.parseLink(sitemap);

					if(!uriParser.isSameDomain()) {
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: Cross-domain sitemaps not supported ["
									+ sitemap
									+ "]."
							);

						continue;
					}

					sitemap = uriParser.getSubUrl();
				}
				catch(const URIException& e) {
					if(this->config.crawlerLogging)
						this->log(
								"WARNING: URI parser error: "
								+ e.whatStr()
								+ "["
								+ sitemap
								+ "]"
						);

					continue;
				}

				// add sitemap to custom URLs if it does not exist yet
				if(
						std::find_if(
								this->customPages.begin(),
								this->customPages.end(),
								[&sitemap](const auto& val) {
									return val.second == sitemap;
								}
						) == this->customPages.end()
				) {
					this->customPages.emplace_back(0, sitemap);

					if(this->config.crawlerLogging)
						this->log("fetched sitemap \"" + sitemap + "\" from 'robots.txt'.");
				}
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

		for(const auto& url : urlList) {
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
					std::string newUrl(url);

					Helper::Strings::replaceAll(newUrl, variable, std::to_string(counter), true);

					if(!alias.empty())
						Helper::Strings::replaceAll(newUrl, alias, std::to_string(counter + aliasAdd), true);

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
				newUrlList.emplace_back(url); // variable not in URL
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
				std::string newUrl(url);

				Helper::Strings::replaceAll(newUrl, variable, std::to_string(counter), true);

				if(!alias.empty())
					Helper::Strings::replaceAll(newUrl, alias, std::to_string(counter + aliasAdd), true);

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

	// initialize queries, throws Thread::Exception
	void Thread::initQueries() {
		// reserve memory for queries
		this->queriesBlackListContent.reserve(this->config.crawlerQueriesBlackListContent.size());
		this->queriesBlackListTypes.reserve(this->config.crawlerQueriesBlackListTypes.size());
		this->queriesBlackListUrls.reserve(this->config.crawlerQueriesBlackListUrls.size());
		this->queriesLinks.reserve(this->config.crawlerQueriesLinks.size());
		this->queriesLinksBlackListContent.reserve(this->config.crawlerQueriesLinksBlackListContent.size());
		this->queriesLinksBlackListTypes.reserve(this->config.crawlerQueriesLinksBlackListTypes.size());
		this->queriesLinksBlackListUrls.reserve(this->config.crawlerQueriesLinksBlackListUrls.size());
		this->queriesLinksWhiteListContent.reserve(this->config.crawlerQueriesLinksWhiteListContent.size());
		this->queriesLinksWhiteListTypes.reserve(this->config.crawlerQueriesLinksWhiteListTypes.size());
		this->queriesLinksWhiteListUrls.reserve(this->config.crawlerQueriesLinksWhiteListUrls.size());
		this->queriesWhiteListContent.reserve(this->config.crawlerQueriesWhiteListContent.size());
		this->queriesWhiteListTypes.reserve(this->config.crawlerQueriesWhiteListTypes.size());
		this->queriesWhiteListUrls.reserve(this->config.crawlerQueriesWhiteListUrls.size());
		this->queriesTokens.reserve(this->config.customTokensQuery.size());
		this->queriesRedirectVars.reserve(this->config.redirectVarQueries.size());

		try {
			// create queries and get query properties
			for(const auto& query : this->config.crawlerQueriesBlackListContent) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesBlackListContent.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesBlackListTypes) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesBlackListTypes.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesBlackListUrls) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesBlackListUrls.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesLinks) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesLinks.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesLinksBlackListContent) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesLinksBlackListContent.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesLinksBlackListTypes) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesLinksBlackListTypes.emplace_back(this->addQuery(properties));
				}

			}

			for(const auto& query : this->config.crawlerQueriesLinksBlackListUrls) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesLinksBlackListUrls.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesLinksWhiteListContent) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesLinksWhiteListContent.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesLinksWhiteListTypes) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesLinksWhiteListTypes.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesLinksWhiteListUrls) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesLinksWhiteListUrls.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesWhiteListContent) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesWhiteListContent.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesWhiteListTypes) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesWhiteListTypes.emplace_back(this->addQuery(properties));
				}
			}

			for(const auto& query : this->config.crawlerQueriesWhiteListUrls) {
				if(query) {
					QueryProperties properties;

					this->database.getQueryProperties(query, properties);

					this->queriesWhiteListUrls.emplace_back(this->addQuery(properties));
				}
			}

			for(
					auto i = this->config.customTokensQuery.begin();
					i != this->config.customTokensQuery.end();
					++i
			) {
				QueryProperties properties;

				if(*i)
					this->database.getQueryProperties(*i, properties);
				else if(this->config.crawlerLogging) {
					const auto index = i - this->config.customTokensQuery.begin();

					if(!(this->config.customTokens.at(index).empty()))
						this->log(
								"WARNING: Ignores token \'"
								+ this->config.customTokens.at(index)
								+ "\' because of missing query."
						);
				}

				this->queriesTokens.emplace_back(this->addQuery(properties));
			}

			if(this->config.redirectQueryContent) {
				QueryProperties properties;

				this->database.getQueryProperties(this->config.redirectQueryContent, properties);

				this->queryRedirectContent = this->addQuery(properties);
			}

			if(this->config.redirectQueryUrl) {
				QueryProperties properties;

				this->database.getQueryProperties(this->config.redirectQueryUrl, properties);

				this->queryRedirectUrl = this->addQuery(properties);
			}

			for(
					auto i = this->config.redirectVarQueries.begin();
					i != this->config.redirectVarQueries.end();
					++i
			) {
				QueryProperties properties;

				if(*i)
					this->database.getQueryProperties(*i, properties);
				else if(this->config.crawlerLogging) {
					const auto index = i - this->config.redirectVarQueries.begin();

					if(!(this->config.redirectVarNames.at(index).empty()))
						this->log(
								"WARNING: Ignores variable \'"
								+ this->config.redirectVarNames.at(index)
								+ " because of missing query."
						);
				}

				this->queriesRedirectVars.emplace_back(this->addQuery(properties));
			}

			if(this->config.expectedQuery) {
				QueryProperties properties;

				this->database.getQueryProperties(this->config.expectedQuery, properties);

				this->queryExpected = this->addQuery(properties);
			}
		}
		catch(const QueryException & e) {
			throw Exception("Crawler::Thread::initQueries(): " + e.whatStr());
		}
	}

	// URL selection (includes locking the URL), return whether there are any URLs to crawl left
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

		for(auto i = this->config.customTokens.begin(); i != this->config.customTokens.end(); ++i) {
			const auto index = i - this->config.customTokens.begin();

			if(result.second.find(*i)) {
				const std::string sourceUrl("https://" + this->config.customTokensSource.at(index));
				std::string content;
				std::string value;
				bool success = false;

				// check token source
				if(!(this->config.customTokensSource.at(index).empty())) {
					// get content for extracting token
					while(this->isRunning()) {
						try {
							// set local network configuration
							this->networking.setConfigCurrent(*this);

							// set custom headers if necessary
							if(!(this->config.customTokensCookies.at(index).empty()))
								this->networking.setCookies(this->config.customTokensCookies.at(index));

							if(!(this->config.customTokenHeaders.empty()))
								this->networking.setHeaders(this->config.customTokenHeaders);

							// get content
							this->networking.getContent(
									sourceUrl,
									this->config.customTokensUsePost.at(index),
									content,
									this->config.crawlerRetryHttp
							);

							// unset custom headers if necessary
							if(!(this->config.customTokensCookies.at(index).empty()))
								this->networking.unsetCookies();

							if(!(this->config.customTokenHeaders.empty()))
								this->networking.unsetHeaders();

							success = true;

							break;
						}
						catch(const CurlException& e) { // error while getting content
							// unset custom headers if necessary
							if(!(this->config.customTokensCookies.at(index).empty()))
								this->networking.unsetCookies();

							if(!(this->config.customTokenHeaders.empty()))
								this->networking.unsetHeaders();

							// check type of error i.e. last cURL code
							if(this->crawlingCheckCurlCode(
									this->networking.getCurlCode(),
									sourceUrl
							)) {
								// reset connection and retry
								if(this->config.crawlerLogging) {
									this->log(e.whatStr() + " [" + sourceUrl + "].");
									this->log("resets connection...");
								}

								this->setStatusMessage("ERROR " + e.whatStr() + " [" + sourceUrl + "]");

								this->networking.resetConnection(this->config.crawlerSleepError);
							}
							else {
								if(this->config.crawlerLogging)
									this->log(
											"WARNING: Could not get token \'"
											+ *i
											+ "\' from "
											+ sourceUrl
											+ ": "
											+ e.whatStr()
									);

								break;
							}
						}
						catch(const Utf8Exception& e) {
							// unset custom headers if necessary
							if(!(this->config.customTokensCookies.at(index).empty()))
								this->networking.unsetCookies();

							if(!(this->config.customTokenHeaders.empty()))
								this->networking.unsetHeaders();

							// write UTF-8 error to log if neccessary
							if(this->config.crawlerLogging)
								this->log("WARNING: " + e.whatStr() + " [" + sourceUrl + "].");

							break;
						}
					}
				}

				if(success) {
					std::queue<std::string> queryWarnings;

					// set content as target for subsequent query
					this->setQueryTarget(content, sourceUrl);

					// get token from content
					if(this->queriesTokens.at(index).resultSingle)
						this->getSingleFromQuery(this->queriesTokens.at(index), value, queryWarnings);
					else if(this->queriesTokens.at(index).resultBool) {
						bool booleanResult = false;

						if(this->getBoolFromQuery(this->queriesTokens.at(index), booleanResult, queryWarnings))
							value = booleanResult ? "true" : "false";
					}
					else
						queryWarnings.emplace(
								"WARNING: Invalid result type of query for token \'"
								+ *i
								+ "\' - not single and not bool."
						);

					// logging if necessary
					this->logWarnings(queryWarnings);

					if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
						this->log(
								"Fetched token \'"
								+ *i
								+ "\' from "
								+ sourceUrl
								+ " [= \'"
								+ value
								+ "\']."
						);
				}

				// replace variable(s) with token(s)
				Helper::Strings::replaceAll(result.second, *i, value, true);
			}
		}

		return result;
	}

	// add custom parameters to URL
	void Thread::crawlingUrlParams(std::string& url) {
		if(!(this->config.crawlerParamsAdd.empty())) {
			bool addQuestionMark = url.find('?') == std::string::npos;

			for(const auto& paramToAdd : this->config.crawlerParamsAdd) {
				if(addQuestionMark) {
					url.push_back('?');

					addQuestionMark = false;
				}
				else
					url.push_back('&');

				url += paramToAdd;
			}
		}
	}

	// crawl content, throws Thread::Exception
	bool Thread::crawlingContent(
			IdString& url,
			const std::string& customCookies,
			const std::vector<std::string>& customHeaders,
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
			const unsigned long httpElapsed =
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

		// start HTTP timer(s)
		if(this->config.crawlerTiming)
			httpTimer.start();

		if(this->config.crawlerSleepHttp)
			this->httpTime = std::chrono::steady_clock::now();

		try {
			// set local networking options
			this->networking.setConfigCurrent(*this);

			// set custom headers if necessary
			if(!customCookies.empty())
				this->networking.setCookies(customCookies);

			if(!customHeaders.empty())
				this->networking.setHeaders(customHeaders);

			// get content
			this->networking.getContent(
					"https://" + this->domain + url.second,
					usePost,
					content,
					this->config.crawlerRetryHttp
			);

			// unset custom headers if necessary
			if(!customCookies.empty())
				this->networking.unsetCookies();

			if(!customHeaders.empty())
				this->networking.unsetHeaders();
		}
		catch(const CurlException& e) { // error while getting content
			// unset custom headers if necessary
			if(!customCookies.empty())
				this->networking.unsetCookies();

			if(!customHeaders.empty())
				this->networking.unsetHeaders();

			// check type of error i.e. last cURL code
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
			// unset custom headers if necessary
			if(!customCookies.empty())
				this->networking.unsetCookies();

			if(!customHeaders.empty())
				this->networking.unsetHeaders();

			// write UTF-8 error to log if neccessary
			if(this->config.crawlerLogging)
				this->log("WARNING: " + e.whatStr() + " [" + url.second + "].");

			// skip URL
			this->crawlingSkip(url, !(this->config.crawlerArchives));
		}

		// check HTTP response code
		const unsigned int responseCode = this->networking.getResponseCode();

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

		// set content as target for subsequent queries
		this->setQueryTarget(content, url.second);

		// perform dynamic redirect if necessary
		if(!(this->crawlingDynamicRedirectContent(url.second, content))) {
			// skip because dynamic redirect failed
			this->crawlingSkip(url, !(this->config.crawlerArchives));

			return false;
		}

		// check content type
		const std::string contentType(this->networking.getContentType());

		if(!(this->crawlingCheckContentType(url.second, contentType))) {
			// skip because of content type (on blacklist or not on whitelist)
			this->crawlingSkip(url, !(this->config.crawlerArchives));

			return false;
		}

		// check content
		if(!(this->crawlingCheckContent(url.second))) {
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
		std::vector<std::string> urls(this->crawlingExtractUrls(url.second, contentType));

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

	// check URL for dynamic redirect and perform it if necessary
	void Thread::crawlingDynamicRedirectUrl(
			std::string& url,
			std::string& customCookies,
			std::vector<std::string>& customHeaders,
			bool& usePost
	) {
		// determine whether to redirect
		if(!(this->config.redirectQueryUrl))
			return;

		bool redirect = false;
		std::queue<std::string> queryWarnings;

		this->getBoolFromRegEx(this->queryRedirectUrl, url, redirect, queryWarnings);

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(!redirect)
			return;

		// preserve old URL for queries
		std::string oldUrl(url);

		// set new URL and whether to use HTTP POST
		url = this->config.redirectTo;
		usePost = this->config.redirectUsePost;

		// handle variables in new URL
		this->crawlingDynamicRedirectUrlVars(oldUrl, url);

		// set new custom cookies header if necessary
		if(!(this->config.redirectCookies.empty())) {
			customCookies = this->config.redirectCookies;

			// handle variables in new custom cookies header
			this->crawlingDynamicRedirectUrlVars(oldUrl, customCookies);
		}

		// set new custom headers if necessary
		customHeaders.reserve(this->config.redirectHeaders.size());

		for(const auto& header : this->config.redirectHeaders) {
			customHeaders.push_back(header);

			// handle variables in new custom header
			this->crawlingDynamicRedirectUrlVars(oldUrl, customHeaders.back());
		}

		// write to log if necessary
		if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("performs dynamic redirect: " + oldUrl + " -> " + url);
	}

	// resolve variables in string (i.e. URL or custom cookies/headers) for dynamic redirect by URL
	void Thread::crawlingDynamicRedirectUrlVars(const std::string& oldUrl, std::string& strInOut) {
		for(auto i = this->config.redirectVarNames.begin(); i != this->config.redirectVarNames.end(); ++i) {
			if(strInOut.find(*i) == std::string::npos)
				continue;

			const auto index = i - this->config.redirectVarNames.begin();

			std::queue<std::string> queryWarnings;
			std::string value;

			// check source type
			if(this->config.redirectVarSources.at(index) != Config::redirectSourceUrl) {
				if(this->config.crawlerLogging)
					this->log(
							"WARNING: Invalid source type for variable \'"
							+ *i
							+ "\' for dynamic redirect - set to empty."
					);
			}
			// check result type
			else if(this->queriesRedirectVars.at(index).resultSingle)
				this->getSingleFromRegEx(this->queriesRedirectVars.at(index), oldUrl, value, queryWarnings);
			else if(this->queriesRedirectVars.at(index).resultBool) {
				bool booleanResult = false;

				if(this->getBoolFromRegEx(this->queriesRedirectVars.at(index), oldUrl, booleanResult, queryWarnings))
					value = booleanResult ? "true" : "false";
			}
			else if(this->config.crawlerLogging)
				this->log(
						"WARNING: Could not get value of variable \'"
						+ *i
						+ "\' for dynamic redirect - set to empty."
				);

			// log warnings if necessary
			this->logWarnings(queryWarnings);

			// replace variable in string
			Helper::Strings::replaceAll(strInOut, *i, value, true);
		}
	}

	// check content for dynamic redirect and perform it if necessary, throws Thread::Exception
	bool Thread::crawlingDynamicRedirectContent(std::string& url, std::string& content) {
		// check whether dynamic redirect (determined by content) is enabled
		if(!(this->config.redirectQueryContent))
			return true;

		// check arguments
		if(url.empty())
			throw Exception("Thread::crawlingDynamicRedirectContent(): No URL specified");

		// determine whether to redirect to new URL
		std::queue<std::string> queryWarnings;
		bool booleanResult = false;

		this->getBoolFromQuery(this->queryRedirectContent, booleanResult, queryWarnings);

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		// check whether no redirect is necessary
		if(!booleanResult)
			return true;

		// preserve old URL for queries
		const std::string oldUrl(url);

		// get new URL
		url = this->config.redirectTo;

		// resolve variables in new URL
		this->crawlingDynamicRedirectContentVars(oldUrl, url);

		// write to log if necessary
		if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("performed dynamic redirect: " + oldUrl + " -> " + url);

		// get custom headers
		std::string customCookies(this->config.redirectCookies);
		std::vector<std::string> customHeaders(this->config.redirectHeaders);

		// resolve variables in custom headers
		this->crawlingDynamicRedirectContentVars(oldUrl, customCookies);

		for(auto& header : customHeaders)
			this->crawlingDynamicRedirectContentVars(oldUrl, header);

		// get new content
		bool success = false;

		while(this->isRunning()) {
			try {
				// set current network configuration
				this->networking.setConfigCurrent(*this);

				// set custom headers if necessary
				if(!customCookies.empty())
					this->networking.setCookies(customCookies);

				if(!customHeaders.empty())
					this->networking.setHeaders(customHeaders);

				// get content
				this->networking.getContent(
						"https://" + this->domain + url,
						this->config.redirectUsePost,
						content,
						this->config.crawlerRetryHttp
				);

				// unset custom headers if necessary
				if(!customCookies.empty())
					this->networking.unsetCookies();

				if(!customHeaders.empty())
					this->networking.unsetHeaders();

				// get HTTP response code
				success = this->crawlingCheckResponseCode(url, this->networking.getResponseCode());

				break;
			}
			catch(const CurlException& e) { // error while getting content
				// unset custom headers if necessary
				if(!customCookies.empty())
					this->networking.unsetCookies();

				if(!customHeaders.empty())
					this->networking.unsetHeaders();

				// check type of error i.e. last cURL code
				if(this->crawlingCheckCurlCode(
						this->networking.getCurlCode(),
						url
				)) {
					// reset connection and retry
					if(this->config.crawlerLogging) {
						this->log(e.whatStr() + " [" + url + "].");
						this->log("resets connection...");
					}

					this->setStatusMessage("ERROR " + e.whatStr() + " [" + url + "]");

					this->networking.resetConnection(this->config.crawlerSleepError);
				}
				else {
					if(this->config.crawlerLogging)
						this->log("WARNING: " + e.whatStr() + " [" + url + "]");

					break;
				}
			}
			catch(const Utf8Exception& e) {
				// unset custom headers if necessary
				if(!customCookies.empty())
					this->networking.unsetCookies();

				if(!customHeaders.empty())
					this->networking.unsetHeaders();

				// write UTF-8 error to log if neccessary
				if(this->config.crawlerLogging)
					this->log("WARNING: " + e.whatStr() + " [" + url + "].");

				break;
			}
		}

		if(!success)
			return false;

		// set new content as target for subsequent queries
		this->setQueryTarget(content, url);

		// check response code and return result
		return this->crawlingCheckResponseCode(url, this->networking.getResponseCode());
	}

	// resolve variables in string (i.e. URL or cookies header) for dynamic redirect by content
	void Thread::crawlingDynamicRedirectContentVars(
			const std::string& oldUrl,
			std::string& strInOut
	) {
		for(auto i = this->config.redirectVarNames.begin(); i != this->config.redirectVarNames.end(); ++i) {
			if(strInOut.find(*i) == std::string::npos)
				continue;

			const auto index = i - this->config.redirectVarNames.begin();
			std::queue<std::string> queryWarnings;
			std::string value;

			// check type of variable source
			switch(this->config.redirectVarSources.at(index)) {
			case Config::redirectSourceUrl:
				// get value from (old) URL
				if(this->queriesRedirectVars.at(index).resultSingle)
					this->getSingleFromRegEx(this->queriesRedirectVars.at(index), oldUrl, value, queryWarnings);
				else if(this->queriesRedirectVars.at(index).resultBool) {
					bool booleanResult = false;

					if(this->getBoolFromRegEx(this->queriesRedirectVars.at(index), oldUrl, booleanResult, queryWarnings))
						value = booleanResult ? "true" : "false";
				}
				else
					this->log(
							"WARNING: Invalid result type of query for dynamic redirect variable \'"
							+ *i
							+ "\' - set to empty."
					);

				break;

			case Config::redirectSourceContent:
				// get value from content
				if(this->queriesRedirectVars.at(index).resultSingle)
					this->getSingleFromQuery(this->queriesRedirectVars.at(index), value, queryWarnings);
				else if(this->queriesRedirectVars.at(index).resultBool) {
					bool booleanResult = false;

					if(this->getBoolFromQuery(this->queriesRedirectVars.at(index), booleanResult, queryWarnings))
						value = booleanResult ? "true" : "false";
				}
				else
					this->log(
							"WARNING: Invalid result type of query for dynamic redirect variable \'"
							+ *i
							+ "\' - set to empty."
					);

				break;

			default:
				if(this->config.crawlerLogging)
					this->log(
							"WARNING: Unknown source type for dynamic redirect variable \'"
							+ *i
							+ "\' - set to empty."
					);
			}

			// log warnings if necessary
			this->logWarnings(queryWarnings);

			// replace variable with value
			Helper::Strings::replaceAll(strInOut, *i, value, true);
		}
	}

	// check whether URL should be added
	bool Thread::crawlingCheckUrl(const std::string& url, const std::string& from) {
		// check argument
		if(url.empty())
			return false;

		// check for invalid UTF-8 character(s) in URL
		std::string utf8Error;

		if(!Helper::Utf8::isValidUtf8(url, utf8Error)) {
			if(this->config.crawlerLogging) {
				if(utf8Error.empty())
					this->log(
							"ignored URL containing invalid UTF-8 character(s) ["
							+ url
							+ " from "
							+ from
							+ "]."
					);
				else
					this->log(
							"ignored URL because "
							+ utf8Error
							+ " ["
							+ url
							+ " from "
							+ from
							+ "]."
					);
			}

			return false;
		}

		std::queue<std::string> queryWarnings;

		// whitelist: only allow URLs that fit a specified whitelist query
		bool whitelist = false;
		bool found = false;

		for(const auto& query : this->queriesWhiteListUrls) {
			if(query.type != QueryStruct::typeNone)
				whitelist = true;

			if(this->getBoolFromRegEx(query, url, found, queryWarnings) && found)
				break;
		}

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(whitelist && !found) {
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log("skipped " + url + " (not whitelisted).");

			return false;
		}

		// blacklist: do not allow URLs that fit a specified blacklist query
		found = false;

		for(const auto& query : this->queriesBlackListUrls)
			if(this->getBoolFromRegEx(query, url, found, queryWarnings) && found)
				break;

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(found && this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("skipped " + url + " (blacklisted).");

		return !found;
	}

	// check whether links should be extracted from URL
	bool Thread::crawlingCheckUrlForLinkExtraction(const std::string& url) {
		// check argument
		if(url.empty())
			return false;

		std::queue<std::string> queryWarnings;

		// whitelist: only allow URLs that fit a specified whitelist query
		bool whitelist = false;
		bool found = false;

		for(const auto& query : this->queriesLinksWhiteListUrls) {
			if(query.type != QueryStruct::typeNone)
				whitelist = true;

			if(this->getBoolFromRegEx(query, url, found, queryWarnings) && found)
					break;
		}

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(whitelist && !found) {
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log("skipped " + url + " (not whitelisted).");

			return false;
		}

		// blacklist: do not allow URLs that fit a specified blacklist query
		found = false;

		for(const auto& query : this->queriesLinksBlackListUrls)
			if(this->getBoolFromRegEx(query, url, found, queryWarnings) && found)
				break;

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(found && this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("skipped " + url + " (blacklisted).");

		return !found;
	}

	// check cURL code and decide whether to retry or skip
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
			if(this->config.crawlerLogging)
				this->log(
						"HTTP error "
						+ std::to_string(responseCode)
						+ " from "
						+ url
						+ " - skips..."
				);

			return false;
		}
		else if(responseCode != 200 && this->config.crawlerLogging)
			this->log(
					"WARNING: HTTP response code "
					+ std::to_string(responseCode)
					+ " from "
					+ url
					+ "."
			);

		return true;
	}

	// check whether specific content type should be crawled
	bool Thread::crawlingCheckContentType(const std::string& url, const std::string& contentType) {
		std::queue<std::string> queryWarnings;

		// check whitelist for content types
		bool whitelist = false;
		bool found = false;

		for(const auto& query : this->queriesWhiteListTypes) {
			if(query.type != QueryStruct::typeNone)
				whitelist = true;

			if(this->getBoolFromRegEx(query, contentType, found, queryWarnings) && found)
				break;
		}

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(whitelist && !found) {
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log("skipped " + url + " (content type \'" + contentType + "\' not whitelisted).");

			return false;
		}

		// check blacklist for content types
		found = false;

		for(const auto& query : this->queriesBlackListTypes)
			if(this->getBoolFromRegEx(query, contentType, found, queryWarnings) && found)
				 break;

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(found && this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("skipped " + url + " (content type \'" + contentType + "\' blacklisted).");

		return !found;
	}

	// check whether specific content type should be used for link extraction
	bool Thread::crawlingCheckContentTypeForLinkExtraction(const std::string& url, const std::string& contentType) {
		std::queue<std::string> queryWarnings;

		// check whitelist for content types
		bool whitelist = false;
		bool found = false;

		for(const auto& query : this->queriesLinksWhiteListTypes) {
			if(query.type != QueryStruct::typeNone)
				whitelist = true;

			if(this->getBoolFromRegEx(query, url, found, queryWarnings) && found)
				break;
		}

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(whitelist && !found) {
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log(
						"skipped link extraction for "
						+ url
						+ " (content type \'"
						+ contentType
						+ "\' not whitelisted)."
				);

			return false;
		}

		// check blacklist for content types
		found = false;

		for(const auto& query : this->queriesLinksBlackListTypes)
			if(this->getBoolFromRegEx(query, contentType, found, queryWarnings) && found)
				 break;

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(found && this->config.crawlerLogging > Config::crawlerLoggingDefault)
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log(
						"skipped link extraction for "
						+ url
						+ " (content type \'"
						+ contentType
						+ "\' blacklisted)."
				);

		return !found;
	}

	// check whether specific content should be crawled, throws Thread::Exception
	bool Thread::crawlingCheckContent(const std::string& url) {
		// check argument
		if(url.empty())
			throw Exception("Crawler::Thread::crawlingCheckContent(): No URL specified");

		std::queue<std::string> queryWarnings;

		// check whitelist for content
		bool whitelist = false;
		bool found = false;

		for(const auto& query : this->queriesWhiteListContent) {
			if(query.type != QueryStruct::typeNone)
				whitelist = true;

			if(this->getBoolFromQuery(query, found, queryWarnings) && found)
				break;
		}

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(whitelist && !found) {
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log("skipped " + url + " (content not whitelisted).");

			return false;
		}

		// check blacklist for content
		found = false;

		for(const auto& query : this->queriesBlackListContent)
			if(this->getBoolFromQuery(query, found, queryWarnings) && found)
				break;

		if(found && this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("skipped " + url + " (content blacklisted).");

		return !found;
	}

	// check whether specific content should be used for link extraction, throws Thread::Exception
	bool Thread::crawlingCheckContentForLinkExtraction(const std::string& url) {
		// check argument
		if(url.empty())
			throw Exception("Crawler::Thread::crawlingCheckContent(): No URL specified");

		std::queue<std::string> queryWarnings;

		// check whitelist for content
		bool whitelist = false;
		bool found = false;

		for(const auto& query : this->queriesLinksWhiteListContent) {
			if(query.type != QueryStruct::typeNone)
				whitelist = true;

			if(this->getBoolFromQuery(query, found, queryWarnings) && found)
				break;
		}

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(whitelist && !found) {
			if(this->config.crawlerLogging > Config::crawlerLoggingDefault)
				this->log("skipped link extraction from " + url + " (content not whitelisted).");

			return false;
		}

		// check blacklist for content
		found = false;

		for(const auto& query : this->queriesLinksBlackListContent)
			if(this->getBoolFromQuery(query, found, queryWarnings) && found)
				break;

		if(found && this->config.crawlerLogging > Config::crawlerLoggingDefault)
			this->log("skipped link extraction from " + url + " (content blacklisted).");

		return !found;
	}

	// save content to database, throws Thread::Exception
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
			std::queue<std::string> warnings;
			std::string xmlContent;

			if(this->getXml(xmlContent, warnings))
				this->database.saveContent(url.first, response, type, xmlContent);
			else
				xmlContent.clear();

			// log warnings if necessary
			this->logWarnings(warnings);

			if(!xmlContent.empty())
				return;
		}

		this->database.saveContent(url.first, response, type, content);
	}

	// extract URLs from content, throws Thread::Exception
	std::vector<std::string> Thread::crawlingExtractUrls(
			const std::string& url,
			const std::string& type
	) {
		std::vector<std::string> urls;

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
				|| !(this->crawlingCheckContentForLinkExtraction(url))
		)
			return urls;

		std::queue<std::string> queryWarnings;

		for(const auto& query : this->queriesLinks) {
			if(query.resultMulti) {
				std::vector<std::string> results;

				this->getMultiFromQuery(query, results, queryWarnings);

				urls.reserve(urls.size() + results.size());

				urls.insert(urls.end(), results.begin(), results.end());
			}
			else {
				std::string result;

				this->getSingleFromQuery(query, result, queryWarnings);

				urls.emplace_back(result);
			}
		}

		// check number of extracted URLs if necessary
		std::string expectedStr;

		this->getSingleFromQuery(this->queryExpected, expectedStr, queryWarnings);

		// log warnings if necessary
		this->logWarnings(queryWarnings);

		if(!expectedStr.empty()) {
			const unsigned long expected = std::stoul(expectedStr);
			std::ostringstream expectedStrStr;

			expectedStrStr.imbue(std::locale(""));

			if(urls.size() < expected) {
				// number of URLs is smaller than expected
				expectedStrStr	<< "number of extracted URLs ["
								<< urls.size()
								<< "] is smaller than expected ["
								<< expected
								<< "] ["
								<< url
								<< "]";

				if(this->config.expectedErrorIfSmaller)
					throw Exception(expectedStrStr.str());
				else if(this->config.crawlerLogging)
					this->log("WARNING: " + expectedStrStr.str() + ".");
			}
			else if(urls.size() > expected) {
				// number of URLs is larger than expected
				expectedStrStr	<< "number of extracted URLs ["
								<< urls.size()
								<< "] is larger than expected ["
								<< expected
								<< "] ["
								<< url
								<< "]";

				// number of URLs is smaller than expected
				if(this->config.expectedErrorIfLarger)
					throw Exception(expectedStrStr.str());
				else if(this->config.crawlerLogging)
					this->log("WARNING: " + expectedStrStr.str() + ".");
			}
			else if(this->config.crawlerLogging == Config::crawlerLoggingVerbose) {
				expectedStrStr	<< "number of extracted URLs ["
								<< urls.size()
								<< "] as expected ["
								<< expected
								<< "] ["
								<< url
								<< "].";

				this->log(expectedStrStr.str());
			}
		}

		// sort and remove duplicates
		Helper::Strings::sortAndRemoveDuplicates(urls, this->config.crawlerUrlCaseSensitive);

		return urls;
	}

	// parse URLs and add them as sub-links (or links if website is cross-domain) to the database if they do not already exist,
	//  throws Thread::Exception
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
				const unsigned long pos1 = urls.at(n - 1).find("https://", 1);
				const unsigned long pos2 = urls.at(n - 1).find("http://", 1);
				unsigned long pos = 0;

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
		const auto oldSize = urls.size();

		urls.erase(std::remove_if(urls.begin(), urls.end(),
				[&maxLength = this->config.crawlerUrlMaxLength](const auto& url) {
					return url.length() > maxLength;
				}
		), urls.end());

		if(this->config.crawlerLogging && urls.size() < oldSize)
			this->log("WARNING: URLs longer than 2,000 Bytes ignored [" + url + "]");

		// if necessary, check for file endings and show warnings
		if(this->config.crawlerLogging && this->config.crawlerWarningsFile)
			for(const auto& url : urls)
				if(url.back() != '/'){
					const auto lastSlash = url.rfind('/');

					if(lastSlash == std::string::npos) {
						if(url.find('.') != std::string::npos)
							this->log("WARNING: Found file \'" + url + "\' [" + url + "]");
					}
					else if(url.find('.', lastSlash + 1) != std::string::npos)
						this->log("WARNING: Found file \'" + url + "\' [" + url + "]");
				}

		// save status message
		const std::string statusMessage(this->getStatusMessage());

		// add URLs that do not exist already in chunks of config-defined size
		unsigned long pos = 0;
		unsigned long chunkSize = 0;

		// check for infinite chunk size
		if(this->config.crawlerUrlChunks)
			chunkSize = this->config.crawlerUrlChunks;
		else
			chunkSize = urls.size();

		while(pos < urls.size() && this->isRunning()) {
			const auto begin = urls.begin() + pos;
			const auto end = urls.begin() + pos + std::min(chunkSize, urls.size() - pos);

			std::queue<std::string> chunk(std::queue<std::string>::container_type(begin, end));

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

	// crawl archives, throws Thread::Exception
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

				std::string archivedUrl(
						this->config.crawlerArchivesUrlsTimemap.at(n) + this->domain + url.second
				);

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
							const std::string contentType(this->networkingArchives->getContentType());

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

									// save status message
									const std::string statusMessage(this->getStatusMessage());

									// go through all mementos
									unsigned long counter = 0;
									const unsigned long total = mementos.size();

									while(!mementos.empty() && this->isRunning()) {
										std::string timeStamp(mementos.front().timeStamp);

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

													// check HTTP response code
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
														if(
																Helper::DateTime::convertSQLTimeStampToTimeStamp(
																		timeStamp
																)
														) {
															unsigned long subUrlPos = mementos.front().url.find(timeStamp);

															if(subUrlPos != std::string::npos) {
																subUrlPos += timeStamp.length();
																timeStamp = archivedContent.substr(17, 14);

																// get URL and validate timestamp
																mementos.front().url =
																		this->config.crawlerArchivesUrlsMemento.at(n)
																		+ timeStamp
																		+ mementos.front().url.substr(subUrlPos);

																if(
																		Helper::DateTime::convertTimeStampToSQLTimeStamp(
																				timeStamp
																		)
																)
																	// follow reference
																	continue;

																else if(this->config.crawlerLogging)
																	// log warning (and ignore reference)
																	this->log(
																			"WARNING: Invalid timestamp \'"
																			+ timeStamp
																			+ "\' from "
																			+ this->config.crawlerArchivesNames.at(n)
																			+ " ["
																			+ url.second
																			+ "]."
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
														// set content as target for subsequent queries
														this->setQueryTarget(archivedContent, mementos.front().url);

														// get content type
														const std::string contentType(
																this->networkingArchives->getContentType()
														);

														// add archived content to database
														this->database.saveArchivedContent(
																url.first,
																mementos.front().timeStamp,
																this->networkingArchives->getResponseCode(),
																contentType,
																archivedContent
														);

														// extract URLs
														std::vector<std::string> urls(
																this->crawlingExtractUrls(
																		url.second,
																		contentType
																)
														);

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
														if(
																this->crawlingCheckCurlCode(
																		this->networkingArchives->getCurlCode(),
																		mementos.front().url
																)
														) {
															// log error
															if(this->config.crawlerLogging) {
																this->log(
																		e.whatStr()
																		+ " ["
																		+ mementos.front().url
																		+ "]."
																);

																this->log(
																		"resets connection to "
																		+ this->config.crawlerArchivesNames.at(n)
																		+ "..."
																);
															}

															// reset connection
															this->setStatusMessage(
																	"ERROR "
																	+ e.whatStr()
																	+ " ["
																	+ mementos.front().url
																	+ "]"
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
																e.whatStr()
																+ " - skips..."
																+ " ["
																+ mementos.front().url
																+ "]"
														);
												}
												catch(const Utf8Exception& e) {
													// write UTF-8 error to log if necessary (and skip)
													if(this->config.crawlerLogging)
														this->log(
																"WARNING: "
																+ e.whatStr()
																+ " - skips..."
																+ " ["
																+ mementos.front().url
																+ "]"
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

									// restore previous status message
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
										"resets connection to "
										+ this->config.crawlerArchivesNames.at(n)
										+ "..."
								);
							}

							this->setStatusMessage(
									"ERROR "
									+ e.whatStr()
									+ " ["
									+ archivedUrl
									+ "]"
							);

							this->networkingArchives->resetConnection(
									this->config.crawlerSleepError
							);

							success = false;
						}
					}
					catch(const Utf8Exception& e) {
						// write UTF-8 error to log if necessary
						if(this->config.crawlerLogging)
							this->log(
									"WARNING: "
									+ e.whatStr()
									+ " ["
									+ archivedUrl
									+ "]"
							);

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

	// crawling successfull, throws Thread::Exception
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

			const unsigned long total = this->database.getNumberOfUrls();

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

	// skip URL after crawling problem, throws Thread::Exception
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

	// retry URL (completely or achives-only) after crawling problem, throws Thread::Exception
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
					warningsTo.emplace(
							"No '>' after '<' for link at "
							+ std::to_string(pos)
							+ "."
					);

					break;
				}

				if(mementoStarted) {
					// memento not finished -> warning
					if(!newMemento.url.empty() && !newMemento.timeStamp.empty())
						mementosTo.emplace(newMemento);

					warningsTo.emplace(
							"New memento started without finishing the old one at "
							+ std::to_string(pos)
							+ "."
					);
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
				else
					warningsTo.emplace(
							"Field seperator missing for new field at "
							+ std::to_string(pos)
							+ "."
					);

				end = mementoContent.find('=', pos + 1);

				if(end == std::string::npos) {
					end = mementoContent.find_first_of(",;");

					if(end == std::string::npos) {
						warningsTo.emplace(
								"Cannot find end of field at "
								+ std::to_string(pos)
								+ "."
						);

						break;
					}

					pos = end;
				}
				else {
					const std::string fieldName(mementoContent.substr(pos, end - pos));
					const unsigned long oldPos = pos;

					pos = mementoContent.find_first_of("\"\'", pos + 1);

					if(pos == std::string::npos) {
						warningsTo.emplace(
								"Cannot find begin of value at "
								+ std::to_string(oldPos)
								+ "."
						);

						++pos;

						continue;
					}

					end = mementoContent.find_first_of("\"\'", pos + 1);

					if(end == std::string::npos) {
						warningsTo.emplace(
								"Cannot find end of value at "
								+ std::to_string(pos)
								+ "."
						);

						break;
					}

					std::string fieldValue(mementoContent.substr(pos + 1, end - pos - 1));

					if(fieldName == "datetime") {
						// parse timestamp
						if(Helper::DateTime::convertLongDateTimeToSQLTimeStamp(fieldValue))
							newMemento.timeStamp = fieldValue;
						else
							warningsTo.emplace(
									"Could not convert timestamp \'"
									+ fieldValue
									+ "\' at "
									+ std::to_string(pos)
									+ "."
							);
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

	// write parsing and query errors as warnings to log if necessary
	void Thread::logWarnings(std::queue<std::string>& warnings) {
		if(this->config.crawlerLogging < Config::crawlerLoggingDefault)
			return;

		while(!warnings.empty()) {
			this->log(warnings.front());

			warnings.pop();
		}
	}

} /* crawlservpp::Module::Crawler */
