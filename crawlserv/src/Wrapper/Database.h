/*
 * Database.h
 *
 * Interface to be inherited by the thread modules.
 * Allows them access to the database by providing basic Database functionality as well as the option to add prepared SQL statements.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef WRAPPER_DATABASE_H_
#define WRAPPER_DATABASE_H_

#include "../Main/Data.h"
#include "../Module/Database.h"
#include "../Struct/DatabaseSettings.h"
#include "../Struct/QueryProperties.h"

#include <cppconn/prepared_statement.h>

#include <mysql_connection.h>

#include <string>
#include <vector>

namespace crawlservpp::Wrapper {

// for convenience
typedef crawlservpp::Main::Database::Column TableColumn;

class Database {
public:
	// constructors
	Database(crawlservpp::Module::Database& dbRef);

	virtual ~Database() = 0;

	// wrapper for setter
	void setSleepOnError(unsigned long seconds);

	// wrapper for logging function
	void log(const std::string& logEntry);

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
	unsigned long addParsingTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
	unsigned long getParsingTableId(unsigned long websiteId, unsigned long listId, const std::string& tableName);
	unsigned long addExtractingTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
	unsigned long getExtractingTableId(unsigned long websiteId, unsigned long listId, const std::string& tableName);
	unsigned long addAnalyzingTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
	unsigned long getAnalyzingTableId(unsigned long websiteId, unsigned long listId, const std::string& tableName);

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

	// not moveable, not copyable
	Database(Database&) = delete;
	Database(Database&&) = delete;
	Database& operator=(Database&) = delete;
	Database& operator=(Database&&) = delete;

protected:
	// reference to the database connection by the thread
	crawlservpp::Module::Database& database;

	// wrapper for getters
	unsigned long getMaxAllowedPacketSize() const;

	// wrappers for managing prepared SQL statements
	void reserveForPreparedStatements(unsigned long numberOfAdditionalPreparedStatements);
	unsigned short addPreparedStatement(const std::string& sqlQuery);
	sql::PreparedStatement& getPreparedStatement(unsigned short id);

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

#endif /* WRAPPER_DATABASE_H_ */
