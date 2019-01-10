/*
 * DatabaseModule.cpp
 *
 * Interface to be inherited by the thread modules.
 * Allows them access to the database by providing basic Database functionality as well as the option to add prepared SQL statements.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "DatabaseModule.h"

// constructor
DatabaseModule::DatabaseModule(DatabaseThread& dbRef) : database(dbRef) {}

// destructor stub
DatabaseModule::~DatabaseModule() {}

// get error message
const std::string& DatabaseModule::getErrorMessage() const {
	return this->errorMessage;
}

// provice access to setting the error sleep time
void DatabaseModule::setSleepOnError(unsigned long seconds) {
	this->database.setSleepOnError(seconds);
}

// provide access to logging functionality
void DatabaseModule::log(const std::string& logModule, const std::string& logEntry) {
	this->database.log(logModule, logEntry);
}

// provide access for getting the domain of a website from the database
std::string DatabaseModule::getWebsiteDomain(unsigned long websiteId) {
	return this->database.getWebsiteDomain(websiteId);
}

// provide access for getting the properties of a query from the database
void DatabaseModule::getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo,
		bool& queryResultBoolTo, bool& queryResultSingleTo, bool& queryResultMultiTo, bool& queryTextOnlyTo) {
	this->database.getQueryProperties(queryId, queryTextTo, queryTypeTo, queryResultBoolTo, queryResultSingleTo, queryResultMultiTo,
			queryTextOnlyTo);
}

// provide access for getting the current configuration
std::string DatabaseModule::getConfigJson(unsigned long configId) {
	return this->database.getConfiguration(configId);
}

// provide access to the functionality for getting the last inserted id
unsigned long DatabaseModule::getLastInsertedId() {
	return this->database.getLastInsertedId();
}

// provide access to the functionality for unlocking all tables in the database
void DatabaseModule::unlockTables() {
	this->database.unlockTables();
}

// add parsed table to database
void DatabaseModule::addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addParsedTable(websiteId, listId, tableName);
}

// add extracted table to database
void DatabaseModule::addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addExtractedTable(websiteId, listId, tableName);
}

// add analyzed table to database
void DatabaseModule::addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addAnalyzedTable(websiteId, listId, tableName);
}

// reset parsing status of id-specified URL list
void DatabaseModule::resetParsingStatus(unsigned long listId) {
	this->database.resetParsingStatus(listId);
}

// reset extracting status of id-specified URL list
void DatabaseModule::resetExtractingStatus(unsigned long listId) {
	this->database.resetExtractingStatus(listId);
}

// reset analyzing status of id-specified URL list
void DatabaseModule::resetAnalyzingStatus(unsigned long listId) {
	this->database.resetAnalyzingStatus(listId);
}

// provide access to the functionality for checking the connection to the database
bool DatabaseModule::checkConnection() {
	return this->database.checkConnection();
}

// provide access to the functionality for locking a table in the database
void DatabaseModule::lockTable(const std::string& tableName) {
	this->database.lockTable(tableName);
}

// provide access to the functionality for locking two tables in the database
void DatabaseModule::lockTables(const std::string& tableName1, const std::string tableName2) {
	this->database.lockTables(tableName1, tableName2);
}

// check whether a specific table exists
bool DatabaseModule::isTableExists(const std::string& tableName) {
	return this->database.isTableExists(tableName);
}

// check whether a specific column in a specific table exists
bool DatabaseModule::isColumnExists(const std::string& tableName, const std::string& columnName) {
	return this->database.isColumnExists(tableName, columnName);
}

// execute SQL command
void DatabaseModule::execute(const std::string& sqlQuery) {
	this->database.execute(sqlQuery);
}

// add prepared SQL statement to the database, return ID of prepared statement
unsigned short DatabaseModule::addPreparedStatement(const std::string& sqlStatementString) {
	PreparedSqlStatement statement;
	statement.string = sqlStatementString;
	statement.statement = this->database.connection->prepareStatement(statement.string);
	this->database.preparedStatements.push_back(statement);
	return this->database.preparedStatements.size();
}

// get prepared SQL statement from database by ID
sql::PreparedStatement * DatabaseModule::getPreparedStatement(unsigned short sqlStatementId) {
	return this->database.preparedStatements.at(sqlStatementId - 1).statement;
}
