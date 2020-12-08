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
 * Implementation of the Thread interface for extractor threads.
 *
 *  Created on: May 9, 2019
 *      Author: ans
 */

#ifndef MODULE_EXTRACTOR_THREAD_HPP_
#define MODULE_EXTRACTOR_THREAD_HPP_

#include "Config.hpp"
#include "Database.hpp"

#include "../Thread.hpp"

#include "../../Helper/DateTime.hpp"
#include "../../Helper/Strings.hpp"
#include "../../Helper/Utf8.hpp"
#include "../../Main/Exception.hpp"
#include "../../Network/Curl.hpp"
#include "../../Network/TorControl.hpp"
#include "../../Query/Container.hpp"
#include "../../Struct/DataEntry.hpp"
#include "../../Struct/NetworkSettings.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/QueryStruct.hpp"
#include "../../Struct/StatusSetter.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/Simple.hpp"

#include "../../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <algorithm>	// std::count_if, std::find, std::find_if
#include <cctype>		// ::tolower
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint32_t, std::uint64_t
#include <exception>	// std::exception
#include <iomanip>		// std::setprecision
#include <ios>			// std::fixed
#include <locale>		// std::locale
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <stdexcept>	// std::logic_error, std::runtime_error
#include <string>		// std::stol, std::stoul, std::string, std::to_string
#include <string_view>	// std::string_view
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Extractor {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Minimum HTTP error code.
	inline constexpr auto httpResponseCodeMin{400};

	//! Maximum HTTP error code.
	inline constexpr auto httpResponseCodeMax{599};

	//! HTTP response code to be ignored when checking for errors.
	inline constexpr auto httpResponseCodeIgnore{200};

	///@}

	/*
	 * DECLARATION
	 */

	//! %Extractor thread.
	class Thread final : public Module::Thread, private Query::Container, private Config {

		// for convenience
		using DateTimeException = Helper::DateTime::Exception;
		using LocaleException = Helper::DateTime::LocaleException;
		using Utf8Exception = Helper::Utf8::Exception;

		using CurlException = Network::Curl::Exception;
		using TorControlException = Network::TorControl::Exception;

		using QueryException = Query::Container::Exception;

		using DataEntry = Struct::DataEntry;
		using NetworkSettings = Struct::NetworkSettings;
		using QueryProperties = Struct::QueryProperties;
		using QueryStruct = Struct::QueryStruct;
		using StatusSetter = Struct::StatusSetter;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using DatabaseLock = Wrapper::DatabaseLock<Database>;

		using IdString = std::pair<std::uint64_t, std::string>;
		using StringString = std::pair<std::string, std::string>;

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

		//! Class for extractor exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		///@name Database Connection
		///@{

		//! %Database connection for the extractor thread.
		Database database;

		///@}
		///@name Networking
		///@{

		//! Networking for the extractor thread.
		Network::Curl networking;

		//! TOR control for the crawler thread.
		Network::TorControl torControl;

		///@}
		///@name Caching
		///@{

		//! Queue of URLs in the cache to still be processed, and their IDs.
		std::queue<IdString> urls;

		//! The time until which the URLs in the cache are locked, as string.
		/*!
		 * In the format @c YYYY-MM-DD HH:MM:SS.
		 */
		std::string cacheLockTime;

		//! Extracted data that has not yet been written to the database.
		std::queue<DataEntry> results;

		//! Linked data that has not yet been written to the database.
		std::queue<DataEntry> linked;

		//! ID cache.
		/*!
		 * To be checked first, to recognize duplicate
		 *  IDs in one dataset, without the need to
		 *  access the database.
		 */
		std::set<std::string> ids;

		//! Queue of URLs in the cache that have been finished.
		std::queue<IdString> finished;

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
		// shadow functions not to be used by the thread
		void pause();
		void start();
		void unpause();
		void stop();
		void interrupt();

		// table names for locking them
		std::string extractingTable;
		std::string targetTable;
		std::string linkedTable;

		// queries
		std::vector<QueryStruct> queriesVariables;
		std::vector<QueryStruct> queriesTokens;
		std::vector<QueryStruct> queriesErrorFail;
		std::vector<QueryStruct> queriesErrorRetry;
		std::vector<QueryStruct> queriesDatasets;
		std::vector<QueryStruct> queriesId;
		std::vector<QueryStruct> queriesDateTime;
		std::vector<QueryStruct> queriesFields;
		std::vector<QueryStruct> queriesRecursive;
		std::vector<QueryStruct> queriesLinkedDatasets;
		std::vector<QueryStruct> queriesLinkedId;
		std::vector<QueryStruct> queriesLinkedFields;
		QueryStruct queryPagingIsNextFrom;
		QueryStruct queryPagingNextFrom;
		QueryStruct queryPagingNumberFrom;
		QueryStruct queryExpected;

		// timing
		std::uint64_t tickCounter{0};
		std::chrono::steady_clock::time_point startTime{std::chrono::steady_clock::time_point::min()};
		std::chrono::steady_clock::time_point pauseTime{std::chrono::steady_clock::time_point::min()};
		std::chrono::steady_clock::time_point idleTime{std::chrono::steady_clock::time_point::min()};

		// state
		bool idle{false};			// waiting for new URLs to be crawled
		std::uint64_t lastUrl{0};	// last extracted URL
		std::string lockTime;		// last locking time for currently extracted URL

		// properties used for progress calculation
		std::uint64_t idFirst{0};	// ID of the first URL fetched
		std::uint64_t idDist{0};	// distance between the IDs of first and last URL fetched
		float posFirstF{0.F};		// position of the first URL fetched as float
		std::uint64_t posDist{0};	// distance between the positions of first and last URL fetched
		std::uint64_t total{0};		// number of total URLs in URL list

		// initializing function
		void initQueries() override;

		// query functions
		void addOptionalQuery(std::uint64_t queryId, QueryStruct& propertiesTo);
		void addQueries(
				const std::vector<std::uint64_t>& queryIds,
				std::vector<QueryStruct>& propertiesTo
		);
		void addQueriesTo(
				const std::vector<std::uint64_t>& queryIds,
				std::vector<QueryStruct>& propertiesTo
		);
		void addQueriesTo(
				std::string_view type,
				const std::vector<std::string>& names,
				const std::vector<std::uint64_t>& queryIds,
				std::vector<QueryStruct>& propertiesTo
		);

		// extracting functions
		void extractingUrlSelection();
		void extractingFetchUrls();
		void extractingCheckUrls();
		std::size_t extractingNext();
		void extractingGetVariableValues(std::vector<StringString>& variables);
		void extractingGetTokenValues(std::vector<StringString>& variables);
		void extractingGetPageTokenValues(
				const std::string& page,
				std::vector<StringString>& tokens,
				const std::vector<StringString>& variables
		);
		std::string extractingGetTokenValue(
				const std::string& name,
				const std::string& source,
				const std::string& setCookies,
				const std::vector<std::string>& setHeaders,
				bool usePost,
				const QueryStruct& query
		);
		void extractingPageContent(
				const std::string& url,
				const std::string& setCookies,
				const std::vector<std::string>& setHeaders,
				std::string& resultTo
		);
		void extractingGetValueFromContent(const QueryStruct& query, std::string& resultTo);
		void extractingGetValueFromUrl(const QueryStruct& query, std::string& resultTo);
		bool extractingPageIsRetry(std::queue<std::string>& queryWarningsTo);
		std::size_t extractingPage(std::uint64_t contentId, const std::string& url);
		std::size_t extractingLinked(std::uint64_t contentId, const std::string& url);
		bool extractingCheckCurlCode(CURLcode curlCode, const std::string& url);
		bool extractingCheckResponseCode(const std::string& url, std::uint32_t responseCode);
		void extractingUrlFinished();
		void extractingSaveLinked();
		void extractingSaveResults(bool warped);
		void extractingReset(std::string_view error, std::string_view url);
		void extractingResetTor();
		void extractingUnset(
				const std::string& unsetCookies,
				const std::vector<std::string>& unsetHeaders
		);
		void extractingFieldWarning(
				std::string_view error,
				std::string_view name,
				std::string_view url,
				bool linked
		);
	};

} /* namespace crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_THREAD_HPP_ */
