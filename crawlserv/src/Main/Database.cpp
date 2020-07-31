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
 * A class to handle database access for crawlserv and its threads.
 * Thread-specific functionality is not implemented in this (parent) class.
 *
 * NOT THREAD-SAFE!
 * Use only one instance per thread.
 * Use instances of the child class Module::Database for module-specific functionality functionality instead.
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Main {

	// initialize static variables
	sql::Driver * Database::driver{nullptr};
	std::mutex Database::lockAccess;
	std::vector<std::string> Database::locks; //NOLINT(fuchsia-statically-constructed-objects)

	#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		std::atomic<std::uint64_t> Database::requestCounter(0);
	#endif

	// constructor: save settings and set default values, throws Database::Exception
	//! Constructor saving settings and setting default values.
	/*!
	 * \param dbSettings Constant reference to
	 *   the database settings.
	 * \param dbModule Constant reference to a
	 *   string containing the name of the module
	 *   for logging purposes.
	 *
	 * \throws Main::Database::Exception if the database
	 *   instance could not be retrieved.
	 */
	Database::Database(const DatabaseSettings& dbSettings, const std::string& dbModule)
			: settings(dbSettings),
			  connectionId(0),
			  maxAllowedPacketSize(0),
			  sleepOnError(0),
			  module(dbModule),
			  ps(_ps()) {
		// get driver instance if necessary
		if(Database::driver == nullptr) {
			Database::driver = get_driver_instance();

			if(Database::driver == nullptr) {
				throw Database::Exception("Could not get database instance");
			}

			// check MySQL version
			if(Database::driver->getMajorVersion() < recommendedMySqlMajorVer) {
				std::cout	<< "\nWARNING: Using MySQL v"
							<< Database::driver->getMajorVersion()
							<< "."
							<< Database::driver->getMinorVersion()
							<< "."
							<< Database::driver->getPatchVersion()
							<< ", version "
							<< recommendedMySqlMajorVer
							<< " or higher is strongly recommended."
							<< std::endl;
			}
		}

		// get MySQL version
		this->mysqlVersion =
							std::to_string(Database::driver->getMajorVersion())
							+ '.'
							+ std::to_string(Database::driver->getMinorVersion())
							+ '.'
							+ std::to_string(Database::driver->getPatchVersion());
	}

	//! Destructor clearing prepared SQL statements and the connection.
	Database::~Database() {
		if(this->module == "server") {
			// log SQL request counter (if available)
			const auto requests{Database::getRequestCounter()};

			if(requests > 0) {
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr << "performed " << requests << " SQL requests.";

				try {
					this->log(logStrStr.str());
				}
				catch(...) { // could not log -> write to stdout
					std::cout.imbue(std::locale(""));

					std::cout	<< '\n'
								<< requests
								<< " SQL requests performed."
								<< std::flush;
				}
			}
		}

		// clear prepared SQL statements
		this->preparedStatements.clear();

		// clear connection
		if(this->connection != nullptr && this->connection->isValid()) {
			this->connection->close();
		}
	}

	/*
	 * SETTERS
	 */

	//! Sets the number of seconds to sleep before trying to reconnect after connection loss.
	/*!
	 * \param seconds The number of seconds to wait
	 *   before trying to reconnect to the MySQL
	 *   server after the connection got lost.
	 */
	void Database::setSleepOnError(std::uint64_t seconds) {
		this->sleepOnError = seconds;
	}

	//! Sets the maximum execution time for MySQL queries, in milliseconds.
	/*!
	 * \note The database connection needs to be
	 *   estanblished before setting the time out.
	 *
	 * \param milliseconds The number of milliseconds
	 *   for a MySQL query to run before it gets
	 *   cancelled, or zero to disable the time-out
	 *   for MySQL queries.
	 *
	 * \throws Main::Database::Exception if a MySQL error
	 *   occurs while setting the execution time.
	 */
	void Database::setTimeOut(std::uint64_t milliseconds) {
		this->checkConnection();

		try {
			// create MySQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// set execution timeout if necessary
			Database::sqlExecute(
					sqlStatement,
					"SET @@max_execution_time = "
					+ std::to_string(milliseconds)
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::setTimeOut", e);
		}
	}

	/*
	 * GETTERS
	 */

	//! Gets the settings of the database.
	/*!
	 * \returns A constant reference to
	 *   the settings of the database.
	 */
	const Database::DatabaseSettings& Database::getSettings() const {
		return this->settings;
	}

	//! Gets the MySQL version string.
	/*!
	 * \returns A constant reference to
	 *   the string containing the version
	 *   of the MySQL driver.
	 */
	const std::string& Database::getMysqlVersion() const {
		return this->mysqlVersion;
	}

	//! Gets the default data directory.
	/*!
	 * \returns A constant reference to
	 *   the string containing the default
	 *   data directory for the MySQL server
	 *   or an empty string if not connected
	 *   to the database.
	 */
	const std::string& Database::getDataDir() const {
		return this->dataDir;
	}

	//! Gets the maximum allowed packet size for communicating with the MySQL server.
	/*!
	 * \returns The maximum allowed packet size
	 *   for communicating with the MySQL server,
	 *   in bytes, or zero if not connected to
	 *   the database.
	 */
	std::uint64_t Database::getMaxAllowedPacketSize() const {
		return this->maxAllowedPacketSize;
	}

	//! Gets the connection ID.
	/*!
	 * \returns The ID of the current connection
	 *   to the MySQL server or zero if not
	 *   connected to the database.
	 */
	std::uint64_t Database::getConnectionId() const {
		return this->connectionId;
	}

	/*
	 * INITIALIZING AND UPDATE FUNCTIONS
	 */

	//! Establishes a connection to the database and retrieves information about the server and the connection.
	/*!
	 * \throws Main::Database::Exception if the MySQL
	 *   driver could not be loaded, a connection
	 *   to the MySQL database could not be
	 *   established, the connection to the
	 *   database is invalid, a SQL statement
	 *   could not be created, a server variable
	 *   could not be retrieved or has an invalid
	 *   value, or the connection ID could not
	 *   be retrieved.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while connecting to the
	 *   MySQL server, or while retrieving information
	 *   about the server and the connection.
	 */
	void Database::connect() {
		// check driver
		if(Database::driver == nullptr) {
			throw Database::Exception(
					"Main::Database::connect():"
					" MySQL driver not loaded"
			);
		}

		try {
			// set options for connecting
			sql::ConnectOptionsMap connectOptions;

			connectOptions["hostName"] = this->settings.host; // set host
			connectOptions["userName"] = this->settings.user; // set username
			connectOptions["password"] = this->settings.password; // set password
			connectOptions["schema"] = this->settings.name; // set schema
			connectOptions["port"] = this->settings.port; // set port
			connectOptions["OPT_RECONNECT"] = false; // don't automatically re-connect
			connectOptions["OPT_CHARSET_NAME"] = "utf8mb4"; // set charset
			connectOptions["characterSetResults"] = "utf8mb4"; // set charset for results
			connectOptions["preInit"] = "SET NAMES utf8mb4"; // set charset for names

			if(this->settings.compression) {
				connectOptions["CLIENT_COMPRESS"] = true; // enable server-client compression
			}

			// connect
			this->connection.reset(Database::driver->connect(connectOptions));

			if(this->connection == nullptr) {
				throw Database::Exception(
						"Main::Database::connect():"
						" Could not connect to database"
				);
			}

			if(!(this->connection->isValid())) {
				throw Database::Exception(
						"Main::Database::connect():"
						" Connection to database is invalid"
				);
			}

			// set max_allowed_packet size to maximum of 1 GiB
			//  NOTE: needs to be set independently, setting it in the connection options leads to "invalid read of size 8"
			this->connection->setClientOption(
					"OPT_MAX_ALLOWED_PACKET",
					static_cast<const void *>(&maxContentSize)
			);

			// run initializing session commands
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			if(sqlStatement == nullptr) {
				throw Database::Exception(
						"Main::Database::connect():"
						" Could not create SQL statement"
				);
			}

			// set lock timeout
			Database::sqlExecute(
					sqlStatement,
					"SET @@innodb_lock_wait_timeout = "
					+ std::to_string(lockTimeOutSec)
			);

			// get and save maximum allowed package size
			SqlResultSetPtr sqlMaxAllowedPacketResult{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT @@max_allowed_packet AS value"
				)
			};

			if(sqlMaxAllowedPacketResult && sqlMaxAllowedPacketResult->next()) {
				if(sqlMaxAllowedPacketResult->isNull("value")) {
					throw Database::Exception(
							"Main::Database::connect():"
							" database variable 'max_allowed_packet'"
							" is NULL"
					);
				}

				this->maxAllowedPacketSize = sqlMaxAllowedPacketResult->getUInt64("value");

				if(this->maxAllowedPacketSize == 0) {
					throw Database::Exception(
							"Main::Database::connect():"
							" database variable 'max_allowed_packet'"
							" is zero"
					);
				}
			}
			else {
				throw Database::Exception(
						"Main::Database::connect():"
						" Could not get 'max_allowed_packet'"
						" from database"
				);
			}

			// get and save connection ID
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT CONNECTION_ID() AS id"
				)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				this->connectionId = sqlResultSet->getUInt64("id");
			}
			else {
				throw Database::Exception(
						"Main::Database::connect():"
						" Could not get MySQL connection ID"
				);
			}

			// get and save main data directory
			SqlResultSetPtr sqlDataDirResult{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT @@datadir AS value"
				)
			};

			if(sqlDataDirResult && sqlDataDirResult->next()) {
				if(sqlDataDirResult->isNull("value")) {
					throw Database::Exception(
							"Main::Database::connect():"
							" database variable 'datadir'"
							" is NULL"
					);
				}

				this->dataDir = sqlDataDirResult->getString("value");

				// trim path and remove last separator if necessary
				Helper::Strings::trim(this->dataDir);

				if(
						this->dataDir.size() > 1
						&& this->dataDir.back() == Helper::FileSystem::getPathSeparator()
				) {
					this->dataDir.pop_back();
				}

				if(this->dataDir.empty()) {
					throw Database::Exception(
							"Main::Database::connect():"
							" database variable 'datadir'"
							" is empty"
					);
				}

				// add main data directory to all data directories
				this->dirs.emplace_back(this->dataDir);
			}
			else {
				throw Database::Exception(
						"Main::Database::connect():"
						" Could not get variable 'datadir'"
						" from database"
				);
			}

			// get and save InnoDB directories
			SqlResultSetPtr sqlInnoDirsResult{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT @@innodb_directories AS value"
				)
			};

			if(sqlInnoDirsResult && sqlInnoDirsResult->next()) {
				if(!(sqlInnoDirsResult->isNull("value"))) {
					const auto innoDirs{
							Helper::Strings::split(
									sqlInnoDirsResult->getString("value"),
									';'
							)
					};

					// add InnoDB directories to all data directories
					this->dirs.insert(
							this->dirs.end(),
							innoDirs.begin(),
							innoDirs.end()
					);
				}
			}
			else {
				throw Database::Exception(
						"Main::Database::connect():"
						" Could not get variable 'innodb_directories'"
						" from database"
				);
			}

			// get additional directories
			SqlResultSetPtr sqlInnoHomeDirResult{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT @@innodb_data_home_dir AS value"
				)
			};

			if(
					sqlInnoHomeDirResult
					&& sqlInnoHomeDirResult->next()
					&& !(sqlInnoHomeDirResult->isNull("value"))
			) {
				const std::string innoHomeDir{
					sqlInnoHomeDirResult->getString("value")
				};

				if(!innoHomeDir.empty()) {
					this->dirs.emplace_back(innoHomeDir);
				}
			}

			SqlResultSetPtr sqlInnoUndoDirResult{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT @@innodb_undo_directory AS value"
				)
			};

			if(
					sqlInnoUndoDirResult
					&& sqlInnoUndoDirResult->next()
					&& !(sqlInnoUndoDirResult->isNull("value"))
			) {
				const std::string innoUndoDir{
					sqlInnoUndoDirResult->getString("value")
				};

				if(!innoUndoDir.empty()) {
					this->dirs.emplace_back(innoUndoDir);
				}
			}

			// sort directories and remvove duplicates
			std::sort(this->dirs.begin(), this->dirs.end());

			this->dirs.erase(
					std::unique(
							this->dirs.begin(),
							this->dirs.end()
					),
					this->dirs.end()
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::connect", e);
		}
	}

	//! Runs initializing SQL commands by processing all @c .sql files in the SQL (sub-)folder.
	/*!
	 * \throws Main::Database::Exception if no SQL
	 *   statement could be created or a @c
	 *   .sql file could not be opened for
	 *   reading.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while executing the
	 *   commands inside the @c .sql files.
	 */
	void Database::initializeSql() {
		// execute all SQL files in SQL directory
		for(const auto& sqlFile : Helper::FileSystem::listFilesInPath(sqlDir, sqlExtension)) {
			this->run(sqlFile);
		}
	}

	//! Prepares SQL statements for getting the last inserted ID, logging and setting the status of a thread.
	/*!
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while preparing those SQL
	 *   statements.
	 */
	void Database::prepare() {
		// reserve memory for SQL statements
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(std::uint16_t));

		try {
			// prepare basic SQL statements
			if(this->ps.lastId == 0) {
				this->ps.lastId = this->addPreparedStatement(
						"SELECT LAST_INSERT_ID() AS id"
				);
			}

			if(this->ps.log == 0) {
				this->ps.log = this->addPreparedStatement(
						"INSERT INTO crawlserv_log(module, entry)"
						" VALUES (?, ?)"
				);
			}

			// prepare thread statements
			if(this->ps.setThreadStatus == 0) {
				this->ps.setThreadStatus = this->addPreparedStatement(
						"UPDATE crawlserv_threads"
						" SET status = ?, paused = ?"
						" WHERE id = ?"
						" LIMIT 1"
				);
			}

			if(this->ps.setThreadStatusMessage == 0) {
				this->ps.setThreadStatusMessage = this->addPreparedStatement(
						"UPDATE crawlserv_threads"
						" SET status = ?"
						" WHERE id = ?"
						" LIMIT 1"
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::prepare", e);
		}
	}

	//! Updates the tables with language and version information in the database.
	/*!
	 * Creates the tables if they do not exist
	 *  yet.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while creating or updating
	 *   those tables.
	 */
	void Database::update() {
		// drop locale table
		this->dropTable("crawlserv_locales");

		// create locale table
		std::vector<TableColumn> columns;

		columns.emplace_back("name", "TEXT NOT NULL");

		this->createTable(
				TableProperties(
						"crawlserv_locales",
						columns,
						"",
						false
				)
		);

		columns.clear();

		// get installed locales
		const auto locales{Helper::Portability::enumLocales()};

		if(!locales.empty()) {
			std::string sqlQuery(
					"INSERT INTO `crawlserv_locales`(name)"
					" VALUES"
			);

			for(std::size_t n{0}; n < locales.size(); ++n) {
				sqlQuery += " (?),";
			}

			sqlQuery.pop_back();

			// check database connection
			this->checkConnection();

			// write installed locales to database
			try {
				// prepare SQL statement
				SqlPreparedStatementPtr sqlStatement{
					this->connection->prepareStatement(
							sqlQuery
					)
				};

				// execute SQL statement
				std::size_t counter{sqlArg1};

				for(const auto& locale : locales) {
					sqlStatement->setString(counter, locale);

					++counter;
				}

				Database::sqlExecute(sqlStatement);
			}
			catch(const sql::SQLException &e) {
				Database::sqlException("Main::Database::update", e);
			}
		}

		// drop versions table
		this->dropTable("crawlserv_versions");

		// create versions table
		columns.emplace_back("name", "TEXT NOT NULL");
		columns.emplace_back("version", "TEXT NOT NULL");

		this->createTable(
				TableProperties(
						"crawlserv_versions",
						columns,
						"",
						false
				)
		);

		columns.clear();

		// get library versions
		auto versions{Helper::Versions::getLibraryVersions()};

		// add crawlserv++ version
		versions.emplace_back("crawlserv++", Main::Version::getString());

		if(!versions.empty()) {
			std::string sqlQuery(
					"INSERT INTO `crawlserv_versions`(name, version)"
					" VALUES"
			);

			for(std::size_t n{0}; n < versions.size(); ++n) {
				sqlQuery += " (?, ?),";
			}

			sqlQuery.pop_back();

			// check database connection
			this->checkConnection();

			// write library versions to database
			try {
				// prepare SQL statement
				SqlPreparedStatementPtr sqlStatement{
					this->connection->prepareStatement(
							sqlQuery
					)
				};

				// execute SQL statement
				std::size_t counter{sqlArg1};

				for(const auto& version : versions) {
					sqlStatement->setString(counter, version.first);
					sqlStatement->setString(counter + 1, version.second);

					counter += 2;
				}

				// execute SQL statement
				Database::sqlExecute(sqlStatement);
			}
			catch(const sql::SQLException &e) {
				Database::sqlException("Main::Database::update", e);
			}
		}
	}

	/*
	 * LOGGING FUNCTIONS
	 */

	//! Adds a log entry to the database for any module.
	/*!
	 * Removes invalid UTF-8 characters if
	 *  necessary. If characters needed to be
	 *  removed, a note will be included in the
	 *  log entry that will be added to the
	 *  database.
	 *
	 * \note String views cannot be used,
	 *  because they are not supported by the
	 *  API for the MySQL database.
	 *
	 * \param logModule Constant reference to
	 *   a string containing the name of the
	 *   module from which the log entry is
	 *   added to the database.
	 * \param logEntry Constant reference
	 *   to a string containing the log entry
	 *   to be added to the database.
	 *
	 * \throws Main::Database::Exception if the
	 *   prepared SQL statement for adding a log
	 *   entry is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the log entry
	 *   to the database. If such an exception
	 *   is thrown, the log entry will be
	 *   written to @c stdout instead.
	 */
	void Database::log(const std::string& logModule, const std::string& logEntry) {
		std::string repairedEntry;

		// repair invalid UTF-8 in argument
		const auto repaired{
			Helper::Utf8::repairUtf8(logEntry, repairedEntry)
		};

		if(repaired) {
			repairedEntry += " [invalid UTF-8 character(s) removed from log]";
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.log == 0) {
			throw Database::Exception(
					"Main::Database::log():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.log)};

		// add entry to database
		try {
			// execute SQL query
			if(logModule.empty()) {
				sqlStatement.setString(sqlArg1, "[unknown]");
			}
			else {
				sqlStatement.setString(sqlArg1, logModule);
			}

			if(logEntry.empty()) {
				sqlStatement.setString(sqlArg2, "[empty]");
			}
			else {
				if(repaired) {
					sqlStatement.setString(sqlArg2, repairedEntry);
				}
				else {
					sqlStatement.setString(sqlArg2, logEntry);
				}
			}

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			// write log entry to console instead
			std::cout << '\n' << logModule << ": " << logEntry << std::flush;

			Database::sqlException("Main::Database::log", e);
		}
	}

	//! Adds a log entry to the database for the current module.
	/*!
	 * Removes invalid UTF-8 characters if
	 *  necessary.
	 *
	 * \note String views cannot be used,
	 *  because they are not supported by
	 *  the API for the MySQL database.
	 *
	 * \param logEntry Constant reference
	 *   to a string containing the log
	 *   entry to be added to the database.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the log
	 *   entry to the database. If such an
	 *   exception is thrown, the log entry
	 *   will be written to @c stdout instead.
	 */
	void Database::log(const std::string& logEntry) {
		this->log(this->module, logEntry);
	}

	//! Gets the number of log entries from the database.
	/*!
	 * The number can be retrieved for a
	 *  specific module or for all modules at
	 *  once.
	 *
	 * \note String views cannot be used,
	 *   because they are not supported by the
	 *   API for the MySQL database.
	 *
	 * \param logModule Constant reference to a
	 *   string containing the name of the
	 *   module, for which the number of log
	 *   entries will be retrieved, or to an
	 *   empty string for all log entries to
	 *   be retrieved.
	 *
	 * \returns The number of log entries for
	 *   the specified module in the database,
	 *   or the total number of log entries, if
	 *   logModule references an empty string.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while getting the number
	 *   of log entries from the database.
	 */
	std::uint64_t Database::getNumberOfLogEntries(const std::string& logModule) {
		std::uint64_t result{0};

		// check connection
		this->checkConnection();

		// create SQL query string
		std::string sqlQuery{"SELECT COUNT(*) FROM `crawlserv_log`"};

		if(!logModule.empty()) {
			sqlQuery += " WHERE module = ?";
		}

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(sqlQuery)
			};

			// execute SQL statement
			if(!logModule.empty()) {
				sqlStatement->setString(sqlArg1, logModule);
			}

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64(sqlArg1);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getNumberOfLogEntries", e);
		}

		return result;
	}

	//! Removes log entries from the database.
	/*!
	 * \note String views cannot be used,
	 *  because they are not supported by
	 *  the API for the MySQL database.
	 *
	 * \param logModule Constant reference to a
	 *   string containing the name of the
	 *   module, whose log entries will be
	 *   removed, or to an empty string for all
	 *   log entries to be removed.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while removing log entries
	 *   from the database.
	 */
	void Database::clearLogs(const std::string& logModule) {
		if(logModule.empty()) {
			// execute SQL query
			this->execute("TRUNCATE TABLE `crawlserv_log`");
		}
		else {
			// check connection
			this->checkConnection();

			try {
				// prepare SQL statement
				SqlPreparedStatementPtr sqlStatement{
					this->connection->prepareStatement(
							"DELETE FROM `crawlserv_log` WHERE module = ?"
					)
				};

				// execute SQL statement
				sqlStatement->setString(sqlArg1, logModule);

				Database::sqlExecute(sqlStatement);

				// reset auto-increment if table is empty
				if(this->isTableEmpty("crawlserv_log")) {
					this->resetAutoIncrement("crawlserv_log");
				}
			}
			catch(const sql::SQLException &e) {
				Database::sqlException("Main::Database::clearLogs", e);
			}
		}
	}

	/*
	 * THREAD FUNCTIONS
	 */

	//! Gets information about all threads from the database.
	/*!
	 * \returns A vector containing information
	 *   about all threads that have been saved
	 *   in the database.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while getting information
	 *   about the threads from the database.
	 */
	std::vector<Database::ThreadDatabaseEntry> Database::getThreads() {
		std::vector<ThreadDatabaseEntry> result;

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL query
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT id, module, status, paused, website, urllist, config, last"
						" FROM `crawlserv_threads`"
				)
			};

			// get results
			if(sqlResultSet) {
				result.reserve(sqlResultSet->rowsCount());

				while(sqlResultSet->next()) {
					result.emplace_back(
							ThreadOptions(
									sqlResultSet->getString("module"),
									sqlResultSet->getUInt64("website"),
									sqlResultSet->getUInt64("urllist"),
									sqlResultSet->getUInt64("config")
							),
							ThreadStatus(
									sqlResultSet->getUInt64("id"),
									sqlResultSet->getString("status"),
									sqlResultSet->getBoolean("paused"),
									sqlResultSet->getUInt64("last")
							)
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getThreads", e);
		}

		return result;
	}

	//! Adds information about a new thread to the database and returns its ID.
	/*!
	 * \param threadOptions Constant reference to
	 *   the options for the new thread.
	 *
	 * \returns A unique ID identifying the new
	 *   thread in the database.
	 *
	 * \throws Main::Database::Exception if the module
	 *   of the thread is empty, or if no module,
	 *   no website, no URL list, or no
	 *   configuration has been specified in the
	 *   given options for the thread.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding information
	 *   about the new thread to the database.
	 */
	std::uint64_t Database::addThread(const ThreadOptions& threadOptions) {
		std::uint64_t result{0};

		// check arguments
		if(threadOptions.module.empty()) {
			throw Database::Exception(
					"Main::Database::addThread():"
					" No thread module specified"
			);
		}

		if(threadOptions.website == 0) {
			throw Database::Exception(
					"Main::Database::addThread():"
					" No website specified"
			);
		}

		if(threadOptions.urlList == 0) {
			throw Database::Exception(
					"Main::Database::addThread():"
					" No URL list specified"
			);
		}

		if(threadOptions.config == 0) {
			throw Database::Exception(
					"Main::Database::addThread():"
					" No configuration specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"INSERT INTO crawlserv_threads(module, website, urllist, config)"
						" VALUES (?, ?, ?, ?)"
				)
			};

			// execute SQL statement
			sqlStatement->setString(sqlArg1, threadOptions.module);
			sqlStatement->setUInt64(sqlArg2, threadOptions.website);
			sqlStatement->setUInt64(sqlArg3, threadOptions.urlList);
			sqlStatement->setUInt64(sqlArg4, threadOptions.config);

			Database::sqlExecute(sqlStatement);

			// get ID
			result = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::addThread", e);
		}

		return result;
	}

	//! Gets the run time of a thread from the database.
	/*!
	 * \param threadId The ID of the thread for
	 *   which the run time will be retrieved
	 *   from the database.
	 *
	 * \returns The total number of seconds
	 *   the given thread has been running,
	 *   and not been paused.
	 *
	 * \throws Main::Database::Exception if no thread
	 *   has been specified, i.e. the thread ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the run
	 *   time of the thread from the database.
	 */
	std::uint64_t Database::getThreadRunTime(std::uint64_t threadId) {
		std::uint64_t result{0};

		// check argument
		if(threadId == 0) {
			throw Database::Exception(
					"Main::Database::getThreadRunTime():"
					" No thread ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT runtime"
						" FROM `crawlserv_threads`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, threadId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("runtime");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getThreadRunTime", e);
		}

		return result;
	}

	//! Gets the pause time of a thread from the database.
	/*!
	 * \param threadId The ID of the thread for
	 *   which the pause time will be retrieved
	 *   from the database.
	 *
	 * \returns The total number of seconds
	 *   the given thread has been paused.
	 *
	 * \throws Main::Database::Exception if no thread
	 *   has been specified, i.e. the thread ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the pause
	 *   time of the thread from the database.
	 */
	std::uint64_t Database::getThreadPauseTime(std::uint64_t threadId) {
		std::uint64_t result{0};

		// check argument
		if(threadId == 0) {
			throw Database::Exception(
					"Main::Database::getThreadPauseTime():"
					" No thread ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT pausetime"
						" FROM `crawlserv_threads`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, threadId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("pausetime");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getThreadPauseTime", e);
		}

		return result;
	}

	//! Updates the status of a thread in the database.
	/*!
	 * Adds the pause state to the status
	 *  message if necessary.
	 *
	 * \note String views cannot be used,
	 *  because they are not supported by the
	 *  API for the MySQL database.
	 *
	 * \param threadId The ID of the thread for
	 *   which the status will be updated in the
	 *   database.
	 * \param threadPaused Specifies whether the
	 *   thread is currently paused.
	 * \param threadStatusMessage Constant
	 *   reference to a string containing the
	 *   current status of the thread.
	 *
	 * \throws Main::Database::Exception if no thread
	 *   has been specified, i.e. the thread ID
	 *   is zero, or the prepared SQL statement
	 *   for setting the thread status is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the status
	 *   of the thread in the database.
	 */
	void Database::setThreadStatus(
			std::uint64_t threadId,
			bool threadPaused,
			const std::string& threadStatusMessage
	) {
		// check argument
		if(threadId == 0) {
			throw Database::Exception(
					"Main::Database::setThreadStatus():"
					" No thread ID specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.setThreadStatus == 0) {
			throw Database::Exception(
					"Main::Database::setThreadStatus():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.setThreadStatus)};

		// create status message
		std::string statusMessage;

		if(threadPaused) {
			if(threadStatusMessage.empty()) {
				statusMessage = "PAUSED";
			}
			else {
				statusMessage = "PAUSED " + threadStatusMessage;
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
			Database::sqlException("Main::Database::setThreadStatus", e);
		}
	}

	//! Updates the status of a thread in the database without changing the pause state of the thread.
	/*!
	 * \note String views cannot be used,
	 *  because they are not supported by the
	 *  API for the MySQL database.
	 *
	 * \param threadId The ID of the thread for
	 *   which the status will be updated in the
	 *   database.
	 * \param threadStatusMessage Constant
	 *   reference to a string containing the
	 *   current status of the thread.
	 *
	 * \throws Main::Database::Exception if no thread
	 *   has been specified, i.e. the thread ID
	 *   is zero, or the prepared SQL statement
	 *   for setting the thread status is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the status
	 *   of the thread in the database.
	 */
	void Database::setThreadStatus(std::uint64_t threadId, const std::string& threadStatusMessage) {
		// check argument
		if(threadId == 0) {
			throw Database::Exception(
					"Main::Database::setThreadStatus():"
					" No thread ID specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.setThreadStatusMessage == 0) {
			throw Database::Exception(
					"Main::Database::setThreadStatus():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.setThreadStatusMessage)};

		try {
			// execute SQL statement
			sqlStatement.setString(sqlArg1, threadStatusMessage);
			sqlStatement.setUInt64(sqlArg2, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::setThreadStatus", e);
		}
	}

	//! Updates the run time of a thread in the database.
	/*!
	 * \param threadId The ID of the thread for
	 *   which the run time will be updated in
	 *   the database.
	 * \param threadRunTime The total number of
	 *   seconds the given thread has been
	 *   running, and not been paused.
	 *
	 * \throws Main::Database::Exception if no thread
	 *   has been specified, i.e. the thread ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the run
	 *   time of the thread in the database.
	 */
	void Database::setThreadRunTime(std::uint64_t threadId, std::uint64_t threadRunTime) {
		// check argument
		if(threadId == 0) {
			throw Database::Exception(
					"Main::Database::setThreadRunTime():"
					" No thread ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"UPDATE crawlserv_threads"
						" SET runtime = ?"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, threadRunTime);
			sqlStatement->setUInt64(sqlArg2, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::setThreadRunTime", e);
		}
	}

	//! Updates the pause time of a thread in the database.
	/*!
	 * \param threadId The ID of the thread for
	 *   which the pause time will be updated
	 *   in the database.
	 * \param threadPauseTime The total number
	 *   of seconds the given thread has been
	 *   paused.
	 *
	 * \throws Main::Database::Exception if no thread
	 *   has been specified, i.e. the thread ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the pause
	 *   time of the thread in the database.
	 */
	void Database::setThreadPauseTime(std::uint64_t threadId, std::uint64_t threadPauseTime) {
		// check argument
		if(threadId == 0) {
			throw Database::Exception(
					"Main::Database::setThreadPauseTime():"
					" No thread ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"UPDATE crawlserv_threads"
						" SET pausetime = ?"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, threadPauseTime);
			sqlStatement->setUInt64(sqlArg2, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::setThreadPauseTime", e);
		}
	}

	//! Removes a thread from the database.
	/*!
	 * \param threadId The ID of the thread to
	 *   be removed from the database.
	 *
	 * \throws Main::Database::Exception if no thread
	 *   has been specified, i.e. the thread ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while deleting the thread
	 *   from the database.
	 */
	void Database::deleteThread(std::uint64_t threadId) {
		// check argument
		if(threadId == 0) {
			throw Database::Exception(
					"Main::Database::deleteThread():"
					" No thread ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"DELETE FROM `crawlserv_threads`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, threadId);

			Database::sqlExecute(sqlStatement);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_threads")) {
				this->resetAutoIncrement("crawlserv_threads");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::deleteThread", e);
		}
	}

	/*
	 * WEBSITE FUNCTIONS
	 */

	//! Adds a new website to the database and returns its ID.
	/*!
	 * \param websiteProperties Constant
	 *   reference to the properties of the new
	 *   website.
	 *
	 * \returns A unique ID identifying the new
	 *   website in the database.
	 *
	 * \throws Main::Database::Exception if the
	 *   namespace or the name of the new
	 *   website is empty, its namespace
	 *   already exists, or the specified data
	 *   directory does not exist.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the new
	 *   website to the database.
	 */
	std::uint64_t Database::addWebsite(const WebsiteProperties& websiteProperties) {
		std::uint64_t result{0};
		std::string timeStamp;

		// check arguments
		if(websiteProperties.nameSpace.empty()) {
			throw Database::Exception(
					"Main::Database::addWebsite():"
					" No website namespace specified"
			);
		}

		if(websiteProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::addWebsite():"
					" No website name specified"
			);
		}

		// check website namespace
		if(this->isWebsiteNamespace(websiteProperties.nameSpace)) {
			throw Database::Exception(
					"Main::Database::addWebsite():"
					" Website namespace already exists"
			);
		}

		// check directory
		if(
				!websiteProperties.dir.empty()
				&& !Helper::FileSystem::isValidDirectory(websiteProperties.dir)
		) {
			throw Database::Exception(
					"Main::Database::addWebsite():"
					" Data directory does not exist"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for adding website
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"INSERT INTO crawlserv_websites(domain, namespace, name, dir)"
						" VALUES (?, ?, ?, ?)"
				)
			};

			// execute SQL statement for adding website
			if(websiteProperties.domain.empty()) {
				sqlStatement->setNull(sqlArg1, 0);
			}
			else {
				sqlStatement->setString(sqlArg1, websiteProperties.domain);
			}

			sqlStatement->setString(sqlArg2, websiteProperties.nameSpace);
			sqlStatement->setString(sqlArg3, websiteProperties.name);

			if(websiteProperties.dir.empty()) {
				sqlStatement->setNull(sqlArg4, 0);
			}
			else {
				sqlStatement->setString(sqlArg4, websiteProperties.dir);
			}

			Database::sqlExecute(sqlStatement);

			// get ID
			result = this->getLastInsertedId();

			// add default URL list
			try {
				this->addUrlList(result, UrlListProperties("default", "Default URL list"));
			}
			catch(const Exception& e) {
				this->deleteWebsite(result);

				throw;
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::addWebsite", e);
		}

		return result;
	}

	//! Gets the domain of a website from the database.
	/*!
	 * \param websiteId The ID of the website
	 *   for which the domain will be retrieved
	 *   from the database.
	 *
	 * \returns A copy of the domain name of
	 *   the given website.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   domain name of the given website from
	 *   the database.
	 */
	std::string Database::getWebsiteDomain(std::uint64_t websiteId) {
		std::string result;

		// check argument
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::getWebsiteDomain():"
					" No website ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT domain"
						" FROM `crawlserv_websites`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(
					sqlResultSet
					&& sqlResultSet->next()
					&& !(sqlResultSet->isNull("domain"))
			) {
				result = sqlResultSet->getString("domain");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getWebsiteDomain", e);
		}

		return result;
	}

	//! Gets the namespace of a website from the database.
	/*!
	 * \param websiteId The ID of the website
	 *   for which the namespace will be
	 *   retrieved from the database.
	 *
	 * \returns A copy of the namespace of the
	 *   given website.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   namespace of the given website from the
	 *   database.
	 */
	std::string Database::getWebsiteNamespace(std::uint64_t websiteId) {
		std::string result;

		// check argument
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::getWebsiteNamespace():"
					" No website ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT namespace"
						" FROM `crawlserv_websites`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getString("namespace");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getWebsiteNamespace", e);
		}

		return result;
	}

	//! Gets the ID and the namespace of the website associated with a URL list from the database.
	/*!
	 * \param listId The ID of the URL list
	 *   for which the ID and namespace of
	 *   the associated website will be
	 *   retrieved from the database.
	 *
	 * \returns A pair containing the ID and
	 *   the namespace of the website associated
	 *   with the given URL list.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   ID and namespace of the website
	 *   associated with the given URL list from
	 *   the database.
	 */
	Database::IdString Database::getWebsiteNamespaceFromUrlList(std::uint64_t listId) {
		std::uint64_t websiteId{0};

		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::getWebsiteNamespaceFromUrlList():"
					" No URL list ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT website"
						" FROM `crawlserv_urllists`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, listId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				websiteId = sqlResultSet->getUInt64("website");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getWebsiteNamespaceFromUrlList", e);
		}

		return IdString(websiteId, this->getWebsiteNamespace(websiteId));
	}

	//! Gets the ID and the namespace of the website associated with a configuration from the database.
	/*!
	 * \param configId The ID of the
	 *   configuration for which the ID and
	 *   namespace of the associated website
	 *   will be retrieved from the database.
	 *
	 * \returns A pair containing the ID and
	 *   the namespace of the website associated
	 *   with the given configuration.
	 *
	 * \throws Main::Database::Exception if no
	 *   configuration has been specified, i.e.
	 *   the configuration ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   ID and namespace of the website
	 *   associated with the given configuration
	 *   from the database.
	 */
	Database::IdString Database::getWebsiteNamespaceFromConfig(std::uint64_t configId) {
		std::uint64_t websiteId{0};

		// check argument
		if(configId == 0) {
			throw Database::Exception(
					"Main::Database::getWebsiteNamespaceFromConfig():"
					" No configuration ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT website"
						" FROM `crawlserv_configs`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, configId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				websiteId = sqlResultSet->getUInt64("website");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getWebsiteNamespaceFromConfig", e);
		}

		return IdString(websiteId, this->getWebsiteNamespace(websiteId));
	}

	//! Gets the ID and the namespace of the website associated with a target table from the database.
	/*!
	 * \param type Constant reference to a
	 *   string containing the type of the
	 *   target table for which the ID and
	 *   namespace of the associated website
	 *   will be retrieved from the database.
	 * \param tableId The ID of the
	 *   target table for which the ID and
	 *   namespace of the associated website
	 *   will be retrieved from the database.
	 *
	 * \returns A pair containing the ID and
	 *   the namespace of the website associated
	 *   with the given target table.
	 *
	 * \throws Main::Database::Exception if no target
	 *   table has been specified, i.e. the
	 *   string containing the type is empty
	 *   or the target table ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   ID and namespace of the website
	 *   associated with the given target table
	 *   from the database.
	 */
	Database::IdString Database::getWebsiteNamespaceFromTargetTable(const std::string& type, std::uint64_t tableId) {
		std::uint64_t websiteId{0};

		// check arguments
		if(type.empty()) {
			throw Database::Exception(
					"Main::Database::getWebsiteNamespaceFromCustomTable():"
					" No table type specified"
			);
		}

		if(tableId == 0) {
			throw Database::Exception(
					"Main::Database::getWebsiteNamespaceFromCustomTable():"
					" No table ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT website"
						" FROM `crawlserv_"
						+ type
						+ "tables`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, tableId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				websiteId = sqlResultSet->getUInt64("website");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getWebsiteNamespaceFromCustomTable", e);
		}

		return IdString(websiteId, this->getWebsiteNamespace(websiteId));
	}

	//! Checks whether a website namespace exists in the database.
	/*!
	 * \param nameSpace Constant reference to a
	 *   string containing the namespace whose
	 *   existence will be checked in the
	 *   database.
	 *
	 * \returns True, if a namespace with the
	 *   given name exists in the database.
	 *   False otherwise.
	 *
	 * \throws Main::Database::Exception if no
	 *   namespace has been specified, i.e.
	 *   the string containing the name of the
	 *   namespace is empty.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   existence of the website namespace in
	 *   the database.
	 */
	bool Database::isWebsiteNamespace(const std::string& nameSpace) {
		bool result{false};

		// check argument
		if(nameSpace.empty()) {
			throw Database::Exception(
					"Main::Database::isWebsiteNamespace():"
					" No namespace specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_websites`"
							" WHERE namespace = ?"
						" )"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setString(sqlArg1, nameSpace);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isWebsiteNamespace", e);
		}

		return result;
	}

	//! Gets a new name for a website namespace to duplicate.
	/*!
	 * Adds a number to the end of the
	 *  namespace. If the namespace already
	 *  ends on a number, this number will be
	 *  increased until the new name does not
	 *  already exist.
	 *
	 * \param websiteNamespace Const reference
	 *   to a string containing the name of the
	 *   website namespace to duplicate.
	 *
	 * \returns A copy of a string containing
	 *   the new name for the duplicated
	 *   website namespace.
	 *
	 * \throws Main::Database::Exception if no
	 *   namespace has been specified, i.e.
	 *   the string containing the name of the
	 *   namespace is empty, or if a conversion
	 *   from string to number failed.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   existence of the website namespace in
	 *   the database.
	 */
	std::string Database::duplicateWebsiteNamespace(const std::string& websiteNamespace) {
		// check argument
		if(websiteNamespace.empty()) {
			throw Database::Exception(
					"Main::Database::duplicateWebsiteNamespace():"
					" No namespace specified"
			);
		}

		std::string nameString;
		std::string numberString;

		const auto end{websiteNamespace.find_last_not_of("0123456789")};

		// separate number at the end of string from the rest of the string
		if(end == std::string::npos) {
			// string is number
			numberString = websiteNamespace;
		}
		else if(end == websiteNamespace.length() - 1) {
			// no number at the end of the string
			nameString = websiteNamespace;
		}
		else {
			// number at the end of the string
			nameString = websiteNamespace.substr(0, end + 1);
			numberString = websiteNamespace.substr(end + 1);
		}

		std::uint64_t n{1};
		std::string result;

		if(!numberString.empty()) {
			try {
				n = std::stoul(numberString, nullptr);
			}
			catch(const std::logic_error& e) {
				throw Exception(
						"Main::Database::duplicateWebsiteNamespace():"
						" Could not convert '"
						+ numberString
						+ "' to unsigned numeric value"
				);
			}
		}

		// check whether number needs to be incremented
		while(true) {
			// increment number at the end of the string
			++n;

			result = nameString + std::to_string(n);

			if(!(this->isWebsiteNamespace(result))) {
				break;
			}
		}

		return result;
	}

	//! Gets the data directory used by a website.
	/*!
	 * \param websiteId The ID of the website
	 *   for which the data directory will be
	 *   retrieved from the database.
	 *
	 * \returns A copy of the data directory of
	 *   the given website or an empty string
	 *   if the default data directory is used.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the data
	 *   directory of the website from the
	 *   database.
	 */
	std::string Database::getWebsiteDataDirectory(std::uint64_t websiteId) {
		std::string result;

		// check argument
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::getWebsiteDataDirectory():"
					" No website ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT dir"
						" FROM `crawlserv_websites`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(
					sqlResultSet
					&& sqlResultSet->next()
					&& !(sqlResultSet->isNull("dir"))
			) {
				result = sqlResultSet->getString("dir");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getWebsiteNamespace", e);
		}

		return result;
	}

	//! Gets the number of URLs that will be modified by updating a website from the database.
	/*!
	 * \note URLs will only be modified if the
	 *  type of the domain changes from using a
	 *  specific domain to cross-domain, or
	 *  vice versa, because the name of the
	 *  domain is saved as part of the URLs if
	 *  the website is cross-domain.
	 *
	 * \param websiteId The ID of the website
	 *   for which the number of URLs that
	 *   will be modified will be retrieved
	 *   from the database.
	 * \param websiteProperties A constant
	 *   reference to the new website
	 *   properties.
	 *
	 * \returns The number of URLs that will be
	 *   modified by replacing the current
	 *   properties of the website with the given
	 *   ones.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero, or if no website namespace or
	 *   name has been specified in the provided
	 *   website options.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   number of URLs that will be modified
	 *   from the database.
	 */
	std::uint64_t Database::getChangedUrlsByWebsiteUpdate(
			std::uint64_t websiteId,
			const WebsiteProperties& websiteProperties
	) {
		std::uint64_t result{0};

		// check arguments
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::getChangedUrlsByWebsiteUpdate():"
					" No website ID specified"
			);
		}

		if(websiteProperties.nameSpace.empty()) {
			throw Database::Exception(
					"Main::Database::getChangedUrlsByWebsiteUpdate():"
					" No website namespace specified"
			);
		}

		if(websiteProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::getChangedUrlsByWebsiteUpdate():"
					" No website name specified"
			);
		}

		// get old namespace and domain
		const auto oldNamespace{this->getWebsiteNamespace(websiteId)};
		const auto oldDomain{this->getWebsiteDomain(websiteId)};

		// check connection
		this->checkConnection();

		try {
			if(oldDomain.empty() != websiteProperties.domain.empty()) {
				// get URL lists
				auto urlLists{this->getUrlLists(websiteId)};

				// create SQL statement
				SqlStatementPtr sqlStatement{this->connection->createStatement()};

				if(oldDomain.empty() && !websiteProperties.domain.empty()) {
					// type changed from cross-domain to specific domain:
					// 	change URLs from absolute (URLs without protocol) to relative (sub-URLs),
					// 	discarding URLs with different domain
					while(!urlLists.empty()) {
						// count URLs of same domain
						std::string queryString{
							"SELECT COUNT(*) AS result FROM `crawlserv_"
						};

						queryString += oldNamespace;
						queryString += '_';
						queryString += urlLists.front().second;
						queryString += "` WHERE url LIKE '";
						queryString += websiteProperties.domain;
						queryString += "/%' OR url LIKE '";

						// ignore "www." prefix (will be removed if website were to be updated)
						if(
								websiteProperties.domain.size() > wwwPrefix.length()
								&& websiteProperties.domain.substr(0, wwwPrefix.length()) == wwwPrefix
						) {
							queryString += websiteProperties.domain.substr(wwwPrefix.length());
						}
						else {
							queryString += wwwPrefix;
							queryString += websiteProperties.domain;
						}

						queryString += "/%'";

						SqlResultSetPtr sqlResultSet{
							Database::sqlExecuteQuery(
									sqlStatement,
									queryString
							)
						};

						if(sqlResultSet && sqlResultSet->next()) {
							result += sqlResultSet->getUInt64("result");
						}

						urlLists.pop();
					}
				}
				else {
					// type changed from specific domain to cross-domain:
					// 	change URLs from relative (sub-URLs) to absolute (URLs without protocol)
					while(!urlLists.empty()) {
						// count URLs
						SqlResultSetPtr sqlResultSet{
							Database::sqlExecuteQuery(
									sqlStatement,
									"SELECT COUNT(*) AS result"
									" FROM `crawlserv_"
									+ oldNamespace
									+ "_"
									+ urlLists.front().second
									+ "`"
							)
						};

						if(sqlResultSet && sqlResultSet->next()) {
							result += sqlResultSet->getUInt64("result");
						}

						urlLists.pop();
					}
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getChangedUrlsByWebsiteUpdate", e);
		}

		return result;
	}

	//! Gets the number of URLs that will be lost by updating a website from the database.
	/*!
	 * \note URLs might only be lost if the type
	 *  of the domain changes from cross-domain
	 *  to using a specific domain, because in
	 *  this case, all URLs that do not contain
	 *  the newly specified domain name, will
	 *  be lost.
	 *
	 * \param websiteId The ID of the website
	 *   for which the number of URLs that will
	 *   be lost will be retrieved from the
	 *   database.
	 * \param websiteProperties A constant
	 *   reference to the new website
	 *   properties.
	 *
	 * \returns The number of URLs that will be
	 *   lost by replacing the current
	 *   properties of the website with the given
	 *   ones.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero, or if no website namespace or
	 *   name has been specified in the provided
	 *   website options.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   number of URLs that will be lost from
	 *   the database.
	 */
	std::uint64_t Database::getLostUrlsByWebsiteUpdate(
			std::uint64_t websiteId,
			const WebsiteProperties& websiteProperties
	) {
		std::uint64_t result{0};

		// check arguments
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::getLostUrlsByWebsiteUpdate():"
					" No website ID specified"
			);
		}

		if(websiteProperties.nameSpace.empty()) {
			throw Database::Exception(
					"Main::Database::getLostUrlsByWebsiteUpdate():"
					" No website namespace specified"
			);
		}

		if(websiteProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::getLostUrlsByWebsiteUpdate():"
					" No website name specified"
			);
		}

		// get old namespace and domain
		const auto oldNamespace{this->getWebsiteNamespace(websiteId)};
		const auto oldDomain{this->getWebsiteDomain(websiteId)};

		// check connection
		this->checkConnection();

		try {
			if(oldDomain.empty() && !websiteProperties.domain.empty()) {
				// get URL lists
				auto urlLists{this->getUrlLists(websiteId)};

				// create SQL statement
				SqlStatementPtr sqlStatement{this->connection->createStatement()};

				while(!urlLists.empty()) {
					// count URLs of different domain
					std::string queryString{
						"SELECT COUNT(*) AS result FROM `crawlserv_"
					};

					queryString += oldNamespace;
					queryString += "_";
					queryString += urlLists.front().second;
					queryString += "` WHERE url NOT LIKE '";
					queryString += websiteProperties.domain;
					queryString += "/%' AND url NOT LIKE '";

					// ignore "www." prefix (will be removed if website were to be updated)
					if(
							websiteProperties.domain.size() > wwwPrefix.length()
							&& websiteProperties.domain.substr(0, wwwPrefix.length()) == wwwPrefix
					) {
						queryString += websiteProperties.domain.substr(wwwPrefix.length());
					}
					else {
						queryString += wwwPrefix;
						queryString += websiteProperties.domain;
					}

					queryString += "/%'";

					SqlResultSetPtr sqlResultSet{
						Database::sqlExecuteQuery(
								sqlStatement,
								queryString
						)
					};

					if(sqlResultSet && sqlResultSet->next()) {
						result += sqlResultSet->getUInt64("result");
					}

					urlLists.pop();
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getLostUrlsByWebsiteUpdate", e);
		}

		return result;
	}

	//! Updates a website and all associated tables in the database.
	/*!
	 * Replaces the current website properties
	 *  with the given ones.
	 *
	 * Changing the namespace will result in
	 *  the renaming of all associated tables.
	 *
	 * URLs will be changed, if the type of the
	 *  domain changes from using a specific
	 *  domain to cross-domain, or vice versa,
	 *  because the name of the domain is saved
	 *  as part of the URLs if the website is
	 *  cross-domain.
	 *
	 * \warning URLs might be lost if the type
	 *  of the domain changes from cross-domain
	 *  to using a specific domain, because in
	 *  this case, all URLs that do not contain
	 *  the newly specified domain name, will
	 *  be lost.
	 *
	 * \param websiteId The ID of the website
	 *   that will be updated in the database.
	 * \param websiteProperties A constant
	 *   reference to the new website
	 *   properties.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero, if no website namespace or
	 *   name has been specified in the provided
	 *   website options, or if the new
	 *   namespace already exists in the
	 *   database.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the
	 *   website, its URLs and all associated
	 *   tables in the database.
	 */
	void Database::updateWebsite(std::uint64_t websiteId, const WebsiteProperties& websiteProperties) {
		// check arguments
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::updateWebsite():"
					" No website ID specified"
			);
		}

		if(websiteProperties.nameSpace.empty()) {
			throw Database::Exception(
					"Main::Database::updateWebsite():"
					" No website namespace specified"
			);
		}

		if(websiteProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::updateWebsite():"
					" No website name specified"
			);
		}

		// get old namespace and domain
		const auto oldNamespace{this->getWebsiteNamespace(websiteId)};
		const auto oldDomain{this->getWebsiteDomain(websiteId)};

		// check website namespace if necessary
		if(websiteProperties.nameSpace != oldNamespace) {
			if(this->isWebsiteNamespace(websiteProperties.nameSpace)) {
				throw Database::Exception(
						"Main::Database::updateWebsite():"
						" Website namespace already exists"
				);
			}
		}

		// check connection
		this->checkConnection();

		try {
			// check whether the type of the website (can either be cross-domain or a specific domain) has changed
			if(oldDomain.empty() != websiteProperties.domain.empty()) {
				// get URL lists
				auto urlLists{this->getUrlLists(websiteId)};

				// create SQL statement for modifying and deleting URLs
				SqlStatementPtr urlStatement{this->connection->createStatement()};

				if(oldDomain.empty() && !websiteProperties.domain.empty()) {
					// type changed from cross-domain to specific domain:
					// 	change URLs from absolute (URLs without protocol) to relative (sub-URLs),
					// 	discarding URLs with different domain
					while(!urlLists.empty()) {
						// update URLs of same domain
						std::string queryString{
							"UPDATE `crawlserv_"
						};

						queryString += oldNamespace;
						queryString += "_";
						queryString += urlLists.front().second;
						queryString += "` SET url = SUBSTR(url, LOCATE('/', url)) WHERE url LIKE '";
						queryString += websiteProperties.domain;
						queryString += "/%' OR url LIKE '";

						if(
								websiteProperties.domain.size() > wwwPrefix.length()
								&& websiteProperties.domain.substr(0, wwwPrefix.length()) == wwwPrefix
						) {
							queryString += websiteProperties.domain.substr(wwwPrefix.length());
						}
						else {
							queryString += "www.";
							queryString += websiteProperties.domain;
						}

						queryString += "/%'";

						Database::sqlExecute(
								urlStatement,
								queryString
						);

						// delete URLs of different domain
						Database::sqlExecute(
								urlStatement,
								"DELETE FROM `crawlserv_"
								+ oldNamespace
								+ "_"
								+ urlLists.front().second
								+ "`"
								" WHERE LEFT(url, 1) != '/'"
						);

						urlLists.pop();
					}
				}
				else if(!oldDomain.empty() && websiteProperties.domain.empty()) {
					// type changed from specific domain to cross-domain:
					// 	change URLs from relative (sub-URLs) to absolute (URLs without protocol)
					auto urlLists{this->getUrlLists(websiteId)};

					while(!urlLists.empty()) {
						std::string queryString{
							"UPDATE `crawlserv_"
						};

						queryString += oldNamespace;
						queryString += "_";
						queryString += urlLists.front().second;
						queryString += "` SET url = CONCAT('";
						queryString += oldDomain;
						queryString += "', url)";

						// update URLs
						Database::sqlExecute(
							urlStatement,
							queryString
						);

						urlLists.pop();
					}
				}
			}

			// check whether namespace has changed
			if(websiteProperties.nameSpace != oldNamespace) {
				// create SQL statement for renaming
				SqlStatementPtr renameStatement{this->connection->createStatement()};

				// rename tables
				auto urlLists{this->getUrlLists(websiteId)};

				while(!urlLists.empty()) {
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ oldNamespace
							+ "_"
							+ urlLists.front().second
							+ "`"
							" RENAME TO `crawlserv_"
							+ websiteProperties.nameSpace
							+ "_"
							+ urlLists.front().second
							+ "`"
					);

					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ oldNamespace
							+ "_"
							+ urlLists.front().second
							+ "_crawled`"
							" RENAME TO `crawlserv_"
							+ websiteProperties.nameSpace
							+ "_"
							+ urlLists.front().second
							+ "_crawled`"
					);

					// rename crawling table
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ oldNamespace
							+ "_"
							+ urlLists.front().second
							+ "_crawling`"
							" RENAME TO `crawlserv_"
							+ websiteProperties.nameSpace
							+ "_"
							+ urlLists.front().second
							+ "_crawling`"
					);

					// rename parsing tables
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ oldNamespace
							+ "_"
							+ urlLists.front().second
							+ "_parsing`"
							" RENAME TO `crawlserv_"
							+ websiteProperties.nameSpace
							+ "_"
							+ urlLists.front().second
							+ "_parsing`"
					);

					auto tables{this->getTargetTables("parsed", urlLists.front().first)};

					while(!tables.empty()) {
						Database::sqlExecute(
								renameStatement,
								"ALTER TABLE `crawlserv_"
								+ oldNamespace
								+ "_"
								+ urlLists.front().second
								+ "_parsed_"
								+ tables.front().second
								+ "`"
								" RENAME TO `crawlserv_"
								+ websiteProperties.nameSpace
								+ "_"
								+ urlLists.front().second
								+ "_parsed_"
								+ tables.front().second
								+ "`"
						);

						tables.pop();
					}

					// rename extracting tables
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ oldNamespace
							+ "_"
							+ urlLists.front().second
							+ "_extracting`"
							" RENAME TO `crawlserv_"
							+ websiteProperties.nameSpace
							+ "_"
							+ urlLists.front().second
							+ "_extracting`"
					);

					this->getTargetTables("extracted", urlLists.front().first).swap(tables);

					while(!tables.empty()) {
						Database::sqlExecute(
								renameStatement,
								"ALTER TABLE `crawlserv_"
								+ oldNamespace
								+ "_"
								+ urlLists.front().second
								+ "_extracted_"
								+ tables.front().second
								+ "`"
								" RENAME TO `crawlserv_"
								+ websiteProperties.nameSpace
								+ "_"
								+ urlLists.front().second
								+ "_extracted_"
								+ tables.front().second
								+ "`"
						);

						tables.pop();
					}

					// rename analyzing tables
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ oldNamespace
							+ "_"
							+ urlLists.front().second
							+ "_analyzing`"
							" RENAME TO `crawlserv_"
							+ websiteProperties.nameSpace
							+ "_"
							+ urlLists.front().second
							+ "_analyzing`"
					);

					this->getTargetTables("analyzed", urlLists.front().first).swap(tables);

					while(!tables.empty()) {
						Database::sqlExecute(
								renameStatement,
								"ALTER TABLE `crawlserv_"
								+ oldNamespace
								+ "_"
								+ urlLists.front().second
								+ "_analyzed_"
								+ tables.front().second
								+ "`"
								" RENAME TO `crawlserv_"
								+ websiteProperties.nameSpace
								+ "_"
								+ urlLists.front().second
								+ "_analyzed_"
								+ tables.front().second
								+ "`"
						);

						tables.pop();
					}

					// URL list done
					urlLists.pop();
				}

				// prepare SQL statement for updating
				SqlPreparedStatementPtr updateStatement{
					this->connection->prepareStatement(
							"UPDATE crawlserv_websites"
							" SET domain = ?, namespace = ?, name = ?"
							" WHERE id = ?"
							" LIMIT 1"
					)
				};

				// execute SQL statement for updating
				if(websiteProperties.domain.empty()) {
					updateStatement->setNull(sqlArg1, 0);
				}
				else {
					updateStatement->setString(sqlArg1, websiteProperties.domain);
				}

				updateStatement->setString(sqlArg2, websiteProperties.nameSpace);
				updateStatement->setString(sqlArg3, websiteProperties.name);
				updateStatement->setUInt64(sqlArg4, websiteId);

				Database::sqlExecute(updateStatement);
			}
			else {
				// prepare SQL statement for updating
				SqlPreparedStatementPtr updateStatement{
					this->connection->prepareStatement(
							"UPDATE crawlserv_websites"
							" SET domain = ?, name = ?"
							" WHERE id = ?"
							" LIMIT 1"
					)
				};

				// execute SQL statement for updating
				if(websiteProperties.domain.empty()) {
					updateStatement->setNull(sqlArg1, 0);
				}
				else {
					updateStatement->setString(sqlArg1, websiteProperties.domain);
				}

				updateStatement->setString(sqlArg2, websiteProperties.name);
				updateStatement->setUInt64(sqlArg3, websiteId);

				Database::sqlExecute(updateStatement);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::updateWebsite", e);
		}

		// check whether data directory has to be changed
		if(this->getWebsiteDataDirectory(websiteId) != websiteProperties.dir) {
			this->moveWebsite(websiteId, websiteProperties);
		}
	}

	//! Deletes a website and all associated data from the database.
	/*!
	 * \warning All associated URL lists will
	 *   also be removed.
	 *
	 * \param websiteId The ID of the website
	 *   that will be deleted from the
	 *   database.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while getting the
	 *   namespace of the website, deleting all
	 *   associated URL lists, or deleting the
	 *   website from the database.
	 */
	void Database::deleteWebsite(std::uint64_t websiteId) {
		// check argument
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::deleteWebsite():"
					" No website ID specified"
			);
		}

		// get website namespace
		const auto websiteNamespace{this->getWebsiteNamespace(websiteId)};

		try {
			// delete URL lists
			auto urlLists{this->getUrlLists(websiteId)};

			while(!urlLists.empty()) {
				// delete URL list
				this->deleteUrlList(urlLists.front().first);

				urlLists.pop();
			}

			// check connection
			this->checkConnection();

			// prepare SQL statement for deletion of website
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"DELETE FROM `crawlserv_websites`"
						" WHERE id = ? LIMIT 1"
				)
			};

			// execute SQL statements for deletion of website
			sqlStatement->setUInt64(sqlArg1, websiteId);

			Database::sqlExecute(sqlStatement);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_websites")) {
				this->resetAutoIncrement("crawlserv_websites");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::deleteWebsite", e);
		}
	}

	//! Duplicates a website, its URL lists, queries, and configurations in the database.
	/*!
	 * A namespace for the new website will
	 *  automatically be created by using
	 *  duplicateWebsiteNamespace.
	 *
	 * A name for the new website will
	 *  automatically be created by adding
	 *  " (copy)" to the name of the
	 *  duplicated website.
	 *
	 * \note No retrieved data (i.e. target
	 *   tables) will be duplicated alongside
	 *   the website, its URL lists, queries,
	 *   and configurations.
	 *
	 * \param websiteId The ID of the website
	 *   to duplicate in the database.
	 * \param queries Constant reference to
	 *   a vector containing pairs of all
	 *   module names with their queries (i.e.
	 *   vectors with key, value pairs including
	 *   the "cat" and "name" keys). This vector
	 *   will be used to update the queries
	 *   inside the duplicated configurations.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero, or if an error occured while
	 *   parsing the duplicated configurations.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while getting information
	 *   about the website, creating the new
	 *   namespace of the website, or duplicating
	 *   the website, its URL lists, queries, and
	 *   configurations.
	 */
	std::uint64_t Database::duplicateWebsite(std::uint64_t websiteId, const Queries& queries) {
		std::uint64_t newId{0};

		// check argument
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::duplicateWebsite():"
					" No website ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for geting website info
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT name, namespace, domain, dir"
						" FROM `crawlserv_websites`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement for geting website info
			sqlStatement->setUInt64(sqlArg1, websiteId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				const std::string websiteNamespace{
					sqlResultSet->getString("namespace")
				};
				const std::string websiteName{
					sqlResultSet->getString("name")
				};

				std::string websiteDomain;
				std::string websiteDir;

				if(!(sqlResultSet->isNull("domain"))) {
					websiteDomain = sqlResultSet->getString("domain");
				}

				if(!(sqlResultSet->isNull("dir"))) {
					websiteDir = sqlResultSet->getString("dir");
				}

				// create new namespace and new name
				const auto newNamespace{
					Database::duplicateWebsiteNamespace(
							websiteNamespace
					)
				};
				const auto newName{
					websiteName + " (copy)"
				};

				// add website
				newId = this->addWebsite(
						WebsiteProperties(
								websiteDomain,
								newNamespace,
								newName,
								websiteDir
						)
				);

				// prepare SQL statement for geting URL list info
				sqlStatement.reset(
						this->connection->prepareStatement(
								"SELECT name, namespace"
								" FROM `crawlserv_urllists`"
								" WHERE website = ?"
						)
				);

				// execute SQL statement for geting URL list info
				sqlStatement->setUInt64(sqlArg1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					// add URL lists with same name (except for "default", which has already been created)
					const std::string urlListName{sqlResultSet->getString("namespace")};

					if(urlListName != "default") {
						this->addUrlList(
								newId,
								UrlListProperties(
										sqlResultSet->getString("namespace"),
										urlListName
								)
						);
					}
				}

				// prepare SQL statement for getting queries
				IdPairs ids;

				sqlStatement.reset(
						this->connection->prepareStatement(
							"SELECT id, name, query, type, resultbool, resultsingle,"
								" resultmulti, resultsubsets, textonly"
							" FROM `crawlserv_queries`"
							" WHERE website = ?"
						)
				);

				// execute SQL statement for getting queries
				sqlStatement->setUInt64(sqlArg1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					// copy query
					const std::uint64_t oldQueryId{sqlResultSet->getUInt64("id")};

					const std::uint64_t newQueryId{
						this->addQuery(
								newId,
								QueryProperties(
										sqlResultSet->getString("name"),
										sqlResultSet->getString("query"),
										sqlResultSet->getString("type"),
										sqlResultSet->getBoolean("resultbool"),
										sqlResultSet->getBoolean("resultsingle"),
										sqlResultSet->getBoolean("resultmulti"),
										sqlResultSet->getBoolean("resultsubsets"),
										sqlResultSet->getBoolean("textonly")
								)
						)
					};

					ids.emplace_back(oldQueryId, newQueryId);
				}

				// prepare SQL statement for getting configurations
				sqlStatement.reset(
						this->connection->prepareStatement(
								"SELECT module, name, config"
								" FROM `crawlserv_configs`"
								" WHERE website = ?"
						)
				);

				// execute SQL statement for getting configurations
				sqlStatement->setUInt64(sqlArg1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					const std::string module{sqlResultSet->getString("module")};
					std::string config{sqlResultSet->getString("config")};

					// find module in queries
					auto modIt{
						std::find_if(
								queries.cbegin(),
								queries.cend(),
								[&module](const auto& p) {
										return p.first == module;
								}
						)
					};

					if(modIt != queries.cend()) {
						// update queries in configuration
						rapidjson::Document jsonConfig;

						try {
							jsonConfig = Helper::Json::parseRapid(config);
						}
						catch(const JsonException& e) {
							throw Exception(
									"Main::Database::duplicateWebsite():"
									" Could not parse configuration ("
									+ std::string(e.view())
									+ ")"
							);
						}

						if(!jsonConfig.IsArray()) {
							throw Exception(
									"Main::Database::duplicateWebsite():"
									" Configuration is no valid JSON array: '"
									+ Helper::Json::stringify(jsonConfig)
									+ "'"
							);
						}

						for(auto& configEntry : jsonConfig.GetArray()) {
							if(!configEntry.IsObject()) {
								throw Exception(
										"Main::Database::duplicateWebsite():"
										" Configuration contains invalid entry '"
										+ Helper::Json::stringify(configEntry)
										+ "'"
								);
							}

							if(!configEntry.HasMember("name")) {
								throw Exception(
										"Main::Database::duplicateWebsite():"
										" Configuration entry '"
										+ Helper::Json::stringify(configEntry)
										+ "' does not include 'name'"
								);
							}

							if(!configEntry["name"].IsString()) {
								throw Exception(
										"Main::Database::duplicateWebsite():"
										" Configuration entry '"
										+ Helper::Json::stringify(configEntry)
										+ "' does not include valid string for 'name'"
								);
							}

							if(!configEntry.HasMember("value")) {
								throw Exception(
										"Main::Database::duplicateWebsite():"
										" Configuration entry '"
										+ Helper::Json::stringify(configEntry)
										+ "' does not include 'value'"
								);
							}

							std::string cat;
							const std::string name(
									configEntry["name"].GetString(),
									configEntry["name"].GetStringLength()
							);

							if(name != "_algo") {
								if(!configEntry.HasMember("cat")) {
									throw Exception(
											"Main::Database::duplicateWebsite():"
											" Configuration entry '"
											+ Helper::Json::stringify(configEntry)
											+ "' does not include 'cat'"
									);
								}

								if(!configEntry["cat"].IsString()) {
									throw Exception(
											"Main::Database::duplicateWebsite():"
											" Configuration entry '"
											+ Helper::Json::stringify(configEntry)
											+ "' does not include valid string for 'cat'"
									);
								}

								std::string(
										configEntry["cat"].GetString(),
										configEntry["cat"].GetStringLength()
								).swap(cat);
							}

							const auto queryIt{
								std::find_if(
										modIt->second.cbegin(),
										modIt->second.cend(),
										[&cat, &name](const auto& p) {
											return p.first == cat
													&& p.second == name;
										}
								)
							};

							if(queryIt != modIt->second.cend()) {
								if(configEntry["value"].IsArray()) {
									for(auto& arrayElement : configEntry["value"].GetArray()) {
										if(!arrayElement.IsUint64()) {
											throw Exception(
													"Main::Database::duplicateWebsite():"
													" Configuration entry '"
													+ Helper::Json::stringify(configEntry)
													+ "' includes invalid query ID '"
													+ Helper::Json::stringify(arrayElement)
													+ "'"
											);
										}

										const auto queryId{arrayElement.GetUint64()};

										const auto idsIt{
											std::find_if(
													ids.cbegin(),
													ids.cend(),
													[&queryId](const auto& p) {
														return p.first == queryId;
													}
											)
										};

										if(idsIt != ids.cend()) {
											rapidjson::Value newValue(idsIt->second);

											arrayElement.Swap(newValue);
										}
									}
								}
								else {
									if(!configEntry["value"].IsUint64()) {
										throw Exception(
												"Main::Database::duplicateWebsite():"
												" Configuration entry '"
												+ Helper::Json::stringify(configEntry)
												+ "' includes invalid query ID '"
												+ Helper::Json::stringify(configEntry["value"])
												+ "'"
										);
									}

									const auto queryId{configEntry["value"].GetUint64()};

									const auto idsIt{
										std::find_if(
												ids.cbegin(),
												ids.cend(),
												[&queryId](const auto& p) {
													return p.first == queryId;
												}
										)
									};

									if(idsIt != ids.cend()) {
										rapidjson::Value newValue(idsIt->second);

										configEntry["value"].Swap(newValue);
									}
								}
							}
						}

						config = Helper::Json::stringify(jsonConfig);
					}

					// add configuration
					this->addConfiguration(
							newId,
							ConfigProperties(
									module,
									sqlResultSet->getString("name"),
									config
							)
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::duplicateWebsite", e);
		}

		return newId;
	}

	//! Moves a website and all associated data to another data directory in the database.
	/*!
	 * Creates temporary copies of all tables
	 *  first, enough memory needs therefore to
	 *  be available if both data directories
	 *  are located on the same physical device.
	 *
	 * \warning Disables the checking of
	 *   constraints in the database while
	 *   copying and moving the tables.
	 *
	 * \param websiteId The ID of the website
	 *   that will be moved to another data
	 *   directory in the database.
	 * \param websiteProperties Constant
	 *   reference to the properties of the
	 *   website to be moved, including the
	 *   new data directory.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while moving the website
	 *   and all associated data to another data
	 *   directory in the database.
	 */
	void Database::moveWebsite(std::uint64_t websiteId, const WebsiteProperties& websiteProperties) {
		// check argument
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::duplicateWebsite():"
					" No website ID specified"
			);
		}

#ifdef MAIN_DATABASE_LOG_MOVING
		Timer::Simple timer;

		std::cout	<< "\n\nMOVING website "
					<< websiteProperties.name
					<< " to '"
					<< websiteProperties.dir
					<< "'..."
					<< std::flush;
#endif
		// create table list
		std::vector<std::string> tables;

		// go through all affected URL lists
		auto urlLists{this->getUrlLists(websiteId)};

		while(!urlLists.empty()) {
			// get tables with parsed, extracted and analyzed content
			auto parsedTables{
				this->getTargetTables(
						"parsed",
						urlLists.front().first
				)
			};
			auto extractedTables{
				this->getTargetTables(
						"extracted",
						urlLists.front().first
				)
			};
			auto analyzedTables{
				this->getTargetTables(
						"analyzed",
						urlLists.front().first
				)
			};

			// reserve additional memory for all table names of the current URL list
			tables.reserve(
					tables.size()
					+ numUrlListTables
					+ parsedTables.size()
					+ extractedTables.size()
					+ analyzedTables.size()
			);

			// add main table for URL list
			tables.emplace_back(
					"crawlserv_"
					+ websiteProperties.nameSpace
					+ "_"
					+ urlLists.front().second
			);

			// add status tables for URL list
			tables.emplace_back(
					"crawlserv_"
					+ websiteProperties.nameSpace
					+ "_"
					+ urlLists.front().second
					+ "_crawling"
			);
			tables.emplace_back(
					"crawlserv_"
					+ websiteProperties.nameSpace
					+ "_"
					+ urlLists.front().second
					+ "_parsing"
			);
			tables.emplace_back(
					"crawlserv_"
					+ websiteProperties.nameSpace
					+ "_"
					+ urlLists.front().second
					+ "_extracting"
			);
			tables.emplace_back(
					"crawlserv_"
					+ websiteProperties.nameSpace
					+ "_"
					+ urlLists.front().second
					+ "_analyzing"
			);

			// add table with crawled content for URL list
			tables.emplace_back(
					"crawlserv_"
					+ websiteProperties.nameSpace
					+ "_"
					+ urlLists.front().second
					+ "_crawled"
			);

			// add tables with parsed content for URL list
			while(!parsedTables.empty()) {
				tables.emplace_back(
						"crawlserv_"
						+ websiteProperties.nameSpace
						+ "_"
						+ urlLists.front().second
						+ "_parsed_"
						+ parsedTables.front().second
				);

				parsedTables.pop();
			}

			// add tables with extracted content for URL list
			while(!extractedTables.empty()) {
				tables.emplace_back(
						"crawlserv_"
						+ websiteProperties.nameSpace
						+ "_"
						+ urlLists.front().second
						+ "_extracted_"
						+ extractedTables.front().second
				);

				extractedTables.pop();
			}

			// add tables with analyzed content for URL list
			while(!analyzedTables.empty()) {
				tables.emplace_back(
						"crawlserv_"
						+ websiteProperties.nameSpace
						+ "_"
						+ urlLists.front().second
						+ "_analyzed_"
						+ analyzedTables.front().second
				);

				analyzedTables.pop();
			}

			// go to next URL list
			urlLists.pop();
		}

		// remove temporary tables in reverse order (to avoid problems with constraints)
		for(auto it{tables.crbegin()}; it != tables.crend(); ++it) {
			this->dropTable(*it + "_tmp");
		}

		// clone tables to new data directory (without data or constraints)
		std::queue<StringQueueOfStrings> constraints;

		for(const auto& table : tables) {
#ifdef MAIN_DATABASE_LOG_MOVING
			std::cout << "\n Cloning: `" << table << "`" << std::flush;
#endif

			constraints.emplace(
					table,
					this->cloneTable(
							table,
							websiteProperties.dir
					)
			);
		}

		// check connection
		this->checkConnection();

		try { // start first transaction in-scope (copying data)
			Transaction transaction(*this, "READ UNCOMMITTED");

			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// add constraints to tables
			while(!constraints.empty()) {
				if(constraints.front().second.empty()) {
					constraints.pop();

					continue;
				}

				std::string toAdd;

				while(!constraints.front().second.empty()) {
					auto constraint{constraints.front().second.front()};

					// check reference and use temporary table if the table lies inside the namespace of the website
					const auto pos{constraint.find(" `")};
					const auto end{constraint.find("` ")};

					if(pos != std::string::npos && end != std::string::npos) {
						const std::string nspace{
							"crawlserv_"
							+ websiteProperties.nameSpace
							+ "_"
						};

						const std::string referenced(
								constraint,
								pos + 2,
								end - pos - 2
						);

						if(referenced.substr(0, nspace.length()) == nspace) {
							constraint.insert(end, "_tmp");
						}

						toAdd += " ADD " + constraint;

						if(constraint.back() != ',') {
							toAdd += ',';
						}
					}

					constraints.front().second.pop();
				}

				if(toAdd.empty()) {
					constraints.pop();

					continue;
				}

#ifdef MAIN_DATABASE_LOG_MOVING
				std::cout	<< "\n Adding constraint(s) to `"
							<< constraints.front().first
							<< "_tmp`"
							<< std::flush;
#endif

				toAdd.pop_back();

				Database::sqlExecute(
						sqlStatement,
						"ALTER TABLE `"
						+ constraints.front().first
						+ "_tmp`"
						+ toAdd
				);

				constraints.pop();
			}

			// disable key checking to speed up copying
			Database::sqlExecute(
					sqlStatement,
					"SET UNIQUE_CHECKS = 0, FOREIGN_KEY_CHECKS = 0"
			);

			// copy data to tables
			for(const auto& table : tables) {
#ifdef MAIN_DATABASE_LOG_MOVING
				std::cout << "\n Copying: `" << table << "`" << std::flush;
#endif

				// get number of rows to copy
				std::uint64_t count{0};

				SqlResultSetPtr result1{
					Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT COUNT(*) AS count FROM `" + table + "`"
					)
				};

				if(result1->next() && !(result1->isNull("count"))) {
					count = result1->getUInt64("count");
				}

				// get names of columns to copy
				std::string columns;

				SqlResultSetPtr result2{
					Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT "
						" COLUMN_NAME AS name "
						"FROM INFORMATION_SCHEMA.COLUMNS "
						"WHERE"
						" TABLE_SCHEMA = '" + this->settings.name + "'"
						" AND TABLE_NAME = '" + table + "'"
					)
				};

				while(result2->next()) {
					if(!(result2->isNull("name"))) {
						columns += "`" + result2->getString("name") + "`, ";
					}
				}

				if(columns.empty()) {
					continue;
				}

				columns.pop_back();
				columns.pop_back();

#ifdef MAIN_DATABASE_LOG_MOVING
				if(count < nAtOnce100) {
#endif
					// copy all at once
					std::string queryString{
						"INSERT INTO `"
					};

					queryString += table;
					queryString += "_tmp`(";
					queryString += columns;
					queryString += ") SELECT ";
					queryString += columns;
					queryString += " FROM `";
					queryString += table;
					queryString += "`";

					Database::sqlExecute(sqlStatement, queryString);
#ifdef MAIN_DATABASE_LOG_MOVING
				}
				else {
					// copy in 100 (or 101) steps for logging the progress
					std::cout << "     " << std::flush;

					const auto step{count / nAtOnce100};

					for(auto n{0}; n <= nAtOnce100; ++n) {
						std::string queryString{
							"INSERT INTO `"
						};

						queryString += table;
						queryString += "_tmp`(";
						queryString += columns;
						queryString += ") SELECT ";
						queryString += columns;
						queryString += " FROM `";
						queryString += table;
						queryString +=	"` AS t JOIN ("
									   	" SELECT"
									   	" COALESCE(MAX(id), 0) AS offset"
										" FROM `";
						queryString += table;
						queryString +=	"_tmp`"
										" ) AS m"
										" ON t.id > m.offset"
										" ORDER BY t.id"
										" LIMIT ";
						queryString += std::to_string(step);

						Database::sqlExecute(
								sqlStatement,
								queryString
						);

						std::cout << "\b\b\b\b";

						if(n < nAtOnce100) {
							std::cout << ' ';
						}

						if(n < nAtOnce10) {
							std::cout << ' ';
						}

						std::cout << std::to_string(n) << '%' << std::flush;
					}
				}
#endif
			}

			// re-enable key checking
			Database::sqlExecute(
					sqlStatement,
					"SET UNIQUE_CHECKS = 1, FOREIGN_KEY_CHECKS = 1"
			);

			// first transaction successful (data copied)
			transaction.success();

