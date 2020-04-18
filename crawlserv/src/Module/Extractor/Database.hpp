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
 * This class provides database functionality for an extractor thread by implementing the Wrapper::Database interface.
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
#include <cstddef>		// std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
#include <memory>		// std::unique_ptr
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <utility>		// std::pair
#include <vector>		// std::vector

namespace crawlservpp::Module::Extractor {

	class Database : public Wrapper::Database {
		// for convenience
		using DataEntry = Struct::DataEntry;
		using TargetTableProperties = Struct::TargetTableProperties;
		using TableColumn = Struct::TableColumn;

		using IdString = std::pair<std::uint64_t, std::string>;
		using StringString = std::pair<std::string, std::string>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		Database(Module::Database& dbRef);
		virtual ~Database();

		// setters
		void setCacheSize(std::uint64_t setCacheSize);
		void setReextract(bool isReextract);
		void setExtractCustom(bool isParseCustom);
		void setRawContentIsSource(bool isRawContentIsSource);
		void setSources(std::queue<StringString>& tablesAndColumns);
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields);
		void setOverwrite(bool isOverwrite);

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

		// extracting functions
		std::uint32_t checkExtractingTable();
		bool getContent(std::uint64_t urlId, IdString& contentTo);
		void getLatestParsedData(std::uint64_t urlId, std::size_t sourceIndex, std::string& resultTo);
		void updateOrAddEntries(std::queue<DataEntry>& entries);
		void setUrlsFinishedIfLockOk(std::queue<IdString>& finished);
		void updateTargetTable();

		// constant strings for table aliases (public)
		const std::string extractingTableAlias;
		const std::string targetTableAlias;

		// class for Parser::Database exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// options
		std::uint64_t cacheSize;
		bool reextract;
		bool extractCustom;
		std::string targetTableName;
		std::vector<std::string> targetFieldNames;
		bool overwrite;

		// sources
		bool rawContentIsSource;
		std::queue<StringString> sources;

		// table names and target table ID
		std::string urlListTable;
		std::string extractingTable;
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
			std::uint16_t checkExtractingTable;
			std::uint16_t getContent;
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

		// prepared SQL statements for getting parsed data
		std::vector<std::uint16_t> psGetLatestParsedData;

		// internal helper function
		bool checkEntrySize(DataEntry& entry);
		std::string queryLockUrls(std::uint32_t numberOfUrls);
		std::string queryUpdateOrAddEntries(std::uint32_t numberOfEntries);
		std::string querySetUrlsFinishedIfLockOk(std::uint32_t numberOfUrls);
		std::string queryUnlockUrlsIfOk(std::uint32_t numberOfUrls);
	};

} /* crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_DATABASE_HPP_ */
