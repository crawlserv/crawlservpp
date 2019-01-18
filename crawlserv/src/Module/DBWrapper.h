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

#include "../Main/Data.h"
#include "../Struct/DatabaseSettings.h"

#include <cppconn/prepared_statement.h>

#include <mysql_connection.h>

#include <string>
#include <vector>

namespace crawlservpp::Module {
	class DBWrapper {
	public:
		DBWrapper(DBThread& dbRef);
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
		unsigned long getMaxAllowedPacketSize() const;
		void unlockTables();

		// wrappers for indexing module tables
		void addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		void addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		void addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);

		// wrappers for resetting module status
		void resetParsingStatus(unsigned long listId);
		void resetExtractingStatus(unsigned long listId);
		void resetAnalyzingStatus(unsigned long listId);

		// wrappers for custom data functions used by algorithms
		unsigned long strlen(const std::string& str);
		void getCustomData(crawlservpp::Main::Data::GetValue& data);
		void getCustomData(crawlservpp::Main::Data::GetFields& data);
		void getCustomData(crawlservpp::Main::Data::GetFieldsMixed& data);
		void getCustomData(crawlservpp::Main::Data::GetColumn& data);
		void getCustomData(crawlservpp::Main::Data::GetColumns& data);
		void getCustomData(crawlservpp::Main::Data::GetColumnsMixed& data);
		void insertCustomData(const crawlservpp::Main::Data::InsertValue& data);
		void insertCustomData(const crawlservpp::Main::Data::InsertFields& data);
		void insertCustomData(const crawlservpp::Main::Data::InsertFieldsMixed& data);
		void updateCustomData(const crawlservpp::Main::Data::UpdateValue& data);
		void updateCustomData(const crawlservpp::Main::Data::UpdateFields& data);
		void updateCustomData(const crawlservpp::Main::Data::UpdateFieldsMixed& data);

	protected:
		// reference to the database connection by the thread
		DBThread& database;

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
		void reservePreparedStatements(unsigned long numPreparedStatements);
		unsigned short addPreparedStatement(const std::string& sqlStatementString);
		sql::PreparedStatement * getPreparedStatement(unsigned short sqlStatementId);
	};
}

#endif /* MODULE_DBWRAPPER_H_ */
