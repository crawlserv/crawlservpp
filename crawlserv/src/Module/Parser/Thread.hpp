/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
 * Implementation of the Thread interface for parser threads.
 *
 *  Created on: Oct 11, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_THREAD_HPP_
#define MODULE_PARSER_THREAD_HPP_

#include "Config.hpp"
#include "Database.hpp"

#include "../Thread.hpp"

#include "../../Helper/CommaLocale.hpp"
#include "../../Helper/DateTime.hpp"
#include "../../Helper/DotLocale.hpp"
#include "../../Helper/Json.hpp"
#include "../../Helper/Strings.hpp"
#include "../../Main/Exception.hpp"
#include "../../Query/Container.hpp"
#include "../../Struct/DataEntry.hpp"
#include "../../Struct/QueryProperties.hpp"
#include "../../Struct/QueryStruct.hpp"
#include "../../Struct/StatusSetter.hpp"
#include "../../Struct/ThreadOptions.hpp"
#include "../../Struct/ThreadStatus.hpp"
#include "../../Timer/Simple.hpp"

#include <algorithm>	// std::find, std::find_if
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint64_t
#include <iomanip>		// std::setprecision
#include <ios>			// std::fixed
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <stdexcept>	// std::logic_error, std::runtime_error
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Parser {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The number of processed contents after which the thread status will be updated.
	inline constexpr std::uint8_t updateContentCounterEvery{25};

	///@}

	/*
	 * DECLARATION
	 */

	//! %Parser thread.
	class Thread final : public Module::Thread, public Query::Container, private Config {
		// for convenience
		using DateTimeException = Helper::DateTime::Exception;
		using LocaleException = Helper::DateTime::LocaleException;
		using JsonException = Helper::Json::Exception;

		using DataEntry = Struct::DataEntry;
		using QueryProperties = Struct::QueryProperties;
		using QueryStruct = Struct::QueryStruct;
		using StatusSetter = Struct::StatusSetter;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;

		using QueryException = Query::Container::Exception;

		using DatabaseLock = Wrapper::DatabaseLock<Database>;

		using IdString = std::pair<std::uint64_t, std::string>;

	public:
		///@name Construction
		///@{

		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions,
				const ThreadStatus& threadStatus
		);

		Thread(
				Main::Database& dbBase,
				const ThreadOptions& threadOptions
		);

		///@}

		//! Class for parser exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		///@name Database Connection
		///@{

		//! %Database connection for the parser thread.
		Database database;

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

		//! Parsed data that has not yet been written to the database.
		std::queue<DataEntry> results;

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
		// table names for locking them
		std::string parsingTable;
		std::string targetTable;

		// queries
		/*
		 * make sure to initialize AND delete them!
		 *  -> initQueries(), deleteQueries()
		 */
		std::vector<QueryStruct> queriesSkip;
		std::vector<QueryStruct> queriesContentIgnore;
		std::vector<QueryStruct> queriesId;
		std::vector<QueryStruct> queriesDateTime;
		std::vector<QueryStruct> queriesFields;

		// timing
		std::uint64_t tickCounter{};
		std::chrono::steady_clock::time_point startTime{std::chrono::steady_clock::time_point::min()};
		std::chrono::steady_clock::time_point pauseTime{std::chrono::steady_clock::time_point::min()};
		std::chrono::steady_clock::time_point idleTime{std::chrono::steady_clock::time_point::min()};

		// parsing state
		bool idle{false};			// waiting for new URLs to be crawled
		bool idFromUrlOnly{false};	// ID is exclusively parsed from URL
		std::uint64_t lastUrl{};	// last URL
		std::string lockTime;		// last locking time for currently parsed URL

		// properties used for progress calculation
		std::uint64_t idFirst{};	// ID of the first URL fetched
		std::uint64_t idDist{};		// distance between the IDs of first and last URL fetched
		float posFirstF{};			// position of the first URL fetched as float
		std::uint64_t posDist{};	// distance between the positions of first and last URL fetched
		std::uint64_t total{};		// number of total URLs in URL list

		// initializing functions
		void setUpConfig(std::queue<std::string>& warningsTo);
		void setUpLogging();
		void setUpContainer();
		void setUpDatabase();
		void setUpTableNames();
		void setUpTarget();
		void setUpSqlStatements();
		void setUpQueries();
		void checkParsingTable();
		void setUpTimers();
		void ready();
		void logWarnings(std::queue<std::string>& warnings);

		// query functions
		void initQueries() override;
		void deleteQueries() override;
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

		// parsing functions
		void parsingUrlSelection();
		void parsingFetchUrls();
		void parsingCheckUrls();
		std::size_t parsingNext();
		bool parsingContent(const IdString& content, std::string_view parsedId);
		void parsingUrlFinished(bool success);
		void parsingSaveResults(bool warped);
		void parsingFieldWarning(
				std::string_view error,
				std::string_view name,
				std::string_view url
		);

		// shadow functions not to be used by the thread
		void pause();
		void start();
		void unpause();
		void stop();
		void interrupt();
	};

} /* namespace crawlservpp::Module::Parser */

#endif /* MODULE_PARSER_THREAD_HPP_ */
