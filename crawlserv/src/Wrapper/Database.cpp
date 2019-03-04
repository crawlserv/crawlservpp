/*
 * Database.cpp
 *
 * Interface to be inherited by the thread modules.
 * Allows them access to the database by providing basic Database functionality as well as the option to add prepared SQL statements.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

namespace crawlservpp::Wrapper {

// constructor: initialize database
Database::Database(crawlservpp::Module::Database& dbRef) : database(dbRef) {}

// destructor stub
Database::~Database() {}

/*
 * WRAPPER FOR SETTER
 */

// set the number of seconds to wait before (first and last) re-try on connection loss to mySQL server
void Database::setSleepOnError(unsigned long seconds) {
	this->database.setSleepOnError(seconds);
}

/*
 * WRAPPER FOR LOGGING FUNCTION
 */

// add a log entry to the database
void Database::log(const std::string& logEntry) {
	this->database.log(logEntry);
}

/*
 * WRAPPER FOR WEBSITE FUNCTION
 */

// get website domain from the database by its ID
std::string Database::getWebsiteDomain(unsigned long websiteId) {
	return this->database.getWebsiteDomain(websiteId);
}

/*
 * WRAPPERS FOR URL LIST FUNCTIONS
 */

// reset parsing status of ID-specified URL list
void Database::resetParsingStatus(unsigned long listId) {
	this->database.resetParsingStatus(listId);
}

// reset extracting status of ID-specified URL list
void Database::resetExtractingStatus(unsigned long listId) {
	this->database.resetExtractingStatus(listId);
}

// reset analyzing status of ID-specified URL list
void Database::resetAnalyzingStatus(unsigned long listId) {
	this->database.resetAnalyzingStatus(listId);
}

/*
 * WRAPPER FOR QUERY FUNCTION
 */

// get the properties of a query from the database by its ID
void Database::getQueryProperties(unsigned long queryId, crawlservpp::Struct::QueryProperties& queryPropertiesTo) {
	this->database.getQueryProperties(queryId, queryPropertiesTo);
}

/*
 * WRAPPER FOR CONFIGURATION FUNCTION
 */

// get a configuration from the database by its ID
std::string Database::getConfiguration(unsigned long configId) {
	return this->database.getConfiguration(configId);
}

/*
 * WRAPPERS FOR TABLE INDEXING FUNCTIONS
 */

// lock custom tables of the specified type
void Database::lockCustomTables(const std::string& type, unsigned long websiteId, unsigned long listId, unsigned long timeOut) {
	this->database.lockCustomTables(type, websiteId, listId, timeOut);
}

// add a custom table of the specified type to the database if such a table does not exist already, return table ID
unsigned long Database::addCustomTable(const CustomTableProperties& properties) {
	return this->database.addCustomTable(properties);
}

// get custom tables of the specified type for an ID-specified URL list from the database
std::vector<std::pair<unsigned long, std::string>> Database::getCustomTables(const std::string& type, unsigned long listId) {
	return this->database.getCustomTables(type, listId);
}

// get the ID of a custom table of the specified type from the database by its website ID, URL list ID and table name
unsigned long Database::getCustomTableId(const std::string& type, unsigned long websiteId, unsigned long listId,
		const std::string& tableName) {
	return this->database.getCustomTableId(type, websiteId, listId, tableName);
}

// get the name of a custom table of the specified type from the database by its ID
std::string Database::getCustomTableName(const std::string& type, unsigned long tableId) {
	return this->database.getCustomTableName(type, tableId);
}

// delete custom table of the specified type from the database by its ID
void Database::deleteCustomTable(const std::string& type, unsigned long tableId) {
	this->database.deleteCustomTable(type, tableId);
}

// unlock custom tables of the specified type
void Database::unlockCustomTables(const std::string& type) {
	this->database.unlockCustomTables(type);
}

/*
 * WRAPPER FOR TABLE LOCKING FUNCTION
 */

// release table locks in the database (if necessary)
void Database::releaseLocks() {
	this->database.releaseLocks();
}

/*
 * WRAPPERS FOR CUSTOM DATA FUNCTIONS USED BY ALGORITHMS
 */

// get one custom value from one field of a row in the database
void Database::getCustomData(crawlservpp::Main::Data::GetValue& data) {
	this->database.getCustomData(data);
}

// get custom values from multiple fields of a row in the database
void Database::getCustomData(crawlservpp::Main::Data::GetFields& data) {
	this->database.getCustomData(data);
}

// get custom values from multiple fields of a row with different types in the database
void Database::getCustomData(crawlservpp::Main::Data::GetFieldsMixed& data) {
	this->database.getCustomData(data);
}

