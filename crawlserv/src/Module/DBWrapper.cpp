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

// get module error message
const std::string& DBWrapper::getModuleErrorMessage() const {
	return this->errorMessage;
}

// provice access to setting the error sleep time
void DBWrapper::setSleepOnError(unsigned long seconds) {
	this->database.setSleepOnError(seconds);
}

// provide access to logging functionality
void DBWrapper::log(const std::string& logModule, const std::string& logEntry) {
	this->database.log(logModule, logEntry);
}

// provide access to the functionality for getting the domain of a website from the database
std::string DBWrapper::getWebsiteDomain(unsigned long websiteId) {
	return this->database.getWebsiteDomain(websiteId);
}

// provide access to the functionality for getting the properties of a query from the database
void DBWrapper::getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo,
		bool& queryResultBoolTo, bool& queryResultSingleTo, bool& queryResultMultiTo, bool& queryTextOnlyTo) {
	this->database.getQueryProperties(queryId, queryTextTo, queryTypeTo, queryResultBoolTo, queryResultSingleTo, queryResultMultiTo,
			queryTextOnlyTo);
}

// provide access to the functionality for getting the current configuration
std::string DBWrapper::getConfigJson(unsigned long configId) {
	return this->database.getConfiguration(configId);
}

// provide access to the functionality for getting the last inserted id
unsigned long DBWrapper::getLastInsertedId() {
	return this->database.getLastInsertedId();
}

// provide access to the functionality for getting the maximum allowed packet size
unsigned long DBWrapper::getMaxAllowedPacketSize() const {
	return this->database.getMaxAllowedPacketSize();
}

// provide access to the functionality for unlocking all tables in the database
void DBWrapper::unlockTables() {
	this->database.unlockTables();
}

// provide access to the functionality for adding parsed table to database
void DBWrapper::addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addParsedTable(websiteId, listId, tableName);
}

// provide access to the functionality for adding extracted table to database
void DBWrapper::addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addExtractedTable(websiteId, listId, tableName);
}

// provide access to the functionality for adding analyzed table to database
void DBWrapper::addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addAnalyzedTable(websiteId, listId, tableName);
}

// provide access to the functionality for resetting parsing status of id-specified URL list
void DBWrapper::resetParsingStatus(unsigned long listId) {
	this->database.resetParsingStatus(listId);
}

// provide access to the functionality for resetting extracting status of id-specified URL list
void DBWrapper::resetExtractingStatus(unsigned long listId) {
	this->database.resetExtractingStatus(listId);
}

// provide access to the functionality for resetting analyzing status of id-specified URL list
void DBWrapper::resetAnalyzingStatus(unsigned long listId) {
	this->database.resetAnalyzingStatus(listId);
}

// provide access to getting the length of a string as it were in the database
unsigned long DBWrapper::strlen(const std::string& str) {
	return this->database.strlen(str);
}

// provide access to the functionality for getting one custom value from one field of a row in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetValue& data) {
	this->database.getCustomData(data);
}

// provide access to the functionality for getting custom values from multiple fields of a row in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetFields& data) {
	this->database.getCustomData(data);
}

// provide access to the functionality for getting custom values from multiple fields of a row with different types in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetFieldsMixed& data) {
	this->database.getCustomData(data);
}

// provide access to the functionality for getting custom values from one column in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetColumn& data) {
	this->database.getCustomData(data);
}

// provide access to the functionality for getting custom values from multiple columns in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetColumns& data) {
	this->database.getCustomData(data);
}

// provide access to the functionality for getting custom values from multiple columns with different types in the database
void DBWrapper::getCustomData(crawlservpp::Main::Data::GetColumnsMixed& data) {
	this->database.getCustomData(data);
}

// provide access to the functionality for inserting one custom value into a field of a row the database
void DBWrapper::insertCustomData(const crawlservpp::Main::Data::InsertValue& data) {
	this->database.insertCustomData(data);
}

// provide access to the functionality for inserting custom values into multiple fields of a row in the database
void DBWrapper::insertCustomData(const crawlservpp::Main::Data::InsertFields& data) {
	this->database.insertCustomData(data);
}

// provide access to the functionality for inserting custom values into multiple fields of a row with different types in the database
void DBWrapper::insertCustomData(const crawlservpp::Main::Data::InsertFieldsMixed& data) {
	this->database.insertCustomData(data);
}

// provide access to the functionality for updating one custom value in one field of a row in the database
void DBWrapper::updateCustomData(const crawlservpp::Main::Data::UpdateValue& data) {
	this->database.updateCustomData(data);
}

// provide access to the functionality for updating custom values in multiple fields of a row in the database
void DBWrapper::updateCustomData(const crawlservpp::Main::Data::UpdateFields& data) {
	this->database.updateCustomData(data);
}

// provide access to the functionality for updating custom values in multiple fields of a row with different types in the database
void DBWrapper::updateCustomData(const crawlservpp::Main::Data::UpdateFieldsMixed& data) {
	this->database.updateCustomData(data);
}

// provide access to the functionality for checking the connection to the database
bool DBWrapper::checkConnection() {
	return this->database.checkConnection();
}

// provide access to database for geting database error message
const std::string& DBWrapper::getDatabaseErrorMessage() const {
	return this->database.getErrorMessage();
}

// provide access to the functionality for locking a table in the database
void DBWrapper::lockTable(const std::string& tableName) {
	this->database.lockTable(tableName);
}

// provide access to the functionality for locking two tables in the database
void DBWrapper::lockTables(const std::string& tableName1, const std::string tableName2) {
	this->database.lockTables(tableName1, tableName2);
}

// provide access to the functionality for checking whether a specific table exists
bool DBWrapper::isTableExists(const std::string& tableName) {
	return this->database.isTableExists(tableName);
}

// provide access to the functionality for checking whether a specific column in a specific table exists
bool DBWrapper::isColumnExists(const std::string& tableName, const std::string& columnName) {
	return this->database.isColumnExists(tableName, columnName);
}

// provide access to the functionality for executing a SQL command
void DBWrapper::execute(const std::string& sqlQuery) {
	this->database.execute(sqlQuery);
}

// provide access to the functionality for reserving memory for prepared SQL statements
void DBWrapper::reservePreparedStatements(unsigned long numStatements) {
	this->database.preparedStatements.reserve(this->database.preparedStatements.size() + numStatements);
}

// provide access to the functionality for adding prepared SQL statement to the database, return ID of prepared statement
unsigned short DBWrapper::addPreparedStatement(const std::string& sqlStatementString) {
	DBThread::PreparedSqlStatement statement;
	statement.string = sqlStatementString;
	statement.statement = this->database.connection->prepareStatement(statement.string);
	this->database.preparedStatements.push_back(statement);
	return this->database.preparedStatements.size();
}

// provide access to the functionality for getting prepared SQL statement from database by ID
sql::PreparedStatement * DBWrapper::getPreparedStatement(unsigned short sqlStatementId) {
	return this->database.preparedStatements.at(sqlStatementId - 1).statement;
}

}
