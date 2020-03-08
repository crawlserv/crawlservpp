/*
 *
 * ---
 *
 *  Copyright (C) 2019-2020 Anselm Schmidt (ans[ät]ohai.su)
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

		using IdString = std::pair<size_t, std::string>;
		using StringString = std::pair<std::string, std::string>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		Database(Module::Database& dbRef);
		virtual ~Database();

		// setters
		void setCacheSize(size_t setCacheSize);
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
		std::string fetchUrls(size_t lastId, std::queue<IdString>& cache, size_t lockTimeout);
		size_t getUrlPosition(size_t urlId);
		size_t getNumberOfUrls();

		// URL locking functions
		std::string getLockTime(size_t lockTimeout);
		std::string getUrlLockTime(size_t urlId);
		std::string renewUrlLockIfOk(size_t urlId, const std::string& lockTime, size_t lockTimeout);
		bool unLockUrlIfOk(size_t urlId, const std::string& lockTime);
		void unLockUrlsIfOk(std::queue<IdString>& urls, std::string& lockTime);

		// extracting functions
		unsigned int checkExtractingTable();
		bool getContent(size_t urlId, IdString& contentTo);
		void getLatestParsedData(size_t urlId, size_t sourceIndex, std::string& resultTo);
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
		size_t cacheSize;
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
		size_t targetTableId;
		std::string targetTableFull;

	private:
		// IDs of prepared SQL statements
		struct _ps {
			unsigned short fetchUrls;
			unsigned short lockUrl;
			unsigned short lock10Urls;
			unsigned short lock100Urls;
			unsigned short lock1000Urls;
			unsigned short getUrlPosition;
			unsigned short getNumberOfUrls;
			unsigned short getLockTime;
			unsigned short getUrlLockTime;
			unsigned short renewUrlLockIfOk;
			unsigned short unLockUrlIfOk;
			unsigned short checkExtractingTable;
			unsigned short getContent;
			unsigned short updateOrAddEntry;
			unsigned short updateOrAdd10Entries;
			unsigned short updateOrAdd100Entries;
			unsigned short updateOrAdd1000Entries;
			unsigned short setUrlFinishedIfLockOk;
			unsigned short set10UrlsFinishedIfLockOk;
			unsigned short set100UrlsFinishedIfLockOk;
			unsigned short set1000UrlsFinishedIfLockOk;
			unsigned short updateTargetTable;
		} ps;

		// prepared SQL statements for getting parsed data
		std::vector<unsigned short> psGetLatestParsedData;

		// internal helper function
		bool checkEntrySize(DataEntry& entry);
		std::string queryLockUrls(unsigned int numberOfUrls);
		std::string queryUpdateOrAddEntries(unsigned int numberOfEntries);
		std::string querySetUrlsFinishedIfLockOk(unsigned int numberOfUrls);
		std::string queryUnlockUrlsIfOk(unsigned int numberOfUrls);
	};

} /* crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_DATABASE_HPP_ */
