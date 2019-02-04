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
#include "../Struct/QueryProperties.h"

#include <cppconn/prepared_statement.h>

#include <mysql_connection.h>

#include <string>
#include <vector>

namespace crawlservpp::Module {

	// for convenience
	typedef crawlservpp::Main::Database::Column TableColumn;

	class DBWrapper {
	public:
		DBWrapper(DBThread& dbRef);
		virtual ~DBWrapper() = 0;

		// wrapper for setter
		void setSleepOnError(unsigned long seconds);

		// wrapper for logging function
		void log(const std::string& logModule, const std::string& logEntry);

		// wrapper for website function
		std::string getWebsiteDomain(unsigned long websiteId);

		// wrappers for URL list functions
		void resetParsingStatus(unsigned long listId);
		void resetExtractingStatus(unsigned long listId);
		void resetAnalyzingStatus(unsigned long listId);

		// wrapper for query function
		void getQueryProperties(unsigned long queryId, crawlservpp::Struct::QueryProperties& queryPropertiesTo);

		// wrapper for configuration function
		std::string getConfiguration(unsigned long configId);

		// wrappers for table indexing functions
		void addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		void addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		void addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);

		// wrapper for table lock function
		void releaseLocks();

		// wrappers for custom data functions used by algorithms
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

		// wrapper for getters
		unsigned long getMaxAllowedPacketSize() const;

		// wrappers for managing prepared SQL statements
		void reservePreparedStatements(unsigned long numPreparedStatements);
		unsigned short addPreparedStatement(const std::string& sqlStatementString);
		sql::PreparedStatement& getPreparedStatement(unsigned short sqlStatementId);

		// wrappers for database helper functions
		void checkConnection();
		unsigned long getLastInsertedId();
		void lockTable(const std::string& tableName);
		void lockTables(const std::string& tableName1, const std::string tableName2);
		void unlockTables();
		bool isTableExists(const std::string& tableName);
		bool isColumnExists(const std::string& tableName, const std::string& columnName);
		void createTable(const std::string& tableName, const std::vector<TableColumn>& columns, bool compressed);
		void addColumn(const std::string& tableName, const TableColumn& column);
		void compressTable(const std::string& tableName);
		void deleteTableIfExists(const std::string& tableName);
	};
}

#endif /* MODULE_DBWRAPPER_H_ */
