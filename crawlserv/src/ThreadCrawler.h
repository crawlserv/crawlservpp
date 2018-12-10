/*
 * ThreadCrawler.h
 *
 * Implementation of the Thread interface for crawler threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef THREADCRAWLER_H_
#define THREADCRAWLER_H_

#include "ConfigCrawler.h"
#include "Database.h"
#include "DatabaseCrawler.h"
#include "Networking.h"
#include "RegEx.h"
#include "Thread.h"
#include "TimerStartStop.h"
#include "URIParser.h"
#include "XMLDocument.h"
#include "XPath.h"

#include "namespaces/DateTime.h"
#include "namespaces/Strings.h"
#include "structs/IdString.h"
#include "structs/Memento.h"
#include "structs/ThreadOptions.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <locale>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

class ThreadCrawler: public Thread {
public:
	ThreadCrawler(Database& database, unsigned long crawlerId, const std::string& crawlerStatus, bool crawlerPaused,
			const ThreadOptions& threadOptions, unsigned long crawlerLast);
	ThreadCrawler(Database& database, const ThreadOptions& threadOptions);
	virtual ~ThreadCrawler();

protected:
	DatabaseCrawler database;
	Networking networking;

	bool onInit(bool resumed) override;
	bool onTick() override;
	void onPause() override;
	void onUnpause() override;
	void onClear(bool interrupted) override;

private:
	// hide functions not to be used by thread
	void start();
	void pause();
	void unpause();
	void stop();
	void interrupt();

	// configuration, domain, URI parser and separate networking for archive.org
	ConfigCrawler config;
	std::string domain;
	URIParser * parser;
	Networking * networkingArchives;

	// queries (including sub-struct for query identification)
	struct Query {
		static const unsigned char typeRegEx = 0;
		static const unsigned char typeXPath = 1;
		unsigned char type;
		unsigned long index;
		bool resultBool;
		bool resultSingle;
		bool resultMulti;
	};
	std::vector<RegEx*> queriesRegEx;
	std::vector<XPath*> queriesXPath;
	std::vector<ThreadCrawler::Query> queriesBlackListContent;
	std::vector<ThreadCrawler::Query> queriesBlackListTypes;
	std::vector<ThreadCrawler::Query> queriesBlackListUrls;
	std::vector<ThreadCrawler::Query> queriesLinks;
	std::vector<ThreadCrawler::Query> queriesWhiteListContent;
	std::vector<ThreadCrawler::Query> queriesWhiteListTypes;
	std::vector<ThreadCrawler::Query> queriesWhiteListUrls;

	// timing
	unsigned long long tickCounter;
	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point pauseTime;
	std::chrono::steady_clock::time_point idleTime;

	// custom URLs
	unsigned long startPageId;
	std::vector<IdString> customPages;

	// crawling state
	IdString nextUrl;				// next URL (currently crawled URL in automatic mode)
	std::string lockTime;			// last locking time for currently crawled URL
	IdString manualUrl;				// custom URL to be retried
	unsigned long manualCounter;	// number of crawled custom URLs
	bool startCrawled;				// start page has been successfully crawled
	bool manualOff;					// manual mode has been turned off (will happen after first successful URL from database is crawled)
	std::string crawledContent;		// crawled content
	unsigned long retryCounter;		// number of retries
	bool archiveRetry;				// archive needs to be retried
	std::chrono::steady_clock::time_point httpTime;// time of last HTTP request (only used when HTTP sleep is enabled)

	// initializing functions
	void initCustomUrls();
	void initDoGlobalCounting(std::vector<std::string>& urlList, const std::string& variable, long start, long end, long step);
	std::vector<std::string> initDoLocalCounting(const std::string& url, const std::string& variable, long start, long end, long step);
	void initQueries();
	ThreadCrawler::Query addQuery(const std::string& queryText, const std::string& queryType, bool queryResultBool,
			bool queryResultSingle, bool queryResultMulti, bool queryTextOnly);

	// crawling functions
	bool crawlingUrlSelection(IdString& urlTo);
	bool crawlingContent(const IdString& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo, std::string& timerStrTo);
	bool crawlingCheckUrl(const std::string& url);
	bool crawlingCheckResponseCode(const std::string& url, long responseCode);
	bool crawlingCheckContentType(const IdString& url, const std::string& contentType);
	bool crawlingCheckContent(const IdString& url, const std::string& content, const XMLDocument& doc);
	void crawlingSaveContent(const IdString& url, unsigned int response, const std::string& type, const std::string& content,
			const XMLDocument& doc);
	std::vector<std::string> crawlingExtractUrls(const IdString& url, const std::string& content, const XMLDocument& doc);
	void crawlingParseAndAddUrls(const IdString& url, std::vector<std::string>& urls, unsigned long& newUrlsTo, bool archived);
	bool crawlingArchive(const IdString& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo);
	void crawlingSuccess(const IdString& url);
	void crawlingSkip(const IdString& url);
	void crawlingRetry(const IdString& url, bool archiveOnly);

	// helper function for memento crawling
	static std::string parseMementos(std::string mementoContent, std::vector<std::string>& warningsTo, std::vector<Memento>& mementosTo);
};

#endif /* THREADCRAWLER_H_ */
