/*
 * Thread.hpp
 *
 * Implementation of the Thread interface for crawler threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_THREAD_HPP_
#define MODULE_CRAWLER_THREAD_HPP_

#include "Config.hpp"
#include "Database.hpp"

#include "../Thread.hpp"

#include "../../Helper/DateTime.hpp"
#include "../../Helper/Strings.hpp"
#include "../../Main/Exception.hpp"
#include "../../Network/Curl.hpp"
#include "../../Parsing/URI.hpp"
#include "../../Parsing/XML.hpp"
#include "../../Query/Container.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Timer/StartStop.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <queue>
#include <functional>
#include <fstream>
#include <locale>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace crawlservpp::Module::Crawler {

	class Thread: public Module::Thread, public Query::Container, public Config {
		// for convenienc
		typedef Main::Exception Exception;
		typedef Network::Curl::Exception CurlException;
		typedef Parsing::URI::Exception URIException;
		typedef Parsing::XML::Exception XMLException;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Query::RegEx::Exception RegExException;
		typedef Query::XPath::Exception XPathException;

		typedef std::pair<unsigned long, std::string> IdString;

	public:
		// constructors
		Thread(
				Main::Database& database,
				unsigned long crawlerId,
				const std::string& crawlerStatus,
				bool crawlerPaused,
				const ThreadOptions& threadOptions,
				unsigned long crawlerLast
		);
		Thread(Main::Database& database, const ThreadOptions& threadOptions);

		// destructor
		virtual ~Thread();

	protected:
		// database and networking for thread
		Database database;
		Network::Curl networking;

		// table names for locking
		std::string urlListTable;
		std::string crawlingTable;

		// implemented thread functions
		void onInit(bool resumed) override;
		void onTick() override;
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

		// domain, URI parser and separate networking for archive.org
		std::string domain;
		std::unique_ptr<Parsing::URI> parser;
		std::unique_ptr<Network::Curl> networkingArchives;

		// queries
		std::vector<QueryStruct> queriesBlackListContent;
		std::vector<QueryStruct> queriesBlackListTypes;
		std::vector<QueryStruct> queriesBlackListUrls;
		std::vector<QueryStruct> queriesLinks;
		std::vector<QueryStruct> queriesWhiteListContent;
		std::vector<QueryStruct> queriesWhiteListTypes;
		std::vector<QueryStruct> queriesWhiteListUrls;
		QueryStruct queryCanonicalCheck;

		// custom URLs
		IdString startPage;
		std::vector<IdString> customPages;

		// crawling state
		IdString nextUrl;				// next URL (currently crawled URL in automatic mode)
		std::string lockTime;			// last locking time for currently crawled URL
		IdString manualUrl;				// custom URL to be retried
		unsigned long manualCounter;	// number of crawled custom URLs
		bool startCrawled;				// start page has been successfully crawled
		bool manualOff;					// manual mode has been turned off (after first URL from database is crawled)
		std::string crawledContent;		// crawled content
		unsigned long retryCounter;		// number of retries
		bool archiveRetry;				// only archive needs to be retried

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;
		std::chrono::steady_clock::time_point httpTime;	// time of last HTTP request (only used when HTTP sleep is enabled)

		// initializing functions
		void initCustomUrls();
		void initDoGlobalCounting(
				std::vector<std::string>& urlList,
				const std::string& variable,
				long start,
				long end,
				long step
		);
		std::vector<std::string> initDoLocalCounting(
				const std::string& url,
				const std::string& variable,
				long start,
				long end,
				long step
		);
		void initQueries() override;

		// crawling functions
		bool crawlingUrlSelection(IdString& urlTo);
		bool crawlingContent(
				const IdString& url,
				unsigned long& checkedUrlsTo,
				unsigned long& newUrlsTo,
				std::string& timerStrTo
		);
		bool crawlingCheckUrl(const std::string& url);
		bool crawlingCheckCurlCode(CURLcode curlCode, const std::string& url);
		bool crawlingCheckResponseCode(const std::string& url, long responseCode);
		bool crawlingCheckContentType(const std::string& url, const std::string& contentType);
		bool crawlingCheckConsistency(const std::string& url, const std::string& content);
		bool crawlingCheckCanonical(const std::string& url,	const Parsing::XML& doc);
		bool crawlingCheckContent(const std::string& url, const std::string& content, const Parsing::XML& doc);
		void crawlingSaveContent(
				const IdString& url,
				unsigned int response,
				const std::string& type,
				const std::string& content,
				const Parsing::XML& doc
		);
		std::vector<std::string> crawlingExtractUrls(
				const std::string& url,
				const std::string& content,
				const Parsing::XML& doc
		);
		void crawlingParseAndAddUrls(
				const std::string& url,
				std::vector<std::string>& urls,
				unsigned long& newUrlsTo,
				bool archived
		);
		bool crawlingArchive(
				IdString& url,
				unsigned long& checkedUrlsTo,
				unsigned long& newUrlsTo
		);
		void crawlingSuccess(const IdString& url);
		void crawlingSkip(const IdString& url);
		void crawlingRetry(const IdString& url, bool archiveOnly);

		// helper function for memento crawling
		static std::string parseMementos(
				std::string mementoContent,
				std::queue<std::string>& warningsTo,
				std::queue<Memento>& mementosTo
		);
	};

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_THREAD_HPP_ */
