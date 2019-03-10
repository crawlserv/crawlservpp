/*
 * Database.cpp
 *
 * Database functionality for a single thread. Only implements module-independent functionality, for module-specific functionality use the
 *  child classes of the DatabaseModule interface instead.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module {

	// constructor
	Database::Database(const DatabaseSettings& dbSettings, const std::string& dbModule)
				: Main::Database(dbSettings, dbModule), ps({0}) {
		if(Main::Database::driver) Main::Database::driver->threadInit();
		else throw(Database::Exception("MySQL driver not loaded"));
	}

	// destructor stub
	Database::~Database() {
		if(Main::Database::driver) Main::Database::driver->threadEnd();
	}

	// prepare SQL statements for thread management
	void Database::prepare() {
		// prepare basic functions
		this->Main::Database::prepare();

		// check connection
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(unsigned short));

		// prepare general SQL statements for thread
		if(!(this->ps.setThreadStatusMessage)) this->ps.setThreadStatusMessage = this->addPreparedStatement(
				"UPDATE crawlserv_threads SET status = ?, paused = ? WHERE id = ? LIMIT 1");
		if(!(this->ps.setThreadProgress)) this->ps.setThreadProgress = this->addPreparedStatement(
				"UPDATE crawlserv_threads SET progress = ? WHERE id = ? LIMIT 1");
		if(!(this->ps.setThreadLast)) this->ps.setThreadLast = this->addPreparedStatement(
				"UPDATE crawlserv_threads SET last = ? WHERE id = ? LIMIT 1");
	}

	// set the status message of a thread (and add the pause state)
	void Database::setThreadStatusMessage(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadStatusMessage))
			throw Database::Exception("Missing prepared SQL statement for Module::Database::setThreadStatusMessage(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setThreadStatusMessage);

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
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Module::Database::setThreadStatusMessage", e); }
	}

	// set the progress of a thread to between 0 for 0% and 1 for 100% in database
	void Database::setThreadProgress(unsigned long threadId, float threadProgress) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadProgress))
			throw Database::Exception("Missing prepared SQL statement for Module::Database::setThreadProgress(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setThreadProgress);

		try {
			// execute SQL statement
			sqlStatement.setDouble(1, threadProgress);
			sqlStatement.setUInt64(2, threadId);
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Module::Database::setThreadProgress", e); }
	}

	// set last id of thread in database
	void Database::setThreadLast(unsigned long threadId, unsigned long threadLast) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadLast))
			throw Database::Exception("Missing prepared SQL statement for Module::Database::setThreadLast(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setThreadLast);

		try {
			// execute SQL statement
			sqlStatement.setUInt64(1, threadLast);
			sqlStatement.setUInt64(2, threadId);
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Module::Database::setThreadLast", e); }
	}

} /* crawlservpp::Module */
