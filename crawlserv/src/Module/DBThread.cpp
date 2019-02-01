/*
 * DBThread.cpp
 *
 * Database functionality for a single thread. Only implements module-independent functionality, for module-specific functionality use the
 *  child classes of the DatabaseModule interface instead.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "DBThread.h"

namespace crawlservpp::Module {

// constructor
DBThread::DBThread(const crawlservpp::Struct::DatabaseSettings& dbSettings)
		: crawlservpp::Main::Database(dbSettings) {
	this->psSetThreadStatusMessage = 0;
	this->psSetThreadProgress = 0;
	this->psSetThreadLast = 0;
	if(crawlservpp::Main::Database::driver)
		crawlservpp::Main::Database::driver->threadInit();
	else throw(Database::Exception("MySQL driver not loaded"));
}

// destructor stub
DBThread::~DBThread() {
	if(crawlservpp::Main::Database::driver)
		crawlservpp::Main::Database::driver->threadEnd();
}

// prepare SQL statements for thread management
bool DBThread::prepare() {
	// prepare basic functions
	this->Database::prepare();

	// check connection
	if(!(this->checkConnection())) return false;

	// reserve memory
	this->preparedStatements.reserve(3);

	try {
		// prepare general SQL statements for thread
		if(!(this->psSetThreadStatusMessage)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET status = ?, paused = ? WHERE id = ? LIMIT 1";
			statement.statement = std::unique_ptr<sql::PreparedStatement>(this->connection->prepareStatement(statement.string));
			this->preparedStatements.push_back(std::move(statement));
			this->psSetThreadStatusMessage = this->preparedStatements.size();
		}
		// prepare general SQL statements for thread
		if(!(this->psSetThreadProgress)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET progress = ? WHERE id = ? LIMIT 1";
			statement.statement = std::unique_ptr<sql::PreparedStatement>(this->connection->prepareStatement(statement.string));
			this->preparedStatements.push_back(std::move(statement));
			this->psSetThreadProgress = this->preparedStatements.size();
		}
		if(!(this->psSetThreadLast)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET last = ? WHERE id = ? LIMIT 1";
			statement.statement = std::unique_ptr<sql::PreparedStatement>(this->connection->prepareStatement(statement.string));
			this->preparedStatements.push_back(std::move(statement));
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
void DBThread::setThreadStatusMessage(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage) {
	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	// check prepared SQL statement
	if(!(this->psSetThreadStatusMessage))
		throw Database::Exception("Missing prepared SQL statement for Database::setThreadStatusMessage(...)");
	sql::PreparedStatement& sqlStatement = *(this->preparedStatements.at(this->psSetThreadStatusMessage - 1).statement);

	// create status message
	std::string statusMessage;
	if(threadPaused) {
		if(!threadStatusMessage.empty()) statusMessage = "{PAUSED} " + threadStatusMessage;
		else statusMessage = "{PAUSED}";
	}
	else statusMessage = threadStatusMessage;

	try {
		// execute SQL statement
		sqlStatement.setString(1, statusMessage);
		sqlStatement.setBoolean(2, threadPaused);
		sqlStatement.setUInt64(3, threadId);
		sqlStatement.execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadStatusMessage() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
}

// set the progress of a thread to between 0 for 0% and 1 for 100% in database
void DBThread::setThreadProgress(unsigned long threadId, float threadProgress) {
	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	// check prepared SQL statement
	if(!(this->psSetThreadProgress))
		throw Database::Exception("Missing prepared SQL statement for Database::setThreadProgress(...)");
	sql::PreparedStatement& sqlStatement = *(this->preparedStatements.at(this->psSetThreadProgress - 1).statement);

	try {
		// execute SQL statement
		sqlStatement.setDouble(1, threadProgress);
		sqlStatement.setUInt64(2, threadId);
		sqlStatement.execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadProgress() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
}

// set last id of thread in database
void DBThread::setThreadLast(unsigned long threadId, unsigned long threadLast) {
	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	// check prepared SQL statement
	if(!(this->psSetThreadLast)) throw Database::Exception("Missing prepared SQL statement for Database::setThreadLast(...)");
	sql::PreparedStatement& sqlStatement = *(this->preparedStatements.at(this->psSetThreadLast - 1).statement);

	try {
		// execute SQL statement
		sqlStatement.setUInt64(1, threadLast);
		sqlStatement.setUInt64(2, threadId);
		sqlStatement.execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadLast() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
}

}
