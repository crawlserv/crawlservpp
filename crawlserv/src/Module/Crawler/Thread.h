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
#include "../../Struct/ThreadOptions.h"
#include "../../Timer/StartStop.h"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <fstream>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace crawlservpp::Module::Crawler {
	class Thread: public crawlservpp::Module::Thread, public crawlservpp::Query::Container {
		// for convenience
		typedef crawlservpp::Query::Container::QueryStruct QueryStruct;

	public:
		Thread(crawlservpp::Main::Database& database, unsigned long crawlerId, const std::string& crawlerStatus, bool crawlerPaused,
				const crawlservpp::Struct::ThreadOptions& threadOptions, unsigned long crawlerLast);
		Thread(crawlservpp::Main::Database& database, const crawlservpp::Struct::ThreadOptions& threadOptions);
		virtual ~Thread();

	protected:
		// database and networking for thread
		Database database;
		crawlservpp::Network::Curl networking;

		// implemented thread functions
		bool onInit(bool resumed) override;
		bool onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear(bool interrupted) override;

		// shadow pause function not to be used by thread
		void pause();

	private:
		// hide other functions not to be used by thread
		void start();
		void unpause();
		void stop();
		void interrupt();

		// structure for mementos (archived versions of websites)
		struct Memento {
			std::string url;
			std::string timeStamp;
		};

		// configuration, domain, URI parser and separate networking for archive.org
		Config config;
		std::string domain;
		std::unique_ptr<crawlservpp::Parsing::URI> parser;
		std::unique_ptr<crawlservpp::Network::Curl> networkingArchives;

		// queries
		std::vector<QueryStruct> queriesBlackListContent;
		std::vector<QueryStruct> queriesBlackListTypes;
		std::vector<QueryStruct> queriesBlackListUrls;
		std::vector<QueryStruct> queriesLinks;
		std::vector<QueryStruct> queriesWhiteListContent;
		std::vector<QueryStruct> queriesWhiteListTypes;
		std::vector<QueryStruct> queriesWhiteListUrls;
		QueryStruct queryCanonicalCheck;

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;

		// custom URLs
		unsigned long startPageId;
		std::vector<std::pair<unsigned long, std::string>> customPages;

		// crawling state
		std::pair<unsigned long, std::string> nextUrl;	// next URL (currently crawled URL in automatic mode)
		std::string lockTime;							// last locking time for currently crawled URL
		std::pair<unsigned long, std::string> manualUrl;// custom URL to be retried
		unsigned long manualCounter;					// number of crawled custom URLs
		bool startCrawled;								// start page has been successfully crawled
		bool manualOff;									// manual mode has been turned off (after first URL from database is crawled)
		std::string crawledContent;						// crawled content
		unsigned long retryCounter;						// number of retries
		bool archiveRetry;								// archive needs to be retried
		std::chrono::steady_clock::time_point httpTime;	// time of last HTTP request (only used when HTTP sleep is enabled)

		// initializing functions
		void initCustomUrls();
		void initDoGlobalCounting(std::vector<std::string>& urlList, const std::string& variable, long start, long end, long step);
		std::vector<std::string> initDoLocalCounting(const std::string& url, const std::string& variable, long start, long end,
				long step);
		void initQueries() override;

		// crawling functions
		bool crawlingUrlSelection(std::pair<unsigned long, std::string>& urlTo);
		bool crawlingContent(const std::pair<unsigned long, std::string>& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo,
				std::string& timerStrTo);
		bool crawlingCheckUrl(const std::string& url);
		bool crawlingCheckCurlCode(CURLcode curlCode, const std::string& url);
		bool crawlingCheckResponseCode(const std::string& url, long responseCode);
		bool crawlingCheckContentType(const std::pair<unsigned long, std::string>& url, const std::string& contentType);
		bool crawlingCheckContent(const std::pair<unsigned long, std::string>& url, const std::string& content,
				const crawlservpp::Parsing::XML& doc);
		void crawlingSaveContent(const std::pair<unsigned long, std::string>& url, unsigned int response, const std::string& type,
				const std::string& content,
				const crawlservpp::Parsing::XML& doc);
		std::vector<std::string> crawlingExtractUrls(const std::pair<unsigned long, std::string>& url, const std::string& content,
				const crawlservpp::Parsing::XML& doc);
		void crawlingParseAndAddUrls(const std::pair<unsigned long, std::string>& url, std::vector<std::string>& urls,
				unsigned long& newUrlsTo, bool archived);
		bool crawlingArchive(const std::pair<unsigned long, std::string>& url, unsigned long& checkedUrlsTo, unsigned long& newUrlsTo);
		void crawlingSuccess(const std::pair<unsigned long, std::string>& url);
		void crawlingSkip(const std::pair<unsigned long, std::string>& url);
		void crawlingRetry(const std::pair<unsigned long, std::string>& url, bool archiveOnly);

		// helper function for memento crawling
		static std::string parseMementos(std::string mementoContent, std::vector<std::string>& warningsTo,
				std::vector<Memento>& mementosTo);
	};
}

#endif /* MODULE_CRAWLER_THREAD_H_ */
