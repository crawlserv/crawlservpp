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

#include "structs/DatabaseSettings.h"

#include "DatabaseThread.h"

#include <cppconn/prepared_statement.h>
#include <mysql_connection.h>

#include <string>
#include <vector>

class DatabaseModule {
public:
	DatabaseModule(DatabaseThread& dbRef);
	virtual ~DatabaseModule() = 0;

	// get error message
	const std::string& getErrorMessage() const;

	// wrappers for basic functionality implemented by Database and DatabaseThread
	void setSleepOnError(unsigned long seconds);
	void log(const std::string& logModule, const std::string& logEntry);
	std::string getWebsiteDomain(unsigned long websiteId);
	void getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo, bool& queryResultBoolTo,
			bool& queryResultSingleTo, bool& queryResultMultiTo, bool& queryTextOnlyTo);
	std::string getConfigJson(unsigned long configId);
	void unlockTables();
	unsigned long getLastInsertedId();

protected:
	// reference to the database connection by the thread
	DatabaseThread& database;

	// own error message
	std::string errorMessage;

	// check connection to database (try to reconnect if necessary)
	bool checkConnection();

	// lock and unlock tables
	void lockTable(const std::string& tableName);
	void lockTables(const std::string& tableName1, const std::string tableName2);

	// manage prepared SQL statements
	unsigned short addPreparedStatement(const std::string& sqlStatementString);
	sql::PreparedStatement * getPreparedStatement(unsigned short sqlStatementId);
};

#endif /* DATABASEMODULE_H_ */
