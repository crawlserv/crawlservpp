/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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
							  loggingLevel(std::numeric_limits<std::uint8_t>::max()),
							  loggingMin(1),
							  loggingVerbose(std::numeric_limits<std::uint8_t>::max()),
							  debugLogging(dbSettings.debugLogging),
							  debugDir(dbSettings.debugDir),
							  ps(_ps()) {
		if(Main::Database::driver)
			Main::Database::driver->threadInit();
		else
			throw Exception("MySQL driver not loaded");
	}

	// destructor stub
	Database::~Database() {
		if(Main::Database::driver)
			Main::Database::driver->threadEnd();

		if(this->loggingFile.is_open())
			this->loggingFile.close();
	}

	// set general module options and convert IDs to strings
	void Database::setOptions(const ModuleOptions& moduleOptions) {
		this->options = moduleOptions;

		if(moduleOptions.threadId)
			this->threadIdString = std::to_string(moduleOptions.threadId);

		this->websiteIdString = std::to_string(moduleOptions.websiteId);
		this->urlListIdString = std::to_string(moduleOptions.urlListId);
	}

	// set ID of thread and convert it to string
	void Database::setThreadId(std::uint64_t threadId) {
		if(!threadId)
			throw Exception("No thread ID specified");

		this->options.threadId = threadId;
		this->threadIdString = std::to_string(threadId);
	}

	// set current, minimal and verbose logging levels
	void Database::setLogging(std::uint8_t level, std::uint8_t min, std::uint8_t verbose) {
		this->loggingLevel = level;
		this->loggingMin = min;
		this->loggingVerbose = verbose;

		// initialize debug logging if necessary
		if(this->debugLogging && !(this->threadIdString.empty())) {
			const std::string loggingFileName(this->debugDir + Helper::FileSystem::getPathSeparator() + this->threadIdString);

			if(this->loggingFile.is_open()) {
				this->loggingFile.close();
				this->loggingFile.clear();
			}

			this->loggingFile.open(this->debugDir + Helper::FileSystem::getPathSeparator() + this->threadIdString.c_str());

			if(!(this->loggingFile.is_open()))
				throw Exception("Could not open \'" + loggingFileName + "\' for writing.");
		}
	}

	// prepare SQL statements for thread management
	void Database::prepare() {
		// prepare basic functions
		this->Main::Database::prepare();

		// check connection
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(std::uint16_t));

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

	// write thread-specific log entry to the database
	//  NOTE: log entry will not be written if the logging level is lower than the specified level for the entry
	void Database::log(std::uint8_t level, const std::string& logEntry) {
		if(level <= this->loggingLevel)
			// write log entry to database
			this->Main::Database::log("[#" + this->threadIdString + "] " + logEntry);

		if(this->debugLogging && this->loggingFile.is_open()) {
			// repair log entry if necessary
			std::string repairedEntry;

			const bool repaired = Helper::Utf8::repairUtf8(logEntry, repairedEntry);

			// write log entry to file
			if(repaired)
				this->loggingFile
						<< "["
						<< Helper::DateTime::now()
						<< "] "
						<< repairedEntry
						<< " [invalid UTF-8 character(s) removed from log]\n"
						<< std::flush;
			else
				this->loggingFile
						<< "["
						<< Helper::DateTime::now()
						<< "] "
						<< logEntry
						<< "\n"
						<< std::flush;
		}
	}

	// write multiple thread-specific log entries to the database
	//  NOTE: log entries will not be written if the logging level is lower than the specified level for the entries
	void Database::log(std::uint8_t level, std::queue<std::string>& logEntries) {
		if(level <= this->loggingLevel) {
			while(!logEntries.empty()) {
				// write log entry to database
				this->Main::Database::log("[#" + this->threadIdString + "] " + logEntries.front());

				if(this->debugLogging && this->loggingFile.is_open()) {
					// repair log entry if necessary
					std::string repairedEntry;

					const bool repaired = Helper::Utf8::repairUtf8(logEntries.front(), repairedEntry);

					if(repaired)
						repairedEntry += " [invalid UTF-8 character(s) removed from log]";

					// write log entry to file
					this->loggingFile << repairedEntry << "\n";
				}

				logEntries.pop();
			}

			if(this->debugLogging && this->loggingFile.is_open())
				this->loggingFile.flush();
		}
		else
			std::queue<std::string>().swap(logEntries);
	}

	// check whether a certain log level is active
	bool Database::isLogLevel(std::uint8_t level) const {
		return level <= this->loggingLevel;
	}

	// set the status message of a thread (and add the pause state), throws Database::Exception
	void Database::setThreadStatusMessage(std::uint64_t threadId, bool threadPaused, const std::string& threadStatusMessage) {
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
	void Database::setThreadProgress(std::uint64_t threadId, float threadProgress, std::uint64_t threadRunTime) {
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
	void Database::setThreadLast(std::uint64_t threadId, std::uint64_t threadLast) {
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
