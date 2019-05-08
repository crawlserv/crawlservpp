/*
 * Database.cpp
 *
 * Database functionality for a single thread.
 *
 * Only implements module-independent functionality. For module-specific functionality use the
 *  child classes of the Wrapper::Database interface instead.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module {

	// constructor
	Database::Database(const DatabaseSettings& dbSettings, const std::string& dbModule)
							: Main::Database(dbSettings, dbModule),
							  logging(true),
							  verbose(false),
							  ps(_ps()) {
		if(Main::Database::driver)
			Main::Database::driver->threadInit();
		else
			throw(Exception("MySQL driver not loaded"));
	}

	// destructor stub
	Database::~Database() {
		if(Main::Database::driver)
			Main::Database::driver->threadEnd();
	}

	// set general module options and convert IDs to strings
	void Database::setOptions(const ModuleOptions& moduleOptions) {
		std::ostringstream idStrStr;

		idStrStr << moduleOptions.threadId;

		this->threadIdString = idStrStr.str();

		idStrStr.str("");

		idStrStr.clear();

		idStrStr << moduleOptions.websiteId;

		this->websiteIdString = idStrStr.str();

		idStrStr.str("");

		idStrStr.clear();

		idStrStr << moduleOptions.urlListId;

		this->urlListIdString = idStrStr.str();

		this->options = moduleOptions;
	}

	// set logging options
	void Database::setLogging(bool isLogging, bool isVerbose) {
		this->logging = isLogging;
		this->verbose = isVerbose;
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
		if(!(this->ps.setThreadStatusMessage))
			this->ps.setThreadStatusMessage = this->addPreparedStatement(
					"UPDATE crawlserv_threads"
					" SET status = ?,"
					" paused = ?"
					" WHERE id = ?"
					" LIMIT 1"
			);

		if(!(this->ps.setThreadProgress))
			this->ps.setThreadProgress = this->addPreparedStatement(
					"UPDATE crawlserv_threads"
					" SET progress = ?,"
					" runtime = ?"
					" WHERE id = ?"
					" LIMIT 1"
			);

		if(!(this->ps.setThreadLast))
			this->ps.setThreadLast = this->addPreparedStatement(
					"UPDATE crawlserv_threads"
					" SET last = ?"
					" WHERE id = ?"
					" LIMIT 1"
			);
	}

	// add thread-specific logging entry to database
	void Database::log(const std::string& logEntry) {
		this->Main::Database::log("[#" + this->threadIdString + "] " + logEntry);
	}

	// set the status message of a thread (and add the pause state), throws Database::Exception
	void Database::setThreadStatusMessage(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadStatusMessage))
			throw Exception("Missing prepared SQL statement for Module::Database::setThreadStatusMessage(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.setThreadStatusMessage));

		// create status message
		std::string statusMessage;

		if(threadPaused) {
			if(!threadStatusMessage.empty())
				statusMessage = "{PAUSED} " + threadStatusMessage;
			else
				statusMessage = "{PAUSED}";
		}
		else
			statusMessage = threadStatusMessage;

		try {
			// execute SQL statement
			sqlStatement.setString(1, statusMessage);
			sqlStatement.setBoolean(2, threadPaused);
			sqlStatement.setUInt64(3, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Module::Database::setThreadStatusMessage", e); }
	}

	// set the progress of a thread to between 0 for 0% and 1 for 100% in database (and update runtime),
	//  throws Database::Exception
	void Database::setThreadProgress(unsigned long threadId, float threadProgress, unsigned long threadRunTime) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadProgress))
			throw Exception("Missing prepared SQL statement for Module::Database::setThreadProgress(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.setThreadProgress));

		try {
			// execute SQL statement
			sqlStatement.setDouble(1, threadProgress);
			sqlStatement.setUInt64(2, threadRunTime);
			sqlStatement.setUInt64(3, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Module::Database::setThreadProgress", e); }
	}

	// set last ID of thread in database, throws Database::Exception
	void Database::setThreadLast(unsigned long threadId, unsigned long threadLast) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadLast))
			throw Exception("Missing prepared SQL statement for Module::Database::setThreadLast(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.setThreadLast));

		try {
			// execute SQL statement
			sqlStatement.setUInt64(1, threadLast);
			sqlStatement.setUInt64(2, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Module::Database::setThreadLast", e); }
	}

} /* crawlservpp::Module */