#ifdef MAIN_DATABASE_LOG_MOVING
			std::cout << "\n Committing changes" << std::flush;
#endif
		} // transaction will be committed on success or rolled back on exception

		// <EXCEPTION HANDLING>
		catch(const sql::SQLException &e) {
#ifdef MAIN_DATABASE_LOG_MOVING
			std::cout << "\n " << e.what() << std::endl;
#endif

			Database::sqlException("Main::Database::moveWebsite", e);
		}
#ifdef MAIN_DATABASE_LOG_MOVING
		catch(const Exception& e) {
			std::cout << "\n " << e.view() << std::endl;

			throw;
		}
#endif
		// </EXCEPTION HANDLING>

		// check connection
		this->checkConnection();

		try { // start second transaction in-scope (replacing tables)
			Transaction transaction(*this);

			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// delete old tables in reverse order (to avoid problems with constraints)
			for(auto it{tables.crbegin()}; it != tables.crend(); ++it) {
#ifdef MAIN_DATABASE_LOG_MOVING
				std::cout << "\n Deleting: `" << *it << "`" << std::flush;
#endif

				Database::sqlExecute(
						sqlStatement,
						"DROP TABLE IF EXISTS `" + *it + "`"
				);
			}

			// rename new tables
			for(const auto& table : tables) {
#ifdef MAIN_DATABASE_LOG_MOVING
				std::cout << "\n Renaming: `" << table << "_tmp`" << std::flush;
#endif

				std::string queryString{
					"RENAME TABLE `"
				};

				queryString += table;
				queryString += "_tmp` TO `";
				queryString += table;
				queryString += "`";

				Database::sqlExecute(
						sqlStatement,
						queryString
				);
			}

			// update directory for website
			Database::sqlExecute(
					sqlStatement,
					"UPDATE `crawlserv_websites`"
					" SET dir = '" + websiteProperties.dir + "'"
					" WHERE id = " + std::to_string(websiteId) +
					" LIMIT 1"
			);

			// second transaction successful (tables replaced)
			transaction.success();

#ifdef MAIN_DATABASE_LOG_MOVING
			std::cout << "\n Committing changes" << std::flush;
#endif
		} // transaction will be committed on success or rolled back on exception

		// <EXCEPTION HANDLING>
		catch(const sql::SQLException &e) {
#ifdef MAIN_DATABASE_LOG_MOVING
			std::cout << "\n " << e.what() << std::endl;
#endif

			Database::sqlException("Main::Database::moveWebsite", e);
		}
