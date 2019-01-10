/*
 * DatabaseThread.cpp
 *
 * Database functionality for a single thread. Only implements module-independent functionality, for module-specific functionality use the
 *  child classes of the DatabaseModule interface instead.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "DatabaseThread.h"

// constructor
DatabaseThread::DatabaseThread(const DatabaseSettings& dbSettings) : Database(dbSettings) {
	this->psSetThreadStatusMessage = 0;
	this->psSetThreadProgress = 0;
	this->psSetThreadLast = 0;
	if(Database::driver) Database::driver->threadInit();
	else throw(std::runtime_error("MySQL driver not loaded"));
}

// destructor stub
DatabaseThread::~DatabaseThread() {
	if(Database::driver) Database::driver->threadEnd();
}

// prepare SQL statements for thread management
bool DatabaseThread::prepare() {
	// prepare basic functions
	this->Database::prepare();

	// check connection
	if(!(this->checkConnection())) return false;

	try {
		// prepare general SQL statements for thread
		if(!(this->psSetThreadStatusMessage)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET status = ?, paused = ? WHERE id = ? LIMIT 1";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psSetThreadStatusMessage = this->preparedStatements.size();
		}
		// prepare general SQL statements for thread
		if(!(this->psSetThreadProgress)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET progress = ? WHERE id = ? LIMIT 1";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psSetThreadProgress = this->preparedStatements.size();
		}
		if(!(this->psSetThreadLast)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET last = ? WHERE id = ? LIMIT 1";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psSetThreadLast = this->preparedStatements.size();
		}
	}
	catch(sql::SQLException &e) {
		// set error message
		std::ostringstream errorStrStr;
		errorStrStr << "prepare() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();
		return false;
	}

	return true;
}

// set the status message of a thread (and add the pause state)
void DatabaseThread::setThreadStatusMessage(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage) {
	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check prepared SQL statement
	if(!(this->psSetThreadStatusMessage))
		throw std::runtime_error("Missing prepared SQL statement for Database::setThreadStatusMessage(...)");
	sql::PreparedStatement * sqlStatement = this->preparedStatements.at(this->psSetThreadStatusMessage - 1).statement;
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Database::setThreadStatusMessage(...) is NULL");

	// create status message
	std::string statusMessage;
	if(threadPaused) {
		if(threadStatusMessage.length()) statusMessage = "{PAUSED} " + threadStatusMessage;
		else statusMessage = "{PAUSED}";
	}
	else statusMessage = threadStatusMessage;

	try {
		// execute SQL statement
		sqlStatement->setString(1, statusMessage);
		sqlStatement->setBoolean(2, threadPaused);
		sqlStatement->setUInt64(3, threadId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadStatusMessage() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// set the progress of a thread to between 0 for 0% and 1 for 100% in database
void DatabaseThread::setThreadProgress(unsigned long threadId, float threadProgress) {
	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check prepared SQL statement
	if(!(this->psSetThreadProgress))
		throw std::runtime_error("Missing prepared SQL statement for Database::setThreadProgress(...)");
	sql::PreparedStatement * sqlStatement = this->preparedStatements.at(this->psSetThreadProgress - 1).statement;
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Database::setThreadProgress(...) is NULL");

	try {
		// execute SQL statement
		sqlStatement->setDouble(1, threadProgress);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadProgress() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// set last id of thread in database
void DatabaseThread::setThreadLast(unsigned long threadId, unsigned long threadLast) {
	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check prepared SQL statement
	if(!(this->psSetThreadLast)) throw std::runtime_error("Missing prepared SQL statement for Database::setThreadLast(...)");
	sql::PreparedStatement * sqlStatement = this->preparedStatements.at(this->psSetThreadLast - 1).statement;
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Database::setThreadLast(...) is NULL");

	try {
		// execute SQL statement
		sqlStatement->setUInt64(1, threadLast);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadLast() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}
