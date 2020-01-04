/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/Simple.hpp"

#include "../../_extern/jsoncons/include/jsoncons/json.hpp"
#include "../../_extern/jsoncons/include/jsoncons_ext/jsonpath/json_query.hpp"
#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <boost/lexical_cast.hpp>

#include <algorithm>	// std::count_if, std::find, std::find_if
#include <cctype>		// ::tolower
#include <chrono>		// std::chrono
#include <functional>	// std::bind
#include <iomanip>		// std::setprecision
#include <ios>			// std::fixed
#include <locale>		// std::locale
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <stdexcept>	// std::logic_error, std::runtime_error
#include <string>		// std::string, std::to_string
#include <thread>		// std::this_thread
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Extractor {

	class Thread: public Module::Thread, private Query::Container, private Config {

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
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using DatabaseLock = Wrapper::DatabaseLock<Database>;

		using IdString = std::pair<unsigned long, std::string>;
		using StringString = std::pair<std::string, std::string>;

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

		// class for Extractor exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// database for the thread
		Database database;
		Network::Curl networking;
		Network::TorControl torControl;

		// table names for locking
		std::string extractingTable;
		std::string targetTable;

		// cache
		std::queue<IdString> urls;
		std::string cacheLockTime;
		std::queue<DataEntry> results;
		std::queue<IdString> finished;

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

		// queries
		std::vector<QueryStruct> queriesVariables;
		std::vector<QueryStruct> queriesTokens;
		std::vector<QueryStruct> queriesError;
		std::vector<QueryStruct> queriesDatasets;
		std::vector<QueryStruct> queriesId;
		std::vector<QueryStruct> queriesDateTime;
		std::vector<QueryStruct> queriesFields;
		std::vector<QueryStruct> queriesRecursive;
		QueryStruct queryPagingIsNextFrom;
		QueryStruct queryPagingNextFrom;
		QueryStruct queryPagingNumberFrom;
		QueryStruct queryExpected;

		// timing
		unsigned long long tickCounter;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::steady_clock::time_point pauseTime;
		std::chrono::steady_clock::time_point idleTime;

		// state
		bool idle;					// waiting for new URLs to be crawled
		unsigned long lastUrl;		// last extracted URL
		std::string lockTime;		// last locking time for currently extracted URL

		// properties used for progress calculation
		unsigned long idFirst;		// ID of the first URL fetched
		unsigned long idDist;		// distance between the IDs of first and last URL fetched
		float posFirstF;			// position of the first URL fetched as float
		unsigned long posDist;		// distance between the positions of first and last URL fetched
		unsigned long total;		// number of total URLs in URL list

		// initializing function
		void initTargetTable();
		void initQueries() override;

		// extracting functions
		void extractingUrlSelection();
		void extractingFetchUrls();
		void extractingCheckUrls();
		unsigned long extractingNext();
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
				const std::string& cookies,
				const std::vector<std::string>& headers,
				bool usePost,
				const QueryStruct& query
		);
		void extractingPageContent(
				const std::string& url,
				const std::string& cookies,
				const std::vector<std::string>& headers,
				std::string& resultTo
		);
		void extractingGetValueFromContent(const QueryStruct& query, std::string& resultTo);
		void extractingGetValueFromUrl(const QueryStruct& query, std::string& resultTo);
		unsigned long extractingPage(unsigned long contentId, const std::string& url);
		bool extractingCheckCurlCode(CURLcode curlCode, const std::string& url);
		bool extractingCheckResponseCode(const std::string& url, long responseCode);
		void extractingUrlFinished();
		void extractingSaveResults(bool warped);
		void extractingResetTor();
	};

} /* crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_THREAD_HPP_ */
