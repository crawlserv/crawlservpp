/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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

	//! Constructor setting database settings and the module.
	/*!
	 * \param dbSettings Constant reference
	 *   to a structure containing the
	 *   database settings.
	 * \param dbModule Constant reference to
	 *   a string containing the name of the
	 *   module of the current thread.
	 *
	 * \throws Module::Database::Exception if
	 *   the MySQL driver has not been loaded.
	 *
	 * \sa Struct::DatabaseSettings
	 */
	Database::Database(const DatabaseSettings& dbSettings, const std::string& dbModule)
	 : Main::Database(dbSettings, dbModule),
	   debugLogging(dbSettings.debugLogging),
	   debugDir(dbSettings.debugDir) {
		if(Main::Database::driver == nullptr) {
			throw Exception("MySQL driver not loaded");
		}

		// initialize the thread for the MySQL driver
		Main::Database::driver->threadInit();
	}

	//! Destructor clearing the thread.
	/*!
	 * Also closes the logging file, if debug
	 *  logging via file had been active.
	 */
	Database::~Database() {
		// clear the thread for the MySQL driver
		if(Main::Database::driver != nullptr) {
			Main::Database::driver->threadEnd();
		}

		// close the logging file
		if(this->loggingFile.is_open()) {
			this->loggingFile.close();
		}
	}

	//! Sets general options for the module.
	/*!
	 * Also converts all IDs to strings for
	 *  easier usage later on.
	 *
	 * \param moduleOptions Constant
	 *   reference to a structure
	 *   containing the options for the
	 *   current module.
	 *
	 * \sa Struct::ModuleOptions
	 */
	void Database::setOptions(const ModuleOptions& moduleOptions) {
		this->options = moduleOptions;

		if(moduleOptions.threadId > 0) {
			this->threadIdString = std::to_string(moduleOptions.threadId);
		}

		this->websiteIdString = std::to_string(moduleOptions.websiteId);
		this->urlListIdString = std::to_string(moduleOptions.urlListId);
	}

	//! Sets the ID of the thread.
	/*!
	 * Also converts it to a string for easier
	 *  use later on.
	 *
	 * \param threadId Sets the ID of the
	 *   thread.
	 *
	 * \throws Module::Database::Exception if
	 *   no thread ID has been specified, i.e.
	 *   the given thread ID is zero.
	 */
	void Database::setThreadId(std::uint64_t threadId) {
		if(threadId == 0) {
			throw Exception(
					"Module::Database::setThreadId():"
					" No thread ID specified"
			);
		}

		this->options.threadId = threadId;
		this->threadIdString = std::to_string(threadId);
	}

	//! Sets the current, minimal, and verbose logging levels.
	/*!
	 * Initializes debug logging via logging
	 *  file if necessary.
	 *
	 * \param level The current logging
	 *   level.
	 * \param min The minimum logging
	 *   level.
	 * \param verbose The verbose logging
	 *   level.
	 *
	 * \throws Module::Database::Exception if
	 *   the logging file could not be opened
	 *   for writing.
	 */
	void Database::setLogging(std::uint8_t level, std::uint8_t min, std::uint8_t verbose) {
		this->loggingLevel = level;
		this->loggingMin = min;
		this->loggingVerbose = verbose;

		// initialize debug logging if necessary
		if(this->debugLogging && !(this->threadIdString.empty())) {
			std::string loggingFileName;

			loggingFileName.reserve(
					this->debugDir.length()
					+ this->threadIdString.length()
					+ 1 // for the path separator
			);

			loggingFileName = this->debugDir;

			loggingFileName += Helper::FileSystem::getPathSeparator();
			loggingFileName += this->threadIdString;

			if(this->loggingFile.is_open()) {
				this->loggingFile.close();
				this->loggingFile.clear();
			}

			this->loggingFile.open(loggingFileName.c_str());

			if(!(this->loggingFile.is_open())) {
				throw Exception(
						"Could not open '"
						+ loggingFileName
						+ "' for writing."
				);
			}
		}
	}

	//! Prepares SQL statements for basic thread management.
	void Database::prepare() {
		// prepare basic functions
		this->Main::Database::prepare();

		// check connection
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(std::size_t));

		// prepare general SQL statements for thread
		this->addPreparedStatement(
				"UPDATE `crawlserv_threads`"
				" SET status = ?,"
				" paused = ?"
				" WHERE id = ?"
				" LIMIT 1",
				this->ps.setThreadStatusMessage
		);

		this->addPreparedStatement(
				"UPDATE `crawlserv_threads`"
				" SET progress = ?,"
				" runtime = ?"
				" WHERE id = ?"
				" LIMIT 1",
				this->ps.setThreadProgress
		);

		this->addPreparedStatement(
				"UPDATE `crawlserv_threads`"
				" SET"
				"  last = ?,"
				"  processed = ?"
				" WHERE id = ?"
				" LIMIT 1",
				this->ps.setThreadLast
		);
	}

	//! Writes a thread-specific log entry to the database.
	/*!
	 * Removes invalid UTF-8 characters if
	 *  necessary.
	 *
	 * If debug logging is active, the entry
	 *  will be written to the logging file
	 *  as well.
	 *
	 * The log entry will not be written to the
	 *   database, if the current logging level
	 *   is lower than the specified logging
	 *   level. The logging level does not
	 *   affect the writing of logging entries
	 *   being to the logging file when debug
	 *   logging is active.
	 *
	 * \note String views cannot be used, because
	 *   they are not supported by the API for the
	 *   MySQL database.
	 *
	 * \param level The logging level for the
	 *   entry. The entry will only be written
	 *   to the database, if the current logging
	 *   level is at least the logging level for
	 *   the entry.
	 * \param logEntry Constant reference to a
	 *   string containing the log entry.
	 */
	void Database::log(std::uint8_t level, const std::string& logEntry) {
		if(level <= this->loggingLevel) {
			// write log entry to database
			this->Main::Database::log(
					"[#"
					+ this->threadIdString
					+ "] "
					+ logEntry
			);
		}

		if(this->debugLogging && this->loggingFile.is_open()) {
			// repair log entry if necessary
			std::string repairedEntry;

			const bool repaired{
				Helper::Utf8::repairUtf8(
						logEntry,
						repairedEntry
				)
			};

			// write log entry to file
			if(repaired) {
				this->loggingFile
						<< "["
						<< Helper::DateTime::now()
						<< "] "
						<< repairedEntry
						<< " [invalid UTF-8 character(s) removed from log]\n"
						<< std::flush;
			}
			else {
				this->loggingFile
						<< "["
						<< Helper::DateTime::now()
						<< "] "
						<< logEntry
						<< "\n"
						<< std::flush;
			}
		}
	}

	//! Writes multiple thread-specific log entries to the database.
	/*!
	 * Removes invalid UTF-8 characters if
	 *  necessary.
	 *
	 * If debug logging is active, the entries
	 *  will be written to the logging file
	 *  as well.
	 *
	 * The log entries will not be written to
	 *   the database, if the current logging
	 *   level is lower than the specified
	 *   logging level. The logging level does
	 *   not affect the writing of logging
	 *   entries being to the logging file when
	 *   debug logging is active.
	 *
	 * \note String views cannot be used, because
	 *   they are not supported by the API for the
	 *   MySQL database.
	 *
	 * \param level The logging level for the
	 *   entries. The entries will only be
	 *   written to the database, if the current
	 *   logging level is at least the logging
	 *   level for the entry.
	 * \param logEntries Reference to a queue of
	 *   strings containing the log entries to
	 *   be written. It will be emptied
	 *   regardless whether the log entries
	 *   will be written to the database.
	 */
	void Database::log(std::uint8_t level, std::queue<std::string>& logEntries) {
		if(level > this->loggingLevel && (!this->debugLogging)) {
			Helper::Memory::free(logEntries);

			return;
		}

		while(!logEntries.empty()) {
			if(level <= this->loggingLevel) {
				// write log entry to database
				this->Main::Database::log(
						"[#"
						+ this->threadIdString
						+ "] "
						+ logEntries.front()
				);
			}

			if(this->debugLogging && this->loggingFile.is_open()) {
				// repair log entry if necessary
				std::string repairedEntry;

				const bool repaired{
					Helper::Utf8::repairUtf8(
							logEntries.front(),
							repairedEntry
					)
				};

				if(repaired) {
					repairedEntry += " [invalid UTF-8 character(s) removed from log]";
				}

				// write log entry to file
				this->loggingFile << repairedEntry << "\n";
			}

			logEntries.pop();
		}

		if(this->debugLogging && this->loggingFile.is_open()) {
			this->loggingFile.flush();
		}
	}

	//! Checks whether a certain logging level is active.
	/*!
	 * \param level The logging level to be
	 *   checked for.
	 *
	 * \returns True, if given logging level is active,
	 *   i.e. if the current logging level larger or
	 *   equal to the given logging level.
	 *   False otherwise.
	 */
	bool Database::isLogLevel(std::uint8_t level) const {
		return level <= this->loggingLevel;
	}

	//! Saves the current status of a thread to the database.
	/*!
	 * Adds the pause state if necessary.
	 *
	 * \param threadId The ID of the thread
	 *   for which status message and pause
	 *   state will be written to the
	 *   database.
	 * \param threadPaused Specified whether
	 *   the thread is currently paused.
	 * \param threadStatusMessage Constant
	 *   reference to a string containing
	 *   the current status of the thread.
	 *
	 * \throws Module::Database::Exception if
	 *   the prepared SQL statement for
	 *   setting the status is missing.
	 * \throws Main::Database::Exception if a
	 *   MySQL error occured while saving
	 *   the status of the thread to the
	 *   database.
	 */
	void Database::setThreadStatusMessage(
			std::uint64_t threadId,
			bool threadPaused,
			const std::string& threadStatusMessage
	) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.setThreadStatusMessage == 0) {
			throw Exception(
					"Module::Database::setThreadStatusMessage():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{
			this->getPreparedStatement(this->ps.setThreadStatusMessage)
		};

		// create status message
		std::string statusMessage;

		if(threadPaused) {
			if(!threadStatusMessage.empty()) {
				statusMessage = "{PAUSED} " + threadStatusMessage;
			}
			else {
				statusMessage = "{PAUSED}";
			}
		}
		else {
			statusMessage = threadStatusMessage;
		}

		try {
			// execute SQL statement
			sqlStatement.setString(sqlArg1, statusMessage);
			sqlStatement.setBoolean(sqlArg2, threadPaused);
			sqlStatement.setUInt64(sqlArg3, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Module::Database::setThreadStatusMessage", e);
		}
	}

	//! Saves the current progress and runtime of a thread to the database.
	/*!
	 * \param threadId The ID of the thread
	 *   for which progress and runtime
	 *   will be written to the database.
	 * \param threadProgress The current
	 *   progress of the thread between
	 *   @c 0.f (0%) and @c 1.f (100%).
	 * \param threadRunTime The current
	 *   runtime of the thread in seconds.
	 *
	 * \throws Module::Database::Exception if
	 *   the prepared SQL statement for
	 *   setting the progress is missing.
	 * \throws Main::Database::Exception if a
	 *   MySQL error occured while saving
	 *   the progress of the thread to the
	 *   database.
	 */
	void Database::setThreadProgress(
			std::uint64_t threadId,
			float threadProgress,
			std::uint64_t threadRunTime
	) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.setThreadProgress == 0) {
			throw Exception(
					"Module::Database::setThreadProgress():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{
			this->getPreparedStatement(this->ps.setThreadProgress)
		};

		try {
			// execute SQL statement
			sqlStatement.setDouble(sqlArg1, threadProgress);
			sqlStatement.setUInt64(sqlArg2, threadRunTime);
			sqlStatement.setUInt64(sqlArg3, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Module::Database::setThreadProgress", e);
		}
	}

	//! Saves the last ID processed by the thread to the database.
	/*!
	 * \param threadId The ID of the thread
	 *   for which the last processed ID
	 *   will be written to the database.
	 * \param threadLast The last ID
	 *   processed by the thread.
	 * \param threadProcessed The number of
	 *   IDs that have been processed.
	 *
	 * \throws Module::Database::Exception if
	 *   the prepared SQL statement for
	 *   setting the last ID is missing.
	 * \throws Main::Database::Exception if a
	 *   MySQL error occured while saving
	 *   the last ID processed by the thread
	 *   to the database.
	 */
	void Database::setThreadLast(
			std::uint64_t threadId,
			std::uint64_t threadLast,
			std::uint64_t threadProcessed
	) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.setThreadLast == 0) {
			throw Exception(
					"Module::Database::setThreadLast():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{
			this->getPreparedStatement(this->ps.setThreadLast)
		};

		try {
			// execute SQL statement
			sqlStatement.setUInt64(sqlArg1, threadLast);
			sqlStatement.setUInt64(sqlArg2, threadProcessed);
			sqlStatement.setUInt64(sqlArg3, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Module::Database::setThreadLast", e);
		}
	}

} /* namespace crawlservpp::Module */
