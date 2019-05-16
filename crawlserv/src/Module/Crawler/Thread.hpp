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
#include "../../Helper/Json.hpp"
#include "../../Helper/Strings.hpp"
#include "../../Helper/Utf8.hpp"
#include "../../Main/Exception.hpp"
#include "../../Network/Curl.hpp"
#include "../../Parsing/URI.hpp"
#include "../../Parsing/XML.hpp"
#include "../../Query/Container.hpp"
#include "../../Query/JsonPath.hpp"
#include "../../Query/JsonPointer.hpp"
#include "../../Query/RegEx.hpp"
#include "../../Query/XPath.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/StartStop.hpp"
#include "../../Wrapper/DatabaseLock.hpp"

#include "../../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <curl/curl.h>

#include <algorithm>	// std::find
#include <cctype>		// ::tolower
#include <chrono>		// std::chrono
#include <queue>		// std::queue
#include <functional>	// std::bind
#include <iomanip>		// std::setprecision
#include <ios>			// std::fixed
#include <locale>		// std::locale
#include <memory>		// std::make_unique, std::unique_ptr
#include <sstream>		// std::ostringstream
#include <stdexcept>	// std::logic_error, std::runtime_error
#include <string>		// std::string, std::to_string
#include <thread>		// std::this_thread
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Crawler {

	class Thread: public Module::Thread, private Query::Container, private Config {
		// for convenienc
		typedef Helper::Json::Exception JsonException;
		typedef Helper::Utf8::Exception Utf8Exception;
		typedef Network::Curl::Exception CurlException;
		typedef Parsing::URI::Exception URIException;
		typedef Parsing::XML::Exception XMLException;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::ThreadStatus ThreadStatus;
		typedef Query::JsonPath::Exception JsonPathException;
		typedef Query::JsonPointer::Exception JsonPointerException;
		typedef Query::RegEx::Exception RegExException;
		typedef Query::XPath::Exception XPathException;
		typedef Wrapper::DatabaseLock<Database> DatabaseLock;

		typedef std::pair<unsigned long, std::string> IdString;

	public:
		// constructors
		Thread(
				Main::Database& database,
				const std::string& cookieDirectory,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);

		Thread(
				Main::Database& database,
				const std::string& cookieDirectory,
				const ThreadOptions& threadOptions
		);

		// destructor
		virtual ~Thread();

		// sub-class for Crawler exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	protected:
		// database and networking for thread
		Database database;
		Network::Curl networking;

		// table names for locking
		std::string urlListTable;
		std::string crawlingTable;

		// implemented thread functions
		void onInit() override;
		void onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear() override;

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

		// constant for cookie directory
		const std::string cookieDir;

		// domain, URI parser and separate networking for archives
		std::string domain;
		std::unique_ptr<Parsing::URI> parser;
		std::unique_ptr<Network::Curl> networkingArchives;

		// queries
		std::vector<QueryStruct> queriesBlackListContent;
		std::vector<QueryStruct> queriesBlackListTypes;
		std::vector<QueryStruct> queriesBlackListUrls;
		std::vector<QueryStruct> queriesLinks;
		std::vector<QueryStruct> queriesLinksBlackListContent;
		std::vector<QueryStruct> queriesLinksBlackListTypes;
		std::vector<QueryStruct> queriesLinksBlackListUrls;
		std::vector<QueryStruct> queriesLinksWhiteListContent;
		std::vector<QueryStruct> queriesLinksWhiteListTypes;
		std::vector<QueryStruct> queriesLinksWhiteListUrls;
		std::vector<QueryStruct> queriesWhiteListContent;
		std::vector<QueryStruct> queriesWhiteListTypes;
		std::vector<QueryStruct> queriesWhiteListUrls;
		std::vector<QueryStruct> queriesTokens;
		QueryStruct queryRedirectContent;
		QueryStruct queryRedirectUrl;
		std::vector<QueryStruct> queriesRedirectVars;
		QueryStruct queryExpected;

		// custom URLs
		IdString startPage;
		std::vector<IdString> customPages;

		// crawling state
		IdString nextUrl;				// next URL (currently crawled URL in automatic mode)
		std::string lockTime;			// last locking time for currently crawled URL
		IdString manualUrl;				// custom URL to be retried
		unsigned long manualCounter;	// number of crawled custom URLs
		bool startCrawled;				// start page has been successfully crawled
		bool manualOff;					// manual mode has been turned off (after first URL is crawled)
		std::string crawledContent;		// crawled content
		unsigned long retryCounter;		// number of retries
		bool archiveRetry;				// only archive needs to be retried

		// parsing data and state
		Parsing::XML parsedXML;					// parsed XML/HTML
		rapidjson::Document parsedJsonRapid;	// parsed JSON (using RapidJSON)
		jsoncons::json parsedJsonCons;			// parsed JSON (using jsoncons)
		bool xmlParsed;							// XML/HTML has been parsed
		bool jsonParsedRapid;					// JSON has been parsed using RapidJSON
		bool jsonParsedCons;					// JSON has been parsed using jsoncons
		std::string xmlParsingError;			// error while parsing XML/HTML
		std::string jsonParsingError;			// error while parsing JSON

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;
		std::chrono::steady_clock::time_point httpTime;	// time of last HTTP request
														// (only used when HTTP sleep is enabled)

		// initializing functions
		void initCustomUrls();
		void initRobotsTxt();
		void initDoGlobalCounting(
				std::vector<std::string>& urlList,
				const std::string& variable,
				const std::string& alias,
				long start,
				long end,
				long step,
				long aliasAdd
		);
		std::vector<std::string> initDoLocalCounting(
				const std::string& url,
				const std::string& variable,
				const std::string& alias,
				long start,
				long end,
				long step,
				long aliasAdd
		);
		void initQueries() override;

		// crawling functions
		bool crawlingUrlSelection(IdString& urlTo, bool& usePostTo);
		IdString crawlingReplaceTokens(const IdString& url);
		void crawlingUrlParams(std::string& url);
		bool crawlingContent(
				IdString& url,
				const std::string& customCookies,
				bool usePost,
				unsigned long& checkedUrlsTo,
				unsigned long& newUrlsTo,
				std::string& timerStrTo
		);
		void crawlingDynamicRedirectUrl(std::string& url, std::string& customCookies, bool& usePost);
		void crawlingDynamicRedirectUrlVars(const std::string& oldUrl, std::string& strInOut);
		bool crawlingDynamicRedirectContent(std::string& url, std::string& content);
		void crawlingDynamicRedirectContentVars(
				const std::string& oldUrl,
				const std::string& oldContent,
				std::string& strInOut
		);
		bool crawlingCheckUrl(const std::string& url, const std::string& from);
		bool crawlingCheckUrlForLinkExtraction(const std::string& url);
		bool crawlingCheckCurlCode(CURLcode curlCode, const std::string& url);
		bool crawlingCheckResponseCode(const std::string& url, long responseCode);
		bool crawlingCheckContentType(const std::string& url, const std::string& contentType);
		bool crawlingCheckContentTypeForLinkExtraction(
				const std::string& url,
				const std::string& contentType
		);
		bool crawlingCheckContent(const std::string& url, const std::string& content);
		bool crawlingCheckContentForLinkExtraction(
				const std::string& url,
				const std::string& content
		);
		void crawlingSaveContent(
				const IdString& url,
				unsigned int response,
				const std::string& type,
				const std::string& content
		);
		std::vector<std::string> crawlingExtractUrls(
				const std::string& url,
				const std::string& type,
				const std::string& content
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
				unsigned long& newUrlsTo,
				bool unlockUrl
		);
		void crawlingSuccess(const IdString& url);
		void crawlingSkip(const IdString& url, bool unlockUrl);
		void crawlingRetry(const IdString& url, bool archiveOnly);

		// private helper functions
		bool parseXml(const std::string& content);
		bool parseJsonRapid(const std::string& content);
		bool parseJsonCons(const std::string& content);
		void resetParsingState();
		void logParsingErrors(const std::string& url);

		// static helper function for memento crawling
		static std::string parseMementos(
				std::string mementoContent,
				std::queue<std::string>& warningsTo,
				std::queue<Memento>& mementosTo
		);
	};

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_THREAD_HPP_ */
