/*
 * DBWrapper.cpp
 *
 * Interface to be inherited by the thread modules.
 * Allows them access to the database by providing basic Database functionality as well as the option to add prepared SQL statements.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "DBWrapper.h"

namespace crawlservpp::Module {

// constructor: initialize database
DBWrapper::DBWrapper(DBThread& dbRef) : database(dbRef) {}

// destructor stub
DBWrapper::~DBWrapper() {}

/*
 * WRAPPER FOR SETTER
 */

// set the number of seconds to wait before (first and last) re-try on connection loss to mySQL server
void DBWrapper::setSleepOnError(unsigned long seconds) {
	this->database.setSleepOnError(seconds);
}

/*
 * WRAPPER FOR LOGGING FUNCTION
 */

// add a log entry to the database
void DBWrapper::log(const std::string& logModule, const std::string& logEntry) {
	this->database.log(logModule, logEntry);
}

/*
 * WRAPPER FOR WEBSITE FUNCTION
 */

// get website domain from the database by its ID
std::string DBWrapper::getWebsiteDomain(unsigned long websiteId) {
	return this->database.getWebsiteDomain(websiteId);
}

/*
 * WRAPPERS FOR URL LIST FUNCTIONS
 */

// reset parsing status of ID-specified URL list
void DBWrapper::resetParsingStatus(unsigned long listId) {
	this->database.resetParsingStatus(listId);
}

// reset extracting status of ID-specified URL list
void DBWrapper::resetExtractingStatus(unsigned long listId) {
	this->database.resetExtractingStatus(listId);
}

// reset analyzing status of ID-specified URL list
void DBWrapper::resetAnalyzingStatus(unsigned long listId) {
	this->database.resetAnalyzingStatus(listId);
}

/*
 * WRAPPER FOR QUERY FUNCTION
 */

// get the properties of a query from the database by its ID
void DBWrapper::getQueryProperties(unsigned long queryId, crawlservpp::Struct::QueryProperties& queryPropertiesTo) {
	this->database.getQueryProperties(queryId, queryPropertiesTo);
}

/*
 * WRAPPER FOR CONFIGURATION FUNCTION
 */

// get a configuration from the database by its ID
std::string DBWrapper::getConfiguration(unsigned long configId) {
	return this->database.getConfiguration(configId);
}

/*
 * WRAPPERS FOR TABLE INDEXING FUNCTIONS
 */

// add a parsed table to the database if a such a table does not exist already
void DBWrapper::addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addParsedTable(websiteId, listId, tableName);
}

// add an extracted table to the database if such a table does not exist already
void DBWrapper::addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addExtractedTable(websiteId, listId, tableName);
}

// add an analyzed table to the database if such a table does not exist already
void DBWrapper::addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addAnalyzedTable(websiteId, listId, tableName);
}

/*
 * WRAPPER FOR TABLE LOCKING FUNCTION
 */

// release table locks in the database (if necessary)
void DBWrapper::releaseLocks() {
	this->database.releaseLocks();
}

/*
 * WRAPPERS FOR CUSTOM DATA FUNCTIONS USED BY ALGORITHMS
 */

// get one custom value from one field of a row in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetValue& data) {
	this->database.getCustomData(data);
}

// get custom values from multiple fields of a row in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetFields& data) {
	this->database.getCustomData(data);
}

// get custom values from multiple fields of a row with different types in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetFieldsMixed& data) {
	this->database.getCustomData(data);
}

// get custom values from one column in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetColumn& data) {
	this->database.getCustomData(data);
}

// get custom values from multiple columns of the same type in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetColumns& data) {
	this->database.getCustomData(data);
}

// get custom values from multiple columns of different types in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetColumnsMixed& data) {
	this->database.getCustomData(data);
}

// insert one custom value into a row in the database
void DBWrapper::insertCustomData(const crawlservpp::Main::Data::InsertValue& data) {
	this->database.insertCustomData(data);
}

// insert custom values into multiple fields of the same type into a row in the database
void DBWrapper::insertCustomData(const crawlservpp::Main::Data::InsertFields& data) {
	this->database.insertCustomData(data);
}

