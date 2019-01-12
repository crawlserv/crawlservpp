/*
 * DatabaseModule.h
 *
 * Interface to be inherited by the thread modules.
 * Allows them access to the database by providing basic Database functionality as well as the option to add prepared SQL statements.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef DATABASEMODULE_H_
#define DATABASEMODULE_H_

#include "DatabaseThread.h"

#include "structs/DatabaseSettings.h"

#include <cppconn/prepared_statement.h>

#include <mysql_connection.h>

#include <string>
#include <vector>

class DatabaseModule {
public:
	DatabaseModule(DatabaseThread& dbRef);
	virtual ~DatabaseModule() = 0;

	// get module error message
	const std::string& getModuleErrorMessage() const;

	// wrappers for basic functionality implemented by Database and DatabaseThread
	void setSleepOnError(unsigned long seconds);
	void log(const std::string& logModule, const std::string& logEntry);
	std::string getWebsiteDomain(unsigned long websiteId);
	void getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo, bool& queryResultBoolTo,
			bool& queryResultSingleTo, bool& queryResultMultiTo, bool& queryTextOnlyTo);
	std::string getConfigJson(unsigned long configId);
	unsigned long getLastInsertedId();
	void unlockTables();

	// wrappers for indexing module tables
	void addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
	void addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
	void addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);

	// wrappers for resetting module status
	void resetParsingStatus(unsigned long listId);
	void resetExtractingStatus(unsigned long listId);
	void resetAnalyzingStatus(unsigned long listId);

protected:
	// reference to the database connection by the thread
	DatabaseThread& database;

	// module error message
	std::string errorMessage;

	// check connection to database (try to reconnect if necessary)
	bool checkConnection();

	// get database error message
	const std::string& getDatabaseErrorMessage() const;

	// lock and unlock tables
	void lockTable(const std::string& tableName);
	void lockTables(const std::string& tableName1, const std::string tableName2);

	// table and column checking
	bool isTableExists(const std::string& tableName);
	bool isColumnExists(const std::string& tableName, const std::string& columnName);

	// execute SQL query
	void execute(const std::string& sqlQuery);

	// manage prepared SQL statements
	unsigned short addPreparedStatement(const std::string& sqlStatementString);
	sql::PreparedStatement * getPreparedStatement(unsigned short sqlStatementId);
};

#endif /* DATABASEMODULE_H_ */
