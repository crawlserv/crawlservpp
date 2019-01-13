/*
 * Thread.h
 *
 * Implementation of the Thread interface for crawler threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_THREAD_H_
#define MODULE_CRAWLER_THREAD_H_

#include "Config.h"
#include "Database.h"

#include "../Thread.h"

#include "../../Helper/DateTime.h"
#include "../../Helper/Strings.h"
#include "../../Network/Curl.h"
#include "../../Parsing/URI.h"
#include "../../Parsing/XML.h"
#include "../../Query/Container.h"
#include "../../Struct/IdString.h"
#include "../../Struct/Memento.h"
#include "../../Struct/ThreadOptions.h"
#include "../../Timer/StartStop.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <locale>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace crawlservpp::Module::Crawler {
	class Thread: public crawlservpp::Module::Thread, public crawlservpp::Query::Container {
	public:
		Thread(crawlservpp::Global::Database& database, unsigned long crawlerId, const std::string& crawlerStatus, bool crawlerPaused,
				const crawlservpp::Struct::ThreadOptions& threadOptions, unsigned long crawlerLast);
		Thread(crawlservpp::Global::Database& database, const crawlservpp::Struct::ThreadOptions& threadOptions);
		virtual ~Thread();

	protected:
		// database and networking for thread
		crawlservpp::Module::Crawler::Database database;
		crawlservpp::Network::Curl networking;

		// implemented thread functions
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
		crawlservpp::Module::Crawler::Config config;
		std::string domain;
		crawlservpp::Parsing::URI * parser;
		crawlservpp::Network::Curl * networkingArchives;

		// queries
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesBlackListContent;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesBlackListTypes;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesBlackListUrls;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesLinks;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesWhiteListContent;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesWhiteListTypes;
		std::vector<crawlservpp::Query::Container::QueryStruct> queriesWhiteListUrls;

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;

		// custom URLs
		unsigned long startPageId;
		std::vector<crawlservpp::Struct::IdString> customPages;

		// crawling state
		crawlservpp::Struct::IdString nextUrl;		// next URL (currently crawled URL in automatic mode)
		std::string lockTime;			// last locking time for currently crawled URL
		crawlservpp::Struct::IdString manualUrl;		// custom URL to be retried
		unsigned long manualCounter;	// number of crawled custom URLs
		bool startCrawled;				// start page has been successfully crawled
		bool manualOff;					// manual mode has been turned off (will happen after first URL from database is crawled)
		std::string crawledContent;		// crawled content
		unsigned long retryCounter;		// number of retries
		bool archiveRetry;				// archive needs to be retried
		std::chrono::steady_clock::time_point httpTime;// time of last HTTP request (only used when HTTP sleep is enabled)

		// initializing functions
		void initCustomUrls();
		void initDoGlobalCounting(std::vector<std::string>& urlList, const std::string& variable, long start, long end, long step);
		std::vector<std::string> initDoLocalCounting(const std::string& url, const std::string& variable, long start, long end,
				long step);
		void initQueries() override;

		// crawling functions
		bool crawlingUrlSelection(crawlservpp::Struct::IdString& urlTo);
		bool crawlingContent(const crawlservpp::Struct::IdString& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo,
				std::string& timerStrTo);
		bool crawlingCheckUrl(const std::string& url);
		bool crawlingCheckResponseCode(const std::string& url, long responseCode);
		bool crawlingCheckContentType(const crawlservpp::Struct::IdString& url, const std::string& contentType);
		bool crawlingCheckContent(const crawlservpp::Struct::IdString& url, const std::string& content, const crawlservpp::Parsing::XML& doc);
		void crawlingSaveContent(const crawlservpp::Struct::IdString& url, unsigned int response, const std::string& type,
				const std::string& content,
				const crawlservpp::Parsing::XML& doc);
		std::vector<std::string> crawlingExtractUrls(const crawlservpp::Struct::IdString& url, const std::string& content, const crawlservpp::Parsing::XML& doc);
		void crawlingParseAndAddUrls(const crawlservpp::Struct::IdString& url, std::vector<std::string>& urls, unsigned long& newUrlsTo,
				bool archived);
		bool crawlingArchive(const crawlservpp::Struct::IdString& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo);
		void crawlingSuccess(const crawlservpp::Struct::IdString& url);
		void crawlingSkip(const crawlservpp::Struct::IdString& url);
		void crawlingRetry(const crawlservpp::Struct::IdString& url, bool archiveOnly);

		// helper function for memento crawling
		static std::string parseMementos(std::string mementoContent, std::vector<std::string>& warningsTo,
				std::vector<crawlservpp::Struct::Memento>& mementosTo);
	};
}

#endif /* MODULE_CRAWLER_THREAD_H_ */