// insert custom values into multiple fields of different types into a row in the database
void DBWrapper::insertCustomData(const crawlservpp::Main::Data::InsertFieldsMixed& data) {
	this->database.insertCustomData(data);
}

// update one custom value in one field of a row in the database
void DBWrapper::updateCustomData(const crawlservpp::Main::Data::UpdateValue& data) {
	this->database.updateCustomData(data);
}

// update custom values in multiple fields of a row with the same type in the database
void DBWrapper::updateCustomData(const crawlservpp::Main::Data::UpdateFields& data) {
	this->database.updateCustomData(data);
}

// update custom values in multiple fields of a row with different types in the database
void DBWrapper::updateCustomData(const crawlservpp::Main::Data::UpdateFieldsMixed& data) {
	this->database.updateCustomData(data);
}

/*
 * WRAPPERS FOR GETTERS (protected)
 */

// get the maximum allowed packet size
unsigned long DBWrapper::getMaxAllowedPacketSize() const {
	return this->database.getMaxAllowedPacketSize();
}

/*
 * WRAPPERS FOR MANAGING PREPARED SQL STATEMENTS (protected)
 */

// reserve memory for prepared SQL statements
void DBWrapper::reservePreparedStatements(unsigned long numStatements) {
	this->database.preparedStatements.reserve(this->database.preparedStatements.size() + numStatements);
}

// add prepared SQL statement to the database and return the ID of the newly added prepared statement
unsigned short DBWrapper::addPreparedStatement(const std::string& sqlStatementString) {
	DBThread::PreparedSqlStatement statement;
	statement.string = sqlStatementString;
	statement.statement = std::unique_ptr<sql::PreparedStatement>(this->database.connection->prepareStatement(statement.string));
	this->database.preparedStatements.push_back(std::move(statement));
	return this->database.preparedStatements.size();
}

// get prepared SQL statement from database by its ID
sql::PreparedStatement& DBWrapper::getPreparedStatement(unsigned short sqlStatementId) {
	return *(this->database.preparedStatements.at(sqlStatementId - 1).statement);
}

/*
 * WRAPPERS FOR DATABASE HELPER FUNCTIONS (protected)
 */

// check whether the connection to the database is still valid and try to re-connect if necesssary (set error message on failure)
void DBWrapper::checkConnection() {
	this->database.checkConnection();
}

// get the last inserted ID from the database
unsigned long DBWrapper::getLastInsertedId() {
	return this->database.getLastInsertedId();
}

// lock a table in the database for writing
void DBWrapper::lockTable(const std::string& tableName) {
	this->database.lockTable(tableName);
}

// lock two tables in the database for writing (plus the alias 'a' for reading the first and the alias 'b' for reading the second table)
void DBWrapper::lockTables(const std::string& tableName1, const std::string tableName2) {
	this->database.lockTables(tableName1, tableName2);
}

// unlock tables in the database
void DBWrapper::unlockTables() {
	this->database.unlockTables();
}

// check whether a specific table exists in the database
bool DBWrapper::isTableExists(const std::string& tableName) {
	return this->database.isTableExists(tableName);
}

// check whether a specific column of a specific table exists in the database
bool DBWrapper::isColumnExists(const std::string& tableName, const std::string& columnName) {
	return this->database.isColumnExists(tableName, columnName);
}

// add a table to the database (the primary key 'id' will be created automatically; WARNING: check existence beforehand!)
void DBWrapper::createTable(const std::string& tableName, const std::vector<TableColumn>& columns, bool compressed) {
	this->database.createTable(tableName, columns, compressed);
}

// add a column to a table in the database
void DBWrapper::addColumn(const std::string& tableName, const TableColumn& column) {
	this->database.addColumn(tableName, column);
}

// compress a table in the database
void DBWrapper::compressTable(const std::string& tableName) {
	this->database.compressTable(tableName);
}

// delete a table from the database if it exists
void DBWrapper::deleteTableIfExists(const std::string& tableName) {
	this->database.deleteTableIfExists(tableName);
}

}
