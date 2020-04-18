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
 * This class provides database functionality for a parser thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_DATABASE_HPP_
#define MODULE_PARSER_DATABASE_HPP_

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

#include <algorithm>	// std::count_if
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <locale>		// std::locale
#include <memory>		// std::unique_ptr
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Parser {

	class Database : public Wrapper::Database {
		// for convenience
		using TargetTableProperties = Struct::TargetTableProperties;
		using DataEntry = Struct::DataEntry;
		using TableColumn = Struct::TableColumn;

		using IdString = std::pair<std::uint64_t, std::string>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		Database(Module::Database& dbRef);
		virtual ~Database();

		// setters
		void setCacheSize(std::uint64_t setCacheSize);
		void setReparse(bool isReparse);
		void setParseCustom(bool isParseCustom);
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields);

		// prepare target table and SQL statements for parser
		void initTargetTable();
		void prepare();

		// URL functions
		std::string fetchUrls(std::uint64_t lastId, std::queue<IdString>& cache, std::uint32_t lockTimeout);
		std::uint64_t getUrlPosition(std::uint64_t urlId);
		std::uint64_t getNumberOfUrls();

		// URL locking functions
		std::string getLockTime(std::uint32_t lockTimeout);
		std::string getUrlLockTime(std::uint64_t urlId);
		std::string renewUrlLockIfOk(std::uint64_t urlId, const std::string& lockTime, std::uint32_t lockTimeout);
		bool unLockUrlIfOk(std::uint64_t urlId, const std::string& lockTime);
		void unLockUrlsIfOk(std::queue<IdString>& urls, std::string& lockTime);

		// parsing functions
		std::uint32_t checkParsingTable();
		std::uint64_t getNumberOfContents(std::uint64_t urlId);
		bool getLatestContent(std::uint64_t urlId, std::uint64_t index, IdString& contentTo);
		std::queue<IdString> getAllContents(std::uint64_t urlId);
		std::uint64_t getContentIdFromParsedId(const std::string& parsedId);
		void updateOrAddEntries(std::queue<DataEntry>& entries);
		void setUrlsFinishedIfLockOk(std::queue<IdString>& finished);
		void updateTargetTable();

		// constant strings for table aliases (public)
		const std::string parsingTableAlias;
		const std::string targetTableAlias;

		// class for Parser::Database exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// options
		std::uint64_t cacheSize;
		bool reparse;
		bool parseCustom;
		std::string targetTableName;
		std::vector<std::string> targetFieldNames;

		// table names and target table ID
		std::string urlListTable;
		std::string parsingTable;
		std::string analyzingTable;
		std::uint64_t targetTableId;
		std::string targetTableFull;

	private:
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
			std::uint16_t checkParsingTable;
			std::uint16_t getNumberOfContents;
			std::uint16_t getLatestContent;
			std::uint16_t getAllContents;
			std::uint16_t getContentIdFromParsedId;
			std::uint16_t updateOrAddEntry;
			std::uint16_t updateOrAdd10Entries;
			std::uint16_t updateOrAdd100Entries;
			std::uint16_t updateOrAdd1000Entries;
			std::uint16_t setUrlFinishedIfLockOk;
			std::uint16_t set10UrlsFinishedIfLockOk;
			std::uint16_t set100UrlsFinishedIfLockOk;
			std::uint16_t set1000UrlsFinishedIfLockOk;
			std::uint16_t updateTargetTable;
		} ps;

		// internal helper function
		bool checkEntrySize(DataEntry& entry);
		std::string queryLockUrls(std::size_t numberOfUrls);
		std::string queryUpdateOrAddEntries(std::size_t numberOfEntries);
		std::string querySetUrlsFinishedIfLockOk(std::size_t numberOfUrls);
		std::string queryUnlockUrlsIfOk(std::size_t numberOfUrls);
	};

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_PARSER_DATABASE_HPP_ */
