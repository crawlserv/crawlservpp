/*
 * Database.h
 *
 * A class to handle database access for crawlserv and its threads. Only basic functionality that is used by both server and threads is
 *  implemented in this (parent) class.
 *
 * NOT THREAD-SAFE! Use instances of the child class DatabaseThread for thread-specific functionality and child classes of DatabaseModule
 *  for module-specific functionality instead.
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#ifndef DATABASE_H_
#define DATABASE_H_

#define DATABASE_DELETE(X) { if(X) { X->close(); delete X; X = NULL; } }

#include "structs/DatabaseSettings.h"
#include "structs/IdString.h"
#include "structs/PreparedSqlStatement.h"
#include "structs/ThreadDatabaseEntry.h"
#include "structs/ThreadOptions.h"

#include "namespaces/Helpers.h"

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
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

class Database {
public:
	Database(const DatabaseSettings& dbSettings);
	virtual ~Database();

	bool connect();
	bool initializeSql();
	bool prepare();
	void setSleepOnError(unsigned long seconds);

	// getters of member variables
	const DatabaseSettings& getSettings() const;
	const std::string& getErrorMessage() const;

	// logging functions
	void log(const std::string& logModule, const std::string& logEntry);
	unsigned long getNumberOfLogEntries(const std::string& logModule);
	void clearLogs(const std::string& logModule);

	// thread functions
	std::vector<ThreadDatabaseEntry> getThreads();
	unsigned long addThread(const std::string& threadModule, const ThreadOptions& threadOptions);
	unsigned long getThreadRunTime(unsigned long threadId);
	unsigned long getThreadPauseTime(unsigned long threadId);
	void setThreadStatus(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage);
	void setThreadRunTime(unsigned long threadId, unsigned long threadRunTime);
	void setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime);
	void deleteThread(unsigned long threadId);

	// website functions
	unsigned long addWebsite(const std::string& websiteName, const std::string& websiteNameSpace, const std::string& websiteDomain);
	std::string getWebsiteDomain(unsigned long id);
	std::string getWebsiteNameSpace(unsigned long websiteId);
	IdString getWebsiteNameSpaceFromUrlList(unsigned long listId);
	IdString getWebsiteNameSpaceFromConfig(unsigned long configId);
	IdString getWebsiteNameSpaceFromParsedTable(unsigned long tableId);
	IdString getWebsiteNameSpaceFromExtractedTable(unsigned long tableId);
	IdString getWebsiteNameSpaceFromAnalyzedTable(unsigned long tableId);
	bool isWebsiteNameSpace(const std::string& nameSpace);
	std::string duplicateWebsiteNameSpace(const std::string& websiteNameSpace);
	void updateWebsite(unsigned long websiteId, const std::string& websiteName, const std::string& websiteNameSpace,
			const std::string& websiteDomain);
	void deleteWebsite(unsigned long websiteId);
	unsigned long duplicateWebsite(unsigned long websiteId);

	// URL list functions
	unsigned long addUrlList(unsigned long websiteId, const std::string& listName, const std::string& listNameSpace);
	std::vector<IdString> getUrlLists(unsigned long websiteId);
	std::string getUrlListNameSpace(unsigned long listId);
	IdString getUrlListNameSpaceFromParsedTable(unsigned long listId);
	IdString getUrlListNameSpaceFromExtractedTable(unsigned long listId);
	IdString getUrlListNameSpaceFromAnalyzedTable(unsigned long listId);
	bool isUrlListNameSpace(unsigned long websiteId, const std::string& nameSpace);
	void updateUrlList(unsigned long listId, const std::string& listName, const std::string& listNameSpace);
	void deleteUrlList(unsigned long listId);

	// query functions
	unsigned long addQuery(unsigned long websiteId, const std::string& queryName, const std::string& queryText,
			const std::string& queryType, bool queryResultBool, bool queryResultSingle, bool queryResultMulti, bool queryTextOnly);
	void getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo, bool& queryResultBoolTo,
			bool& queryResultSingleTo, bool& queryResultMultiTo, bool queryTextOnlyTo);
	void updateQuery(unsigned long queryId, const std::string& queryName, const std::string& queryText,
			const std::string& queryType, bool queryResultBool, bool queryResultSingle, bool queryResultMulti, bool queryTextOnly);
	void deleteQuery(unsigned long queryId);
	unsigned long duplicateQuery(unsigned long queryId);

	// configuration functions
	unsigned long addConfiguration(unsigned long websiteId, const std::string& configModule, const std::string& configName,
			const std::string& config);
	const std::string getConfiguration(unsigned long configId);
	void updateConfiguration(unsigned long configId, const std::string& configName, const std::string& config);
	void deleteConfiguration(unsigned long configId);
	unsigned long duplicateConfiguration(unsigned long configId);

	// table functions
	std::vector<IdString> getParsedTables(unsigned long listId);
	std::string getParsedTable(unsigned long tableId);
	void deleteParsedTable(unsigned long tableId);
	std::vector<IdString> getExtractedTables(unsigned long listId);
	std::string getExtractedTable(unsigned long tableId);
	void deleteExtractedTable(unsigned long tableId);
	std::vector<IdString> getAnalyzedTables(unsigned long listId);
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
	bool isParsedTable(unsigned long tableId);
	bool isExtractedTable(unsigned long tableId);
	bool isAnalyzedTable(unsigned long tableId);

protected:
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
	bool isTableEmpty(const std::string& tableName);
	void resetAutoIncrement(const std::string& tableName);
	void lockTable(const std::string& tableName);
	void lockTables(const std::string& tableName1, const std::string& tableName2);
	void unlockTables();

private:
	// private connection information
	const DatabaseSettings settings;
	unsigned long sleepOnError;

	// run file with SQL commands
	bool run(const std::string& sqlFile);

	// ids of prepared SQL statements
	unsigned short psLog;
	unsigned short psLastId;
	unsigned short psSetThreadStatus;
};

#endif /* DATABASE_H_ */
