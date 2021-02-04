/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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
#include "../../Helper/Utf8.hpp"
#include "../../Main/Exception.hpp"
#include "../../Network/Curl.hpp"
#include "../../Network/TorControl.hpp"
#include "../../Parsing/URI.hpp"
#include "../../Query/Container.hpp"
#include "../../Struct/NetworkSettings.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/QueryStruct.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/Simple.hpp"
#include "../../Timer/StartStop.hpp"
#include "../../Wrapper/DatabaseLock.hpp"
#include "../../Wrapper/DatabaseTryLock.hpp"

#include <curl/curl.h>

#include <algorithm>	// std::count, std::find_if, std::min, std::remove_if, std::transform
#include <cctype>		// ::tolower
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::int64_t, std::uint32_t, std::uint64_t
#include <iomanip>		// std::setprecision
#include <ios>			// std::fixed
#include <locale>		// std::locale
#include <memory>		// std::make_unique, std::unique_ptr
#include <queue>		// std::queue
#include <sstream>		// std::istringstream, std::ostringstream
#include <stdexcept>	// std::logic_error
#include <string>		// std::getline, std::stoul, std::string, std::to_string
#include <string_view>	// std::string_view, std::string_view_literals
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Crawler {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! The minimum length of a robots.txt line containing a useful sitemap.
	inline constexpr auto robotsMinLineLength{9};

	//! The first letters of a robots.txt line containing a sitemap.
	inline constexpr auto robotsFirstLetters{7};

	//! The beginning of a robots.txt line containing a sitemap.
	inline constexpr auto robotsSitemapBegin{"sitemap:"sv};

	//! The relative URL of robots.txt.
	inline constexpr auto robotsRelativeUrl{"/robots.txt"sv};

	//! The number of custom URLs after which the thread status will be updated.
	inline constexpr auto updateCustomUrlCountEvery{100};

	//! Minimum HTTP error code.
	inline constexpr auto httpResponseCodeMin{400};

	//! Maximum HTTP error code.
	inline constexpr auto httpResponseCodeMax{599};

	//! HTTP response code to be ignored when checking for errors.
	inline constexpr auto httpResponseCodeIgnore{200};

	//! The "www." in the beginning of a domain.
	inline constexpr auto wwwString{"www."sv};

	//! The beginning of a URL containing the HTTPS protocol.
	inline constexpr auto httpsString{"https://"sv};

	//! The beginning of a HTTPS URL to be ignored.
	inline constexpr auto httpsIgnoreString{"https://www."sv};

	//! The beginning of a URL containing the HTTP protocol.
	inline constexpr auto httpString{"http://"sv};

	//! The beginning of a HTTP URL to be ignored.
	inline constexpr auto httpIgnoreString{"http://www."sv};

	//! The content type of a memento.
	inline constexpr auto archiveMementoContentType{"application/link-format"sv};

	//! The reference string in a memento referencing another memento.
	inline constexpr auto archiveRefString{"found capture at "sv};

	//! The length of a memento time stamp.
	inline constexpr auto archiveRefTimeStampLength{14};

	//! Number of milliseconds before renewing URL lock while crawling archives.
	inline constexpr auto archiveRenewUrlLockEveryMs{1000};

	///@}

	/*
	 * DECLARATION
	 */

	//! %Crawler thread.
	class Thread final : public Module::Thread, public Query::Container, private Config {
		// for convenience
		using DateTimeException = Helper::DateTime::Exception;
		using Utf8Exception = Helper::Utf8::Exception;

		using CurlException = Network::Curl::Exception;
		using TorControlException = Network::TorControl::Exception;

		using URIException = Parsing::URI::Exception;

		using QueryException = Query::Container::Exception;

		using NetworkSettings = Struct::NetworkSettings;
		using QueryProperties = Struct::QueryProperties;
		using QueryStruct = Struct::QueryStruct;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using DatabaseLock = Wrapper::DatabaseLock<Database>;
		using DatabaseTryLock = Wrapper::DatabaseTryLock<Database>;

		using IdString = std::pair<std::uint64_t, std::string>;
		using TimeString = std::pair<std::chrono::steady_clock::time_point, std::string>;

	public:
		///@name Construction
		///@{

		Thread(
				Main::Database& dbBase,
				std::string_view cookieDirectory,
				const ThreadOptions& threadOptions,
				const NetworkSettings& networkSettings,
				const ThreadStatus& threadStatus
		);

		Thread(
				Main::Database& dbBase,
				std::string_view cookieDirectory,
				const ThreadOptions& threadOptions,
				const NetworkSettings& networkSettings
		);

		///@}

		//! Class for crawler exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		///@name Database Connection
		///@{

		//! %Database connection for the crawler thread.
		Database database;

		///@}
		///@name Networking
		///@{

		//! %Network settings for the crawler thread.
		const NetworkSettings networkOptions;

		//! Networking for the crawler thread.
		Network::Curl networking;

		//! TOR control for the crawler thread.
		Network::TorControl torControl;

		///@}
		///@name Implemented Thread Functions
		///@{

		void onInit() override;
		void onTick() override;
		void onPause() override;
		void onUnpause() override;
		void onClear() override;
		void onReset() override;

		///@}

	private:
		// structure for mementos (archived versions of websites)
		struct Memento {
			std::string url;
			std::string timeStamp;
		};

		// constant for cookie directory
		const std::string_view cookieDir;

		// table names for locking them
		std::string urlListTable;
		std::string crawlingTable;

		// domain, URI parser and (optional) separate networking for archives
		std::string domain;
		bool noSubDomain{false};
		Parsing::URI uriParser;
		std::unique_ptr<Network::Curl> networkingArchives;

		// queries
		/*
		 * make sure to initialize AND delete them!
		 *  -> initQueries(), deleteQueries()
		 */
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
		std::vector<TimeString> customTokens;

		// crawling state
		std::uint64_t penultimateId{};	// penultimate ID (last ID saved in parent class)
		IdString nextUrl;				// next URL (currently crawled URL in automatic mode)
		std::string lockTime;			// last locking time for currently crawled URL
		IdString manualUrl;				// custom URL to be retried
		std::size_t manualCounter{};	// number of crawled custom URLs
		bool startCrawled{false};		// start page has been successfully crawled
		bool manualOff{false};			// manual mode has been turned off (after first URL is crawled)
		std::string crawledContent;		// crawled content
		std::uint64_t retryCounter{};	// number of retries so far
		bool archiveRetry{false};		// only archive needs to be retried
		std::vector<std::string> mCache;// cache of processed mementos (to be skipped on retry)

		// timing
		std::uint64_t tickCounter{};
		std::chrono::steady_clock::time_point startTime{std::chrono::steady_clock::time_point::min()};
		std::chrono::steady_clock::time_point pauseTime{std::chrono::steady_clock::time_point::min()};
		std::chrono::steady_clock::time_point idleTime{std::chrono::steady_clock::time_point::min()};
		/*
		 * time of last HTTP request – only used when HTTP sleep is enabled
		 */
		std::chrono::steady_clock::time_point httpTime{std::chrono::steady_clock::time_point::min()};

		// initializing functions
		void setUpConfig(std::queue<std::string>& warningsTo);
		void checkQuery();
		void setUpLogging();
		void setUpContainer();
		void setUpDatabase();
		void setUpTableNames();
		void setUpSqlStatements();
		void checkUrlList();
		void setUpDomain();
		void setUpUriParser();
		void setUpNetworking();
		void setUpTor();
		void setUpCustomUrls();
		void setUpQueries();
		void setUpNetworkingArchives();
		void setUpTimers();
		void logWarnings(std::queue<std::string>& warnings);
		void initCustomUrls();
		void initRobotsTxt();
		void initDoGlobalCounting(
				std::vector<std::string>& urlList,
				const std::string& variable,
				const std::string& alias,
				std::int64_t start,
				std::int64_t end,
				std::int64_t step,
				std::int64_t aliasAdd
		);
		std::vector<std::string> initDoLocalCounting(
				const std::string& url,
				const std::string& variable,
				const std::string& alias,
				std::int64_t start,
				std::int64_t end,
				std::int64_t step,
				std::int64_t aliasAdd
		);
		void initTokenCache();

		// query functions
		void initQueries() override;
		void deleteQueries() override;
		void addOptionalQuery(std::uint64_t queryId, QueryStruct& propertiesTo);
		void addQueries(
				const std::vector<std::uint64_t>& queryIds,
				std::vector<QueryStruct>& propertiesTo
		);
		void addQueriesTo(
				std::string_view type,
				const std::vector<std::string>& names,
				const std::vector<std::uint64_t>& queryIds,
				std::vector<QueryStruct>& propertiesTo
		);

		// crawling functions
		bool crawlingUrlSelection(IdString& urlTo, bool& usePostTo);
		IdString crawlingReplaceTokens(const IdString& url);
		std::string crawlingGetTokenValue(std::size_t index, const std::string& name);
		void crawlingUrlParams(std::string& url);
		bool crawlingContent(
				IdString& url,
				const std::string& customCookies,
				const std::vector<std::string>& customHeaders,
				bool usePost,
				std::size_t& checkedUrlsTo,
				std::size_t& newUrlsTo,
				std::string& timerStrTo
		);
		void crawlingDynamicRedirectUrl(
				std::string& url,
				std::string& customCookies,
				std::vector<std::string>& customHeaders,
				bool& usePost
		);
		void crawlingDynamicRedirectUrlVars(const std::string& oldUrl, std::string& strInOut);
		bool crawlingDynamicRedirectContent(std::string& url, std::string& content);
		void crawlingDynamicRedirectContentVars(
				const std::string& oldUrl,
				std::string& strInOut
		);
		bool crawlingCheckUrl(const std::string& url, const std::string& from);
		bool crawlingCheckUrlForLinkExtraction(const std::string& url);
		bool crawlingCheckCurlCode(CURLcode curlCode, const std::string& url);
		bool crawlingCheckResponseCode(const std::string& url, std::uint32_t responseCode);
		bool crawlingCheckContentType(const std::string& url, const std::string& contentType);
		bool crawlingCheckContentTypeForLinkExtraction(
				const std::string& url,
				const std::string& contentType
		);
		bool crawlingCheckContent(const std::string& url);
		bool crawlingCheckContentForLinkExtraction(const std::string& url);
		void crawlingSaveContent(
				const IdString& url,
				std::uint32_t response,
				const std::string& type,
				const std::string& content
		);
		std::vector<std::string> crawlingExtractUrls(
				const std::string& url,
				const std::string& type
		);
		void crawlingParseAndAddUrls(
				const std::string& url,
				std::vector<std::string>& urls,
				std::size_t& newUrlsTo,
				bool archived
		);
		bool crawlingArchive(
				IdString& url,
				std::size_t& checkedUrlsTo,
				std::size_t& newUrlsTo,
				bool crawlingFailed
		);
		void crawlingSuccess(const IdString& url);
		void crawlingSkip(const IdString& url, bool unlockUrl);
		void crawlingRetry(const IdString& url, bool archiveOnly);
		void crawlingReset(std::string_view error, std::string_view url);
		void crawlingResetArchive(
				std::string_view error,
				std::string_view url,
				std::string_view archive
		);
		void crawlingResetTor();
		void crawlingUnsetCustom(bool unsetCookies, bool unsetHeaders);
		void crawlingClearMementoCache();

		// internal static helper function
		static std::string parseMementos(
				std::string mementoContent,
				std::queue<std::string>& warningsTo,
				std::queue<Memento>& mementosTo
		);

		// shadow functions not to be used by the thread
		void pause();
		void start();
		void unpause();
		void stop();
		void interrupt();
	};

} /* namespace crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_THREAD_HPP_ */
