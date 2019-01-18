/*
 * Database.h
 *
 * A class to handle database access for crawlserv and its threads.
 * Thread-specific functionality is not implemented in this (parent) class.
 *
 * NOT THREAD-SAFE!
 * Use instances of the child class DatabaseThread for thread-specific functionality
 * and child classes of DatabaseModule for module-specific functionality instead.
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#ifndef MAIN_DATABASE_H_
#define MAIN_DATABASE_H_

#define MAIN_DATABASE_DELETE(X) { if(X) { X->close(); delete X; X = NULL; } }

#include "Data.h"

#include "../Helper/FileSystem.h"
#include "../Struct/DatabaseSettings.h"
#include "../Struct/ThreadDatabaseEntry.h"
#include "../Struct/ThreadOptions.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <experimental/filesystem>

#include <chrono>
#include <ctime>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace crawlservpp::Main {
	class Database {
	public:
		Database(const crawlservpp::Struct::DatabaseSettings& dbSettings);
		virtual ~Database();

		bool connect();
		bool initializeSql();
		bool prepare();
		void setSleepOnError(unsigned long seconds);

		// getters of member variables
		const crawlservpp::Struct::DatabaseSettings& getSettings() const;
		const std::string& getErrorMessage() const;

		// logging functions
		void log(const std::string& logModule, const std::string& logEntry);
		unsigned long getNumberOfLogEntries(const std::string& logModule);
		void clearLogs(const std::string& logModule);

		// thread functions
		std::vector<crawlservpp::Struct::ThreadDatabaseEntry> getThreads();
		unsigned long addThread(const std::string& threadModule, const crawlservpp::Struct::ThreadOptions& threadOptions);
		unsigned long getThreadRunTime(unsigned long threadId);
		unsigned long getThreadPauseTime(unsigned long threadId);
		void setThreadStatus(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage);
		void setThreadStatus(unsigned long threadId, const std::string& threadStatusMessage);
		void setThreadRunTime(unsigned long threadId, unsigned long threadRunTime);
		void setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime);
		void deleteThread(unsigned long threadId);

		// website functions
		unsigned long addWebsite(const std::string& websiteName, const std::string& websiteNamespace,
				const std::string& websiteDomain);
		std::string getWebsiteDomain(unsigned long id);
		std::string getWebsiteNamespace(unsigned long websiteId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromUrlList(unsigned long listId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromConfig(unsigned long configId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromParsedTable(unsigned long tableId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromExtractedTable(unsigned long tableId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromAnalyzedTable(unsigned long tableId);
		bool isWebsiteNamespace(const std::string& nameSpace);
		std::string duplicateWebsiteNamespace(const std::string& websiteNamespace);
		void updateWebsite(unsigned long websiteId, const std::string& websiteName, const std::string& websiteNamespace,
				const std::string& websiteDomain);
		void deleteWebsite(unsigned long websiteId);
		unsigned long duplicateWebsite(unsigned long websiteId);

		// URL list functions
		unsigned long addUrlList(unsigned long websiteId, const std::string& listName, const std::string& listNamespace);
		std::vector<std::pair<unsigned long, std::string>> getUrlLists(unsigned long websiteId);
		std::string getUrlListNamespace(unsigned long listId);
		std::pair<unsigned long, std::string> getUrlListNamespaceFromParsedTable(unsigned long listId);
		std::pair<unsigned long, std::string> getUrlListNamespaceFromExtractedTable(unsigned long listId);
		std::pair<unsigned long, std::string> getUrlListNamespaceFromAnalyzedTable(unsigned long listId);
		bool isUrlListNamespace(unsigned long websiteId, const std::string& nameSpace);
		void updateUrlList(unsigned long listId, const std::string& listName, const std::string& listNamespace);
		void deleteUrlList(unsigned long listId);
		void resetParsingStatus(unsigned long listId);
		void resetExtractingStatus(unsigned long listId);
		void resetAnalyzingStatus(unsigned long listId);

		// query functions
		unsigned long addQuery(unsigned long websiteId, const std::string& queryName, const std::string& queryText,
				const std::string& queryType, bool queryResultBool, bool queryResultSingle, bool queryResultMulti,
				bool queryTextOnly);
		void getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo,
				bool& queryResultBoolTo, bool& queryResultSingleTo, bool& queryResultMultiTo, bool& queryTextOnlyTo);
		void updateQuery(unsigned long queryId, const std::string& queryName, const std::string& queryText,
				const std::string& queryType, bool queryResultBool, bool queryResultSingle, bool queryResultMulti,
				bool queryTextOnly);
		void deleteQuery(unsigned long queryId);
		unsigned long duplicateQuery(unsigned long queryId);

		// configuration functions
		unsigned long addConfiguration(unsigned long websiteId, const std::string& configModule, const std::string& configName,
				const std::string& config);
		const std::string getConfiguration(unsigned long configId);
		void updateConfiguration(unsigned long configId, const std::string& configName,	const std::string& config);
		void deleteConfiguration(unsigned long configId);
		unsigned long duplicateConfiguration(unsigned long configId);

		// table functions
		void addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		std::vector<std::pair<unsigned long, std::string>> getParsedTables(unsigned long listId);
		std::string getParsedTable(unsigned long tableId);
		void deleteParsedTable(unsigned long tableId);
		void addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		std::vector<std::pair<unsigned long, std::string>> getExtractedTables(unsigned long listId);
		std::string getExtractedTable(unsigned long tableId);
		void deleteExtractedTable(unsigned long tableId);
		void addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		std::vector<std::pair<unsigned long, std::string>> getAnalyzedTables(unsigned long listId);
		std::string getAnalyzedTable(unsigned long tableId);
		void deleteAnalyzedTable(unsigned long tableId);
		void releaseLocks();

		// validation functions
		bool isWebsite(unsigned long websiteId);
		bool isUrlList(unsigned long urlListId);
		bool isUrlList(unsigned long websiteId, unsigned long urlListId);
		bool isQuery(unsigned long queryId);
		bool isQuery(unsigned long websiteId, unsigned long queryId);
		bool isConfiguration(unsigned long configId);
		bool isConfiguration(unsigned long websiteId, unsigned long configId);

	protected:
		// structure for prepared SQL statements
		struct PreparedSqlStatement {
			std::string string;
			sql::PreparedStatement * statement;
		};

		// shared connection information
		sql::Connection * connection;
		std::string errorMessage;
		static sql::Driver * driver;
		bool tablesLocked;

		// prepared statements
		std::vector<PreparedSqlStatement> preparedStatements;

		// database helper functions
		bool checkConnection();
		unsigned long getLastInsertedId();
		unsigned long getMaxAllowedPacketSize() const;
		void resetAutoIncrement(const std::string& tableName);
		void lockTable(const std::string& tableName);
		void lockTables(const std::string& tableName1, const std::string& tableName2);
		void unlockTables();
		bool isTableEmpty(const std::string& tableName);
		bool isTableExists(const std::string& tableName);
		bool isColumnExists(const std::string& tableName, const std::string& columnName);
		void execute(const std::string& sqlQuery);

		// custom data functions for algorithms
		unsigned long strlen(const std::string& str);
		void getCustomData(Data::GetValue& data);
		void getCustomData(Data::GetFields& data);
		void getCustomData(Data::GetFieldsMixed& data);
		void getCustomData(Data::GetColumn& data);
		void getCustomData(Data::GetColumns& data);
		void getCustomData(Data::GetColumnsMixed& data);
		void insertCustomData(const Data::InsertValue& data);
		void insertCustomData(const Data::InsertFields& data);
		void insertCustomData(const Data::InsertFieldsMixed& data);
		void updateCustomData(const Data::UpdateValue& data);
		void updateCustomData(const Data::UpdateFields& data);
		void updateCustomData(const Data::UpdateFieldsMixed& data);

	private:
		// private connection information
		const crawlservpp::Struct::DatabaseSettings settings;
		unsigned long maxAllowedPacketSize;
		unsigned long sleepOnError;

		// run file with SQL commands
		bool run(const std::string& sqlFile);

		// ids of prepared SQL statements
		unsigned short psLog;
		unsigned short psLastId;
		unsigned short psSetThreadStatus;
		unsigned short psSetThreadStatusMessage;
		unsigned short psStrlen;

		// prevent use of these
		Database(const Database&);
		void operator=(Database&);
	};
}

#endif /* MAIN_DATABASE_H_ */
