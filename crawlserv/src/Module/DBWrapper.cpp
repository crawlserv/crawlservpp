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

// provide access to the functionality for getting the domain of a website from the database
std::string crawlservpp::Module::DBWrapper::getWebsiteDomain(unsigned long websiteId) {
	return this->database.getWebsiteDomain(websiteId);
}

// provide access to the functionality for getting the properties of a query from the database
void crawlservpp::Module::DBWrapper::getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo,
		bool& queryResultBoolTo, bool& queryResultSingleTo, bool& queryResultMultiTo, bool& queryTextOnlyTo) {
	this->database.getQueryProperties(queryId, queryTextTo, queryTypeTo, queryResultBoolTo, queryResultSingleTo, queryResultMultiTo,
			queryTextOnlyTo);
}

// provide access to the functionality for getting the current configuration
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

// provide access to the functionality for adding parsed table to database
void crawlservpp::Module::DBWrapper::addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addParsedTable(websiteId, listId, tableName);
}

// provide access to the functionality for adding extracted table to database
void crawlservpp::Module::DBWrapper::addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addExtractedTable(websiteId, listId, tableName);
}

// provide access to the functionality for adding analyzed table to database
void crawlservpp::Module::DBWrapper::addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	this->database.addAnalyzedTable(websiteId, listId, tableName);
}

// provide access to the functionality for resetting parsing status of id-specified URL list
void crawlservpp::Module::DBWrapper::resetParsingStatus(unsigned long listId) {
	this->database.resetParsingStatus(listId);
}

// provide access to the functionality for resetting extracting status of id-specified URL list
void crawlservpp::Module::DBWrapper::resetExtractingStatus(unsigned long listId) {
	this->database.resetExtractingStatus(listId);
}

// provide access to the functionality for resetting analyzing status of id-specified URL list
void crawlservpp::Module::DBWrapper::resetAnalyzingStatus(unsigned long listId) {
	this->database.resetAnalyzingStatus(listId);
}

// provide access to the functionality for getting an entry from a text column
void crawlservpp::Module::DBWrapper::getText(const std::string& tableName, const std::string& columnName, const std::string& condition,
		std::string& resultTo) {
	this->database.getText(tableName, columnName, condition, resultTo);
}

// provide access to the functionality for getting the contents of a text column
void crawlservpp::Module::DBWrapper::getTexts(const std::string& tableName, const std::string& columnName,
		std::vector<std::string>& resultTo) {
	this->database.getTexts(tableName, columnName, resultTo);
}

// provide access to the functionality for getting specific entries of a text column
void crawlservpp::Module::DBWrapper::getTexts(const std::string& tableName, const std::string& columnName, const std::string& condition,
		unsigned long limit, std::vector<std::string>& resultTo) {
	this->database.getTexts(tableName, columnName, condition, limit, resultTo);
}

// provide access to the functionality for inserting entry into a text column
void crawlservpp::Module::DBWrapper::insertText(const std::string& tableName, const std::string& columnName, const std::string& text) {
	this->database.insertText(tableName, columnName, text);
}

// provide access to the functionality for inserting entry into a text column and entry into a unsigned long column
void crawlservpp::Module::DBWrapper::insertTextUInt64(const std::string& tableName, const std::string& textColumnName,
		const std::string& numberColumnName, const std::string& text, unsigned long number) {
	this->database.insertTextUInt64(tableName, textColumnName, numberColumnName, text, number);
}

// provide access to the functionality for inserting entries into a text column
void crawlservpp::Module::DBWrapper::insertTexts(const std::string& tableName, const std::string& columnName,
		const std::vector<std::string>& texts) {
	this->database.insertTexts(tableName, columnName, texts);
}

// provide access to the functionality for updating entry in a text column
void crawlservpp::Module::DBWrapper::updateText(const std::string& tableName, const std::string& columnName, const std::string& condition,
		std::string& text) {
	this->database.updateText(tableName, columnName, condition, text);
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

// provide access to the functionality for checking whether a specific table exists
bool crawlservpp::Module::DBWrapper::isTableExists(const std::string& tableName) {
	return this->database.isTableExists(tableName);
}

// provide access to the functionality for checking whether a specific column in a specific table exists
bool crawlservpp::Module::DBWrapper::isColumnExists(const std::string& tableName, const std::string& columnName) {
	return this->database.isColumnExists(tableName, columnName);
}

// provide access to the functionality for executing a SQL command
void crawlservpp::Module::DBWrapper::execute(const std::string& sqlQuery) {
	this->database.execute(sqlQuery);
}

// provide access to the functionality for adding prepared SQL statement to the database, return ID of prepared statement
unsigned short crawlservpp::Module::DBWrapper::addPreparedStatement(const std::string& sqlStatementString) {
	crawlservpp::Struct::PreparedSqlStatement statement;
	statement.string = sqlStatementString;
	statement.statement = this->database.connection->prepareStatement(statement.string);
	this->database.preparedStatements.push_back(statement);
	return this->database.preparedStatements.size();
}

// provide access to the functionality for getting prepared SQL statement from database by ID
sql::PreparedStatement * crawlservpp::Module::DBWrapper::getPreparedStatement(unsigned short sqlStatementId) {
	return this->database.preparedStatements.at(sqlStatementId - 1).statement;
}
