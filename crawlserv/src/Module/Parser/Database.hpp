/*
 * Database.hpp
 *
 * This class provides database functionality for a parser thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_PARSER_DATABASE_HPP_
#define MODULE_PARSER_DATABASE_HPP_

#include "../../Struct/ParsingEntry.hpp"
#include "../../Struct/TableColumn.hpp"
#include "../../Struct/TargetTableProperties.hpp"
#include "../../Wrapper/Database.hpp"

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
		typedef Wrapper::TargetTablesLock TargetTablesLock;

		typedef std::function<bool()> CallbackIsRunning;
		typedef std::pair<unsigned long, std::string> IdString;
		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;

	public:
		Database(Module::Database& dbRef);
		virtual ~Database();

		// setters
		void setId(unsigned long analyzerId);
		void setWebsite(unsigned long websiteId);
		void setUrlList(unsigned long listId);
		void setNamespaces(const std::string& website, const std::string& urlList);
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
		std::string fetchUrls(unsigned long lastId, std::queue<IdString>& cache, unsigned long lockTimeout);
		unsigned long getUrlPosition(unsigned long urlId);
		unsigned long getNumberOfUrls();

		// URL locking functions
		std::string getLockTime(unsigned long lockTimeout);
		std::string getUrlLockTime(unsigned long urlId);
		std::string renewUrlLockIfOk(unsigned long urlId, const std::string& lockTime, unsigned long lockTimeout);
		bool unLockUrlIfOk(unsigned long urlId, const std::string& lockTime);
		void unLockUrlsIfOk(std::queue<IdString>& urls, const std::string& lockTime);

		// parsing functions
		unsigned int checkParsingTable();
		bool getLatestContent(unsigned long urlId, unsigned long index, IdString& contentTo);
		std::queue<IdString> getAllContents(unsigned long urlId);
		unsigned long getContentIdFromParsedId(const std::string& parsedId);
		void updateOrAddEntries(std::queue<ParsingEntry>& entries, std::queue<std::string>& logEntriesTo);
		void setUrlsFinishedIfLockOk(std::queue<IdString>& finished);
		void updateTargetTable();

		// constant strings for table aliases (public)
		const std::string parsingTableAlias;
		const std::string targetTableAlias;

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
			unsigned short checkParsingTable;
			unsigned short getLatestContent;
			unsigned short getAllContents;
			unsigned short getContentIdFromParsedId;
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

		// internal helper function
		bool checkEntrySize(ParsingEntry& entry, std::queue<std::string>& logEntriesTo);
		std::string queryLockUrls(unsigned int numberOfUrls);
		std::string queryUpdateOrAddEntries(unsigned int numberOfEntries);
		std::string querySetUrlsFinishedIfLockOk(unsigned int numberOfUrls);
		std::string queryUnlockUrlsIfOk(unsigned int numberOfUrls);
	};

} /* crawlservpp::Module::Crawler */

#endif /* MODULE_PARSER_DATABASE_HPP_ */
