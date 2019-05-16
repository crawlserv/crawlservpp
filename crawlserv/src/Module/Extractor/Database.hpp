/*
 * Database.hpp
 *
 * This class provides database functionality for a extractor thread by implementing the Wrapper::Database interface.
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
		typedef Struct::DataEntry DataEntry;
		typedef Struct::TargetTableProperties TargetTableProperties;
		typedef Struct::TableColumn TableColumn;

		typedef std::pair<unsigned long, std::string> IdString;
		typedef std::pair<std::string, std::string> StringString;
		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;

	public:
		Database(Module::Database& dbRef);
		virtual ~Database();

		// setters
		void setCacheSize(unsigned long setCacheSize);
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
		std::string fetchUrls(unsigned long lastId, std::queue<IdString>& cache, unsigned long lockTimeout);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL locking functions
		std::string getLockTime(unsigned long lockTimeout);
		std::string getUrlLockTime(unsigned long urlId);
		std::string renewUrlLockIfOk(unsigned long urlId, const std::string& lockTime, unsigned long lockTimeout);
		bool unLockUrlIfOk(unsigned long urlId, const std::string& lockTime);
		void unLockUrlsIfOk(std::queue<IdString>& urls, std::string& lockTime);

		// extracting functions
		unsigned int checkExtractingTable();
		bool getContent(unsigned long urlId, IdString& contentTo);
		std::string getParsedData(unsigned long urlId, unsigned long sourceIndex);
		void updateOrAddEntries(std::queue<DataEntry>& entries);
		void setUrlsFinishedIfLockOk(std::queue<IdString>& finished);
		void updateTargetTable();

		// constant strings for table aliases (public)
		const std::string extractingTableAlias;
		const std::string targetTableAlias;

		// sub-class for Parser::Database exceptions
		class Exception : public Main::Exception {
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
			virtual ~Exception() {}
		};

	protected:
		// options
		unsigned long cacheSize;
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
		unsigned long targetTableId;
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
		std::vector<unsigned short> psGetParsedData;

		// internal helper function
		bool checkEntrySize(DataEntry& entry);
		std::string queryLockUrls(unsigned int numberOfUrls);
		std::string queryUpdateOrAddEntries(unsigned int numberOfEntries);
		std::string querySetUrlsFinishedIfLockOk(unsigned int numberOfUrls);
		std::string queryUnlockUrlsIfOk(unsigned int numberOfUrls);
	};

} /* crawlservpp::Module::Extractor */

#endif /* MODULE_EXTRACTOR_DATABASE_HPP_ */
