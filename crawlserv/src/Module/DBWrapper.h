/*
 * DBWrapper.h
 *
 * Interface to be inherited by the thread modules.
 * Allows them access to the database by providing basic Database functionality as well as the option to add prepared SQL statements.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_DBWRAPPER_H_
#define MODULE_DBWRAPPER_H_

#include "DBThread.h"

#include "../Struct/DatabaseSettings.h"

#include <cppconn/prepared_statement.h>

#include <mysql_connection.h>

#include <string>
#include <vector>

namespace crawlservpp::Module {
	class DBWrapper {
	public:
		DBWrapper(crawlservpp::Module::DBThread& dbRef);
		virtual ~DBWrapper() = 0;

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

		// wrappers for data functions used by algorithms
		void getText(const std::string& tableName, const std::string& columnName, const std::string& condition, std::string& resultTo);
		void getTexts(const std::string& tableName, const std::string& columnName, std::vector<std::string>& resultTo);
		void getTexts(const std::string& tableName, const std::string& columnName, const std::string& condition, unsigned long limit,
				std::vector<std::string>& resultTo);
		void insertText(const std::string& tableName, const std::string& columnName, const std::string& text);
		void insertTextUInt64(const std::string& tableName, const std::string& textColumnName, const std::string& numberColumnName,
				const std::string& text, unsigned long number);
		void insertTexts(const std::string& tableName, const std::string& columnName, const std::vector<std::string>& texts);
		void updateText(const std::string& tableName, const std::string& columnName, const std::string& condition, std::string& text);

	protected:
		// reference to the database connection by the thread
		crawlservpp::Module::DBThread& database;

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
}

#endif /* MODULE_DBWRAPPER_H_ */