// get custom values from one column in the database
void Database::getCustomData(crawlservpp::Main::Data::GetColumn& data) {
	this->database.getCustomData(data);
}

// get custom values from multiple columns of the same type in the database
void Database::getCustomData(crawlservpp::Main::Data::GetColumns& data) {
	this->database.getCustomData(data);
}

// get custom values from multiple columns of different types in the database
void Database::getCustomData(crawlservpp::Main::Data::GetColumnsMixed& data) {
	this->database.getCustomData(data);
}

// insert one custom value into a row in the database
void Database::insertCustomData(const crawlservpp::Main::Data::InsertValue& data) {
	this->database.insertCustomData(data);
}

// insert custom values into multiple fields of the same type into a row in the database
void Database::insertCustomData(const crawlservpp::Main::Data::InsertFields& data) {
	this->database.insertCustomData(data);
}

// insert custom values into multiple fields of different types into a row in the database
void Database::insertCustomData(const crawlservpp::Main::Data::InsertFieldsMixed& data) {
	this->database.insertCustomData(data);
}

// update one custom value in one field of a row in the database
void Database::updateCustomData(const crawlservpp::Main::Data::UpdateValue& data) {
	this->database.updateCustomData(data);
}

// update custom values in multiple fields of a row with the same type in the database
void Database::updateCustomData(const crawlservpp::Main::Data::UpdateFields& data) {
	this->database.updateCustomData(data);
}

// update custom values in multiple fields of a row with different types in the database
void Database::updateCustomData(const crawlservpp::Main::Data::UpdateFieldsMixed& data) {
	this->database.updateCustomData(data);
}

/*
 * WRAPPERS FOR GETTERS (protected)
 */

// get the maximum allowed packet size
unsigned long Database::getMaxAllowedPacketSize() const {
	return this->database.getMaxAllowedPacketSize();
}

/*
 * WRAPPERS FOR MANAGING PREPARED SQL STATEMENTS (protected)
 */

// reserve memory for prepared SQL statements
void Database::reserveForPreparedStatements(unsigned long numberOfAdditionalPreparedStatements) {
	this->database.reserveForPreparedStatements(numberOfAdditionalPreparedStatements);
}

// add prepared SQL statement and return the ID of the newly added prepared statement
unsigned short Database::addPreparedStatement(const std::string& sqlQuery) {
	return this->database.addPreparedStatement(sqlQuery);
}

// get reference to prepared SQL statement by its ID
//  WARNING: Do not run Database::checkConnection() while using this reference!
sql::PreparedStatement& Database::getPreparedStatement(unsigned short id) {
	return this->database.getPreparedStatement(id);
}

/*
 * WRAPPERS FOR DATABASE HELPER FUNCTIONS (protected)
 */

// check whether the connection to the database is still valid and try to re-connect if necesssary, throws Database::Exception
//  WARNING: afterwards, old references to prepared SQL statements may be invalid!
void Database::checkConnection() {
	this->database.checkConnection();
}

// get the last inserted ID from the database
unsigned long Database::getLastInsertedId() {
	return this->database.getLastInsertedId();
}

// lock a table in the database for writing
void Database::lockTable(const std::string& tableName) {
	this->database.lockTable(tableName);
}

// lock two tables in the database for writing (plus the alias 'a' for reading the first and the alias 'b' for reading the second table)
void Database::lockTables(const std::string& tableName1, const std::string tableName2) {
	this->database.lockTables(tableName1, tableName2);
}

// unlock tables in the database
void Database::unlockTables() {
	this->database.unlockTables();
}

// check whether a specific table exists in the database
bool Database::isTableExists(const std::string& tableName) {
	return this->database.isTableExists(tableName);
}

// check whether a specific column of a specific table exists in the database
bool Database::isColumnExists(const std::string& tableName, const std::string& columnName) {
	return this->database.isColumnExists(tableName, columnName);
}

// add a table to the database (the primary key 'id' will be created automatically)
void Database::createTable(const std::string& tableName, const std::vector<TableColumn>& columns, bool compressed) {
	this->database.createTable(tableName, columns, compressed);
}

// add a column to a table in the database
void Database::addColumn(const std::string& tableName, const TableColumn& column) {
	this->database.addColumn(tableName, column);
}

// compress a table in the database
void Database::compressTable(const std::string& tableName) {
	this->database.compressTable(tableName);
}

// delete a table from the database if it exists
void Database::deleteTable(const std::string& tableName) {
	this->database.deleteTable(tableName);
}

// catch SQL exception and re-throw it as ConnectionException or Exception
void Database::sqlException(const std::string& function, const sql::SQLException& e) {
	this->database.sqlException(function, e);
}

}
