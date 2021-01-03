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
 * This class provides database functionality for a parser thread
 *  by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_DATABASE_HPP_
#define MODULE_PARSER_DATABASE_HPP_

#include "../../Helper/Portability/mysqlcppconn.h"
#include "../../Main/Exception.hpp"
#include "../../Struct/DataEntry.hpp"
#include "../../Struct/StatusSetter.hpp"
#include "../../Struct/TableColumn.hpp"
#include "../../Struct/TargetTableProperties.hpp"
#include "../../Wrapper/Database.hpp"

#include "Config.hpp"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <algorithm>	// std::count_if
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
#include <vector>		// std::vector

namespace crawlservpp::Module::Parser {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! Maximum size of database content (= 1 GiB).
	inline constexpr auto maxContentSize{1073741824};

	//! Maximum size of database content as string.
	inline constexpr auto maxContentSizeString{"1 GiB"sv};

	///@}
	///@name Constants for MySQL Queries
	///@{

	//! Process one value at once.
	inline constexpr auto oneAtOnce{1};

	//! Process ten values at once.
	inline constexpr auto nAtOnce10{10};

	//! Process one hundred values at once.
	inline constexpr auto nAtOnce100{100};

	//! First argument in a SQL query.
	inline constexpr auto sqlArg1{1};

	//! Second argument in a SQL query.
	inline constexpr auto sqlArg2{2};

	//! Third argument in a SQL query.
	inline constexpr auto sqlArg3{3};

	//! Fourth argument in a SQL query.
	inline constexpr auto sqlArg4{4};

	//! Fifth argument in a SQL query.
	inline constexpr auto sqlArg5{5};

	//! Sixth argument in a SQL query.
	inline constexpr auto sqlArg6{6};

	//! Alias, used in SQL queries, for the parsing table.
	inline constexpr auto parsingTableAlias{"a"sv};

	//! Alias, used in SQL queries, for the target table.
	inline constexpr auto targetTableAlias{"b"sv};

	//! Minimum number of columns in the target table.
	inline constexpr auto minTargetColumns{4};

	//! Number of arguments for locking one URL.
	inline constexpr auto numArgsLockUrl{3};

	//! Minimum number of arguments to add or update a data entry.
	inline constexpr auto minArsgAddUpdateData{5};

	//! Number of arguments for setting one URL to finished.
	inline constexpr auto numArgsFinishUrl{2};

	//! The maximum value of a DATETIME in the database.
	inline constexpr auto maxDateTimeValue{"9999-12-31 23:59:59"sv};

	///@}

	/*
	 * DECLARATION
	 */

	//! Class providing database functionality for parser threads by implementing Wrapper::Database.
	class Database final : public Wrapper::Database {
		// for convenience
		using DataEntry = Struct::DataEntry;
		using StatusSetter = Struct::StatusSetter;
		using TableColumn = Struct::TableColumn;
		using TargetTableProperties = Struct::TargetTableProperties;

		using IdString = std::pair<std::uint64_t, std::string>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		///@name Construction
		///@{

		explicit Database(Module::Database& dbThread);

		///@}
		///@name Parser-specific Setters
		///@{

		// setters
		void setCacheSize(std::uint64_t setCacheSize);
		void setMaxBatchSize(std::uint16_t setMaxBatchSize);
		void setReparse(bool isReparse);
		void setParseCustom(bool isParseCustom);
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields);

		///@}
		///@name Target Table Initialization
		///@{

		void initTargetTable();

		///@}
		///@name Prepared SQL Statements
		///@{

		void prepare();

		///@}
		///@name URLs
		///@{

		[[nodiscard]] std::string fetchUrls(
				std::uint64_t lastId,
				std::queue<IdString>& cache,
				std::uint32_t lockTimeout
		);
		[[nodiscard]] std::uint64_t getUrlPosition(std::uint64_t urlId);
		[[nodiscard]] std::uint64_t getNumberOfUrls();

		///@}
		///@name URL Locking
		///@{

		[[nodiscard]] std::string getLockTime(std::uint32_t lockTimeout);
		[[nodiscard]] std::string getUrlLockTime(std::uint64_t urlId);
		[[nodiscard]] std::string renewUrlLockIfOk(
				std::uint64_t urlId,
				const std::string& lockTime,
				std::uint32_t lockTimeout
		);
		bool unLockUrlIfOk(std::uint64_t urlId, const std::string& lockTime);
		void unLockUrlsIfOk(std::queue<IdString>& urls, std::string& lockTime);

		///@}
		///@name Parsing
		///@{

		std::uint32_t checkParsingTable();
		[[nodiscard]] std::uint64_t getNumberOfContents(std::uint64_t urlId);
		bool getLatestContent(
				std::uint64_t urlId,
				const std::string& lastDateTime,
				IdString& contentTo,
				std::string& dateTimeTo
		);
		[[nodiscard]] std::queue<IdString> getAllContents(std::uint64_t urlId);
		[[nodiscard]] std::uint64_t getContentIdFromParsedId(
				const std::string& parsedId
		);
		void updateOrAddEntries(
				std::queue<DataEntry>& entries,
				StatusSetter& statusSetter
		);
		void setUrlsFinishedIfLockOk(std::queue<IdString>& finished);
		void updateTargetTable();

		///@}

		//! Class for parser database exceptions.
		MAIN_EXCEPTION_CLASS();

	private:
		// options
		std::uint64_t cacheSize{defaultCacheSize};
		std::uint16_t maxBatchSize{defaultMaxBatchSize};
		bool reParse{false};
		bool parseCustom{true};
		std::string targetTableName;
		std::vector<std::string> targetFieldNames;

		// table names and target table ID
		std::string urlListTable;
		std::string parsingTable;
		std::uint64_t targetTableId{};
		std::string targetTableFull;

		// IDs of prepared SQL statements
		struct _ps {
			std::size_t fetchUrls{};
			std::size_t lockUrl{};
			std::size_t lock10Urls{};
			std::size_t lock100Urls{};
			std::size_t lockMaxUrls{};
			std::size_t getUrlPosition{};
			std::size_t getNumberOfUrls{};
			std::size_t getLockTime{};
			std::size_t getUrlLockTime{};
			std::size_t renewUrlLockIfOk{};
			std::size_t unLockUrlIfOk{};
			std::size_t checkParsingTable{};
			std::size_t getNumberOfContents{};
			std::size_t getLatestContent{};
			std::size_t getAllContents{};
			std::size_t getContentIdFromParsedId{};
			std::size_t updateOrAddEntry{};
			std::size_t updateOrAdd10Entries{};
			std::size_t updateOrAdd100Entries{};
			std::size_t updateOrAddMaxEntries{};
			std::size_t setUrlFinishedIfLockOk{};
			std::size_t set10UrlsFinishedIfLockOk{};
			std::size_t set100UrlsFinishedIfLockOk{};
			std::size_t setMaxUrlsFinishedIfLockOk{};
			std::size_t updateTargetTable{};
		} ps;

		// internal helper function
		bool checkEntrySize(DataEntry& entry);
		[[nodiscard]] std::string queryLockUrls(std::size_t numberOfUrls);
		[[nodiscard]] std::string queryUpdateOrAddEntries(std::size_t numberOfEntries);
		[[nodiscard]] std::string querySetUrlsFinishedIfLockOk(std::size_t numberOfUrls);
		[[nodiscard]] std::string queryUnlockUrlsIfOk(std::size_t numberOfUrls);
	};

} /* namespace crawlservpp::Module::Parser */

#endif /* MODULE_PARSER_DATABASE_HPP_ */
