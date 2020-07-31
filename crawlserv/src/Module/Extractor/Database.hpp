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
 * This class provides database functionality for an extractor thread
 *  by implementing the Wrapper::Database interface.
 *
 *  Created on: May 9, 2019
 *      Author: ans
 */

#ifndef MODULE_EXTRACTOR_DATABASE_HPP_
#define MODULE_EXTRACTOR_DATABASE_HPP_

#include "../../Helper/Portability/mysqlcppconn.h"
#include "../../Main/Exception.hpp"
#include "../../Struct/DataEntry.hpp"
#include "../../Struct/TableColumn.hpp"
#include "../../Struct/TargetTableProperties.hpp"
#include "../../Wrapper/Database.hpp"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <algorithm>	// std::count_if, std::find_if
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <memory>		// std::unique_ptr
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view, std::string_view_literals
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Extractor {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! The default size of the URL cache.
	constexpr auto defaultUrlCacheSize{2500};

	//! The minimum number of columns in the target table.
	constexpr auto minTargetColumns{4};

	//! The minimum number of columns in the linked target table.
	constexpr auto minLinkedColumns{2};

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

	//! Process one thousand values at once.
	constexpr auto nAtOnce1000{1000};

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

	//! Alias, used in SQL queries, for the extracting table.
	constexpr auto extractingTableAlias{"a"sv};

	//! Alias, used in SQL queries, for the target table.
	constexpr auto targetTableAlias{"b"sv};

	//! Alias, used in SQL queries, for the linked target table.
	constexpr auto linkedTableAlias{"c"sv};

	///@}

	/*
	 * DECLARATION
	 */

	//! Class providing database functionality for extractor threads by implementing Wrapper::Database.
	class Database final : public Wrapper::Database {
		// for convenience
		using DataEntry = Struct::DataEntry;
		using TargetTableProperties = Struct::TargetTableProperties;
		using TableColumn = Struct::TableColumn;

		using IdString = std::pair<std::uint64_t, std::string>;
		using StringString = std::pair<std::string, std::string>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		///@name Construction
		///@{

		explicit Database(Module::Database& dbThread);

		///@}
		///@name Extractor-specific Setters
		///@{

		void setCacheSize(std::uint64_t setCacheSize);
		void setReExtract(bool isReExtract);
		void setExtractCustom(bool isExtractCustom);
		void setRawContentIsSource(bool isRawContentIsSource);
		void setSources(std::queue<StringString>& tablesAndColumns);
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields);
		void setLinkedTable(const std::string& table);
		void setLinkedField(const std::string& field);
		void setLinkedFields(const std::vector<std::string>& fields);
		void setOverwrite(bool isOverwrite);
		void setOverwriteLinked(bool isOverwrite);

		///@}
		///@name Target Table Initialization
		///@{

		void initTargetTables();

		///@}
		///@name Prepared SQL Statements
		///@{

		void prepare();

		///@}
		///@name URLs
		///@{

		[[nodiscard]] std::string fetchUrls(std::uint64_t lastId, std::queue<IdString>& cache, std::uint32_t lockTimeout);
		[[nodiscard]] std::uint64_t getUrlPosition(std::uint64_t urlId);
		[[nodiscard]] std::uint64_t getNumberOfUrls();

		///@}
		///@name URL Locking
		///@{

		[[nodiscard]] std::string getLockTime(std::uint32_t lockTimeout);
		[[nodiscard]] std::string getUrlLockTime(std::uint64_t urlId);
		[[nodiscard]] std::string renewUrlLockIfOk(std::uint64_t urlId, const std::string& lockTime, std::uint32_t lockTimeout);
		bool unLockUrlIfOk(std::uint64_t urlId, const std::string& lockTime);
		void unLockUrlsIfOk(std::queue<IdString>& urls, std::string& lockTime);

		///@}
		///@name Extracting
		///@{

		std::uint32_t checkExtractingTable();
		bool getContent(std::uint64_t urlId, IdString& contentTo);
		void getLatestParsedData(std::uint64_t urlId, std::size_t sourceIndex, std::string& resultTo);
		void updateOrAddEntries(std::queue<DataEntry>& entries);
		void updateOrAddLinked(std::queue<DataEntry>& entries);
		void setUrlsFinishedIfLockOk(std::queue<IdString>& finished);
		void updateTargetTable();

		//! Class for parser database exceptions.
		MAIN_EXCEPTION_CLASS();

	private:
		// options
		std::uint64_t cacheSize{defaultUrlCacheSize};
		bool reExtract{false};
		bool extractCustom{false};
		std::string targetTableName;
		std::string linkedTableName;
		std::vector<std::string> targetFieldNames;
		std::vector<std::string> linkedFieldNames;
		bool overwrite{true};
		bool overwriteLinked{true};
		bool linked{false};

		// sources
		bool rawContentIsSource{false};
		std::queue<StringString> sources;

		// table names, target table IDs, and linked field
		std::string urlListTable;
		std::string extractingTable;
		std::uint64_t targetTableId{0};
		std::uint64_t linkedTableId{0};
		std::string targetTableFull;
		std::string linkedTableFull;
		std::string linkedField;
		std::uint64_t linkedIndex{0};

		// IDs of prepared SQL statements
		struct _ps {
			std::uint16_t fetchUrls;
			std::uint16_t lockUrl;
			std::uint16_t lock10Urls;
			std::uint16_t lock100Urls;
			std::uint16_t lock1000Urls;
			std::uint16_t getUrlPosition;
			std::uint16_t getNumberOfUrls;
			std::uint16_t getLockTime;
			std::uint16_t getUrlLockTime;
			std::uint16_t renewUrlLockIfOk;
			std::uint16_t unLockUrlIfOk;
			std::uint16_t checkExtractingTable;
			std::uint16_t getContent;
			std::uint16_t updateOrAddEntry;
			std::uint16_t updateOrAddLinked;
			std::uint16_t updateOrAdd10Entries;
			std::uint16_t updateOrAdd10Linked;
			std::uint16_t updateOrAdd100Entries;
			std::uint16_t updateOrAdd100Linked;
			std::uint16_t updateOrAdd1000Entries;
			std::uint16_t updateOrAdd1000Linked;
			std::uint16_t setUrlFinishedIfLockOk;
			std::uint16_t set10UrlsFinishedIfLockOk;
			std::uint16_t set100UrlsFinishedIfLockOk;
			std::uint16_t set1000UrlsFinishedIfLockOk;
			std::uint16_t updateTargetTable;
		} ps{};

		// prepared SQL statements for getting parsed data
		std::vector<std::uint16_t> psGetLatestParsedData;

		// internal helper functions
		bool checkEntrySize(DataEntry& entry);
		[[nodiscard]] std::string queryLockUrls(std::size_t numberOfUrls);
		[[nodiscard]] std::string queryUpdateOrAddEntries(std::size_t numberOfEntries);
		[[nodiscard]] std::string queryUpdateOrAddLinked(std::size_t numberOfEntries);
		[[nodiscard]] std::string querySetUrlsFinishedIfLockOk(std::size_t numberOfUrls);
		[[nodiscard]] std::string queryUnlockUrlsIfOk(std::size_t numberOfUrls);
	};

} /* namespace crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_DATABASE_HPP_ */
