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
#include "../../Timer/StartStop.hpp"
#include "../../Wrapper/DatabaseLock.hpp"
#include "../../Wrapper/DatabaseTryLock.hpp"

#include <curl/curl.h>

#include <algorithm>	// std::count, std::find_if, std::min, std::remove_if, std::transform
#include <cctype>		// ::tolower
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::int64_t, std::uint32_t, std::uint64_t
#include <queue>		// std::queue
#include <functional>	// std::bind
#include <iomanip>		// std::setprecision
#include <ios>			// std::fixed
#include <locale>		// std::locale
#include <memory>		// std::make_unique, std::unique_ptr
#include <queue>		// std::queue
#include <sstream>		// std::istringstream, std::ostringstream
#include <stdexcept>	// std::logic_error
#include <string>		// std::getline, std::stoul, std::string, std::to_string
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Crawler {

	class Thread: public Module::Thread, private Query::Container, private Config {
		// for convenienc
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
		// constructors
		Thread(
				Main::Database& dbBase,
				const std::string& cookieDirectory,
				const ThreadOptions& threadOptions,
				const NetworkSettings& serverNetworkSettings,
				const ThreadStatus& threadStatus
		);

		Thread(
				Main::Database& dbBase,
				const std::string& cookieDirectory,
				const ThreadOptions& threadOptions,
				const NetworkSettings& serverNetworkSettings
		);

		// destructor
		virtual ~Thread();

		// class for Crawler exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// database and networking for thread
		Database database;
		const NetworkSettings networkOptions;
		Network::Curl networking;
		Network::TorControl torControl;

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
		bool noSubDomain;
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
		std::vector<TimeString> customTokens;

		// crawling state
		IdString nextUrl;				// next URL (currently crawled URL in automatic mode)
		std::string lockTime;			// last locking time for currently crawled URL
		IdString manualUrl;				// custom URL to be retried
		std::size_t manualCounter;		// number of crawled custom URLs
		bool startCrawled;				// start page has been successfully crawled
		bool manualOff;					// manual mode has been turned off (after first URL is crawled)
		std::string crawledContent;		// crawled content
		std::uint64_t retryCounter;		// number of retries so far
		bool archiveRetry;				// only archive needs to be retried

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
		void initQueries() override;

		// crawling functions
		bool crawlingUrlSelection(IdString& urlTo, bool& usePostTo);
		IdString crawlingReplaceTokens(const IdString& url);
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
				bool unlockUrl
		);
		void crawlingSuccess(const IdString& url);
		void crawlingSkip(const IdString& url, bool unlockUrl);
		void crawlingRetry(const IdString& url, bool archiveOnly);
		void crawlingReset(const std::string& error, const std::string& url);
		void crawlingResetArchive(
				const std::string& error,
				const std::string& url,
				const std::string& archive
		);
		void crawlingResetTor();

		// static helper function for memento crawling
		static std::string parseMementos(
				std::string mementoContent,
				std::queue<std::string>& warningsTo,
				std::queue<Memento>& mementosTo
		);
	};

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_THREAD_HPP_ */
