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

// constructor
crawlservpp::Module::DBWrapper::DBWrapper(crawlservpp::Module::DBThread& dbRef) : database(dbRef) {}

// destructor stub
crawlservpp::Module::DBWrapper::~DBWrapper() {}

// get module error message
const std::string& crawlservpp::Module::DBWrapper::getModuleErrorMessage() const {
	return this->errorMessage;
}

// provice access to setting the error sleep time
void crawlservpp::Module::DBWrapper::setSleepOnError(unsigned long seconds) {
	this->database.setSleepOnError(seconds);
}

// provide access to logging functionality
void crawlservpp::Module::DBWrapper::log(const std::string& logModule, const std::string& logEntry) {
	this->database.log(logModule, logEntry);
}

// provide access for getting the domain of a website from the database
std::string crawlservpp::Module::DBWrapper::getWebsiteDomain(unsigned long websiteId) {
	return this->database.getWebsiteDomain(websiteId);
}

// provide access for getting the properties of a query from the database
void crawlservpp::Module::DBWrapper::getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo,
		bool& queryResultBoolTo, bool& queryResultSingleTo, bool& queryResultMultiTo, bool& queryTextOnlyTo) {
	this->database.getQueryProperties(queryId, queryTextTo, queryTypeTo, queryResultBoolTo, queryResultSingleTo, queryResultMultiTo,
			queryTextOnlyTo);
}

// provide access for getting the current configuration
std::string crawlservpp::Module::DBWrapper::getConfigJson(unsigned long configId) {
	return this->database.getConfiguration(configId);
}

// provide access to the functionality for getting the last inserted id
unsigned long crawlservpp::Module::DBWrapper::getLastInsertedId() {
	return this->database.getLastInsertedId();
}

// provide access to the functionality for unlocking all tables in the database
void crawlservpp::Module::DBWrapper::unlockTables() {
	this->database.unlockTables();
}

// add parsed table to database
void crawlservpp::Module::DBWrapper::addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addParsedTable(websiteId, listId, tableName);
}

// add extracted table to database
void crawlservpp::Module::DBWrapper::addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addExtractedTable(websiteId, listId, tableName);
}

// add analyzed table to database
void crawlservpp::Module::DBWrapper::addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addAnalyzedTable(websiteId, listId, tableName);
}

// reset parsing status of id-specified URL list
void crawlservpp::Module::DBWrapper::resetParsingStatus(unsigned long listId) {
	this->database.resetParsingStatus(listId);
}

// reset extracting status of id-specified URL list
void crawlservpp::Module::DBWrapper::resetExtractingStatus(unsigned long listId) {
	this->database.resetExtractingStatus(listId);
}

// reset analyzing status of id-specified URL list
void crawlservpp::Module::DBWrapper::resetAnalyzingStatus(unsigned long listId) {
	this->database.resetAnalyzingStatus(listId);
}

// provide access to the functionality for checking the connection to the database
bool crawlservpp::Module::DBWrapper::checkConnection() {
	return this->database.checkConnection();
}

// provide access to database for geting database error message
const std::string& crawlservpp::Module::DBWrapper::getDatabaseErrorMessage() const {
	return this->database.getErrorMessage();
}

// provide access to the functionality for locking a table in the database
void crawlservpp::Module::DBWrapper::lockTable(const std::string& tableName) {
	this->database.lockTable(tableName);
}

// provide access to the functionality for locking two tables in the database
void crawlservpp::Module::DBWrapper::lockTables(const std::string& tableName1, const std::string tableName2) {
	this->database.lockTables(tableName1, tableName2);
}

// check whether a specific table exists
bool crawlservpp::Module::DBWrapper::isTableExists(const std::string& tableName) {
	return this->database.isTableExists(tableName);
}

// check whether a specific column in a specific table exists
bool crawlservpp::Module::DBWrapper::isColumnExists(const std::string& tableName, const std::string& columnName) {
	return this->database.isColumnExists(tableName, columnName);
}

// execute SQL command
void crawlservpp::Module::DBWrapper::execute(const std::string& sqlQuery) {
	this->database.execute(sqlQuery);
}

// add prepared SQL statement to the database, return ID of prepared statement
unsigned short crawlservpp::Module::DBWrapper::addPreparedStatement(const std::string& sqlStatementString) {
	crawlservpp::Struct::PreparedSqlStatement statement;
	statement.string = sqlStatementString;
	statement.statement = this->database.connection->prepareStatement(statement.string);
	this->database.preparedStatements.push_back(statement);
	return this->database.preparedStatements.size();
}

// get prepared SQL statement from database by ID
sql::PreparedStatement * crawlservpp::Module::DBWrapper::getPreparedStatement(unsigned short sqlStatementId) {
	return this->database.preparedStatements.at(sqlStatementId - 1).statement;
}