#ifdef MAIN_DATABASE_LOG_MOVING
		catch(const Exception& e) {
			std::cout << "\n " << e.view() << std::endl;

			throw;
		}
#endif
		// </EXCEPTION HANDLING>

#ifdef MAIN_DATABASE_LOG_MOVING
		// log success and elapsed time
		std::cout	<< "\n MOVED website "
					<< websiteProperties.name
					<< " in "
					<< timer.tickStr()
					<< '.'
					<< std::endl;
#endif
	}

	/*
	 * URL LIST FUNCTIONS
	 */

	//! Adds a new URL list to the database and returns its ID.
	/*!
	 * \param websiteId The ID of the website
	 *   to which the new URL list will be
	 *   added.
	 * \param listProperties Constant reference
	 *   to the properties of the new URL list.
	 *
	 * \returns A unique ID identifying the new
	 *   URL list in the database.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero, no namespace or name has been
	 *   specfied in the given properties of the
	 *   new URL list, or if the specified URL
	 *   list namespace already exists in the
	 *   database.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the new
	 *   URL list to the database.
	 */
	std::uint64_t Database::addUrlList(
			std::uint64_t websiteId,
			const UrlListProperties& listProperties
	) {
		std::uint64_t newId{0};

		// check arguments
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::addUrlList():"
					" No website ID specified"
			);
		}

		if(listProperties.nameSpace.empty()) {
			throw Database::Exception(
					"Main::Database::addUrlList():"
					" No URL list namespace specified"
			);
		}

		if(listProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::addUrlList():"
					" No URL list name specified"
			);
		}

		// get website namespace and data directory
		const auto websiteNamespace{this->getWebsiteNamespace(websiteId)};
		const auto websiteDataDirectory{this->getWebsiteDataDirectory(websiteId)};

		// check URL list namespace
		if(this->isUrlListNamespace(websiteId, listProperties.nameSpace)) {
			throw Database::Exception(
					"Main::Database::addUrlList():"
					" URL list namespace already exists"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for adding URL list
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"INSERT INTO crawlserv_urllists(website, namespace, name)"
						" VALUES (?, ?, ?)"
				)
			};

			// execute SQL query for adding URL list
			sqlStatement->setUInt64(sqlArg1, websiteId);
			sqlStatement->setString(sqlArg2, listProperties.nameSpace);
			sqlStatement->setString(sqlArg3, listProperties.name);

			Database::sqlExecute(sqlStatement);

			// get ID of newly added URL list
			newId = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::addUrlList", e);
		}

		// maximum number of columns created here
		constexpr auto maxColumns{6};

		// create table for URL list
		std::vector<TableColumn> columns;

		columns.reserve(maxColumns);

		columns.emplace_back("manual", "BOOLEAN DEFAULT FALSE NOT NULL");
		columns.emplace_back("url", "VARCHAR(2000) NOT NULL");
		columns.emplace_back("hash", "INT UNSIGNED DEFAULT 0 NOT NULL", true);

		this->createTable(
				TableProperties(
						"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace,
						columns,
						websiteDataDirectory,
						false
				)
		);

		columns.clear();

		// create table for crawled content
		columns.emplace_back(
				"url", "BIGINT UNSIGNED NOT NULL",
				"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace,
				"id"
		);
		columns.emplace_back(
				"crawltime",
				"DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP NOT NULL",
				true
		);
		columns.emplace_back("archived", "BOOLEAN DEFAULT FALSE NOT NULL");
		columns.emplace_back("response", "SMALLINT UNSIGNED NOT NULL DEFAULT 0");
		columns.emplace_back("type", "TINYTEXT NOT NULL");
		columns.emplace_back("content", "LONGTEXT NOT NULL");

		this->createTable(
				TableProperties(
						"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace + "_crawled",
						columns,
						websiteDataDirectory,
						true
				)
		);

		columns.clear();

		// create table for crawling
		columns.emplace_back(
				"url",
				"BIGINT UNSIGNED NOT NULL UNIQUE", // 1-to-1 relationship
				"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace,
				"id"
		);
		columns.emplace_back("locktime", "DATETIME DEFAULT NULL");
		columns.emplace_back("success", "BOOLEAN DEFAULT FALSE NOT NULL");

		this->createTable(
				TableProperties(
						"crawlserv_"
						+ websiteNamespace
						+ "_"
						+ listProperties.nameSpace
						+ "_crawling",
						columns,
						websiteDataDirectory,
						false
				)
		);

		columns.clear();

		// create table for parsing
		columns.emplace_back(
				"target",
				"BIGINT UNSIGNED NOT NULL",
				"crawlserv_parsedtables",
				"id"
		);
		columns.emplace_back(
				"url",
				"BIGINT UNSIGNED NOT NULL",
				"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace,
				"id"
		);
		columns.emplace_back("locktime", "DATETIME DEFAULT NULL");
		columns.emplace_back("success", "BOOLEAN DEFAULT FALSE NOT NULL");

		this->createTable(
				TableProperties(
						"crawlserv_" + websiteNamespace  + "_" + listProperties.nameSpace + "_parsing",
						columns,
						websiteDataDirectory,
						false
				)
		);

		columns.clear();

		// create table for extracting
		columns.emplace_back(
				"target",
				"BIGINT UNSIGNED NOT NULL",
				"crawlserv_extractedtables",
				"id"
		);
		columns.emplace_back(
				"url",
				"BIGINT UNSIGNED NOT NULL",
				"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace,
				"id"
		);
		columns.emplace_back("locktime", "DATETIME DEFAULT NULL");
		columns.emplace_back("success", "BOOLEAN DEFAULT FALSE NOT NULL");

		this->createTable(
				TableProperties(
						"crawlserv_" + websiteNamespace  + "_" + listProperties.nameSpace + "_extracting",
						columns,
						websiteDataDirectory,
						false
				)
		);

		columns.clear();

		// create table for analyzing
		columns.emplace_back(
				"target",
				"BIGINT UNSIGNED NOT NULL",
				"crawlserv_analyzedtables",
				"id"
		);
		columns.emplace_back("chunk_id", "BIGINT UNSIGNED DEFAULT NULL");
		columns.emplace_back("chunk_label", "TINYTEXT DEFAULT NULL");
		columns.emplace_back("algo", "TINYTEXT NOT NULL");
		columns.emplace_back("locktime", "DATETIME DEFAULT NULL");
		columns.emplace_back("success", "BOOLEAN DEFAULT FALSE NOT NULL");

		this->createTable(
				TableProperties(
						"crawlserv_"
						+ websiteNamespace
						+ "_"
						+ listProperties.nameSpace
						+ "_analyzing",
						columns,
						websiteDataDirectory,
						false
				)
		);

		columns.clear();

		return newId;
	}

	//! Gets all URL lists associated with a website from the database.
	/*!
	 * \param websiteId The ID of the website
	 *   for which the associated URL lists will
	 *   be retrieved.
	 *
	 * \returns A queue with pairs containing the
	 *   IDs and the namespaces of the URL lists
	 *   associated with the given website.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the URL
	 *   lists from the database.
	 */
	std::queue<Database::IdString> Database::getUrlLists(std::uint64_t websiteId) {
		std::queue<IdString> result;

		// check arguments
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::getUrlLists():"
					" No website ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT id, namespace"
						" FROM `crawlserv_urllists`"
						" WHERE website = ?"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get results
			if(sqlResultSet) {
				while(sqlResultSet->next()) {
					result.emplace(
							sqlResultSet->getUInt64("id"),
							sqlResultSet->getString("namespace")
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getUrlLists", e);
		}

		return result;
	}

	//! Inserts URLs that do not already exist into a URL list in the database, returns the number of added URLs.
	/*!
	 * \param listId The ID of the URL list to
	 *   which those URLs will be added that the
	 *   list does not already contain.
	 * \param urls Reference to a queue
	 *   containing the URLs to be inserted, if
	 *   they do not already exist in the given
	 *   URL list. The queue will be emptied,
	 *   even if URLs will not be added, because
	 *   they already exist in the URL list.
	 *
	 * \returns The number of URLs actually
	 *   added.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the list
	 *   ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   existence of the URLs and adding the
	 *   non-existing URLs to the URL list.
	 */
	std::size_t Database::mergeUrls(std::uint64_t listId, std::queue<std::string>& urls) {
		std::size_t urlsAdded{0};

		// check arguments
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::mergeUrls():"
					" No URL list ID specified"
			);
		}

		if(urls.empty()) {
			return 0;
		}

		// get ID and namespaces of website
		const auto website{this->getWebsiteNamespaceFromUrlList(listId)};

		// get namespace of URL list and generate name of URL list table
		const std::string urlListTable{
			"crawlserv_"
			+ website.second
			+ "_"
			+ this->getUrlListNamespace(listId)
		};

		// generate SQL string for URL hashing
		std::string hashQuery{"CRC32( "};

		if(this->isUrlListCaseSensitive(listId)) {
			hashQuery += "?";
		}
		else {
			hashQuery += "LOWER( ? )";
		}

		hashQuery += " )";

		// generate query for each 1,000 (or less) URLs
		while(!urls.empty()) {
			// generate INSERT INTO ... VALUES clause
			std::string sqlQueryStr{
				"INSERT IGNORE INTO `"
				+ urlListTable
				+ "`(id, url, hash) VALUES "
			};

			// generate placeholders
			const auto max{urls.size() > nAtOnce1000 ? nAtOnce1000 : urls.size()};

			for(std::size_t n{0}; n < max; ++n) {
				sqlQueryStr += "(" // begin of VALUES arguments
								" ("
									"SELECT id FROM"
									" ("
										"SELECT id, url"
										" FROM `";
				sqlQueryStr += urlListTable;
				sqlQueryStr += 			"`"
										" AS `a";
				sqlQueryStr += std::to_string(n + 1);
				sqlQueryStr +=			"`"
										" WHERE hash = ";
				sqlQueryStr += hashQuery;
				sqlQueryStr +=		" ) AS tmp2 WHERE url = ? LIMIT 1"
								" ),"
								"?, ";
				sqlQueryStr += hashQuery;
				sqlQueryStr += "), "; // end of VALUES arguments
			}

			// remove last comma and space
			sqlQueryStr.pop_back();
			sqlQueryStr.pop_back();

			// check connection
			this->checkConnection();

			// number of arguments for adding one URL
			constexpr auto numArgsAdd{4};

			try {
				// prepare SQL statement
				SqlPreparedStatementPtr sqlStatement{
					this->connection->prepareStatement(
							sqlQueryStr
					)
				};

				// execute SQL query
				const std::size_t max{urls.size() > nAtOnce1000 ? nAtOnce1000 : urls.size()};

				for(std::size_t n{0}; n < max; ++n) {
					sqlStatement->setString(n * numArgsAdd + sqlArg1, urls.front());
					sqlStatement->setString(n * numArgsAdd + sqlArg2, urls.front());
					sqlStatement->setString(n * numArgsAdd + sqlArg3, urls.front());
					sqlStatement->setString(n * numArgsAdd + sqlArg4, urls.front());

					urls.pop();
				}

				const auto added{Database::sqlExecuteUpdate(sqlStatement)};

				if(added > 0) {
					urlsAdded += added;
				}
			}
			catch(const sql::SQLException &e) {
				Database::sqlException("Main::Database::mergeUrls", e);
			}
		}

		return urlsAdded;
	}

	//! Gets all URLs from a URL list in the database.
	/*!
	 * \param listId The ID of the URL list from
	 *   which the URLs will be retrieved.
	 *
	 * \returns A queue of strings containing
	 *   all URLs in the given URL list.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the list
	 *   ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   URLs from the given URL list in the
	 *   database.
	 */
	std::queue<std::string> Database::getUrls(std::uint64_t listId) {
		std::queue<std::string> result;

		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::getUrls():"
					" No URL list ID specified"
			);
		}

		// get ID and namespaces of website
		const auto website{this->getWebsiteNamespaceFromUrlList(listId)};

		// get namespace of URL list and generate name of URL list table
		const std::string urlListTable{
			"crawlserv_"
			+ website.second
			+ "_"
			+ this->getUrlListNamespace(listId)
		};

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT url FROM `" + urlListTable + "`"
				)
			};

			// get results
			if(sqlResultSet) {
				while(sqlResultSet->next()) {
					result.emplace(sqlResultSet->getString("url"));
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getUrls", e);
		}

		return result;
	}

	//! Gets all URLs and their IDs from a URL list in the database.
	/*!
	 * \param listId The ID of the URL list from
	 *   which the URLs and their IDs will be
	 *   retrieved.
	 *
	 * \returns A queue of pairs containing
	 *   the IDs of all URLs in the given URL
	 *   list and the URLs themselves.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the list
	 *   ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   URLs and their IDs from the given URL
	 *   list in the database.
	 */
	std::queue<Database::IdString> Database::getUrlsWithIds(std::uint64_t listId) {
		std::queue<IdString> result;

		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::getUrlsWithIds():"
					" No URL list ID specified"
			);
		}

		// get ID and namespaces of website
		const auto website{this->getWebsiteNamespaceFromUrlList(listId)};

		// get namespace of URL list and generate name of URL list table
		const std::string urlListTable{
			"crawlserv_"
			+ website.second
			+ "_"
			+ this->getUrlListNamespace(listId)
		};

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT id, url FROM `"
						+ urlListTable
						+ "`"
				)
			};

			// get results
			if(sqlResultSet) {
				while(sqlResultSet->next()) {
					result.emplace(
							sqlResultSet->getUInt64("id"),
							sqlResultSet->getString("url")
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getUrlsWithIds", e);
		}

		return result;
	}

	//! Gets the namespace of a URL list from the database.
	/*!
	 * \param listId The ID of the URL list
	 *   whose namespace will be retrieved.
	 *
	 * \returns A copy of the namespace of the
	 *   given URL list.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the list
	 *   ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   namespace of the URL list.
	 */
	std::string Database::getUrlListNamespace(std::uint64_t listId) {
		std::string result;

		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::getUrlListNamespace():"
					" No URL list ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT namespace"
						" FROM `crawlserv_urllists`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL query
			sqlStatement->setUInt64(sqlArg1, listId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getString("namespace");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getUrlListNamespace", e);
		}

		return result;
	}

	//! Gets the namespace of a URL list from the database by using a target table.
	/*!
	 * \param type Constant reference to a
	 *   string containing the type of the
	 *   target table whose URL list's
	 *   namespace will be retrieved.
	 * \param tableId The ID of the target
	 * 	 table whose URL list's namespace
	 * 	 will be retrieved.
	 *
	 * \returns A copy of the namespace of the
	 *   URL list associated with the given
	 *   target table.
	 *
	 * \throws Main::Database::Exception if no target
	 *   table type has been specified, i.e. the
	 *   type is empty, or no target table has
	 *   been specified, i.e. the string
	 *   containing the type is empty or the
	 *   target table ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   namespace of the URL list.
	 */
	Database::IdString Database::getUrlListNamespaceFromTargetTable(
			const std::string& type,
			std::uint64_t tableId
	) {
		std::uint64_t urlListId{0};

		// check arguments
		if(type.empty()) {
			throw Database::Exception(
					"Main::Database::getUrlListNamespaceFromCustomTable():"
					" No table type specified"
			);
		}

		if(tableId == 0) {
			throw Database::Exception(
					"Main::Database::getUrlListNamespaceFromCustomTable():"
					" No table ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT urllist"
						" FROM `crawlserv_"
						+ type
						+ "tables`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL query
			sqlStatement->setUInt64(sqlArg1, tableId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				urlListId = sqlResultSet->getUInt64("urllist");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getUrlListNamespaceFromCustomTable", e);
		}

		return IdString(urlListId, this->getUrlListNamespace(urlListId));
	}

	//! Checks whether a URL list namespace for a specific website exists in the database.
	/*!
	 * \param websiteId The ID of the website
	 *   for which the URL list namespace will
	 *   be checked.
	 * \param nameSpace Constant reference to
	 *   a string containing the URL list
	 *   namespace to be checked.
	 *
	 * \returns True, if the given URL list
	 *   namespace for the given website
	 *   already exists in the database. False
	 *   otherwise.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero, or no URL list namespace has
	 *   been specified, i.e. the namespace is
	 *   empty.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   existence of the URL list namespace in
	 *   the database.
	 */
	bool Database::isUrlListNamespace(std::uint64_t websiteId, const std::string& nameSpace) {
		bool result{false};

		// check arguments
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::isUrlListNamespace():"
					" No website ID specified"
			);
		}

		if(nameSpace.empty()) {
			throw Database::Exception(
					"Main::Database::isUrlListNamespace():"
					" No namespace specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_urllists`"
							" WHERE website = ?"
							" AND namespace = ?"
						" )"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);
			sqlStatement->setString(sqlArg2, nameSpace);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isUrlListNamespace", e);
		}

		return result;
	}

	//! Updates a URL list and all associated tables in the database.
	/*!
	 * Replaces the current URL list properties
	 *  with the given ones.
	 *
	 * Changing the namespace will result in
	 *  the renaming of all associated tables.
	 *
	 * \param listId The ID of the URL list
	 *   that will be updated in the database.
	 * \param listProperties A constant
	 *   reference to the new URL list
	 *   properties.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the
	 *   URL list ID is zero, if no URL list
	 *   namespace or name has been specified in
	 *   the provided website options, or if the
	 *   new namespace already exists in the
	 *   database.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the
	 *   URL list and all associated tables in
	 *   the database.
	 */
	void Database::updateUrlList(std::uint64_t listId, const UrlListProperties& listProperties) {
		// check arguments
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::updateUrlList():"
					" No website ID specified"
			);
		}

		if(listProperties.nameSpace.empty()) {
			throw Database::Exception(
					"Main::Database::updateUrlList():"
					" No URL list namespace specified"
			);
		}

		if(listProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::updateUrlList():"
					" No URL list name specified"
			);
		}

		// get website namespace and URL list name
		const auto websiteNamespace{
			this->getWebsiteNamespaceFromUrlList(listId)
		};
		const auto oldListNamespace{
			this->getUrlListNamespace(listId)
		};

		// check website namespace if necessary
		if(listProperties.nameSpace != oldListNamespace) {
			if(
					this->isUrlListNamespace(
							websiteNamespace.first,
							listProperties.nameSpace
					)
			) {
				throw Database::Exception(
						"Main::Database::updateUrlList():"
						" URL list namespace already exists"
				);
			}
		}

		// check connection
		this->checkConnection();

		try {
			if(listProperties.nameSpace != oldListNamespace) {
				// create SQL statement for renaming
				SqlStatementPtr renameStatement{this->connection->createStatement()};

				// rename URL list table
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ oldListNamespace
						+ "`"
						" RENAME TO `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ listProperties.nameSpace
						+ "`"
				);

				// rename crawling tables
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ oldListNamespace
						+ "_crawled`"
						" RENAME TO `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ listProperties.nameSpace
						+ "_crawled`"
				);

				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ oldListNamespace
						+ "_crawling`"
						" RENAME TO `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ listProperties.nameSpace
						+ "_crawling`"
				);

				// rename parsing tables
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ oldListNamespace
						+ "_parsing`"
						" RENAME TO `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ listProperties.nameSpace
						+ "_parsing`"
				);

				auto tables{
					this->getTargetTables(
							"parsed",
							listId
					)
				};

				while(!tables.empty()) {
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ websiteNamespace.second
							+ "_"
							+ oldListNamespace
							+ "_parsed_"
							+ tables.front().second + "`"
							" RENAME TO `crawlserv_"
							+ websiteNamespace.second
							+ "_"
							+ listProperties.nameSpace
							+ "_parsed_"
							+ tables.front().second
							+ "`"
					);

					tables.pop();
				}

				// rename extracting tables
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ oldListNamespace
						+ "_extracting`"
						" RENAME TO `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ listProperties.nameSpace
						+ "_extracting`"
				);

				this->getTargetTables("extracted", listId).swap(tables);

				while(!tables.empty()) {
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ websiteNamespace.second
							+ "_"
							+ oldListNamespace
							+ "_extracted_"
							+ tables.front().second
							+ "`"
							" RENAME TO `crawlserv_"
							+ websiteNamespace.second
							+ "_"
							+ listProperties.nameSpace
							+ "_extracted_"
							+ tables.front().second
							+ "`"
					);

					tables.pop();
				}

				// rename analyzing tables
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ oldListNamespace
						+ "_analyzing`"
						" RENAME TO `crawlserv_"
						+ websiteNamespace.second
						+ "_"
						+ listProperties.nameSpace
						+ "_analyzing`"
				);

				this->getTargetTables("analyzed", listId).swap(tables);

				while(!tables.empty()) {
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_"
							+ websiteNamespace.second
							+ "_"
							+ oldListNamespace
							+ "_analyzed_"
							+ tables.front().second
							+ "`"
							" RENAME TO `crawlserv_"
							+ websiteNamespace.second
							+ "_"
							+ listProperties.nameSpace
							+ "_analyzed_"
							+ tables.front().second
							+ "`"
					);

					tables.pop();
				}

				// prepare SQL statement for updating
				SqlPreparedStatementPtr updateStatement{
					this->connection->prepareStatement(
							"UPDATE crawlserv_urllists"
							" SET namespace = ?, name = ?"
							" WHERE id = ?"
							" LIMIT 1"
					)
				};

				// execute SQL statement for updating
				updateStatement->setString(sqlArg1, listProperties.nameSpace);
				updateStatement->setString(sqlArg2, listProperties.name);
				updateStatement->setUInt64(sqlArg3, listId);

				Database::sqlExecute(updateStatement);
			}
			else {
				// prepare SQL statement for updating
				SqlPreparedStatementPtr updateStatement{
					this->connection->prepareStatement(
							"UPDATE crawlserv_urllists"
							" SET name = ?"
							" WHERE id = ?"
							" LIMIT 1"
					)
				};

				// execute SQL statement for updating
				updateStatement->setString(sqlArg1, listProperties.name);
				updateStatement->setUInt64(sqlArg2, listId);

				Database::sqlExecute(updateStatement);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::updateUrlList", e);
		}
	}

	//! Deletes a URL list and all associated data from the database.
	/*!
	 * \warning All associated data will be
	 *   removed alongside the URL list.
	 *
	 * \param listId The ID of the URL list
	 *   that will be deleted from the
	 *   database.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while getting the
	 *   namespace of the URL list, deleting all
	 *   associated tables, or deleting the URL
	 *   list from the database.
	 */
	void Database::deleteUrlList(std::uint64_t listId) {
		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::deleteUrlList():"
					" No URL list ID specified"
			);
		}

		// get website namespace and URL list name
		const auto websiteNamespace{
			this->getWebsiteNamespaceFromUrlList(listId)
		};
		const auto listNamespace{
			this->getUrlListNamespace(listId)
		};

		// delete parsing tables
		auto tables{this->getTargetTables("parsed", listId)};

		while(!tables.empty()) {
			this->deleteTargetTable("parsed", tables.front().first);

			tables.pop();
		}

		// delete extracting tables
		this->getTargetTables("extracted", listId).swap(tables);

		while(!tables.empty()) {
			this->deleteTargetTable("extracted", tables.front().first);

			tables.pop();
			}

		// delete analyzing tables
		this->getTargetTables("analyzed", listId).swap(tables);

		while(!tables.empty()) {
			this->deleteTargetTable("analyzed", tables.front().first);

			tables.pop();
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for deleting URL list
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
					"DELETE FROM `crawlserv_urllists`"
					" WHERE id = ?"
					" LIMIT 1"
				)
			};

			// execute SQL statement for deleting URL list
			sqlStatement->setUInt64(sqlArg1, listId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::deleteUrlList", e);
		}

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_urllists")) {
			this->resetAutoIncrement("crawlserv_urllists");
		}

		// delete tables
		this->dropTable(
				"crawlserv_"
				+ websiteNamespace.second
				+ "_"
				+ listNamespace
				+ "_crawled"
		);
		this->dropTable(
				"crawlserv_"
				+ websiteNamespace.second
				+ "_"
				+ listNamespace
				+ "_crawling"
		);
		this->dropTable(
				"crawlserv_"
				+ websiteNamespace.second
				+ "_"
				+ listNamespace
				+ "_parsing"
		);
		this->dropTable(
				"crawlserv_"
				+ websiteNamespace.second
				+ "_"
				+ listNamespace
				+ "_extracting"
		);
		this->dropTable(
				"crawlserv_"
				+ websiteNamespace.second
				+ "_"
				+ listNamespace
				+ "_analyzing"
		);
		this->dropTable(
				"crawlserv_"
				+ websiteNamespace.second
				+ "_"
				+ listNamespace
		);
	}

	//! Deletes URLs from a URL list in the database and returns the number of deleted URLs.
	/*!
	 * \param listId The ID of the URL list
	 *   that will be deleted from the
	 *   database.
	 * \param urlIds Reference to a queue
	 *   containing the IDs of the URLs to be
	 *   deleted. It will be cleared while
	 *   deleting the URLs, so that the
	 *   referenced queue will be empty when
	 *   the deletion succeeds.
	 *
	 * \returns The number of URLs deleted
	 *   from the given URL list in the
	 *   database. It might differ from the
	 *   number of URLs given, as duplicates
	 *   and non-existing URLs will not be
	 *   counted.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while deleting the URLs
	 *   from the given URL list in the
	 *   database.
	 */
	std::size_t Database::deleteUrls(
			std::uint64_t listId,
			std::queue<std::uint64_t>& urlIds
	) {
		// check arguments
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::deleteUrlList():"
					" No URL list ID specified"
			);
		}

		if(urlIds.empty()) {
			return 0;
		}

		// get website namespace and URL list name
		const auto websiteNamespace{
			this->getWebsiteNamespaceFromUrlList(listId)
		};
		const auto listNamespace{
			this->getUrlListNamespace(listId)
		};

		// get maximum length of SQL query
		const auto maxLength{this->getMaxAllowedPacketSize()};

		// check connection
		this->checkConnection();

		// the number of additional letters in the MySQL query per URL (used for " id=")
		constexpr auto numAddLettersPerUrl{4};

		std::size_t result{0};

		try {
			while(!urlIds.empty()) {
				// create SQL query
				std::string sqlQuery{
					"DELETE FROM `crawlserv_"
					+ websiteNamespace.second
					+ "_"
					+ listNamespace
					+ "`"
					" WHERE"
				};

				// add maximum possible number of URLs to the SQL query
				while(true) {
					// check whether there are more IDs to process
					if(urlIds.empty()) {
						break;
					}

					// convert ID to string
					const auto idString{std::to_string(urlIds.front())};

					// check whether maximum length of SQL query will be exceeded
					if(
							sqlQuery.length()
							+ idString.length()
							+ numAddLettersPerUrl
							>= maxLength
					) {
						break;
					}

					// add ID to SQL query
					sqlQuery += " id=";
					sqlQuery += idString;
					sqlQuery += " OR";

					// remove ID from queue
					urlIds.pop();
				}

				// remove last three letters (" OR") from SQL query
				sqlQuery.pop_back();
				sqlQuery.pop_back();
				sqlQuery.pop_back();

				// execute SQL query and get number of deleted URLs
				const auto removed{this->executeUpdate(sqlQuery)};

				if(removed > 0) {
					result += removed;
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::deleteUrls", e);
		}

		return result;
	}

	//! Resets the parsing status of all URLs in a URL list in the database.
	/*!
	 * \param listId The ID of the URL list
	 *   for which the parsing status will be
	 *   reset.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while resetting the
	 *   parsing state of all URLs in the given
	 *   URL list.
	 */
	void Database::resetParsingStatus(std::uint64_t listId) {
		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::resetParsingStatus():"
					" No URL list ID specified"
			);
		}

		// get website namespace and URL list name
		const auto websiteNamespace{
			this->getWebsiteNamespaceFromUrlList(listId)
		};
		const auto listNamespace{
			this->getUrlListNamespace(listId)
		};

		// check connection
		this->checkConnection();

		try {
			// update parsing table
			this->execute(
					"UPDATE `crawlserv_"
					+ websiteNamespace.second
					+ "_"
					+ listNamespace
					+ "_parsing`"
					" SET success = FALSE, locktime = NULL"
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::resetParsingStatus", e);
		}
	}

	//! Resets the extracting status of all URLs in a URL list in the database.
	/*!
	 * \param listId The ID of the URL list
	 *   for which the extracting status will be
	 *   reset.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while resetting the
	 *   extracting state of all URLs in the
	 *   given URL list.
	 */
	void Database::resetExtractingStatus(std::uint64_t listId) {
		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::resetExtractingStatus():"
					" No URL list ID specified"
			);
		}

		// get website namespace and URL list name
		const auto websiteNamespace{
			this->getWebsiteNamespaceFromUrlList(listId)
		};
		const auto listNamespace{
			this->getUrlListNamespace(listId)
		};

		// check connection
		this->checkConnection();

		try {
			// update extracting table
			this->execute(
					"UPDATE `crawlserv_"
					+ websiteNamespace.second
					+ "_"
					+ listNamespace
					+ "_extracting`"
					" SET success = FALSE, locktime = NULL"
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::resetExtractingStatus", e);
		}
	}

	//! Resets the analyzing status of all URLs in a URL list in the database.
	/*!
	 * \param listId The ID of the URL list
	 *   for which the analyzing status will be
	 *   reset.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while resetting the
	 *   analyzing state of all URLs in the
	 *   given URL list.
	 */
	void Database::resetAnalyzingStatus(std::uint64_t listId) {
		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::resetAnalyzingStatus():"
					" No URL list ID specified"
			);
		}

		// get website namespace and URL list name
		const auto websiteNamespace{
			this->getWebsiteNamespaceFromUrlList(listId)
		};
		const auto listNamespace{
			this->getUrlListNamespace(listId)
		};

		// check connection
		this->checkConnection();

		try {
			// update URL list
			this->execute(
					"UPDATE `crawlserv_"
					+ websiteNamespace.second
					+ "_"
					+ listNamespace
					+ "_analyzing`"
					" SET success = FALSE, locktime = NULL"
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::resetAnalyzingStatus", e);
		}
	}

	/*
	 * QUERY FUNCTIONS
	 */

	//! Adds a new query to the database and returns its ID.
	/*!
	 * \param websiteId The ID of the website
	 *   to which the new query will be added,
	 *   or zero for adding a global query to
	 *   the database.
	 * \param queryProperties Constant
	 *   reference to the properties of the new
	 *   query.
	 *
	 * \returns A unique ID identifying the new
	 *   query in the database.
	 *
	 * \throws Main::Database::Exception if no query
	 *   name, text, or type has been specfied
	 *   in the given properties of the new
	 *   query.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the new
	 *   query to the database.
	 */
	std::uint64_t Database::addQuery(std::uint64_t websiteId, const QueryProperties& queryProperties) {
		std::uint64_t newId{0};

		// check arguments
		if(queryProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::addQuery():"
					" No query name specified"
			);
		}

		if(queryProperties.text.empty()) {
			throw Database::Exception(
					"Main::Database::addQuery():"
					" No query text specified"
			);
		}

		if(queryProperties.type.empty()) {
			throw Database::Exception(
					"Main::Database::addQuery():"
					" No query type specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for adding query
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"INSERT INTO crawlserv_queries"
						" ("
							" website,"
							" name,"
							" query,"
							" type,"
							" resultbool,"
							" resultsingle,"
							" resultmulti,"
							" resultsubsets,"
							" textonly"
						" )"
						" VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"
				)
			};

			// execute SQL query for adding query
			if(websiteId > 0) {
				sqlStatement->setUInt64(sqlArg1, websiteId);
			}
			else {
				sqlStatement->setNull(sqlArg1, 0);
			}

			sqlStatement->setString(sqlArg2, queryProperties.name);
			sqlStatement->setString(sqlArg3, queryProperties.text);
			sqlStatement->setString(sqlArg4, queryProperties.type);
			sqlStatement->setBoolean(sqlArg5, queryProperties.resultBool);
			sqlStatement->setBoolean(sqlArg6, queryProperties.resultSingle);
			sqlStatement->setBoolean(sqlArg7, queryProperties.resultMulti);
			sqlStatement->setBoolean(sqlArg8, queryProperties.resultSubSets);
			sqlStatement->setBoolean(sqlArg9, queryProperties.textOnly);

			Database::sqlExecute(sqlStatement);

			// get ID
			newId = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::addQuery", e);
		}

		return newId;
	}

	//! Gets the properties of a query from the database.
	/*!
	 * \param queryId The ID of the query for
	 *   which the properties will be retrieved
	 *   from the database.
	 * \param queryPropertiesTo Reference to the
	 *   structure to which the retrieved
	 *   properties of the query will be
	 *   written.
	 *
	 * \throws Main::Database::Exception if no query
	 *   ID has been specfied, i.e. the query
	 *   ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   properties of the given query from the
	 *   database.
	 */
	void Database::getQueryProperties(std::uint64_t queryId, QueryProperties& queryPropertiesTo) {
		// check argument
		if(queryId == 0) {
			throw Database::Exception(
					"Main::Database::getQueryProperties():"
					" No query ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT name,"
						" query,"
						" type,"
						" resultbool,"
						" resultsingle,"
						" resultmulti,"
						" resultsubsets,"
						" textonly"
						" FROM `crawlserv_queries`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, queryId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				queryPropertiesTo = QueryProperties(
					sqlResultSet->getString("name"),
					sqlResultSet->getString("query"),
					sqlResultSet->getString("type"),
					sqlResultSet->getBoolean("resultbool"),
					sqlResultSet->getBoolean("resultsingle"),
					sqlResultSet->getBoolean("resultmulti"),
					sqlResultSet->getBoolean("resultsubsets"),
					sqlResultSet->getBoolean("textonly")
				);
			}
			else {
				queryPropertiesTo = QueryProperties();
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getQueryProperties", e);
		}
	}

	//! Updates a query in the database.
	/*!
	 * Replaces the current query properties
	 *  with the given ones.
	 *
	 * \param queryId The ID of the query that
	 *   will be updated in the database.
	 * \param queryProperties A constant
	 *   reference to the new query
	 *   properties.
	 *
	 * \throws Main::Database::Exception if no query
	 *   has been specified, i.e. the query ID
	 *   is zero, or if no name, namespace or
	 *   text has been specified in the
	 *   provided query properties.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the
	 *   query in the database.
	 */
	void Database::updateQuery(std::uint64_t queryId, const QueryProperties& queryProperties) {
		// check arguments
		if(queryId == 0) {
			throw Database::Exception(
					"Main::Database::updateQuery():"
					" No query ID specified"
			);
		}

		if(queryProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::updateQuery():"
					" No query name specified"
			);
		}

		if(queryProperties.text.empty()) {
			throw Database::Exception(
					"Main::Database::updateQuery():"
					" No query text specified"
			);
		}

		if(queryProperties.type.empty()) {
			throw Database::Exception(
					"Main::Database::updateQuery():"
					" No query type specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for updating
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
					"UPDATE crawlserv_queries"
					" SET"
					" name = ?,"
					" query = ?,"
					" type = ?,"
					" resultbool = ?,"
					" resultsingle = ?,"
					" resultmulti = ?,"
					" resultsubsets = ?,"
					" textonly = ?"
					" WHERE id = ?"
					" LIMIT 1"
				)
			};

			// execute SQL statement for updating
			sqlStatement->setString(sqlArg1, queryProperties.name);
			sqlStatement->setString(sqlArg2, queryProperties.text);
			sqlStatement->setString(sqlArg3, queryProperties.type);
			sqlStatement->setBoolean(sqlArg4, queryProperties.resultBool);
			sqlStatement->setBoolean(sqlArg5, queryProperties.resultSingle);
			sqlStatement->setBoolean(sqlArg6, queryProperties.resultMulti);
			sqlStatement->setBoolean(sqlArg7, queryProperties.resultSubSets);
			sqlStatement->setBoolean(sqlArg8, queryProperties.textOnly);
			sqlStatement->setUInt64(sqlArg9, queryId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::updateQuery", e);
		}
	}

	//! Moves a query to another website in the database.
	/*!
	 * \param queryId The ID of the query
	 *   that will be moved to another website
	 *   in the database.
	 * \param toWebsiteId The ID of the website
	 *   to which the query will be moved in the
	 *   database, or zero to change the query
	 *   into a global query.
	 *
	 * \throws Main::Database::Exception if no query
	 *   has been specified, i.e. the query ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while moving the query in
	 *   the database.
	 */
	void Database::moveQuery(std::uint64_t queryId, std::uint64_t toWebsiteId) {
		// check argument
		if(queryId == 0) {
			throw Database::Exception(
					"Main::Database::moveQuery():"
					" No query ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"UPDATE `crawlserv_queries`"
						" SET website = ?"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, toWebsiteId);
			sqlStatement->setUInt64(sqlArg2, queryId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::moveQuery", e);
		}
	}

	//! Deletes a query from the database.
	/*!
	 * \param queryId The ID of the query
	 *   that will be deleted from the
	 *   database.
	 *
	 * \throws Main::Database::Exception if no query
	 *   has been specified, i.e. the query ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while deleting the query
	 *   from the database.
	 */
	void Database::deleteQuery(std::uint64_t queryId) {
		// check argument
		if(queryId == 0) {
			throw Database::Exception(
					"Main::Database::deleteQuery():"
					" No query ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"DELETE FROM `crawlserv_queries`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, queryId);

			Database::sqlExecute(sqlStatement);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_queries")) {
				this->resetAutoIncrement("crawlserv_queries");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::deleteQuery", e);
		}
	}

	//! Duplicates a query in the database.
	/*!
	 * \param queryId The ID of the query
	 *   that will be duplicated in the
	 *   database.
	 *
	 * \throws Main::Database::Exception if no query
	 *   has been specified, i.e. the query ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while duplicating the
	 *   query in the database.
	 */
	std::uint64_t Database::duplicateQuery(std::uint64_t queryId) {
		std::uint64_t newId{0};

		// check argument
		if(queryId == 0) {
			throw Database::Exception(
					"Main::Database::duplicateQuery():"
					" No query ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for getting query info
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
					"SELECT"
					" website,"
					" name,"
					" query,"
					" type,"
					" resultbool,"
					" resultsingle,"
					" resultmulti,"
					" resultsubsets,"
					" textonly"
					" FROM `crawlserv_queries`"
					" WHERE id = ?"
					" LIMIT 1"
				)
			};

			// execute SQL statement for getting query info
			sqlStatement->setUInt64(sqlArg1, queryId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				// add query
				newId = this->addQuery(
						sqlResultSet->getUInt64("website"),
						QueryProperties(
								sqlResultSet->getString("name") + " (copy)",
								sqlResultSet->getString("query"),
								sqlResultSet->getString("type"),
								sqlResultSet->getBoolean("resultbool"),
								sqlResultSet->getBoolean("resultsingle"),
								sqlResultSet->getBoolean("resultmulti"),
								sqlResultSet->getBoolean("resultsubsets"),
								sqlResultSet->getBoolean("textonly")
						)
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::duplicateQuery", e);
		}

		return newId;
	}

	/*
	 * CONFIGURATION FUNCTIONS
	 */

	//! Adds a new configuration to the database and returns its ID.
	/*!
	 * \param websiteId The ID of the website
	 *   to which the new query will be added,
	 *   or zero for adding a global
	 *   configuration to the database.
	 * \param configProperties Constant
	 *   reference to the properties of the new
	 *   configuration.
	 *
	 * \returns A unique ID identifying the new
	 *   configuration in the database.
	 *
	 * \throws Main::Database::Exception if no module,
	 *   name, or configuration has been
	 *   specfied in the given properties of the
	 *   new configuration.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the new
	 *   configuration to the database.
	 */
	std::uint64_t Database::addConfiguration(
			std::uint64_t websiteId,
			const ConfigProperties& configProperties
	) {
		std::uint64_t newId{0};

		// check arguments
		if(configProperties.module.empty()) {
			throw Database::Exception(
					"Main::Database::addConfiguration():"
					" No configuration module specified"
			);
		}

		if(configProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::addConfiguration():"
					" No configuration name specified"
			);
		}

		if(configProperties.config.empty()) {
			throw Database::Exception(
					"Main::Database::addConfiguration():"
					" No configuration specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for adding configuration
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"INSERT INTO"
						" `crawlserv_configs`(website, module, name, config)"
						" VALUES (?, ?, ?, ?)"
				)
			};

			// execute SQL query for adding website
			sqlStatement->setUInt64(sqlArg1, websiteId);
			sqlStatement->setString(sqlArg2, configProperties.module);
			sqlStatement->setString(sqlArg3, configProperties.name);
			sqlStatement->setString(sqlArg4, configProperties.config);

			Database::sqlExecute(sqlStatement);

			// get ID
			newId = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::addConfiguration", e);
		}

		return newId;
	}

	//! Gets a configuration from the database.
	/*!
	 * \param configId The ID of the
	 *   configuration to be retrieved from the
	 *   database.
	 *
	 * \returns A copy of the configuration's
	 *   JSON string as stored in the database,
	 *   or an empty string if the given
	 *   configuration does not exist in the
	 *   database.
	 *
	 * \throws Main::Database::Exception if no
	 *   configuration has been specified, i.e.
	 *   the configuration ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   configuration from the database.
	 */
	std::string Database::getConfiguration(std::uint64_t configId) {
		std::string result;

		// check argument
		if(configId == 0) {
			throw Database::Exception(
					"Main::Database::getConfiguration():"
					" No configuration ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT config"
						" FROM `crawlserv_configs`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, configId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getString("config");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getConfiguration", e);
		}

		return result;
	}

	//! Updates a configuration in the database.
	/*!
	 * \note The name of the module will not be
	 *   updated.
	 *
	 * \param configId The ID of the
	 *   configuration to be updated in the
	 *   database.
	 * \param configProperties Constant
	 *   reference to the new properties of the
	 *   configuration. The name of the module
	 *   specified in these properties will be
	 *   ignored.
	 *
	 * \throws Main::Database::Exception if no
	 *   configuration has been specified, i.e.
	 *   the configuration ID is zero, or if no
	 *   name or configuration has been
	 *   specified in the given properties of
	 *   the configuration.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the
	 *   configuration in the database.
	 */

	// update configuration in the database (NOTE: module will not be updated!), throws Database::Exception
	void Database::updateConfiguration(std::uint64_t configId, const ConfigProperties& configProperties) {
		// check arguments
		if(configId == 0) {
			throw Database::Exception(
					"Main::Database::updateConfiguration():"
					" No configuration ID specified"
			);
		}

		if(configProperties.name.empty()) {
			throw Database::Exception(
					"Main::Database::updateConfiguration():"
					" No configuration name specified"
			);
		}

		if(configProperties.config.empty()) {
			throw Database::Exception(
					"Main::Database::updateConfiguration():"
					" No configuration specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for updating
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"UPDATE crawlserv_configs"
						" SET name = ?,"
						" config = ?"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement for updating
			sqlStatement->setString(sqlArg1, configProperties.name);
			sqlStatement->setString(sqlArg2, configProperties.config);
			sqlStatement->setUInt64(sqlArg3, configId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::updateConfiguration", e);
		}
	}

	//! Deletes a configuration from the database.
	/*!
	 * \param configId The ID of the
	 *   configuration that will be deleted
	 *   from the database.
	 *
	 * \throws Main::Database::Exception if no
	 *   configuration has been specified, i.e.
	 *   the configuration ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while deleting the
	 *   configuration from the database.
	 */
	void Database::deleteConfiguration(std::uint64_t configId) {
		// check argument
		if(configId == 0) {
			throw Database::Exception(
					"Main::Database::deleteConfiguration():"
					" No configuration ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"DELETE FROM `crawlserv_configs`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, configId);

			Database::sqlExecute(sqlStatement);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_configs")) {
				this->resetAutoIncrement("crawlserv_configs");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::deleteConfiguration", e);
		}
	}

	//! Duplicates a configuration in the database.
	/*!
	 * \param configId The ID of the
	 *   configuration that will be duplicated
	 *   in the database.
	 *
	 * \throws Main::Database::Exception if no
	 *   configuration has been specified, i.e.
	 *   the configuration ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while duplicating the
	 *   configuration in the database.
	 */
	std::uint64_t Database::duplicateConfiguration(std::uint64_t configId) {
		std::uint64_t newId{0};

		// check argument
		if(configId == 0) {
			throw Database::Exception(
					"Main::Database::duplicateConfiguration():"
					" No configuration ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for getting configuration info
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT website, module, name, config"
						" FROM `crawlserv_configs`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement for getting configuration info
			sqlStatement->setUInt64(sqlArg1, configId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				// add configuration
				newId = this->addConfiguration(
						sqlResultSet->getUInt64("website"),
						ConfigProperties(
								sqlResultSet->getString("module"),
								sqlResultSet->getString("name") + " (copy)",
								sqlResultSet->getString("config")
						)
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::duplicateConfiguration", e);
		}

		return newId;
	}

	/*
	 * TARGET TABLE FUNCTIONS
	 */

	//! Adds a new target table or updates an existing target table in the database.
	/*!
	 * \param properties Constant reference to
	 *   the properties of the new target table,
	 *   or the existing target table to be
	 *   updated.
	 *
	 * \returns If no target table with the
	 *   specified type and name already exists,
	 *   a unique ID identifying the new target
	 *   table in the database. The ID of the
	 *   new table is, however, only unique
	 *   among all target tables of the same
	 *   type. If a target table with the
	 *   specified type and name already exists,
	 *   its ID will be returned instead.
	 *
	 * \throws Main::Database::Exception if no type,
	 *   website, URL list, name, or columns
	 *   have been specfied in the given
	 *   properties of the new target table, or
	 *   if a column of the already existing
	 *   target table cannot be overwritten due
	 *   to incompatibilities between the
	 *   respective data types.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the new
	 *   target table, or updating the existing
	 *   target table in the database.
	 */
	std::uint64_t Database::addTargetTable(const TargetTableProperties& properties) {
		std::uint64_t newId{0};

		// check arguments
		if(properties.type.empty()) {
			throw Database::Exception(
					"Main::Database::addTargetTable():"
					" No table type specified"
			);
		}

		if(properties.website == 0) {
			throw Database::Exception(
					"Main::Database::addTargetTable():"
					" No website ID specified"
			);
		}

		if(properties.urlList == 0) {
			throw Database::Exception(
					"Main::Database::addTargetTable():"
					" No URL list ID specified"
			);
		}

		if(properties.name.empty()) {
			throw Database::Exception(
					"Main::Database::addTargetTable():"
					" No table name specified"
			);
		}

		if(properties.columns.empty()) {
			throw Database::Exception(
					"Main::Database::addTargetTable():"
					" No columns specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// check whether table exists
			if(this->isTableExists(properties.fullName)) {
				// table exists already: add columns that do not exist yet and check columns that do exist
				for(const auto& column : properties.columns) {
					if(!column.name.empty()) {
						if(this->isColumnExists(properties.fullName, column.name)) {
							// column does exist: check types (does not check specifiers like 'UNSIGNED'!)
							std::string columnType(
									column.type,
									0,
									column.type.find(' ')
							);

							auto targetType{
								this->getColumnType(
										properties.fullName,
										column.name
								)
							};

							// convert both column type and target type to lower case
							std::transform(
									columnType.begin(),
									columnType.end(),
									columnType.begin(),
									::tolower
							);
							std::transform(
									targetType.begin(),
									targetType.end(),
									targetType.begin(),
									::tolower
							);

							if(columnType != targetType) {
								std::string exceptionString{
									"Main::Database::addTargetTable():"
									" Cannot overwrite column of type '"
								};

								exceptionString += columnType;
								exceptionString += "' with data of type '";
								exceptionString += targetType;
								exceptionString += "'";

								throw Database::Exception(exceptionString);
							}
						}
						else {
							// column does not exist: add column
							this->addColumn(
									properties.fullName,
									TableColumn(
											column,
											column.name
									)
							);
						}
					}
				}

				// compress table if necessary
				if(properties.compressed) {
					this->compressTable(properties.fullName);
				}
			}
			else {
				// table does not exist: get data directory and create table
				const auto dataDirectory{this->getWebsiteDataDirectory(properties.website)};

				this->createTable(
						TableProperties(
								properties.fullName,
								properties.columns,
								dataDirectory,
								properties.compressed
						)
				);
			}

			// prepare SQL statement for checking for entry
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT id"
						" FROM `crawlserv_"
						+ properties.type
						+ "tables`"
						" USE INDEX(urllist)"
						" WHERE website = ?"
						" AND urllist = ?"
						" AND name LIKE ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement for checking for entry
			sqlStatement->setUInt64(sqlArg1, properties.website);
			sqlStatement->setUInt64(sqlArg2, properties.urlList);
			sqlStatement->setString(sqlArg3, properties.name);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			if(sqlResultSet && sqlResultSet->next()) {
				// entry exists: return ID
				newId = sqlResultSet->getUInt64("id");
			}
			else {
				// entry does not exist already: prepare SQL statement for adding table
				sqlStatement.reset(
						this->connection->prepareStatement(
								"INSERT INTO `crawlserv_"
								+ properties.type
								+ "tables`(website, urllist, name)"
								" VALUES (?, ?, ?)"
						)
				);

				// execute SQL statement for adding table
				sqlStatement->setUInt64(sqlArg1, properties.website);
				sqlStatement->setUInt64(sqlArg2, properties.urlList);
				sqlStatement->setString(sqlArg3, properties.name);

				Database::sqlExecute(sqlStatement);

				// return ID of newly inserted table
				newId = this->getLastInsertedId();
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::addTargetTable", e);
		}

		return newId;
	}

	//! Gets the target tables of the specified type for a URL list from the database.
	/*!
	 * \param type Constant reference to a
	 *   string containing the type of the
	 *   target tables to retrieve.
	 * \param listId The ID of the URL list
	 *   for which to retrieve the target
	 *   tables.
	 *
	 * \returns A queue containing the IDs and
	 *   names of the target tables of the given
	 *   type for the specified URL list.
	 *
	 * \throws Main::Database::Exception if no target
	 *   table has been specified, i.e. the
	 *   string containing the type is empty or
	 *   the target table ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   target tables from the database.
	 */
	std::queue<Database::IdString> Database::getTargetTables(
			const std::string& type,
			std::uint64_t listId
	) {
		std::queue<IdString> result;

		// check arguments
		if(type.empty()) {
			throw Database::Exception(
					"Main::Database::getTargetTables():"
					" No table type specified"
			);
		}

		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::getTargetTables():"
					" No URL list ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT id, name"
						" FROM `crawlserv_"
						+ type
						+ "tables`"
						" WHERE urllist = ?"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, listId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get results
			if(sqlResultSet) {
				while(sqlResultSet->next()) {
					result.emplace(
							sqlResultSet->getUInt64("id"),
							sqlResultSet->getString("name")
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getTargetTables", e);
		}

		return result;
	}

	//! Gets the ID of a target table from the database.
	/*!
	 * \param type Constant reference to a
	 *   string containing the type of the
	 *   target table for which to retrieve
	 *   its ID.
	 * \param listId The ID of the URL list
	 *   associated with the target table
	 *   for which to retrieve its ID.
	 * \param tableName Const reference to
	 *   a string containing the name of
	 *   the target table for which to
	 *   retrieve its ID.
	 *
	 * \returns The ID of the specified target
	 *   table as stored in the database.
	 *
	 * \throws Main::Database::Exception if no target
	 *   table or URL list has been specified,
	 *   i.e. if the string containing the type
	 *   is empty, or the target table or the
	 *   URL list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the ID
	 *   of the target table from the database.
	 */
	std::uint64_t Database::getTargetTableId(
			const std::string& type,
			std::uint64_t listId,
			const std::string& tableName
	) {
		std::uint64_t result{0};

		// check arguments
		if(type.empty()) {
			throw Database::Exception(
					"Main::Database::getTargetTableId():"
					" No table type specified"
			);
		}

		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::getTargetTableId():"
					" No URL list ID specified"
			);
		}

		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::getTargetTableId():"
					" No table name specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT id"
						" FROM `crawlserv_"
						+ type
						+ "tables`"
						" WHERE urllist = ?"
						" AND name LIKE ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, listId);
			sqlStatement->setString(sqlArg2, tableName);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("id");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getTargetTableId", e);
		}

		return result;
	}

	//! Gets the name of a target table from the database.
	/*!
	 * \param type Constant reference to a
	 *   string containing the type of the
	 *   target table for which to retrieve
	 *   its name.
	 * \param tableId The ID of the target
	 *   table for which to retrieve its
	 *   name.
	 *
	 * \returns A copy of the name of the
	 *   specified target table as stored
	 *   in the database.
	 *
	 * \throws Main::Database::Exception if no target
	 *   table has been specified, i.e. the
	 *   string containing the type is empty or
	 *   the target table ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   name of the target table from the
	 *   database.
	 */
	std::string Database::getTargetTableName(const std::string& type, std::uint64_t tableId) {
		std::string result;

		// check arguments
		if(type.empty()) {
			throw Database::Exception(
					"Main::Database::getTargetTableName():"
					" No table type specified"
			);
		}

		if(tableId == 0) {
			throw Database::Exception(
					"Main::Database::getTargetTableName():"
					" No table ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT name"
						" FROM `crawlserv_"
						+ type
						+ "tables`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, tableId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getString("name");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getTargetTableName", e);
		}

		return result;
	}

	//! Deletes a target table from the database.
	/*!
	 * \param type Constant reference to a
	 *   string containing the type of the
	 *   target table to be deleted
	 * \param tableId The ID of the target
	 *   table to be deleted.
	 *
	 * \throws Main::Database::Exception if no target
	 *   table has been specified, i.e. the
	 *   string containing the type is empty or
	 *   the target table ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while deleting the target
	 *   table from the database.
	 */
	void Database::deleteTargetTable(const std::string& type, std::uint64_t tableId) {
		// check arguments
		if(type.empty()) {
			throw Database::Exception(
					"Main::Database::deleteTargetTable():"
					" No table type specified"
			);
		}

		if(tableId == 0) {
			throw Database::Exception(
					"Main::Database::deleteTargetTable():"
					" No table ID specified"
			);
		}

		// get namespace, URL list name and table name
		const auto websiteNamespace{this->getWebsiteNamespaceFromTargetTable(type, tableId)};
		const auto listNamespace{this->getUrlListNamespaceFromTargetTable(type, tableId)};
		const auto tableName{this->getTargetTableName(type, tableId)};
		const std::string metaTableName{"crawlserv_" + type + "tables"};

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for deletion
			SqlPreparedStatementPtr deleteStatement{
				this->connection->prepareStatement(
						"DELETE FROM `"
						+ metaTableName
						+ "` WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement for deletion
			deleteStatement->setUInt64(sqlArg1, tableId);

			Database::sqlExecute(deleteStatement);

			// create SQL statement for dropping table
			SqlStatementPtr dropStatement{
				this->connection->createStatement()
			};

			// execute SQL query for dropping table
			Database::sqlExecute(
					dropStatement,
					"DROP TABLE IF EXISTS `crawlserv_"
					+ websiteNamespace.second
					+ "_"
					+ listNamespace.second
					+ "_"
					+ type
					+ "_"
					+ tableName
					+ "`"
			);

			// reset auto-increment if table is empty
			if(this->isTableEmpty(metaTableName)) {
				this->resetAutoIncrement(metaTableName);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::deleteTargetTable", e);
		}
	}

	/*
	 * VALIDATION FUNCTIONS
	 */

	//! Checks whether the connection to the database is still valid and tries to reconnect if necessary.
	/*!
	 * \warning Afterwards, old references to
	 *   prepared SQL statements might be
	 *   invalid, because the connection to
	 *   the database might have been reset.
	 *
	 * \throws Main::Database::Exception if the MySQL
	 *   driver is not initialized.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while attempting to
	 *   reconnect to the database.
	 */
	void Database::checkConnection() {
		std::uint64_t milliseconds{0};

		// check driver
		if(Database::driver == nullptr) {
			throw Database::Exception(
					"Main::Database::checkConnection():"
					"MySQL driver not initialized"
			);
		}

		try {
			// check connection
			if(this->connection) {
				// check whether re-connect should be performed anyway
				milliseconds = this->reconnectTimer.tick();

				if(milliseconds < reconnectAfterIdleMs) {
					// check whether connection is valid
					if(this->connection->isValid()) {
						return;
					}

					milliseconds = 0;
				}

				// try to re-connect
				if(!(this->connection->reconnect())) {
					// simple re-connect failed:
					//	try to reset connection after sleeping over it for some time
					this->connection.reset();

					try {
						this->connect();
					}
					catch(const Database::Exception& e) {
						if(this->sleepOnError > 0) {
							std::this_thread::sleep_for(
									std::chrono::seconds(
											this->sleepOnError
									)
							);
						}

						this->connect();
					}
				}
			}
			else {
				this->connect();
			}

			// recover prepared SQL statements
			for(auto& preparedStatement : this->preparedStatements) {
				preparedStatement.refresh(this->connection.get());
			}

			// log re-connect on idle if necessary
			if(milliseconds > 0) {
				this->log(
						"re-connected to database after idling for "
						+ Helper::DateTime::secondsToString(
								milliseconds / secToMs
						)
						+ "."
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::checkConnection", e);
		}
	}

	//! Checks whether a website ID is valid.
	/*!
	 * \param websiteId The website ID to be
	 *   checked for in the database.
	 *
	 * \returns True, if a website with the
	 *   given ID exists in the database.
	 *   False otherwise.
	 *
	 * \throws Main::Database::Exception if no website
	 *   has been specified, i.e. the website ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   validity of the given website ID in the
	 *   database.
	 */
	bool Database::isWebsite(std::uint64_t websiteId) {
		bool result{false};

		// check argument
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::isWebsite():"
					" No website ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_websites`"
							" WHERE id = ?"
						" )"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isWebsite", e);
		}

		return result;
	}

	//! Checks whether a URL list ID is valid.
	/*!
	 * \param urlListId The URL list ID to
	 *   be checked for in the database.
	 *
	 * \returns True, if a URL list with the
	 *   given ID exists in the database.
	 *   False otherwise.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   validity of the given URL list ID in the
	 *   database.
	 */
	bool Database::isUrlList(std::uint64_t urlListId) {
		bool result{false};

		// check argument
		if(urlListId == 0) {
			throw Database::Exception(
					"Main::Database::isUrlList():"
					" No URL list ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_urllists`"
							" WHERE id = ?"
						" )"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, urlListId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isUrlList", e);
		}

		return result;
	}

	//! Checks whether a URL list ID is valid for a specific website.
	/*!
	 * \param websiteId The ID of the website
	 *   to which the URL list should belong.
	 * \param urlListId The URL list ID to
	 *   be checked for in the database.
	 *
	 * \returns True, if a URL list with the
	 *   given ID exists in the database and
	 *   belongs to the specified website.
	 *   False otherwise.
	 *
	 * \throws Main::Database::Exception if no website
	 *   or no URL list has been specified, i.e.
	 *   the website ID or the URL list ID is
	 *   zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   validity of the given URL list ID in the
	 *   database.
	 */
	bool Database::isUrlList(std::uint64_t websiteId, std::uint64_t urlListId) {
		bool result{false};

		// check arguments
		if(websiteId == 0) {
			throw Database::Exception(
					"Main::Database::isUrlList():"
					" No website ID specified"
			);
		}

		if(urlListId == 0) {
			throw Database::Exception(
					"Main::Database::isUrlList():"
					" No URL list ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_urllists`"
							" WHERE website = ?"
							" AND id = ?"
						")"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);
			sqlStatement->setUInt64(sqlArg2, urlListId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isUrlList", e);
		}

		return result;
	}

	//! Checks whether a query ID is valid.
	/*!
	 * \param queryId The query ID to be
	 *   checked for in the database.
	 *
	 * \returns True, if a query with the
	 *   given ID exists in the database.
	 *   False otherwise.
	 *
	 * \throws Main::Database::Exception if no query
	 *   has been specified, i.e. the query ID
	 *   is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   validity of the given query ID in the
	 *   database.
	 */
	bool Database::isQuery(std::uint64_t queryId) {
		bool result{false};

		// check argument
		if(queryId == 0) {
			throw Database::Exception(
					"Main::Database::isQuery():"
					" No query ID specified"

			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_queries`"
							" WHERE id = ?"
						" )"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, queryId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isQuery", e);
		}

		return result;
	}

	//! Checks whether a query ID is valid for a specific website.
	/*!
	 * \param websiteId The ID of the website
	 *   to which the query should belong.
	 * \param queryId The query ID to
	 *   be checked for in the database.
	 *
	 * \returns True, if a query with the given
	 *   ID exists in the database and belongs
	 *   to the specified website. False
	 *   otherwise.
	 *
	 * \throws Main::Database::Exception if no website
	 *   or no query has been specified, i.e.
	 *   the website ID or the query ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   validity of the given query ID in the
	 *   database.
	 */
	bool Database::isQuery(std::uint64_t websiteId, std::uint64_t queryId) {
		bool result{false};

		// check arguments
		if(queryId == 0) {
			throw Database::Exception(
					"Main::Database::isQuery():"
					" No query ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_queries`"
							" WHERE"
							" ("
								" website = ?"
								" OR website IS NULL"
							" )"
							" AND id = ?"
						" )"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);
			sqlStatement->setUInt64(sqlArg2, queryId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isQuery", e);
		}

		return result;
	}

	//! Checks whether a configuration ID is valid.
	/*!
	 * \param configId The configuration ID to
	 *   be checked for in the database.
	 *
	 * \returns True, if a configuration with
	 *   the given ID exists in the database.
	 *   False otherwise.
	 *
	 * \throws Main::Database::Exception if no
	 *   configuration has been specified, i.e.
	 *   the configuration ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   validity of the given configuration ID
	 *   in the database.
	 */
	bool Database::isConfiguration(std::uint64_t configId) {
		bool result{false};

		// check argument
		if(configId == 0) {
			throw Database::Exception(
					"Main::Database::isConfiguration():"
					" No configuration ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_configs`"
							" WHERE id = ?"
						" )"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, configId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isConfiguration", e);
		}

		return result;
	}

	//! Checks whether a configuration ID is valid for a specific website.
	/*!
	 * \param websiteId The ID of the website
	 *   to which the configuration should
	 *   belong.
	 * \param configId The configuration ID to
	 *   be checked for in the database.
	 *
	 * \returns True, if a configuration with
	 *   the given ID exists in the database
	 *   and belongs to the specified website.
	 *   False otherwise.
	 *
	 * \throws Main::Database::Exception if no website
	 *   or no configuration has been specified,
	 *   i.e. the website ID or the
	 *   configuration ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   validity of the given configuration ID
	 *   in the database.
	 */
	bool Database::isConfiguration(std::uint64_t websiteId, std::uint64_t configId) {
		bool result{false};

		// check arguments
		if(configId == 0) {
			throw Database::Exception(
					"Main::Database::isConfiguration():"
					" No configuration ID specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM `crawlserv_configs`"
							" WHERE website = ?"
							" AND id = ?"
						" )"
						" AS result"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, websiteId);
			sqlStatement->setUInt64(sqlArg2, configId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isConfiguration", e);
		}

		return result;
	}

	//! Checks whether a data directory is known to the database.
	/*!
	 * \param dir Constant reference to a string
	 *   containing the data directory to be
	 *   checked.
	 *
	 * \returns True, if the exact data directory
	 *   is known to the database. False otherwise.
	 */
	bool Database::checkDataDir(const std::string& dir) {
		return std::find(this->dirs.cbegin(), this->dirs.cend(), dir) != this->dirs.cend();
	}

	/*
	 * LOCKING FUNCTIONS
	 */

	//! Disables database locking by starting a new SQL transaction.
	/*!
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while starting a new SQL
	 *   transaction in the database.
	 */
	void Database::beginNoLock() {
		try {
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			sqlStatement->execute("SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED");
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::beginNoLock", e);
		}
	}

	//! Re-enables database locking by ending the previous SQL transaction.
	/*!
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while ending the previous
	 *   SQL transaction by committing the
	 *   changes to the database.
	 */
	void Database::endNoLock() {
		try {
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			sqlStatement->execute("COMMIT");
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::endNoLock", e);
		}
	}

	/*
	 * GENERAL TABLE FUNCTIONS
	 */

	//! Checks whether a table in the database is empty.
	/*!
	 * \param tableName Constant reference to
	 *   a string containing the name of the
	 *   table whose contents will be checked
	 *   in the database.
	 *
	 * \returns True, if the given table is empty.
	 *   False if it contains data.
	 *
	 * \throws Main::Database::Exception if no table
	 *   name has been specified, i.e. the
	 *   string containing the name is empty.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   content of the given table in the
	 *   database, e.g. if the table does not
	 *   exist.
	 */
	bool Database::isTableEmpty(const std::string& tableName) {
		bool result{false};

		// check argument
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::isTableEmpty():"
					" No table name specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT NOT EXISTS"
						" ("
								" SELECT *"
								" FROM `" + tableName + "`"
						" ) "
						" AS result"
				)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isTableEmpty", e);
		}

		return result;
	}

	//! Checks whether a table exists in the database.
	/*!
	 * \param tableName Constant reference to
	 *   a string containing the name of the
	 *   table whose existence in the database
	 *   will be checked.
	 *
	 * \returns True, if the given table exists
	 *   in the database. False otherwise.
	 *
	 * \throws Main::Database::Exception if no table
	 *   name has been specified, i.e. the
	 *   string containing the name is empty.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   existence of the given table in the
	 *   database.
	 */
	bool Database::isTableExists(const std::string& tableName) {
		bool result{false};

		// check argument
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::isTableExists():"
					" No table name specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create and execute SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT COUNT(*)"
						" AS result"
						" FROM INFORMATION_SCHEMA.TABLES"
						" WHERE TABLE_SCHEMA LIKE ?"
						" AND TABLE_NAME LIKE ?"
						" LIMIT 1"
				)
			};

			// get result
			sqlStatement->setString(sqlArg1, this->settings.name);
			sqlStatement->setString(sqlArg2, tableName);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isTableExists", e);
		}

		return result;
	}

	//! Checks whether a table in the database contains a specific column.
	/*!
	 * \param tableName Constant reference to
	 *   a string containing the name of the
	 *   table in the database in which the
	 *   existence of the column will be
	 *   checked.
	 * \param columnName Constant reference to
	 *   a string containing the name of the
	 *   column to be checked for in the given
	 *   table.
	 *
	 * \returns True, if the given column exists
	 *   in the specified table. False
	 *   otherwise.
	 *
	 * \throws Main::Database::Exception if no table
	 *   or column name has been specified, i.e.
	 *   one of the strings containing the name
	 *   and the column is empty.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the
	 *   existence of the given column, e.g.
	 *   if the specified table does not exist.
	 */
	bool Database::isColumnExists(const std::string& tableName, const std::string& columnName) {
		bool result{false};

		// check arguments
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::isColumnExists():"
					" No table name specified"
			);
		}

		if(columnName.empty()) {
			throw Database::Exception(
					"Main::Database::isColumnExists():"
					" No column name specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT COUNT(*)"
						" AS result"
						" FROM INFORMATION_SCHEMA.COLUMNS"
						" WHERE TABLE_SCHEMA LIKE ?"
						" AND TABLE_NAME LIKE ?"
						" AND COLUMN_NAME LIKE ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setString(sqlArg1, this->settings.name);
			sqlStatement->setString(sqlArg2, tableName);
			sqlStatement->setString(sqlArg3, columnName);

			// get result
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isColumnExists", e);
		}

		return result;
	}

	//! Gets the type of a specific table column from the database.
	/*!
	 * \param tableName Constant reference to
	 *   a string containing the name of the
	 *   table in the database from which the
	 *   type of the column will be retrieved.
	 * \param columnName Constant reference to
	 *   a string containing the name of the
	 *   column whose type will be retrieved.
	 *
	 * \returns A copy of the name of the given
	 *   table column's data type.
	 *
	 * \throws Main::Database::Exception if no table
	 *   or column name has been specified, i.e.
	 *   one of the strings containing the name
	 *   and the column is empty.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   type of the given column, e.g. if the
	 *   specified table does not exist.
	 */
	std::string Database::getColumnType(const std::string& tableName, const std::string& columnName) {
		std::string result;

		// check arguments
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::getColumnType():"
					" No table name specified"
			);
		}

		if(columnName.empty()) {
			throw Database::Exception(
					"Main::Database::getColumnType():"
					" No column name specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT DATA_TYPE"
						" FROM INFORMATION_SCHEMA.COLUMNS"
						" WHERE TABLE_SCHEMA LIKE ?"
						" AND TABLE_NAME LIKE ?"
						" AND COLUMN_NAME LIKE ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setString(sqlArg1, this->settings.name);
			sqlStatement->setString(sqlArg2, tableName);
			sqlStatement->setString(sqlArg3, columnName);

			// get result
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getString("DATA_TYPE");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getColumnType", e);
		}

		return result;
	}

	//! Locks tables in the database.
	/*!
	 * \param tableLocks Reference to a queue
	 *   of string-bool pairs containing the
	 *   table names and whether to grant write
	 *   access to each table.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while locking the
	 *   given tables, e.g. if another table lock
	 *   is active.
	 */
	void Database::lockTables(std::queue<TableNameWriteAccess>& tableLocks) {
		// check argument
		if(tableLocks.empty()) {
			return;
		}

		// create lock string
		std::string lockString;

		while(!tableLocks.empty()) {
			lockString += "`" + tableLocks.front().first + "` ";

			if(tableLocks.front().second) {
				lockString += "WRITE";
			}
			else {
				lockString += "READ";
			}

			lockString += ", ";

			tableLocks.pop();
		}

		lockString.pop_back();
		lockString.pop_back();

		// execute SQL query
		try {
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			sqlStatement->execute("LOCK TABLES " + lockString);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::lockTables", e);
		}
	}

	//! Unlocks all previously locked tables in the database.
	/*!
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while unlocking all
	 *   previously locked tables.
	 */
	void Database::unlockTables() {
		// execute SQL query
		try {
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			sqlStatement->execute("UNLOCK TABLES");
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::unlockTables", e);
		}
	}

	//! Starts a new transaction with the database using a specific isolation level.
	/*!
	 * \param isolationLevel Constant reference
	 *   to a string containing the isolation
	 *   level to be used, or to an empty string
	 *   if the default isolation level should
	 *   be used.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while starting the SQL
	 *   transaction.
	 */
	void Database::startTransaction(const std::string& isolationLevel) {
		// check connection
		this->checkConnection();

		// execute SQL query
		try {
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			if(!isolationLevel.empty()) {
				sqlStatement->execute(
						"SET TRANSACTION ISOLATION LEVEL "
						+ isolationLevel
				);
			}

			sqlStatement->execute("START TRANSACTION");
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::startTransaction", e);
		}
	}

	//! Ends the current transaction with the database, committing the commands on success.
	/*!
	 * Rolls back the commands if the transaction
	 *  was not successful.
	 *
	 * \param success Specifies whether the
	 *   transaction was successful and
	 *   the commands should be committed.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while starting the SQL
	 *   transaction.
	 */
	void Database::endTransaction(bool success) {
		// check connection
		this->checkConnection();

		// execute SQL query
		try {
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			if(success) {
				sqlStatement->execute("COMMIT");
			}
			else {
				sqlStatement->execute("ROLLBACK");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::endTransaction", e);
		}
	}

	/*
	 * CUSTOM DATA FUNCTIONS FOR ALGORITHMS
	 */

	//! Gets a custom value from one column from a table row in the database.
	/*!
	 * \param data Reference to the data
	 *   structure that identifies the column,
	 *   and to which the result will be
	 *   written.
	 *
	 * \throws Main::Database::Exception if no column
	 *   name or no column type is specified in
	 *   the given data structure, or if an
	 *   invalid data type has been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the data.
	 *
	 * \sa Data::GetValue
	 */
	void Database::getCustomData(Data::GetValue& data) {
		// check argument
		if(data.column.empty()) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No column name specified"
			);
		}

		if(data.type == Data::Type::_unknown) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No column type specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT `"
						+ data.column
						+ "`"
						" FROM `"
						+ data.table
						+ "`"
						" WHERE (" +
						data.condition +
						")"
				)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				if(sqlResultSet->isNull(data.column)) {
					data.value = Data::Value();
				}
				else {
					switch(data.type) {
					case Data::Type::_bool:
						data.value = Data::Value(sqlResultSet->getBoolean(data.column));

						break;

					case Data::Type::_double:
						data.value = Data::Value(
								static_cast<double>(sqlResultSet->getDouble(data.column))
						);

						break;

					case Data::Type::_int32:
						data.value = Data::Value(sqlResultSet->getInt(data.column));

						break;

					case Data::Type::_int64:
						data.value = Data::Value(sqlResultSet->getInt64(data.column));

						break;

					case Data::Type::_string:
						data.value = Data::Value(sqlResultSet->getString(data.column));

						break;

					case Data::Type::_uint32:
						data.value = Data::Value(sqlResultSet->getUInt(data.column));

						break;

					case Data::Type::_uint64:
						data.value = Data::Value(sqlResultSet->getUInt64(data.column));

						break;

					default:
						throw Database::Exception(
								"Main::Database::getCustomData():"
								" Invalid data type when getting custom data."
						);
					}
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getCustomData", e);
		}
	}

	//! Gets custom values from multiple columns of the same type from a table row.
	/*!
	 * \param data Reference to the data
	 *   structure that identifies the columns,
	 *   and to which the result will be
	 *   written.
	 *
	 * \throws Main::Database::Exception if no column
	 *   names or no column type are specified in
	 *   the given data structure, or if an
	 *   invalid data type has been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the data.
	 *
	 * \sa Data::GetFields
	 */
	void Database::getCustomData(Data::GetFields& data) {
		// check argument
		if(data.columns.empty()) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No column names specified"
			);
		}

		if(data.type == Data::Type::_unknown) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No column type specified"
			);
		}

		// clear and reserve memory
		data.values.clear();
		data.values.reserve(data.columns.size());

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{
				this->connection->createStatement()
			};

			// create SQL query
			std::string sqlQuery{"SELECT "};

			for(const auto& column : data.columns) {
				sqlQuery += "`";
				sqlQuery += column;
				sqlQuery += "`, ";
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `";
			sqlQuery += data.table;
			sqlQuery += "` WHERE (";
			sqlQuery += data.condition;
			sqlQuery += ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(
						sqlStatement,
						sqlQuery
				)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				switch(data.type) {
				case Data::Type::_bool:
					for(const auto& column : data.columns) {
						if(sqlResultSet->isNull(column)) {
							data.values.emplace_back();
						}
						else {
							data.values.emplace_back(sqlResultSet->getBoolean(column));
						}
					}

					break;

				case Data::Type::_double:
					for(const auto& column : data.columns) {
						if(sqlResultSet->isNull(column)) {
							data.values.emplace_back();
						}
						else {
							data.values.emplace_back(
									static_cast<double>(sqlResultSet->getDouble(column))
							);
						}
					}

					break;

				case Data::Type::_int32:
					for(const auto& column : data.columns) {
						if(sqlResultSet->isNull(column)) {
							data.values.emplace_back();
						}
						else {
							data.values.emplace_back(sqlResultSet->getInt(column));
						}
					}

					break;

				case Data::Type::_int64:
					for(const auto& column : data.columns) {
						if(sqlResultSet->isNull(column)) {
							data.values.emplace_back();
						}
						else {
							data.values.emplace_back(sqlResultSet->getInt64(column));
						}
					}

					break;

				case Data::Type::_string:
					for(const auto& column : data.columns) {
						if(sqlResultSet->isNull(column)) {
							data.values.emplace_back();
						}
						else {
							data.values.emplace_back(sqlResultSet->getString(column));
						}
					}

					break;

				case Data::Type::_uint32:
					for(const auto& column : data.columns) {
						if(sqlResultSet->isNull(column)) {
							data.values.emplace_back();
						}
						else {
							data.values.emplace_back(sqlResultSet->getUInt(column));
						}
					}

					break;

				case Data::Type::_uint64:
					for(const auto& column : data.columns) {
						if(sqlResultSet->isNull(column)) {
							data.values.emplace_back();
						}
						else {
							data.values.emplace_back(sqlResultSet->getUInt64(column));
						}
					}

					break;

				default:
					throw Database::Exception(
							"Main::Database::getCustomData():"
							" Invalid data type when getting custom data."
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getCustomData", e);
		}
	}

	//! Gets custom values from multiple columns of different types from a table row.
	/*!
	 * \param data Reference to the data
	 *   structure that identifies the columns
	 *   and their types, and to which the
	 *   result will be written.
	 *
	 * \throws Main::Database::Exception if no columns
	 *   are specified in the given data
	 *   structure, or if an invalid data type
	 *   has been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the data.
	 *
	 * \sa Data::GetFieldsMixed
	 */
	void Database::getCustomData(Data::GetFieldsMixed& data) {
		// check argument
		if(data.columns_types.empty()) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No columns specified"
			);
		}

		// clear and reserve memory
		data.values.clear();
		data.values.reserve(data.columns_types.size());

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// create SQL query
			std::string sqlQuery{"SELECT "};

			for(const auto& column_type : data.columns_types) {
				sqlQuery += "`";
				sqlQuery += column_type.first;
				sqlQuery += "`, ";
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `";
			sqlQuery += data.table;
			sqlQuery += "` WHERE (";
			sqlQuery += data.condition;
			sqlQuery += ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement, sqlQuery)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				for(const auto& column_type : data.columns_types) {
					if(sqlResultSet->isNull(column_type.first)) {
						data.values.emplace_back();
					}
					else {
						switch(column_type.second) {
						case Data::Type::_bool:
							data.values.emplace_back(sqlResultSet->getBoolean(column_type.first));

							break;

						case Data::Type::_double:
							data.values.emplace_back(
									static_cast<double>(sqlResultSet->getDouble(column_type.first))
							);

							break;

						case Data::Type::_int32:
							data.values.emplace_back(sqlResultSet->getInt(column_type.first));

							break;

						case Data::Type::_int64:
							data.values.emplace_back(sqlResultSet->getInt64(column_type.first));

							break;

						case Data::Type::_string:
							data.values.emplace_back(sqlResultSet->getString(column_type.first));

							break;

						case Data::Type::_uint32:
							data.values.emplace_back(sqlResultSet->getUInt(column_type.first));

							break;

						case Data::Type::_uint64:
							data.values.emplace_back(sqlResultSet->getUInt64(column_type.first));

							break;

						default:
							throw Database::Exception(
									"Main::Database::getCustomData():"
									" Invalid data type when getting custom data."
							);
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getCustomData", e);
		}
	}

	//! Gets custom values from a table column in the database.
	/*!
	 * \param data Reference to the data
	 *   structure that identifies the column,
	 *   and to which the result will be
	 *   written.
	 *
	 * \throws Main::Database::Exception if no column
	 *   or column type is specified in the
	 *   given data structure, or if an invalid
	 *   data type has been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the data.
	 *
	 * \sa Data::GetColumn
	 */
	void Database::getCustomData(Data::GetColumn& data) {
		// check argument
		if(data.column.empty()) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No column specified"
			);
		}

		if(data.type == Data::Type::_unknown) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No column type specified"
			);
		}

		// clear memory
		data.values.clear();

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// create SQL query
			std::string sqlQuery{
				"SELECT `"
				+ data.column
				+ "` FROM `"
				+ data.table
				+ "`"
			};

			if(!data.condition.empty()) {
				sqlQuery += " WHERE (";
				sqlQuery += data.condition;
				sqlQuery += ")";
			}

			if(!data.order.empty()) {
				sqlQuery += " ORDER BY (";
				sqlQuery += data.order;
				sqlQuery += ")";
			}

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement, sqlQuery)
			};

			// get result
			if(sqlResultSet) {
				data.values.reserve(sqlResultSet->rowsCount());

				while(sqlResultSet->next()) {
					if(sqlResultSet->isNull(data.column)) {
						data.values.emplace_back();
					}
					else {
						switch(data.type) {
						case Data::Type::_bool:
							data.values.emplace_back(sqlResultSet->getBoolean(data.column));

							break;

						case Data::Type::_double:
							data.values.emplace_back(
									static_cast<double>(sqlResultSet->getDouble(data.column))
							);

							break;

						case Data::Type::_int32:
							data.values.emplace_back(sqlResultSet->getInt(data.column));

							break;

						case Data::Type::_int64:
							data.values.emplace_back(sqlResultSet->getInt64(data.column));

							break;

						case Data::Type::_string:
							data.values.emplace_back(sqlResultSet->getString(data.column));

							break;

						case Data::Type::_uint32:
							data.values.emplace_back(sqlResultSet->getUInt(data.column));

							break;

						case Data::Type::_uint64:
							data.values.emplace_back(sqlResultSet->getUInt64(data.column));

							break;

						default:
							throw Database::Exception(
									"Main::Database::getCustomData():"
									" Invalid data type when getting custom data."
							);
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getCustomData", e);
		}
	}

	//! Gets custom values from multiple table columns of the same type.
	/*!
	 * \param data Reference to the data
	 *   structure that identifies the columns,
	 *   and to which the result will be
	 *   written.
	 *
	 * \throws Main::Database::Exception if no column
	 *   or column type is specified in the
	 *   given data structure, or if an invalid
	 *   data type has been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the data.
	 *
	 * \sa Data::GetColumns
	 */
	void Database::getCustomData(Data::GetColumns& data) {
		// check argument
		if(data.columns.empty()) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No column name specified"
			);
		}

		if(data.type == Data::Type::_unknown) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No column type specified"
			);
		}

		// clear and reserve memory
		data.values.clear();

		data.values.reserve(data.columns.size());

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// create SQL query
			std::string sqlQuery{"SELECT "};

			for(const auto& column : data.columns) {
				sqlQuery += "`";
				sqlQuery += column;
				sqlQuery += "`, ";

				// add column to result vector
				data.values.emplace_back();
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `";
			sqlQuery += data.table;
			sqlQuery += "`";

			if(!data.condition.empty()) {
				sqlQuery += " WHERE (";
				sqlQuery += data.condition;
				sqlQuery += ")";
			}

			if(!data.order.empty()) {
				sqlQuery += " ORDER BY (";
				sqlQuery += data.order;
				sqlQuery += ")";
			}

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement, sqlQuery)};

			if(sqlResultSet) {
				// reserve memory for results
				for(auto& value : data.values) {
					value.reserve(sqlResultSet->rowsCount());
				}

				// get results
				while(sqlResultSet->next()) {
					for(auto it{data.columns.cbegin()}; it != data.columns.cend(); ++it) {
						auto& column{data.values.at(it - data.columns.cbegin())};

						if(sqlResultSet->isNull(*it)) {
							column.emplace_back();
						}
						else {
							switch(data.type) {
							case Data::Type::_bool:
								column.emplace_back(sqlResultSet->getBoolean(*it));

								break;

							case Data::Type::_double:
								column.emplace_back(
										static_cast<double>(sqlResultSet->getDouble(*it))
								);

								break;

							case Data::Type::_int32:
								column.emplace_back(sqlResultSet->getInt(*it));

								break;

							case Data::Type::_int64:
								column.emplace_back(sqlResultSet->getInt64(*it));

								break;

							case Data::Type::_string:
								column.emplace_back(sqlResultSet->getString(*it));

								break;

							case Data::Type::_uint32:
								column.emplace_back(sqlResultSet->getUInt(*it));

								break;

							case Data::Type::_uint64:
								column.emplace_back(sqlResultSet->getUInt64(*it));

								break;

							default:
								throw Database::Exception(
										"Main::Database::getCustomData():"
										" Invalid data type when getting custom data."
								);
							}
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getCustomData", e);
		}
	}

	//! Gets custom values from multiple table columns of different types.
	/*!
	 * \param data Reference to the data
	 *   structure that identifies the columns
	 *   and their types, and to which the
	 *   result will be written.
	 *
	 * \throws Main::Database::Exception if no columns
	 *   have been specified in the given data
	 *   structure, or if an invalid data type
	 *   has been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the data.
	 *
	 * \sa Data::GetColumnsMixed
	 */
	void Database::getCustomData(Data::GetColumnsMixed& data) {
		// check argument
		if(data.columns_types.empty()) {
			throw Database::Exception(
					"Main::Database::getCustomData():"
					" No columns specified"
			);
		}

		// clear and reserve memory
		data.values.clear();

		data.values.reserve(data.columns_types.size());

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// create SQL query
			std::string sqlQuery("SELECT ");

			for(const auto& column_type : data.columns_types) {
				sqlQuery += "`";
				sqlQuery += column_type.first;
				sqlQuery += "`, ";

				// add column to result vector
				data.values.emplace_back();
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `";
			sqlQuery += data.table;
			sqlQuery += "`";

			if(!data.condition.empty()) {
				sqlQuery += " WHERE (";
				sqlQuery += data.condition;
				sqlQuery += ")";
			}

			if(!data.order.empty()) {
				sqlQuery += " ORDER BY (";
				sqlQuery += data.order;
				sqlQuery += ")";
			}

			// execute SQL statement
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement, sqlQuery)
			};

			if(sqlResultSet) {
				// reserve memory for results
				for(auto& value : data.values) {
					value.reserve(sqlResultSet->rowsCount());
				}

				// get results
				while(sqlResultSet->next()) {
					for(
							auto it{data.columns_types.cbegin()};
							it != data.columns_types.cend();
							++it
					) {
						auto& column{data.values.at(it - data.columns_types.cbegin())};

						if(sqlResultSet->isNull(it->first)) {
							column.emplace_back();
						}
						else {
							switch(it->second) {
							case Data::Type::_bool:
								column.emplace_back(sqlResultSet->getBoolean(it->first));

								break;

							case Data::Type::_double:
								column.emplace_back(
										static_cast<double>(sqlResultSet->getDouble(it->first))
								);

								break;

							case Data::Type::_int32:
								column.emplace_back(sqlResultSet->getInt(it->first));

								break;

							case Data::Type::_int64:
								column.emplace_back(sqlResultSet->getInt64(it->first));

								break;

							case Data::Type::_string:
								column.emplace_back(sqlResultSet->getString(it->first));

								break;

							case Data::Type::_uint32:
								column.emplace_back(sqlResultSet->getUInt(it->first));

								break;

							case Data::Type::_uint64:
								column.emplace_back(sqlResultSet->getUInt64(it->first));

								break;

							default:
								throw Database::Exception(
										"Main::Database::getCustomData():"
										" Invalid data type when getting custom data."
								);
							}
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getCustomData", e);
		}
	}

	//! Inserts a custom value into a table row in the database.
	/*!
	 * \param data Constant reference to a
	 *   structure containing the data to be
	 *   inserted.
	 *
	 * \throws Main::Database::Exception if no column
	 *   name or no column type is specified in
	 *   the given data structure, if the given
	 *   data is too large, or if an invalid data
	 *   has been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while inserting the data.
	 *
	 * \sa Data::InsertValue
	 */
	void Database::insertCustomData(const Data::InsertValue& data) {
		// check argument
		if(data.column.empty()) {
			throw Database::Exception(
					"Main::Database::insertCustomData():"
					" No column name specified"
			);
		}

		if(data.type == Data::Type::_unknown) {
			throw Database::Exception(
					"Main::Database::insertCustomData():"
					" No column type specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"INSERT INTO `" + data.table + "` (`" + data.column + "`)"
						" VALUES (?)"
				)
			};

			// set value
			if(data.value._isnull) {
				sqlStatement->setNull(sqlArg1, 0);
			}
			else {
				switch(data.type) {
				case Data::Type::_bool:
					sqlStatement->setBoolean(sqlArg1, data.value._b);

					break;

				case Data::Type::_double:
					sqlStatement->setDouble(sqlArg1, data.value._d);

					break;

				case Data::Type::_int32:
					sqlStatement->setInt(sqlArg1, data.value._i32);

					break;

				case Data::Type::_int64:
					sqlStatement->setInt64(sqlArg1, data.value._i64);

					break;

				case Data::Type::_string:
					if(data.value._s.size() > this->getMaxAllowedPacketSize()) {
						switch(data.value._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(
									sqlArg1,
									data.value._s.substr(
											0,
											this->getMaxAllowedPacketSize()
									)
							);

							break;

						case Data::Value::_if_too_large::_empty:
							sqlStatement->setString(sqlArg1, "");

							break;

						case Data::Value::_if_too_large::_null:
							sqlStatement->setNull(sqlArg1, 0);

							break;

						default:
							std::ostringstream errStrStr;

							errStrStr.imbue(std::locale(""));

							errStrStr	<< "Main::Database::insertCustomData():"
											" Size ("
										<< data.value._s.size()
										<< " bytes) of custom value for `"
										<< data.table
										<< "`.`"
										<< data.column
										<< "` exceeds the ";

							if(data.value._s.size() > maxContentSize) {
								errStrStr	<< "MySQL data limit of "
											<< maxContentSizeString;
							}
							else {
								errStrStr	<< "current MySQL server limit of "
											<< this->getMaxAllowedPacketSize()
											<< " bytes - adjust the 'max_allowed_packet'"
												" setting on the server accordingly"
												" (to max. "
											<< maxContentSizeString
											<< ").";
							}

							throw Database::Exception(errStrStr.str());
						}
					}
					else {
						sqlStatement->setString(sqlArg1, data.value._s);
					}

					break;

				case Data::Type::_uint32:
					sqlStatement->setUInt(sqlArg1, data.value._ui32);

					break;

				case Data::Type::_uint64:
					sqlStatement->setUInt64(sqlArg1, data.value._ui64);

					break;

				default:
					throw Database::Exception(
							"Main::Database::insertCustomData():"
							" Invalid data type when inserting custom data."
					);
				}
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::insertCustomData", e);
		}
	}

	//! Inserts custom values into multiple table columns of the same type.
	/*!
	 * \param data Constant reference to a
	 *   structure containing the data to be
	 *   inserted.
	 *
	 * \throws Main::Database::Exception if no columns
	 *   or no column type are specified in the
	 *   given data structure, if the given data
	 *   is too large, or if an invalid data has
	 *   been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while inserting the data.
	 *
	 * \sa Data::InsertFields
	 */
	void Database::insertCustomData(const Data::InsertFields& data) {
		// check argument
		if(data.columns_values.empty()) {
			throw Database::Exception(
					"Main::Database::insertCustomData():"
					" No columns specified"
			);
		}

		if(data.type == Data::Type::_unknown) {
			throw Database::Exception(
					"Main::Database::insertCustomData():"
					" No column type specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery{
				"INSERT INTO `"
			};

			sqlQuery += data.table;
			sqlQuery += "` (";

			for(const auto& column_value : data.columns_values) {
				sqlQuery += "`";
				sqlQuery += column_value.first;
				sqlQuery += "`, ";
			}

			// remove last two letters (", ")
			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += ") VALUES(";

			for(std::size_t n{0}; n < data.columns_values.size() - 1; ++n) {
				sqlQuery += "?, ";
			}

			sqlQuery += "?)";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(sqlQuery)
			};

			// set values
			std::size_t counter{sqlArg1};

			switch(data.type) {
			case Data::Type::_bool:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setBoolean(counter, column_value.second._b);
					}

					++counter;
				}

				break;

			case Data::Type::_double:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setDouble(counter, column_value.second._d);
					}

					++counter;
				}

				break;

			case Data::Type::_int32:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setInt(counter, column_value.second._i32);
					}

					++counter;
				}

				break;

			case Data::Type::_int64:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setInt64(counter, column_value.second._i64);
					}

					++counter;
				}

				break;

			case Data::Type::_string:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else if(column_value.second._s.size() > this->getMaxAllowedPacketSize()) {
						switch(column_value.second._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(
									counter,
									column_value.second._s.substr(
											0,
											this->getMaxAllowedPacketSize()
									)
							);

							break;

						case Data::Value::_if_too_large::_empty:
							sqlStatement->setString(counter, "");

							break;

						case Data::Value::_if_too_large::_null:
							sqlStatement->setNull(counter, 0);

							break;

						default:
							std::ostringstream errStrStr;

							errStrStr.imbue(std::locale(""));

							errStrStr	<< "Main::Database::insertCustomData():"
											" Size ("
										<< column_value.second._s.size()
										<< " bytes) of custom value for `"
										<< data.table
										<< "`.`"
										<< column_value.first
										<< "` exceeds the ";

							if(column_value.second._s.size() > maxContentSize) {
								errStrStr	<< "MySQL data limit of "
											<< maxContentSizeString;
							}
							else {
								errStrStr	<< "current MySQL server limit of "
											<< this->getMaxAllowedPacketSize()
											<< " bytes - adjust the 'max_allowed_packet'"
												" setting on the server accordingly"
												" (to max. "
											<< maxContentSizeString
											<< ").";
							}

							throw Database::Exception(errStrStr.str());
						}
					}
					else {
						sqlStatement->setString(counter, column_value.second._s);
					}

					++counter;
				}

				break;

			case Data::Type::_uint32:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setUInt(counter, column_value.second._ui32);
					}

					++counter;
				}

				break;

			case Data::Type::_uint64:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setUInt64(counter, column_value.second._ui64);
					}

					++counter;
				}

				break;

			default:
				throw Database::Exception(
						"Main::Database::insertCustomData():"
						" Invalid data type when inserting custom data."
				);
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::insertCustomData", e);
		}
	}

	//! Inserts custom values into multiple table columns of different types.
	/*!
	 * \param data Constant reference to a
	 *   structure containing the data to be
	 *   inserted.
	 *
	 * \throws Main::Database::Exception if no columns
	 *   are specified in the given data
	 *   structure, if the given data is too
	 *   large, or if an invalid data has been
	 *   encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while inserting the data.
	 *
	 * \sa Data::InsertFieldsMixed
	 */
	void Database::insertCustomData(const Data::InsertFieldsMixed& data) {
		// check argument
		if(data.columns_types_values.empty()) {
			throw Database::Exception(
					"Main::Database::insertCustomData():"
					" No columns specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery{
				"INSERT INTO `"
				+ data.table
				+ "` ("
			};

			for(const auto& column_type_value : data.columns_types_values) {
				sqlQuery += "`";
				sqlQuery += std::get<0>(column_type_value);
				sqlQuery += "`, ";
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += ") VALUES(";

			for(std::size_t n{0}; n < data.columns_types_values.size() - 1; ++n) {
				sqlQuery += "?, ";
			}

			sqlQuery += "?)";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(sqlQuery)
			};

			// set values
			std::size_t counter{1};

			for(const auto& column_type_value : data.columns_types_values) {
				if(std::get<2>(column_type_value)._isnull) {
					sqlStatement->setNull(counter, 0);
				}
				else {
					switch(std::get<1>(column_type_value)) {
					case Data::Type::_bool:
						sqlStatement->setBoolean(counter, std::get<2>(column_type_value)._b);

						break;

					case Data::Type::_double:
						sqlStatement->setDouble(counter, std::get<2>(column_type_value)._d);

						break;

					case Data::Type::_int32:
						sqlStatement->setInt(counter, std::get<2>(column_type_value)._i32);

						break;
					case Data::Type::_int64:
						sqlStatement->setInt64(counter, std::get<2>(column_type_value)._i64);

						break;

					case Data::Type::_string:
						if(std::get<2>(column_type_value)._s.size() > this->getMaxAllowedPacketSize()) {
							switch(std::get<2>(column_type_value)._overflow) {
							case Data::Value::_if_too_large::_trim:
								sqlStatement->setString(
										counter,
										std::get<2>(column_type_value)._s.substr(
												0,
												this->getMaxAllowedPacketSize()
										)
								);

								break;

							case Data::Value::_if_too_large::_empty:
								sqlStatement->setString(counter, "");

								break;

							case Data::Value::_if_too_large::_null:
								sqlStatement->setNull(counter, 0);

								break;

							default:
								std::ostringstream errStrStr;

								errStrStr.imbue(std::locale(""));

								errStrStr	<< "Main::Database::insertCustomData():"
												" Size ("
											<< std::get<2>(column_type_value)._s.size()
											<< " bytes) of custom value for `"
											<< data.table
											<< "`.`"
											<< std::get<0>(column_type_value)
											<< "` exceeds the ";

								if(std::get<2>(column_type_value)._s.size() > maxContentSize) {
									errStrStr	<< "MySQL data limit of "
												<< maxContentSizeString;
								}
								else {
									errStrStr	<< "current MySQL server limit of "
												<< this->getMaxAllowedPacketSize()
												<< " bytes - adjust the 'max_allowed_packet'"
													" setting on the server accordingly"
												 	" (to max. "
												<< maxContentSizeString
												<< ").";
								}

								throw Database::Exception(errStrStr.str());
							}
						}
						else {
							sqlStatement->setString(counter, std::get<2>(column_type_value)._s);
						}

						break;

					case Data::Type::_uint32:
						sqlStatement->setUInt(counter, std::get<2>(column_type_value)._ui32);

						break;

					case Data::Type::_uint64:
						sqlStatement->setUInt64(counter, std::get<2>(column_type_value)._ui64);

						break;

					default:
						throw Database::Exception(
								"Main::Database::insertCustomData():"
								" Invalid data type when inserting custom data."
						);
					}
				}

				++counter;
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::insertCustomData", e);
		}
	}

	//! Updates a custom value in a table row.
	/*!
	 * \param data Constant reference to a
	 *   structure containing the data to be
	 *   updated.
	 *
	 * \throws Main::Database::Exception if no column
	 *   name or no column type is specified in
	 *   the given data structure, if the given
	 *   data is too large, or if an invalid data
	 *   has been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the data.
	 *
	 * \sa Data::UpdateValue
	 */
	void Database::updateCustomData(const Data::UpdateValue& data) {
		// check argument
		if(data.column.empty()) {
			throw Database::Exception(
					"Main::Database::updateCustomData():"
					" No column name specified"
			);
		}

		if(data.type == Data::Type::_unknown) {
			throw Database::Exception(
					"Main::Database::updateCustomData():"
					" No column type specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"UPDATE `" + data.table + "`"
						" SET `" + data.column + "` = ?"
						" WHERE (" + data.condition + ")"
				)
			};

			// set value
			if(data.value._isnull) {
				sqlStatement->setNull(sqlArg1, 0);
			}
			else {
				switch(data.type) {
				case Data::Type::_bool:
					sqlStatement->setBoolean(sqlArg1, data.value._b);

					break;

				case Data::Type::_double:
					sqlStatement->setDouble(sqlArg1, data.value._d);

					break;

				case Data::Type::_int32:
					sqlStatement->setInt(sqlArg1, data.value._i32);

					break;

				case Data::Type::_int64:
					sqlStatement->setInt64(sqlArg1, data.value._i64);

					break;

				case Data::Type::_string:
					if(data.value._s.size() > this->getMaxAllowedPacketSize()) {
						switch(data.value._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(sqlArg1, data.value._s.substr(0, this->getMaxAllowedPacketSize()));

							break;

						case Data::Value::_if_too_large::_empty:
							sqlStatement->setString(sqlArg1, "");

							break;

						case Data::Value::_if_too_large::_null:
							sqlStatement->setNull(sqlArg1, 0);

							break;

						default:
							std::ostringstream errStrStr;

							errStrStr.imbue(std::locale(""));

							errStrStr	<< "Main::Database::updateCustomData():"
											" Size ("
										<< data.value._s.size()
										<< " bytes) of custom value for `"
										<< data.table
										<< "`.`"
										<< data.column
										<< "` exceeds the ";

							if(data.value._s.size() > maxContentSize) {
								errStrStr	<< "MySQL data limit of "
											<< maxContentSizeString;
							}
							else {
								errStrStr	<< "current MySQL server limit of "
											<< this->getMaxAllowedPacketSize()
											<< " bytes - adjust the 'max_allowed_packet'"
												" setting on the server accordingly"
												" (to max. "
											<< maxContentSizeString
											<< ").";
							}

							throw Database::Exception(errStrStr.str());
						}
					}
					else {
						sqlStatement->setString(sqlArg1, data.value._s);
					}

					break;

				case Data::Type::_uint32:
					sqlStatement->setUInt(sqlArg1, data.value._ui32);

					break;

				case Data::Type::_uint64:
					sqlStatement->setUInt64(sqlArg1, data.value._ui64);

					break;

				default:
					throw Database::Exception(
							"Main::Database::updateCustomData():"
							" Invalid data type when updating custom data."
					);
				}
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::updateCustomData", e);
		}
	}

	//! Updates custom values in multiple table columns of the same type.
	/*!
	 * \param data Constant reference to a
	 *   structure containing the data to be
	 *   updated.
	 *
	 * \throws Main::Database::Exception if no columns
	 *   or no column type are specified in the
	 *   given data structure, if the given data
	 *   is too large, or if an invalid data has
	 *   been encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the data.
	 *
	 * \sa Data::UpdateFields
	 */
	void Database::updateCustomData(const Data::UpdateFields& data) {
		// check argument
		if(data.columns_values.empty()) {
			throw Database::Exception(
					"Main::Database::updateCustomData():"
					" No columns specified"
			);
		}

		if(data.type == Data::Type::_unknown) {
			throw Database::Exception(
					"Main::Database::updateCustomData():"
					" No column type specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery{
				"UPDATE `"
				+ data.table
				+ "` SET "
			};

			for(const auto& column_value : data.columns_values) {
				sqlQuery += "`";
				sqlQuery += column_value.first;
				sqlQuery += "` = ?, ";
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " WHERE (";
			sqlQuery += data.condition;
			sqlQuery += ")";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(sqlQuery)
			};

			// set values
			std::size_t counter{sqlArg1};

			switch(data.type) {
			case Data::Type::_bool:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setBoolean(counter, column_value.second._b);
					}

					++counter;
				}

				break;

			case Data::Type::_double:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setDouble(counter, column_value.second._d);
					}

					++counter;
				}

				break;

			case Data::Type::_int32:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setInt(counter, column_value.second._i32);
					}

					++counter;
				}

				break;

			case Data::Type::_int64:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setInt64(counter, column_value.second._i64);
					}

					++counter;
				}

				break;

			case Data::Type::_string:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else if(column_value.second._s.size() > this->getMaxAllowedPacketSize()) {
						switch(column_value.second._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(
									counter,
									column_value.second._s.substr(0, this->getMaxAllowedPacketSize())
							);

							break;

						case Data::Value::_if_too_large::_empty:
							sqlStatement->setString(counter, "");

							break;

						case Data::Value::_if_too_large::_null:
							sqlStatement->setNull(counter, 0);

							break;

						default:
							std::ostringstream errStrStr;

							errStrStr.imbue(std::locale(""));

							errStrStr	<< "Main::Database::updateCustomData():"
											" Size ("
										<< column_value.second._s.size()
										<< " bytes) of custom value for `"
										<< data.table
										<< "`.`"
										<< column_value.first
										<< "` exceeds the ";

							if(column_value.second._s.size() > maxContentSize) {
								errStrStr	<< "MySQL data limit of "
											<< maxContentSizeString;
							}
							else {
								errStrStr	<< "current MySQL server limit of "
											<< this->getMaxAllowedPacketSize()
											<< " bytes - adjust the 'max_allowed_packet'"
												" setting on the server accordingly"
											 	" (to max. "
											<< maxContentSizeString
											<< ").";
							}

							throw Database::Exception(errStrStr.str());
						}
					}
					else {
						sqlStatement->setString(counter, column_value.second._s);
					}

					++counter;
				}

				break;

			case Data::Type::_uint32:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setUInt(counter, column_value.second._ui32);
					}

					++counter;
				}

				break;

			case Data::Type::_uint64:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) {
						sqlStatement->setNull(counter, 0);
					}
					else {
						sqlStatement->setUInt64(counter, column_value.second._ui64);
					}

					++counter;
				}

				break;

			default:
				throw Database::Exception(
						"Main::Database::updateCustomData():"
						" Invalid data type when updating custom data."
				);
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::updateCustomData", e);
		}
	}

	//! Updates custom values in multiple table columns of different types.
	/*!
	 * \param data Constant reference to a
	 *   structure containing the data to be
	 *   updated.
	 *
	 * \throws Main::Database::Exception if no columns
	 *   are specified in the given data
	 *   structure, if the given data is too
	 *   large, or if an invalid data has been
	 *   encountered.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while updating the data.
	 *
	 * \sa Data::UpdateFieldsMixed
	 */
	void Database::updateCustomData(const Data::UpdateFieldsMixed& data) {
		// check argument
		if(data.columns_types_values.empty()) {
			throw Database::Exception(
					"Main::Database::updateCustomData():"
					" No columns specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery{
				"UPDATE `"
				+ data.table
				+ "` SET "
			};

			for(const auto& column_type_value : data.columns_types_values) {
				sqlQuery += "`";
				sqlQuery += std::get<0>(column_type_value);
				sqlQuery += "` = ?, ";
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " WHERE (";
			sqlQuery += data.condition;
			sqlQuery += ")";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(sqlQuery)
			};

			// set values
			std::size_t counter{sqlArg1};

			for(const auto& column_type_value : data.columns_types_values) {
				if(std::get<2>(column_type_value)._isnull) {
					sqlStatement->setNull(counter, 0);
				}
				else {
					switch(std::get<1>(column_type_value)) {
					case Data::Type::_bool:
						sqlStatement->setBoolean(counter, std::get<2>(column_type_value)._b);

						break;

					case Data::Type::_double:
						sqlStatement->setDouble(counter, std::get<2>(column_type_value)._d);

						break;

					case Data::Type::_int32:
						sqlStatement->setInt(counter, std::get<2>(column_type_value)._i32);

						break;

					case Data::Type::_int64:
						sqlStatement->setInt64(counter, std::get<2>(column_type_value)._i64);

						break;

					case Data::Type::_string:
						if(std::get<2>(column_type_value)._s.size() > this->getMaxAllowedPacketSize()) {
							switch(std::get<2>(column_type_value)._overflow) {
							case Data::Value::_if_too_large::_trim:
								sqlStatement->setString(
										counter,
										std::get<2>(column_type_value)._s.substr(
												0,
												this->getMaxAllowedPacketSize()
										)
								);

								break;

							case Data::Value::_if_too_large::_empty:
								sqlStatement->setString(counter, "");

								break;

							case Data::Value::_if_too_large::_null:
								sqlStatement->setNull(counter, 0);

								break;

							default:
								std::ostringstream errStrStr;

								errStrStr.imbue(std::locale(""));

								errStrStr	<< "Main::Database::updateCustomData():"
												" Size ("
											<< std::get<2>(column_type_value)._s.size()
											<< " bytes) of custom value for `"
											<< data.table
											<< "`.`"
											<< std::get<0>(column_type_value)
											<< "` exceeds the ";

								if(std::get<2>(column_type_value)._s.size() > maxContentSize) {
									errStrStr	<< "MySQL data limit of "
												<< maxContentSizeString;
								}
								else {
									errStrStr	<< "current MySQL server limit of "
												<< this->getMaxAllowedPacketSize()
												<< " bytes - adjust the 'max_allowed_packet'"
													" setting on the server accordingly"
												 	" (to max. "
												<< maxContentSizeString
												<< ").";
								}

								throw Database::Exception(errStrStr.str());
							}
						}
						else {
							sqlStatement->setString(counter, std::get<2>(column_type_value)._s);
						}

						break;

					case Data::Type::_uint32:
						sqlStatement->setUInt(counter, std::get<2>(column_type_value)._ui32);

						break;

					case Data::Type::_uint64:
						sqlStatement->setUInt64(counter, std::get<2>(column_type_value)._ui64);

						break;

					default:
						throw Database::Exception(
								"Main::Database::updateCustomData():"
								" Invalid data type when updating custom data."
						);
					}
				}

				++counter;
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::updateCustomData", e);
		}
	}

	/*
	 * HELPER FUNCTIONS FOR PREPARED SQL STATEMENTS (protected)
	 */

	//! Reserves memory for a specific number of additional prepared SQL statements.
	/*!
	 * \param n Number of prepared SQL
	 *   statements for which memory should be
	 *   reserved.
	 */
	void Database::reserveForPreparedStatements(std::size_t n) {
		this->preparedStatements.reserve(
				this->preparedStatements.size()
				+ n
		);
	}

	//! Prepares an additional SQL statement and returns its ID.
	/*!
	 * \param sqlQuery Constant reference to
	 *   a string containing the SQL query for
	 *   the prepared SQL statement.
	 *
	 * \returns A unique ID identifying the
	 *   prepared SQL query in-class.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while preparing and
	 *   adding the SQL statement.
	 */
	std::size_t Database::addPreparedStatement(const std::string& sqlQuery) {
		// check connection
		this->checkConnection();

		// try to prepare SQL statement
		try {
			this->preparedStatements.emplace_back(this->connection.get(), sqlQuery);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::addPreparedStatement", e);
		}

		// The returned ID equals the number of prepared SQL statements
		return this->preparedStatements.size();
	}

	//! Gets a reference to a prepared SQL statement.
	/*!
	 * \warning Do not run checkConnection while
	 *   using this reference, because the
	 *   references will be invalidated when
	 *   reconnecting to the database!
	 *
	 * \param id The ID of the prepared SQL
	 *   statement to retrieve.
	 *
	 * \returns A reference to the prepared
	 *   SQL statement.
	 *
	 *  \throws Main::Database::Exception if a MySQL
	 *    error occured while retrieving the
	 *    prepared SQL statement.
	 */
	sql::PreparedStatement& Database::getPreparedStatement(std::size_t id) {
		try {
			return this->preparedStatements.at(id - 1).get();
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getPreparedStatement", e);

			throw; // will not be used
		}
	}

	/*
	 * DATABASE HELPER FUNCTIONS (protected)
	 */

	//! Gets the last inserted ID from the database.
	/*!
	 * \returns The last inserted ID from the
	 *   database.
	 *
	 * \throws Main::Database::Exception if the
	 *   prepared SQL statement for retrieving
	 *   the last inserted ID from the database
	 *   is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   last inserted ID from the database.
	 */
	std::uint64_t Database::getLastInsertedId() {
		std::uint64_t result{0};

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.lastId == 0) {
			throw Database::Exception(
					"Main::Database::getLastInsertedId():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement{this->getPreparedStatement(this->ps.lastId)};

		try {
			// execute SQL statement
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("id");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::getLastInsertedId", e);
		}

		return result;
	}

	//! Resets the auto-increment value of an empty table in the database.
	/*!
	 * \param tableName Const reference to a
	 *   string containing the name of the table
	 *   whose auto-increment value will be
	 *   reset.
	 *
	 * \throws Main::Database::Exception if no
	 *  table is specified, i.e. the string
	 *  containing the name of the table is
	 *  empty.	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while resetting the
	 *   auto-increment value of the table in
	 *   the database.
	 */
	void Database::resetAutoIncrement(const std::string& tableName) {
		// check argument
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::resetAutoIncrement():"
					" No table name specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			Database::sqlExecute(
					sqlStatement,
					"ALTER TABLE `"
					+ tableName
					+ "` AUTO_INCREMENT = 1"
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::resetAutoIncrement", e);
		}
	}

	//! Adds a lock to the database class, blocking execution.
	/*!
	 * If a lock with the same name already
	 *  exists, the function will block
	 *  execution until this lock has been
	 *  released, or the specified callback
	 *  function returns false.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the lock
	 *   to be waited for and added to the
	 *   database class.
	 * \param isRunningCallback Constant
	 *   reference to a function that will be
	 *   regularly called during a block, to
	 *   enquire whether the thread (or
	 *   application) is still running.
	 *   As soon as this function returns false,
	 *   execution will no longer be blocked,
	 *   even if the lock could not be added.
	 */
	void Database::addDatabaseLock(
			const std::string& name,
			const IsRunningCallback& isRunningCallback
	) {
		while(isRunningCallback()) {
			{ // lock access to locks
				std::lock_guard<std::mutex> accessLock(Database::lockAccess);

				// check whether lock with specified name already exists
				if(
						std::find(
								Database::locks.cbegin(),
								Database::locks.cend(),
								name
						) == Database::locks.cend()
				) {
					// lock does not exist: add lock and exit loop
					Database::locks.emplace_back(name);

					break;
				}
			}

			// sleep before re-attempting to add lock
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepOnLockMs));
		}
	}

	//! Tries to add a lock to the database class, not blocking execution.
	/*!
	 * If a lock with the same name already
	 *  exists, the function will not add
	 *  a lock and return false instead.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the lock
	 *   to be added to the database class if a
	 *   lock with the same name does not exist
	 *   already.
	 *
	 * \returns True, if a lock with the same
	 *   name did not exist already and the lock
	 *   has been added. False, if a lock with
	 *   the same name already exists and no lock
	 *   has been added.
	 */
	bool Database::tryDatabaseLock(const std::string& name) {
		// lock access to locks
		std::lock_guard<std::mutex> accessLock{Database::lockAccess};

		// check whether lock with specified name already exists
		if(
				std::find(
						Database::locks.cbegin(),
						Database::locks.cend(),
						name
				) == Database::locks.cend()
		) {
			// lock does not exist: add lock
			Database::locks.emplace_back(name);

			return true;
		}

		// lock already exists
		return false;
	}

	//! Removes a lock from the database class.
	/*!
	 * Does nothing if a lock with the given
	 *  name does not exist in the database
	 *  class.
	 *
	 * \param name Constant reference to a
	 *   string containing the name of the lock
	 *   to be removed from the database class.
	 */
	void Database::removeDatabaseLock(const std::string& name) {
		std::lock_guard<std::mutex> accessLock{Database::lockAccess};

		Database::locks.erase(
				std::remove(
						Database::locks.begin(),
						Database::locks.end(),
						name
				),
				Database::locks.end()
		);
	}

	//! Checks access to an external data directory in the database.
	/*!
	 * Adds and removes a test table to check
	 *  access to the given data directory.
	 *
	 * \param dir Constant reference to a string
	 *   containing the data directory to be
	 *   checked.
	 *
	 * \throws Main::Database::Exception if no data
	 *   directory is specified, i.e. the string
	 *   containing the directory is empty.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking access
	 *   to the data directory in the database.
	 */
	void Database::checkDirectory(const std::string& dir) {
		// check argument
		if(dir.empty()) {
			throw Database::Exception(
					"Main::Database::checkDirectory():"
					" No external directory specified."
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL commands
			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE IF EXISTS `crawlserv_testaccess`"
			);

			Database::sqlExecute(
					sqlStatement,
					"CREATE TABLE `crawlserv_testaccess(id SERIAL)` DATA DIRECTORY=`"
					+ dir
					+ "`"
			);

			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE `crawlserv_testaccess`"
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::checkDirectory", e);
		}
	}

	/*
	 * TABLE HELPER FUNCTIONS (protected)
	 */

	//! Adds a table to the database.
	/*!
	 * \note A column for the primary key named
	 *   @c id will be created automatically.
	 *
	 * \param properties Constant reference to
	 *   a structure containing the properties
	 *   of the table to be created.
	 *
	 * \throws Main::Database::Exception if no name
	 *	 or columns are specified in the given
	 *	 properties structure, if one of the
	 *	 columns defined there is missing its
	 *	 name or data type, or if a column
	 *	 reference is incomplete.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the table
	 *   to the database.
	 *
	 * \sa Struct::TableProperties
	 */
	void Database::createTable(const TableProperties& properties) {
		// check argument
		if(properties.name.empty()) {
			throw Database::Exception(
					"Main::Database::createTable():"
					" No table name specified"
			);
		}

		if(properties.columns.empty()) {
			throw Database::Exception(
					"Main::Database::createTable():"
					" No columns specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery(
					"CREATE TABLE IF NOT EXISTS `"
					+ properties.name
					+ "`"
						"(id SERIAL"
			);

			std::string propertiesString;

			for(const auto& column : properties.columns) {
				// check values
				if(column.name.empty()) {
					throw Database::Exception(
							"Main::Database::createTable():"
							" A column is missing its name"
					);
				}

				if(column.type.empty()) {
					throw Database::Exception(
							"Main::Database::createTable():"
							" A column is missing its type"
					);
				}

				// add to SQL query
				sqlQuery += ", `";
				sqlQuery += column.name;
				sqlQuery += "` ";
				sqlQuery += column.type;

				// add indices and references
				if(column.indexed) {
					propertiesString += ", INDEX(`";
					propertiesString += column.name;
					propertiesString += "`)";
				}

				if(!column.referenceTable.empty()) {
					if(column.referenceColumn.empty()) {
						throw Database::Exception(
								"Main::Database::createTable():"
								" A column reference is missing its source column name"
						);
					}

					propertiesString += ", FOREIGN KEY(`";
					propertiesString += column.name;
					propertiesString += "`) REFERENCES `";
					propertiesString += column.referenceTable;
					propertiesString += "` (`";
					propertiesString += column.referenceColumn;
					propertiesString += "`) ON UPDATE RESTRICT ON DELETE CASCADE";
				}
			}

			sqlQuery += ", PRIMARY KEY(id)";
			sqlQuery += propertiesString;
			sqlQuery += ")";
			sqlQuery += " CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,"
						" ENGINE=InnoDB";

			if(properties.compressed) {
				sqlQuery += ", ROW_FORMAT=COMPRESSED";
			}

			if(!properties.dataDirectory.empty()) {
				sqlQuery += ", DATA DIRECTORY='";
				sqlQuery += properties.dataDirectory + "'";
			}

			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			Database::sqlExecute(sqlStatement, sqlQuery);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::createTable", e);
		}
	}

	//! Deletes a table from the database.
	/*!
	 * If the table does not exist in the
	 *  database, the database will not be
	 *  changed.
	 *
	 * \param tableName Constant reference to a
	 *   string containing the name of the
	 *   table to be deleted, if it exists.
	 *
	 * \throws Main::Database::Exception if no table
	 *   is specified, i.e. if the string
	 *   containing the name of the table is
	 *   empty.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while removing the table
	 *   from the database.
	 */
	void Database::dropTable(const std::string& tableName) {
		// check argument
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::dropTable():"
					" No table name specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE IF EXISTS `"
					+ tableName
					+ "`"
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::dropTable", e);
		}
	}

	//! Adds a column to a table in the database.
	/*!
	 * \param tableName Constant reference to a
	 *   string containing the name of the
	 *   table to which the column will be
	 *   added.
	 * \param column Constant reference to a
	 *   structure containing the properties
	 *   of the column to be added to the
	 *   table.
	 *
	 * \throws Main::Database::Exception if no table,
	 *   no column, or no column type is
	 *   specified, i.e. if one of the strings
	 *   containing the name of the table, the
	 *   name of the column, and the type of the
	 *   column is empty, or if a column
	 *   reference is incomplete.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the column
	 *   to the given table in the database.
	 *
	 * \sa Struct::TableColumn
	 */
	void Database::addColumn(const std::string& tableName, const TableColumn& column) {
		// check arguments
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::addColumn():"
					" No table name specified"
			);
		}

		if(column.name.empty()) {
			throw Database::Exception(
					"Main::Database::addColumn():"
					" No column name specified"
			);
		}

		if(column.type.empty()) {
			throw Database::Exception(
					"Main::Database::addColumn():"
					" No column type specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery{
				"ALTER TABLE `"
				+ tableName
				+ "` ADD COLUMN `"
				+ column.name
				+ "` "
				+ column.type
			};

			if(!column.referenceTable.empty()) {
				if(column.referenceColumn.empty()) {
					throw Database::Exception(
							"Main::Database::addColumn():"
							" A column reference is missing its source column name"
					);
				}

				sqlQuery += ", ADD FOREIGN KEY(`";
				sqlQuery += column.name;
				sqlQuery += "`) REFERENCES `";
				sqlQuery += column.referenceTable;
				sqlQuery += "`(`";
				sqlQuery += column.referenceColumn;
				sqlQuery += "`) ON UPDATE RESTRICT ON DELETE CASCADE";
			}

			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			Database::sqlExecute(sqlStatement, sqlQuery);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::addColumn", e);
		}
	}

	//! Compresses a table in the database.
	/*!
	 * The function will have no effect om
	 *  the table, if the table is already
	 *  compressed.
	 *
	 * \param tableName Constant reference to a
	 *   string containing the name of the
	 *   table to be compressed.
	 *
	 * \throws Main::Database::Exception if no table
	 *   is specified, i.e. if the string
	 *   containing the name of the table is
	 *   empty, or if a row format could not be
	 *   determined.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while compressing the
	 *   table in the database.
	 */
	void Database::compressTable(const std::string& tableName) {
		bool compressed{false};

		// check argument
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::compressTable():"
					" No table name specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement for checking whether the table is already compressed
			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SELECT LOWER(row_format) = 'compressed'"
						" AS result FROM information_schema.tables"
						" WHERE table_schema = '" + this->settings.name + "'"
						" AND table_name = '" + tableName + "'"
						" LIMIT 1"
				)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				compressed = sqlResultSet->getBoolean("result");
			}
			else {
				throw Database::Exception(
						"Main::Database::compressTable():"
						" Could not determine row format of '"
						+ tableName
						+ "'"
				);
			}

			// execute SQL statement for compressing the table if table is not already compressed
			if(!compressed) {
				Database::sqlExecute(
						sqlStatement,
						"ALTER TABLE `"
						+ tableName
						+ "` ROW_FORMAT=COMPRESSED"
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::compressTable", e);
		}
	}

	//! Clones a table into another data directory, without copying data or constraints.
	/*!
	 * \warning The table @c \<tablename\>_tmp
	 *   may not exist already in the database!
	 *
	 * \param tableName Const reference to a
	 *   string containing the name of the
	 *   table to be cloned.
	 * \param destDir  Const reference to a
	 *   string containing the data directory
	 *   into which the table will be cloned.
	 *
	 * \returns A queue containing the
	 *   constraints that have been dropped
	 *   during the cloning of the table.
	 *
	 * \throws Main::Database::Exception if no table
	 *   is specified, i.e. if the string
	 *   containing the name of the table is
	 *   empty, or if the properties of a
	 *   table could not be retrieved.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while cloning the table
	 *   into the other data directory.
	 */
	std::queue<std::string> Database::cloneTable(const std::string& tableName, const std::string& destDir) {
		std::queue<std::string> constraints;

		// check argument
		if(tableName.empty()) {
			throw Database::Exception(
					"Main::Database::cloneTable():"
					" No table specified."
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// drop temporary table if necessary
			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE IF EXISTS `crawlserv_tmp`"
			);

			// get constraints that will be dropped
			std::string result;

			SqlResultSetPtr sqlResultSet1{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SHOW CREATE TABLE `"
						+ tableName
						+ "`"
				)
			};

			if(sqlResultSet1 && sqlResultSet1->next()) {
				result = sqlResultSet1->getString("Create Table");
			}
			else {
				throw Database::Exception(
						"Main::Database::cloneTable():"
						" Could not get properties of table `"
						+ tableName
						+ "`"
				);
			}

			std::istringstream stream(result);
			std::string line;

			// parse single constraints
			while(std::getline(stream, line)) {
				Helper::Strings::trim(line);

				if(
						line.length() > sqlConstraint.length()
						&& line.substr(0, sqlConstraint.length()) == sqlConstraint
				) {
					line.erase(0, sqlConstraint.length());

					const auto pos{line.find("` ")};

					if(pos != std::string::npos) {
						line.erase(0, pos + 2);

						constraints.emplace(line);
					}
				}
			}

			// create temporary table with similar properties (no data, directory, constraints or increment value)
			Database::sqlExecute(
				sqlStatement,
				"CREATE TABLE `crawlserv_tmp` LIKE `" + tableName + "`"
			);

			// get command to create similar table
			SqlResultSetPtr sqlResultSet2{
				Database::sqlExecuteQuery(
						sqlStatement,
						"SHOW CREATE TABLE `crawlserv_tmp`"
				)
			};

			if(sqlResultSet2 && sqlResultSet2->next()) {
				result = sqlResultSet2->getString("Create Table");
			}
			else {
				throw Database::Exception(
						"Main::Database::cloneTable():"
						" Could not get properties of table `crawlserv_tmp`"
				);
			}

			// drop temporary table
			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE `crawlserv_tmp`"
			);

			// replace table name and add new data directory
			const auto pos{result.find("` ") + 2};

			result = "CREATE TABLE `";

			result += tableName;
			result += "_tmp` ";
			result += result.substr(pos);
			result += " DATA DIRECTORY='";
			result += destDir;
			result += "'";

			// create new table
			Database::sqlExecute(sqlStatement, result);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::cloneTable", e);
		}

		return constraints;
	}

	/*
	 * URL LIST HELPER FUNCTIONS (protected)
	 */

	//! Gets whether the specified URL list is case-sensitive.
	/*!
	 * \param listId The ID of the URL list to
	 *   be checked for case-sensitivity.
	 *
	 * \returns True, if the given URL list is
	 *   case-sensitive. False otherwise.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   case-sensitivity of the URL list from
	 *   the database.
	 */
	bool Database::isUrlListCaseSensitive(std::uint64_t listId) {
		bool result{true};

		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::isUrlListCaseSensitive():"
					" No URL list specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"SELECT case_sensitive"
						" FROM `crawlserv_urllists`"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setUInt64(sqlArg1, listId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("case_sensitive");
			}
			else {
				throw Database::Exception(
						"Main::Database::isUrlListCaseSensitive():"
						" Could not get case sensitivity for URL list #"
						+ std::to_string(listId)
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::isUrlListCaseSensitive", e);
		}

		return result;
	}

	//! Sets whether the specified URL list is case-sensitive.
	/*!
	 * \warning The case-sensitivity should not
	 *   be changed once URLs have been
	 *   retrieved!
	 *
	 * \param listId The ID of the URL list
	 *   whose case-sensitivity will be
	 *   changed.
	 * \param isCaseSensitive Specify whether
	 *   URLs in the given URL list will be
	 *   case-sensitive or not.
	 *
	 * \throws Main::Database::Exception if no URL
	 *   list has been specified, i.e. the URL
	 *   list ID is zero.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while setting the
	 *   case-sensitivity of the URL list.
	 */
	void Database::setUrlListCaseSensitive(std::uint64_t listId, bool isCaseSensitive) {
		// check argument
		if(listId == 0) {
			throw Database::Exception(
					"Main::Database::setUrlListCaseSensitive():"
					" No URL list specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement{
				this->connection->prepareStatement(
						"UPDATE `crawlserv_urllists`"
						" SET case_sensitive = ?"
						" WHERE id = ?"
						" LIMIT 1"
				)
			};

			// execute SQL statement
			sqlStatement->setBoolean(sqlArg1, isCaseSensitive);
			sqlStatement->setUInt64(sqlArg2, listId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::setUrlListCaseSensitive", e);
		}
	}

	/*
	 * EXCEPTION HELPER FUNCTION (protected)
	 */

	//! Catches a SQL exception and re-throws it as a specific or a generic Database::Exception.
	/*!
	 * \note Always throws an exception.
	 *
	 * \param function Constant reference to a
	 *   string containing the name of the
	 *   function in which the exception has
	 *   been thrown.
	 * \param e Constant reference to the SQL
	 *   exception that has been thrown.
	 *
	 * \throws Main::Database::ConnectionException
	 * \throws Main::Database::StorageEngineException
	 * \throws Main::Database::PrivilegesException
	 * \throws Main::Database::WrongArgumentsException
	 * \throws Main::Database::IncorrectPathException
	 * \throws Main::Database::Exception depending on
	 *   the SQL exception thrown.
	 */
	void Database::sqlException(const std::string& function, const sql::SQLException& e) {
		// get error code and create error string
		const auto error{e.getErrorCode()};

		std::string errorStr{function};

		errorStr += "()";

		// check for error code
		if(error > 0) {
			// SQL error with error code
			errorStr += "SQL Error #";
			errorStr += std::to_string(error);
			errorStr += " (State ";
			errorStr += e.getSQLState();
			errorStr += "): ";
		}
		else {
			errorStr += ": ";
		}

		errorStr += std::string(e.what());

		switch(error) {
		// check for connection error
		case sqlSortAborted:
		case sqlTooManyConnections:
		case sqlCannotGetHostName:
		case sqlBadHandShake:
		case sqlServerShutDown:
		case sqlNormalShutdown:
		case sqlGotSignal:
		case sqlShutDownComplete:
		case sqlForcingCloseOfThread:
		case sqlCannotCreateIPSocket:
		case sqlAbortedConnection:
		case sqlReadErrorFromConnectionPipe:
		case sqlPacketsOutOfOrder:
		case sqlCouldNotUncompressPackets:
		case sqlErrorReadingPackets:
		case sqlTimeOutReadingPackets:
		case sqlErrorWritingPackets:
		case sqlTimeOutWritingPackets:
		case sqlNewAbortedConnection:
		case sqlNetErrorReadingFromMaster:
		case sqlNetErrorWritingToMaster:
		case sqlMoreThanMaxUserConnections:
		case sqlLockWaitTimeOutExceeded:
		case sqlNumOfLocksExceedsLockTableSize:
		case sqlServerErrorConnectingToMaster:
		case sqlQueryExecutionInterrupted:
		case sqlUnableToConnectToForeignDataSource:
		case sqlCannotConnectToServerThroughSocket:
		case sqlCannotConnectToServer:
		case sqlUnknownServerHost:
		case sqlServerHasGoneAway:
		case sqlTCPError:
		case sqlErrorInServerHandshake:
		case sqlLostConnectionDuringQuery:
		case sqlClientErrorConnectingToSlave:
		case sqlClientErrorConnectingToMaster:
		case sqlSSLConnectionError:
		case sqlMalformedPacket:
		case sqlInvalidConnectionHandle:
			// throw connection exception
			throw Database::ConnectionException(errorStr);

		// check for storage engine error
		case sqlStorageEngineError:
			throw Database::StorageEngineException(errorStr);

		// check for insufficient privileges error
		case sqlInsufficientPrivileges:
			throw Database::PrivilegesException(errorStr);

		// check for wrong arguments error
		case sqlWrongArguments:
			throw Database::WrongArgumentsException(errorStr);

		// check for incorrect path value error
		case sqlIncorrectPath:
			throw Database::IncorrectPathException(errorStr);

		default:
			// throw general database exception
			throw Database::Exception(errorStr);
		}
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// run file with SQL commands, throws Database::Exception
	void Database::run(const std::string& sqlFile) {
		// check argument
		if(sqlFile.empty()) {
			throw Database::Exception(
					"Main::Database::run():"
					" No SQL file specified"
			);
		}

		// open SQL file
		std::ifstream initSQLFile(sqlFile);

		if(initSQLFile.is_open()) {
			// check connection
			this->checkConnection();

			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};
			std::string line;

			if(sqlStatement == nullptr) {
				throw Database::Exception(
						"Main::Database::run():"
						" Could not create SQL statement"
				);
			}

			// execute lines in SQL file
			while(std::getline(initSQLFile, line)) {
				try {
					if(!line.empty()) {
						Database::sqlExecute(sqlStatement, line);
					}
				}
				catch(const sql::SQLException &e) {
					Database::sqlException("(in " + sqlFile + ")", e);
				}
			}

			// close SQL file
			initSQLFile.close();
		}
		else {
			throw Database::Exception(
					"Main::Database::run():"
					" Could not open '"
					+ sqlFile
					+ "' for reading"
			);
		}
	}

	// execute a SQL query, throws Database::Exception
	void Database::execute(const std::string& sqlQuery) {
		// check argument
		if(sqlQuery.empty()) {
			throw Database::Exception(
					"Main::Database::execute():"
					" No SQL query specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			Database::sqlExecute(sqlStatement, sqlQuery);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::execute", e);
		}
	}

	// execute a SQL query and return updated rows, throws Database::Exception
	int Database::executeUpdate(const std::string& sqlQuery) {
		int result{-1};

		// check argument
		if(sqlQuery.empty()) {
			throw Database::Exception(
					"Main::Database::execute():"
					" No SQL query specified"
			);
		}

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement{this->connection->createStatement()};

			// execute SQL statement
			result = Database::sqlExecuteUpdate(sqlStatement, sqlQuery);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Main::Database::execute", e);
		}

		return result;
	}

	// escape string for usage in SQL commands
	std::string Database::sqlEscapeString(const std::string& in) {
		// check connection
		this->checkConnection();

		// escape string
		return dynamic_cast<sql::mysql::MySQL_Connection *>(
				this->connection.get()
		)->escapeString(in);
	}

} /* namespace crawlservpp::Main */
