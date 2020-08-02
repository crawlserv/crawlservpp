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
 * Database.hpp
 *
 * This class provides database functionality for a crawler thread
 *  by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_CRAWLER_DATABASE_HPP_
#define MODULE_CRAWLER_DATABASE_HPP_

#include "../../Helper/Portability/mysqlcppconn.h"
#include "../../Helper/Utf8.hpp"
#include "../../Main/Exception.hpp"
#include "../../Wrapper/Database.hpp"
#include "../../Wrapper/Database.hpp"

#include "Config.hpp"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <locale>		// std::locale
#include <memory>		// std::unique_ptr
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view, std::string_view_literals
#include <utility>		// std::pair

namespace crawlservpp::Module::Crawler {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! Maximum size of database content (= 1 GiB).
	constexpr auto maxContentSize{1073741824};

	//! Maximum size of database content as string.
	constexpr auto maxContentSizeString{"1 GiB"sv};

	///@}
	///@name Constants for MySQL Queries
	///@{

	//! Process ten values at once.
	constexpr auto nAtOnce10{10};

	//! Process one hundred values at once.
	constexpr auto nAtOnce100{100};

	//! First argument in a SQL query.
	constexpr auto sqlArg1{1};

	//! Second argument in a SQL query.
	constexpr auto sqlArg2{2};

	//! Third argument in a SQL query.
	constexpr auto sqlArg3{3};

	//! Fourth argument in a SQL query.
	constexpr auto sqlArg4{4};

	//! Fifth argument in a SQL query.
	constexpr auto sqlArg5{5};

	//! Alias, used in SQL queries, for the crawling table.
	constexpr auto crawlingTableAlias{"a"sv};

	//! Alias, used in SQL queries, for the URL list table.
	constexpr auto urlListTableAlias{"b"sv};

	///@}

	/*
	 * DECLARATION
	 */

	//! Class providing database functionality for crawler threads by implementing Wrapper::Database.
	class Database final : public Wrapper::Database {
		// for convenience
		using IdString = std::pair<std::uint64_t, std::string>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		///@name Construction
		///@{

		explicit Database(Module::Database& dbThread);

		///@}
		///@name Crawler-specific Setters
		///@{

		void setMaxBatchSize(std::uint16_t setMaxBatchSize);
		void setRecrawl(bool isRecrawl);
		void setUrlCaseSensitive(bool isUrlCaseSensitive);
		void setUrlDebug(bool isUrlDebug);
		void setUrlStartupCheck(bool isUrlStartupCheck);

		///@}
		///@name Prepared SQL Statements
		///@{

		void prepare();

		///@}
		///@name URLs
		///@{

		[[nodiscard]] std::uint64_t getUrlId(const std::string& url);
		[[nodiscard]] IdString getNextUrl(std::uint64_t currentUrlId);
		bool addUrlIfNotExists(const std::string& urlString, bool manual);
		std::size_t addUrlsIfNotExist(std::queue<std::string>& urls, bool manual);
		[[nodiscard]] std::uint64_t getUrlPosition(std::uint64_t urlId);
		[[nodiscard]] std::uint64_t getNumberOfUrls();

		///@}
		///@name URL Checking
		///@{

		void urlDuplicationCheck();
		void urlHashCheck();
		void urlEmptyCheck();
		void urlUtf8Check();

		///@}
		///@name URL Locking
		///@{

		[[nodiscard]] std::string getUrlLockTime(std::uint64_t urlId);
		[[nodiscard]] bool isUrlCrawled(std::uint64_t urlId);
		[[nodiscard]] std::string lockUrlIfOk(std::uint64_t urlId, const std::string& lockTime, std::uint32_t lockTimeout);
		void unLockUrlIfOk(std::uint64_t urlId, const std::string& lockTime);
		void setUrlFinishedIfOk(std::uint64_t urlId, const std::string& lockTime);

		///@}
		///@name Crawling
		///@{

		void saveContent(
				std::uint64_t urlId,
				std::uint32_t response,
				const std::string& type,
				const std::string& content
		);
		void saveArchivedContent(
				std::uint64_t urlId,
				const std::string& timeStamp,
				std::uint32_t response,
				const std::string& type,
				const std::string& content);
		[[nodiscard]] bool isArchivedContentExists(std::uint64_t urlId, const std::string& timeStamp);

		///@}

		//! Class for crawler-specific database exceptions.
		MAIN_EXCEPTION_CLASS();

	private:
		// options
		bool recrawl{false};
		bool urlCaseSensitive{true};
		bool urlDebug{false};
		bool urlStartupCheck{true};
		std::uint16_t maxBatchSize{defaultMaxBatchSize};

		// table names
		std::string urlListTable;
		std::string crawlingTable;

		// IDs of prepared SQL statements
		struct _ps {
			std::uint16_t getUrlId;
			std::uint16_t getNextUrl;
			std::uint16_t addUrlIfNotExists;
			std::uint16_t add10UrlsIfNotExist;
			std::uint16_t add100UrlsIfNotExist;
			std::uint16_t addMaxUrlsIfNotExist;
			std::uint16_t getUrlPosition;
			std::uint16_t getNumberOfUrls;
			std::uint16_t getUrlLockTime;
			std::uint16_t isUrlCrawled;
			std::uint16_t addUrlLockIfOk;
			std::uint16_t renewUrlLockIfOk;
			std::uint16_t unLockUrlIfOk;
			std::uint16_t setUrlFinishedIfOk;
			std::uint16_t saveContent;
			std::uint16_t saveArchivedContent;
			std::uint16_t isArchivedContentExists;
			std::uint16_t urlDuplicationCheck;
			std::uint16_t urlHashCheck;
			std::uint16_t urlHashCorrect;
			std::uint16_t urlEmptyCheck;
			std::uint16_t getUrls;
			std::uint16_t removeDuplicates;
		} ps{};

		// internal helper functions
		std::string queryAddUrlsIfNotExist(std::size_t numberOfUrls, const std::string& hashQuery);
		[[nodiscard]] std::queue<std::string> getUrls();
		std::uint32_t removeDuplicates(const std::string& url);
	};

} /* namespace crawlservpp::Module::Crawler */

#endif /* MODULE_CRAWLER_DATABASE_HPP_ */
