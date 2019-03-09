/*
 * Database.h
 *
 * This class provides database functionality for a parser thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_DATABASE_H_
#define MODULE_PARSER_DATABASE_H_

#include "../Database.h"

#include "../../Struct/ParsingEntry.h"
#include "../../Struct/TableColumn.h"
#include "../../Struct/TargetTableProperties.h"
#include "../../Struct/UrlProperties.h"
#include "../../Wrapper/Database.h"
#include "../../Wrapper/TableLock.h"
#include "../../Wrapper/TargetTablesLock.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <mysql_connection.h>

#include <chrono>
#include <fstream>
#include <functional>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace crawlservpp::Module::Parser {
	class Database : public Wrapper::Database {
		// for convenience
		typedef Main::Database::Exception DatabaseException;
		typedef Struct::TargetTableProperties TargetTableProperties;
		typedef Struct::ParsingEntry ParsingEntry;
		typedef Struct::TableColumn TableColumn;
		typedef Struct::UrlProperties UrlProperties;
		typedef Wrapper::TableLock<Wrapper::Database> TableLock;
		typedef Wrapper::TargetTablesLock TargetTablesLock;

		typedef std::function<bool()> CallbackIsRunning;
		typedef std::pair<unsigned long, std::string> IdString;
		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;

	public:
		Database(Module::Database& dbRef, const std::string& setTargetTableAlias);
		virtual ~Database();

		// setters
		void setId(unsigned long analyzerId);
		void setWebsite(unsigned long websiteId);
		void setWebsiteNamespace(const std::string& websiteNamespace);
		void setUrlList(unsigned long listId);
		void setUrlListNamespace(const std::string& urlListNamespace);
		void setCacheSize(unsigned long setCacheSize);
		void setReparse(bool isReparse);
		void setParseCustom(bool isParseCustom);
		void setLogging(bool isLogging);
		void setVerbose(bool isVerbose);
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields);
		void setTimeoutTargetLock(unsigned long timeOut);

		// prepare target table and SQL statements for parser
		void initTargetTable(CallbackIsRunning isRunning);
		void prepare();

		// URL functions
		void fetchUrls(unsigned long lastId, std::queue<UrlProperties>& cache);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL locking functions
		std::string getLockTime(unsigned long lockId);
		void getLockId(UrlProperties& urlProperties);
		std::string lockUrlIfOk(UrlProperties& urlProperties, unsigned long lockTimeout);
		void unLockUrlIfOk(unsigned long lockId, const std::string& lockTime);

		// parsing functions
		bool getLatestContent(unsigned long urlId, unsigned long index, IdString& contentTo);
		std::queue<IdString> getAllContents(unsigned long urlId);
		unsigned long getContentIdFromParsedId(const std::string& parsedId);
		void updateOrAddEntries(std::queue<ParsingEntry>& entries, std::queue<std::string>& logEntriesTo);
		void setUrlsFinishedIfLockOk(std::queue<IdString>& finished);
		void updateTargetTable();

	protected:
		// options
		std::string idString;
		unsigned long website;
		std::string websiteIdString;
		std::string websiteName;
		unsigned long urlList;
		std::string listIdString;
		std::string urlListName;
		unsigned long cacheSize;
		bool reparse;
		bool parseCustom;
		bool logging;
		bool verbose;
		std::string targetTableName;
		std::vector<std::string> targetFieldNames;
		unsigned long timeoutTargetLock;

		// table names and target table ID
		std::string urlListTable;
		std::string parsingTable;
		std::string analyzingTable;
		unsigned long targetTableId;
		std::string targetTableFull;

	private:
		// IDs of prepared SQL statements
		struct {
			unsigned short fetchUrls;
			unsigned short getUrlPosition;
			unsigned short getNumberOfUrls;
			unsigned short getLockTime;
			unsigned short getLockId;
			unsigned short lockUrlIfOk;
			unsigned short addUrlLock;
			unsigned short unLockUrlIfOk;
			unsigned short getContentIdFromParsedId;
			unsigned short getLatestContent;
			unsigned short getAllContents;
			unsigned short setUrlFinishedIfLockOk;
			unsigned short set10UrlsFinishedIfLockOk;
			unsigned short set100UrlsFinishedIfLockOk;
			unsigned short set1000UrlsFinishedIfLockOk;
			unsigned short updateOrAddEntry;
			unsigned short updateOrAdd10Entries;
			unsigned short updateOrAdd100Entries;
			unsigned short updateOrAdd1000Entries;
			unsigned short updateTargetTable;
		} ps;

		// constant string for table aliases
		const std::string targetTableAlias;

		// internal helper function
		bool checkEntry(ParsingEntry& entry, std::queue<std::string>& logEntriesTo);
		std::string queryUpdateOrAddEntries(unsigned int numberOfEntries);
		std::string querySetUrlsFinishedIfLockOk(unsigned int numberOfUrls);
	};
}

#endif /* MODULE_PARSER_DATABASE_H_ */
