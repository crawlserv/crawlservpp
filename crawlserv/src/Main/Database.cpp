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
	sql::Driver * Database::driver = nullptr;
	std::mutex Database::lockAccess;
	std::vector<std::string> Database::locks;

	#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		std::atomic<unsigned long long> Database::requestCounter(0);
	#endif

	// constructor: save settings and set default values, throws Database::Exception
	Database::Database(const DatabaseSettings& dbSettings, const std::string& dbModule)
			: settings(dbSettings),
			  maxAllowedPacketSize(0),
			  sleepOnError(0),
			  module(dbModule),
			  ps(_ps()) {
		// get driver instance if necessary
		if(!Database::driver) {
			Database::driver = get_driver_instance();

			if(!Database::driver)
				throw Database::Exception("Could not get database instance");

			// check MySQL version
			if(Database::driver->getMajorVersion() < 8)
				std::cout	<< "\nWARNING: Using MySQL v"
							<< Database::driver->getMajorVersion()
							<< "."
							<< Database::driver->getMinorVersion()
							<< "."
							<< Database::driver->getPatchVersion()
							<< ", version 8 or higher is strongly recommended."
							<< std::endl;
		}

		// get MySQL version
		this->mysqlVersion =
							std::to_string(Database::driver->getMajorVersion())
							+ '.'
							+ std::to_string(Database::driver->getMinorVersion())
							+ '.'
							+ std::to_string(Database::driver->getPatchVersion());
	}

	// destructor
	Database::~Database() {
		if(this->module == "server") {
			// log SQL request counter (if available)
			const unsigned long long requests = this->getRequestCounter();

			if(requests) {
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
		if(this->connection && this->connection->isValid())
			this->connection->close();
	}

	/*
	 * SETTERS
	 */

	// set the number of seconds to wait before (first and last) re-try on connection loss to MySQL server
	void Database::setSleepOnError(std::uint64_t seconds) {
		this->sleepOnError = seconds;
	}

	// set the maximum amount of milliseconds for a query before it cancels execution (or 0 for none)
	//  NOTE: database connection needs to be established
	void Database::setTimeOut(std::uint64_t milliseconds) {
		this->checkConnection();

		try {
			// create MySQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// set execution timeout if necessary
			Database::sqlExecute(
					sqlStatement,
					"SET @@max_execution_time = "
					+ std::to_string(milliseconds)
			);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::setTimeOut", e); }
	}

	/*
	 * GETTERS
	 */

	// get settings of the database
	const Database::DatabaseSettings& Database::getSettings() const {
		return this->settings;
	}

	// get MySQL version string
	const std::string& Database::getMysqlVersion() const {
		return this->mysqlVersion;
	}

	// get (default) data directory
	const std::string& Database::getDataDir() const {
		return this->dataDir;
	}

	// get the maximum allowed packet size
	std::uint64_t Database::getMaxAllowedPacketSize() const {
		return this->maxAllowedPacketSize;
	}

	// get MySQL connection ID
	std::uint64_t Database::getConnectionId() const {
		return this->connectionId;
	}

	/*
	 * INITIALIZING FUNCTIONS
	 */

	// connect to the database, throws Database::Exception
	void Database::connect() {
		// check driver
		if(!Database::driver)
			throw Database::Exception("Main::Database::connect(): MySQL driver not loaded");

		try {
			// set options for connecting
			sql::ConnectOptionsMap connectOptions;

			connectOptions["hostName"] = this->settings.host; // set host
			connectOptions["userName"] = this->settings.user; // set username
			connectOptions["password"] = this->settings.password; // set password
			connectOptions["schema"] = this->settings.name; // set schema
			connectOptions["port"] = this->settings.port; // set port
			connectOptions["OPT_RECONNECT"] = false; // don't automatically re-connect (manually recover prepared statements)
			connectOptions["OPT_CHARSET_NAME"] = "utf8mb4"; // set charset
			connectOptions["characterSetResults"] = "utf8mb4"; // set charset for results
			connectOptions["preInit"] = "SET NAMES utf8mb4"; // set charset for names

			if(this->settings.compression)
				connectOptions["CLIENT_COMPRESS"] = true; // enable server-client compression

			// connect
			this->connection.reset(Database::driver->connect(connectOptions));

			if(!(this->connection))
				throw Database::Exception("Main::Database::connect(): Could not connect to database");

			if(!(this->connection->isValid()))
				throw Database::Exception("Main::Database::connect(): Connection to database is invalid");

			// set max_allowed_packet size to maximum of 1 GiB
			//  NOTE: needs to be set independently, setting it in the connection options leads to "invalid read of size 8"
			const int maxAllowedPacket = 1073741824;

			this->connection->setClientOption(
					"OPT_MAX_ALLOWED_PACKET",
					static_cast<const void *>(&maxAllowedPacket)
			);

			// run initializing session commands
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			if(!sqlStatement)
				throw Database::Exception("Main::Database::connect(): Could not create SQL statement");

			// set lock timeout
			Database::sqlExecute(
					sqlStatement,
					"SET @@innodb_lock_wait_timeout = "
					+ std::to_string(MAIN_DATABASE_LOCK_TIMEOUT_SEC)
			);

			// get and save maximum allowed package size
			SqlResultSetPtr sqlMaxAllowedPacketResult(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT @@max_allowed_packet AS value"
					)
			);

			if(sqlMaxAllowedPacketResult && sqlMaxAllowedPacketResult->next()) {
				if(sqlMaxAllowedPacketResult->isNull("value"))
					throw Database::Exception(
							"Main::Database::connect(): database variable \'max_allowed_packet\' is NULL"
					);

				this->maxAllowedPacketSize = sqlMaxAllowedPacketResult->getUInt64("value");

				if(!(this->maxAllowedPacketSize))
					throw Database::Exception(
							"Main::Database::connect(): database variable \'max_allowed_packet\' is zero"
					);
			}
			else
				throw Database::Exception(
						"Main::Database::connect(): Could not get \'max_allowed_packet\' from database"
				);

			// get and save connection ID
			SqlResultSetPtr sqlResultSet(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT CONNECTION_ID() AS id"
					)
			);

			// get result
			if(sqlResultSet && sqlResultSet->next())
				this->connectionId = sqlResultSet->getUInt64("id");
			else
				throw Database::Exception(
						"Main::Database::connect(): Could not get MySQL connection ID"
				);

			// get and save main data directory
			SqlResultSetPtr sqlDataDirResult(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT @@datadir AS value"
					)
			);

			if(sqlDataDirResult && sqlDataDirResult->next()) {
				if(sqlDataDirResult->isNull("value"))
					throw Database::Exception(
							"Main::Database::connect(): database variable \'datadir\' is NULL"
					);

				this->dataDir = sqlDataDirResult->getString("value");

				// trim path and remove last separator if necessary
				Helper::Strings::trim(this->dataDir);

				if(this->dataDir.size() > 1 && this->dataDir.back() == Helper::FileSystem::getPathSeparator())
					this->dataDir.pop_back();

				if(this->dataDir.empty())
					throw Database::Exception(
							"Main::Database::connect(): database variable \'datadir\' is empty"
					);

				// add main data directory to all data directories
				this->dirs.emplace_back(this->dataDir);
			}
			else
				throw Database::Exception(
						"Main::Database::connect(): Could not get variable \'datadir\' from database"
				);

			// get and save InnoDB directories
			SqlResultSetPtr sqlInnoDirsResult(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT @@innodb_directories AS value"
					)
			);

			if(sqlInnoDirsResult && sqlInnoDirsResult->next()) {
				if(!(sqlInnoDirsResult->isNull("value"))) {
					const std::vector<std::string> innoDirs(
							Helper::Strings::split(
									sqlInnoDirsResult->getString("value"),
									';'
							)
					);

					// add InnoDB directories to all data directories
					this->dirs.insert(this->dirs.end(), innoDirs.begin(), innoDirs.end());
				}
			}
			else
				throw Database::Exception(
						"Main::Database::connect(): Could not get variable \'innodb_directories\' from database"
				);

			// get additional directories
			SqlResultSetPtr sqlInnoHomeDirResult(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT @@innodb_data_home_dir AS value"
					)
			);

			if(
					sqlInnoHomeDirResult
					&& sqlInnoHomeDirResult->next()
					&& !(sqlInnoHomeDirResult->isNull("value"))
			) {
				const std::string innoHomeDir(sqlInnoHomeDirResult->getString("value"));

				if(!innoHomeDir.empty())
					this->dirs.emplace_back(innoHomeDir);
			}

			SqlResultSetPtr sqlInnoUndoDirResult(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT @@innodb_undo_directory AS value"
					)
			);

			if(
					sqlInnoUndoDirResult
					&& sqlInnoUndoDirResult->next()
					&& !(sqlInnoUndoDirResult->isNull("value"))
			) {
				const std::string innoUndoDir(sqlInnoUndoDirResult->getString("value"));

				if(!innoUndoDir.empty())
					this->dirs.emplace_back(innoUndoDir);
			}

			// sort directories and remvove duplicates
			std::sort(this->dirs.begin(), this->dirs.end());

			this->dirs.erase(std::unique(this->dirs.begin(), this->dirs.end()), this->dirs.end());
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::connect", e); }
	}

	// run initializing SQL commands by processing all .sql files in the SQL (sub-)folder
	void Database::initializeSql() {
		// execute all SQL files in SQL directory
		for(const auto& sqlFile : Helper::FileSystem::listFilesInPath(MAIN_DATABASE_SQL_DIRECTORY, ".sql"))
			this->run(sqlFile);
	}

	// prepare basic SQL statements (getting last ID, logging and thread status)
	void Database::prepare() {
		// reserve memory for SQL statements
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(std::size_t));

		try {
			// prepare basic SQL statements
			if(!(this->ps.lastId)) {
				this->ps.lastId = this->addPreparedStatement(
						"SELECT LAST_INSERT_ID() AS id"
				);
			}

			if(!(this->ps.log)) {
				this->ps.log = this->addPreparedStatement(
						"INSERT INTO crawlserv_log(module, entry) VALUES (?, ?)"
				);
			}

			// prepare thread statements
			if(!(this->ps.setThreadStatus)) {
				this->ps.setThreadStatus = this->addPreparedStatement(
						"UPDATE crawlserv_threads SET status = ?, paused = ? WHERE id = ? LIMIT 1"
				);
			}

			if(!(this->ps.setThreadStatusMessage)) {
				this->ps.setThreadStatusMessage = this->addPreparedStatement(
						"UPDATE crawlserv_threads SET status = ? WHERE id = ? LIMIT 1"
				);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::prepare", e); }
	}

	// update tables with general information
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
		const std::vector<std::string> locales(Helper::Portability::enumLocales());

		if(!locales.empty()) {
			std::string sqlQuery("INSERT INTO `crawlserv_locales`(name) VALUES");

			for(std::size_t n = 0; n < locales.size(); ++n)
				sqlQuery += " (?),";

			sqlQuery.pop_back();

			// check database connection
			this->checkConnection();

			// write installed locales to database
			try {
				// prepare SQL statement
				SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(
						sqlQuery
				));

				// execute SQL statement
				std::size_t counter = 1;

				for(const auto& locale : locales) {
					sqlStatement->setString(counter, locale);

					++counter;
				}

				Database::sqlExecute(sqlStatement);
			}
			catch(const sql::SQLException &e) { this->sqlException("Main::Database::update", e); }
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
		auto versions(Helper::Versions::getLibraryVersions());

		// add crawlserv++ version
		versions.emplace_back("crawlserv++", Main::Version::getString());

		if(!versions.empty()) {
			std::string sqlQuery("INSERT INTO `crawlserv_versions`(name, version) VALUES");

			for(std::size_t n = 0; n < versions.size(); ++n)
				sqlQuery += " (?, ?),";

			sqlQuery.pop_back();

			// check database connection
			this->checkConnection();

			// write library versions to database
			try {
				// prepare SQL statement
				SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(
						sqlQuery
				));

				// execute SQL statement
				std::size_t counter = 1;

				for(const auto& version : versions) {
					sqlStatement->setString(counter, version.first);
					sqlStatement->setString(counter + 1, version.second);

					counter += 2;
				}

				// execute SQL statement
				Database::sqlExecute(sqlStatement);
			}
			catch(const sql::SQLException &e) { this->sqlException("Main::Database::update", e); }
		}
	}

	/*
	 * LOGGING FUNCTIONS
	 */

	// add a log entry to the database (for any module), remove invalid UTF-8 characters if needed,
	//  throws Database::Exception
	void Database::log(const std::string& logModule, const std::string& logEntry) {
		std::string repairedEntry;

		// repair invalid UTF-8 in argument
		const bool repaired = Helper::Utf8::repairUtf8(logEntry, repairedEntry);

		if(repaired)
			repairedEntry += " [invalid UTF-8 character(s) removed from log]";

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.log))
			throw Database::Exception("Missing prepared SQL statement for Main::Database::log(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.log));

		// add entry to database
		try {
			// execute SQL query
			if(logModule.empty())
				sqlStatement.setString(1, "[unknown]");
			else
				sqlStatement.setString(1, logModule);

			if(logEntry.empty())
				sqlStatement.setString(2, "[empty]");
			else {
				if(repaired)
					sqlStatement.setString(2, repairedEntry);
				else
					sqlStatement.setString(2, logEntry);
			}

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			// write log entry to console instead
			std::cout << '\n' << logModule << ": " << logEntry << std::flush;

			this->sqlException("Main::Database::log", e);
		}
	}

	// add a log entry to the database (for the current module)
	void Database::log(const std::string& logEntry) {
		this->log(this->module, logEntry);
	}

	// get the number of log entries for a specific module from the database
	//	(or for all modules if logModule is an empty string), throws Database::Exception
	std::uint64_t Database::getNumberOfLogEntries(const std::string& logModule) {
		std::uint64_t result = 0;

		// check connection
		this->checkConnection();

		// create SQL query string
		std::string sqlQuery("SELECT COUNT(*) FROM `crawlserv_log`");

		if(!logModule.empty())
			sqlQuery += " WHERE module = ?";

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// execute SQL statement
			if(!logModule.empty())
				sqlStatement->setString(1, logModule);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64(1);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getNumberOfLogEntries", e); }

		return result;
	}

	// remove the log entries of a specific module from the database
	// 	(or all log entries if logModule is an empty string), throws Database::Exception
	void Database::clearLogs(const std::string& logModule) {
		if(logModule.empty())
			// execute SQL query
			this->execute("TRUNCATE TABLE `crawlserv_log`");
		else {
			// check connection
			this->checkConnection();

			try {
				// prepare SQL statement
				SqlPreparedStatementPtr sqlStatement(
						this->connection->prepareStatement(
								"DELETE FROM `crawlserv_log` WHERE module = ?"
						)
				);

				// execute SQL statement
				sqlStatement->setString(1, logModule);

				Database::sqlExecute(sqlStatement);

				// reset auto-increment if table is (still) empty
				if(this->isTableEmpty("crawlserv_log"))
					this->resetAutoIncrement("crawlserv_log");
			}
			catch(const sql::SQLException &e) { this->sqlException("Main::Database::clearLogs", e); }
		}
	}

	/*
	 * THREAD FUNCTIONS
	 */

	// get all threads from the database, throws Database::Exception
	std::vector<Database::ThreadDatabaseEntry> Database::getThreads() {
		std::vector<ThreadDatabaseEntry> result;

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement,
					"SELECT id, module, status, paused, website, urllist, config, last"
					" FROM `crawlserv_threads`"
			));

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
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getThreads", e); }

		return result;
	}

	// add a thread to the database and return its new ID, throws Database::Exception
	std::uint64_t Database::addThread(const ThreadOptions& threadOptions) {
		std::uint64_t result = 0;

		// check arguments
		if(threadOptions.module.empty())
			throw Database::Exception("Main::Database::addThread(): No thread module specified");

		if(!threadOptions.website)
			throw Database::Exception("Main::Database::addThread(): No website specified");

		if(!threadOptions.urlList)
			throw Database::Exception("Main::Database::addThread(): No URL list specified");

		if(!threadOptions.config)
			throw Database::Exception("Main::Database::addThread(): No configuration specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(
					"INSERT INTO crawlserv_threads(module, website, urllist, config) VALUES (?, ?, ?, ?)"
			));

			// execute SQL statement
			sqlStatement->setString(1, threadOptions.module);
			sqlStatement->setUInt64(2, threadOptions.website);
			sqlStatement->setUInt64(3, threadOptions.urlList);
			sqlStatement->setUInt64(4, threadOptions.config);

			Database::sqlExecute(sqlStatement);

			// get ID
			result = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addThread", e); }

		return result;
	}

	// get run time of thread (in seconds) from the database by its ID, throws Database::Exception
	std::uint64_t Database::getThreadRunTime(std::uint64_t threadId) {
		std::uint64_t result = 0;

		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::getThreadRunTime(): No thread ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(
					"SELECT runtime FROM `crawlserv_threads` WHERE id = ? LIMIT 1"
			));

			// execute SQL statement
			sqlStatement->setUInt64(1, threadId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("runtime");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getThreadRunTime", e); }

		return result;
	}

	// get pause time of thread (in seconds) from the database by its ID, throws Database::Exception
	std::uint64_t Database::getThreadPauseTime(std::uint64_t threadId) {
		std::uint64_t result = 0;

		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::getThreadPauseTime(): No thread ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT pausetime"
							" FROM `crawlserv_threads`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, threadId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("pausetime");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getThreadPauseTime", e); }

		return result;
	}

	// update thread status in the database (and add the pause state to the status message if necessary),
	//  throws Database::Exception
	void Database::setThreadStatus(std::uint64_t threadId, bool threadPaused, const std::string& threadStatusMessage) {
		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::setThreadStatus(): No thread ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadStatus))
			throw Database::Exception("Missing prepared SQL statement for Main::Database::setThreadStatus(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.setThreadStatus));

		// create status message
		std::string statusMessage;

		if(threadPaused) {
			if(!threadStatusMessage.empty())
				statusMessage = "PAUSED " + threadStatusMessage;
			else
				statusMessage = "PAUSED";
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
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadStatus", e); }
	}

	// update thread status in the database (without using or changing the pause state),
	//  throws Database::Exception
	void Database::setThreadStatus(std::uint64_t threadId, const std::string& threadStatusMessage) {
		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::setThreadStatus(): No thread ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadStatusMessage))
			throw Database::Exception("Missing prepared SQL statement for Main::Database::setThreadStatus(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.setThreadStatusMessage));

		try {
			// execute SQL statement
			sqlStatement.setString(1, threadStatusMessage);
			sqlStatement.setUInt64(2, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadStatus", e); }
	}

	// set run time of thread (in seconds) in the database, throws Database::Exception
	void Database::setThreadRunTime(std::uint64_t threadId, std::uint64_t threadRunTime) {
		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::setThreadRunTime(): No thread ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"UPDATE crawlserv_threads"
							" SET runtime = ?"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, threadRunTime);
			sqlStatement->setUInt64(2, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadRunTime", e); }
	}

	// set pause time of thread (in seconds) in the database, throws Database::Exception
	void Database::setThreadPauseTime(std::uint64_t threadId, std::uint64_t threadPauseTime) {
		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::setThreadPauseTime(): No thread ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"UPDATE crawlserv_threads"
							" SET pausetime = ?"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, threadPauseTime);
			sqlStatement->setUInt64(2, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadPauseTime", e); }
	}

	// remove thread from the database by its ID, throws Database::Exception
	void Database::deleteThread(std::uint64_t threadId) {
		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::deleteThread(): No thread ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"DELETE FROM `crawlserv_threads`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, threadId);

			Database::sqlExecute(sqlStatement);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_threads"))
				this->resetAutoIncrement("crawlserv_threads");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteThread", e); }
	}

	/*
	 * WEBSITE FUNCTIONS
	 */

	// add a website to the database and return its new ID, throws Database::Exception
	std::uint64_t Database::addWebsite(const WebsiteProperties& websiteProperties) {
		std::uint64_t result = 0;
		std::string timeStamp;

		// check arguments
		if(websiteProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::addWebsite(): No website namespace specified");

		if(websiteProperties.name.empty())
			throw Database::Exception("Main::Database::addWebsite(): No website name specified");

		// check website namespace
		if(this->isWebsiteNamespace(websiteProperties.nameSpace))
			throw Database::Exception("Main::Database::addWebsite(): Website namespace already exists");

		// check directory
		if(!websiteProperties.dir.empty() && !Helper::FileSystem::isValidDirectory(websiteProperties.dir))
			throw Database::Exception("Main::Database::addWebsite(): Data directory does not exist");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for adding website
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"INSERT INTO crawlserv_websites(domain, namespace, name, dir)"
							" VALUES (?, ?, ?, ?)"
					)
			);

			// execute SQL statement for adding website
			if(websiteProperties.domain.empty())
				sqlStatement->setNull(1, 0);
			else
				sqlStatement->setString(1, websiteProperties.domain);

			sqlStatement->setString(2, websiteProperties.nameSpace);
			sqlStatement->setString(3, websiteProperties.name);

			if(websiteProperties.dir.empty())
				sqlStatement->setNull(4, 0);
			else
				sqlStatement->setString(4, websiteProperties.dir);

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
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addWebsite", e); }

		return result;
	}

	// get website domain from the database by its ID, throws Database::Exception
	std::string Database::getWebsiteDomain(std::uint64_t websiteId) {
		std::string result;

		// check argument
		if(!websiteId)
			throw Database::Exception("Main::Database::getWebsiteDomain(): No website ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT domain"
							" FROM `crawlserv_websites`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next() && !(sqlResultSet->isNull("domain")))
				result = sqlResultSet->getString("domain");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteDomain", e); }

		return result;
	}

	// get namespace of website from the database by its ID, throws Database::Exception
	std::string Database::getWebsiteNamespace(std::uint64_t websiteId) {
		std::string result;

		// check argument
		if(!websiteId)
			throw Database::Exception("Main::Database::getWebsiteNamespace(): No website ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT namespace"
							" FROM `crawlserv_websites`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("namespace");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespace", e); }

		return result;
	}

	// get ID and namespace of website from the database by URL list ID, throws Database::Exception
	Database::IdString Database::getWebsiteNamespaceFromUrlList(std::uint64_t listId) {
		std::uint64_t websiteId = 0;

		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::getWebsiteNamespaceFromUrlList(): No URL list ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT website"
							" FROM `crawlserv_urllists`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, listId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				websiteId = sqlResultSet->getUInt64("website");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespaceFromUrlList", e); }

		return IdString(websiteId, this->getWebsiteNamespace(websiteId));
	}

	// get ID and namespace of website from the database by configuration ID, throws Database::Exception
	Database::IdString Database::getWebsiteNamespaceFromConfig(std::uint64_t configId) {
		std::uint64_t websiteId = 0;

		// check argument
		if(!configId)
			throw Database::Exception("Main::Database::getWebsiteNamespaceFromConfig(): No configuration ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT website"
							" FROM `crawlserv_configs`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, configId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				websiteId = sqlResultSet->getUInt64("website");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespaceFromConfig", e); }

		return IdString(websiteId, this->getWebsiteNamespace(websiteId));
	}

	// get ID and namespace of website from the database by target table ID of specified type,
	//  throws Database::Exception
	Database::IdString Database::getWebsiteNamespaceFromTargetTable(const std::string& type, std::uint64_t tableId) {
		std::uint64_t websiteId = 0;

		// check arguments
		if(type.empty())
			throw Database::Exception("Main::Database::getWebsiteNamespaceFromCustomTable(): No table type specified");

		if(!tableId)
			throw Database::Exception("Main::Database::getWebsiteNamespaceFromCustomTable(): No table ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT website"
							" FROM `crawlserv_" + type + "tables`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, tableId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				websiteId = sqlResultSet->getUInt64("website");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespaceFromCustomTable", e); }

		return IdString(websiteId, this->getWebsiteNamespace(websiteId));
	}

	// check whether website namespace exists in the database, throws Database::Exception
	bool Database::isWebsiteNamespace(const std::string& nameSpace) {
		bool result = false;

		// check argument
		if(nameSpace.empty())
			throw Database::Exception("Main::Database::isWebsiteNamespace(): No namespace specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT EXISTS"
							" ("
								" SELECT *"
								" FROM `crawlserv_websites`"
								" WHERE namespace = ?"
							" )"
							" AS result"
					)
			);

			// execute SQL statement
			sqlStatement->setString(1, nameSpace);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isWebsiteNamespace", e); }

		return result;
	}

	// create new website namespace for duplicated website, throws Database::Exception
	std::string Database::duplicateWebsiteNamespace(const std::string& websiteNamespace) {
		// check argument
		if(websiteNamespace.empty())
			throw Database::Exception("Main::Database::duplicateWebsiteNamespace(): No namespace specified");

		std::string nameString, numberString;
		const auto end = websiteNamespace.find_last_not_of("0123456789");

		// separate number at the end of string from the rest of the string
		if(end == std::string::npos)
			// string is number
			numberString = websiteNamespace;
		else if(end == websiteNamespace.length() - 1)
			// no number at the end of the string
			nameString = websiteNamespace;
		else {
			// number at the end of the string
			nameString = websiteNamespace.substr(0, end + 1);
			numberString = websiteNamespace.substr(end + 1);
		}

		std::size_t n = 1;
		std::string result;

		if(!numberString.empty()) {
			try {
				n = std::stoul(numberString, nullptr);
			}
			catch(const std::logic_error& e) {
				throw Exception(
						"Main::Database::duplicateWebsiteNamespace(): Could not convert \'"
						+ numberString
						+ "\' to unsigned numeric value"
				);
			}
		}

		// check whether number needs to be incremented
		while(true) {
			// increment number at the end of the string
			++n;

			result = nameString + std::to_string(n);

			if(!(this->isWebsiteNamespace(result)))
				break;
		}

		return result;
	}

	// get data directory of website or empty string if default directory is used, throws Database::Exception
	std::string Database::getWebsiteDataDirectory(std::uint64_t websiteId) {
		std::string result;

		// check argument
		if(!websiteId)
			throw Database::Exception("Main::Database::getWebsiteDataDirectory(): No website ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT dir"
							" FROM `crawlserv_websites`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next() && !(sqlResultSet->isNull("dir")))
				result = sqlResultSet->getString("dir");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespace", e); }

		return result;
	}

	// get number of URLs that will be modified by updating the website, throws Database::Exception
	std::uint64_t Database::getChangedUrlsByWebsiteUpdate(
			std::uint64_t websiteId,
			const WebsiteProperties& websiteProperties
	) {
		std::uint64_t result = 0;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::getChangedUrlsByWebsiteUpdate(): No website ID specified");

		if(websiteProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::getChangedUrlsByWebsiteUpdate(): No website namespace specified");

		if(websiteProperties.name.empty())
			throw Database::Exception("Main::Database::getChangedUrlsByWebsiteUpdate(): No website name specified");

		// get old namespace and domain
		const std::string oldNamespace = this->getWebsiteNamespace(websiteId);
		const std::string oldDomain = this->getWebsiteDomain(websiteId);

		// check connection
		this->checkConnection();

		try {
			if(oldDomain.empty() != websiteProperties.domain.empty()) {
				// get URL lists
				std::queue<IdString> urlLists(this->getUrlLists(websiteId));

				// create SQL statement
				SqlStatementPtr sqlStatement(this->connection->createStatement());

				if(oldDomain.empty() && !websiteProperties.domain.empty()) {
					// type changed from cross-domain to specific domain:
					// 	change URLs from absolute (URLs without protocol) to relative (sub-URLs),
					// 	discarding URLs with different domain
					while(!urlLists.empty()) {
						// count URLs of same domain
						std::string comparison("url LIKE '" + websiteProperties.domain + "/%'");

						if(websiteProperties.domain.size() > 4 && websiteProperties.domain.substr(0, 4) == "www.")
							comparison += " OR url LIKE '" + websiteProperties.domain.substr(4) + "/%'";
						else
							comparison += " OR url LIKE 'www." + websiteProperties.domain + "/%'";

						SqlResultSetPtr sqlResultSet(
								Database::sqlExecuteQuery(
										sqlStatement,
										"SELECT COUNT(*) AS result"
										" FROM `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "`"
										" WHERE " + comparison
								)
						);

						if(sqlResultSet && sqlResultSet->next())
							result += sqlResultSet->getUInt64("result");

						urlLists.pop();
					}
				}
				else {
					// type changed from specific domain to cross-domain:
					// 	change URLs from relative (sub-URLs) to absolute (URLs without protocol)
					while(!urlLists.empty()) {
						// count URLs
						SqlResultSetPtr sqlResultSet(
								Database::sqlExecuteQuery(
										sqlStatement,
										"SELECT COUNT(*) AS result"
										" FROM `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "`"
								)
						);

						if(sqlResultSet && sqlResultSet->next())
							result += sqlResultSet->getUInt64("result");

						urlLists.pop();
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getChangedUrlsByWebsiteUpdate", e); }

		return result;
	}

	// get number of URLs that will be lost by updating the website, throws Database::Exception
	std::uint64_t Database::getLostUrlsByWebsiteUpdate(
			std::uint64_t websiteId,
			const WebsiteProperties& websiteProperties
	) {
		std::uint64_t result = 0;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::getLostUrlsByWebsiteUpdate(): No website ID specified");

		if(websiteProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::getLostUrlsByWebsiteUpdate(): No website namespace specified");

		if(websiteProperties.name.empty())
			throw Database::Exception("Main::Database::getLostUrlsByWebsiteUpdate(): No website name specified");

		// get old namespace and domain
		const std::string oldNamespace(this->getWebsiteNamespace(websiteId));
		const std::string oldDomain(this->getWebsiteDomain(websiteId));

		// check connection
		this->checkConnection();

		try {
			if(oldDomain.empty() && !websiteProperties.domain.empty()) {
				// get URL lists
				std::queue<IdString> urlLists(this->getUrlLists(websiteId));

				// create SQL statement
				SqlStatementPtr sqlStatement(this->connection->createStatement());

				while(!urlLists.empty()) {
					// count URLs of different domain
					std::string comparison("url NOT LIKE '" + websiteProperties.domain + "/%'");

					if(websiteProperties.domain.size() > 4 && websiteProperties.domain.substr(0, 4) == "www.")
						comparison += " AND url NOT LIKE '" + websiteProperties.domain.substr(4) + "/%'";
					else
						comparison += " AND url NOT LIKE 'www." + websiteProperties.domain + "/%'";

					SqlResultSetPtr sqlResultSet(
							Database::sqlExecuteQuery(
									sqlStatement,
									"SELECT COUNT(*) AS result"
									" FROM `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "`"
									" WHERE " + comparison
							)
					);

					if(sqlResultSet && sqlResultSet->next())
						result += sqlResultSet->getUInt64("result");

					urlLists.pop();
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getLostUrlsByWebsiteUpdate", e); }

		return result;
	}

	// update website (and all associated tables) in the database, throws Database::Exception
	void Database::updateWebsite(std::uint64_t websiteId, const WebsiteProperties& websiteProperties) {
		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::updateWebsite(): No website ID specified");

		if(websiteProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::updateWebsite(): No website namespace specified");

		if(websiteProperties.name.empty())
			throw Database::Exception("Main::Database::updateWebsite(): No website name specified");

		// get old namespace and domain
		const std::string oldNamespace(this->getWebsiteNamespace(websiteId));
		const std::string oldDomain(this->getWebsiteDomain(websiteId));

		// check website namespace if necessary
		if(websiteProperties.nameSpace != oldNamespace)
			if(this->isWebsiteNamespace(websiteProperties.nameSpace))
				throw Database::Exception("Main::Database::updateWebsite(): Website namespace already exists");

		// check connection
		this->checkConnection();

		try {
			// check whether the type of the website (can either be cross-domain or a specific domain) has changed
			if(oldDomain.empty() != websiteProperties.domain.empty()) {
				// get URL lists
				std::queue<IdString> urlLists(this->getUrlLists(websiteId));

				// create SQL statement for modifying and deleting URLs
				SqlStatementPtr urlStatement(this->connection->createStatement());

				if(oldDomain.empty() && !websiteProperties.domain.empty()) {
					// type changed from cross-domain to specific domain:
					// 	change URLs from absolute (URLs without protocol) to relative (sub-URLs),
					// 	discarding URLs with different domain
					while(!urlLists.empty()) {
						// update URLs of same domain
						std::string comparison("url LIKE '" + websiteProperties.domain + "/%'");

						if(websiteProperties.domain.size() > 4 && websiteProperties.domain.substr(0, 4) == "www.")
							comparison += " OR url LIKE '" + websiteProperties.domain.substr(4) + "/%'";
						else
							comparison += " OR url LIKE 'www." + websiteProperties.domain + "/%'";

						Database::sqlExecute(
								urlStatement,
								"UPDATE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "`"
								" SET url = SUBSTR(url, LOCATE('/', url))"
								" WHERE " + comparison
						);

						// delete URLs of different domain
						Database::sqlExecute(
								urlStatement,
								"DELETE FROM `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "`"
								" WHERE LEFT(url, 1) != '/'"
						);

						urlLists.pop();
					}
				}
				else if(!oldDomain.empty() && websiteProperties.domain.empty()) {
					// type changed from specific domain to cross-domain:
					// 	change URLs from relative (sub-URLs) to absolute (URLs without protocol)
					std::queue<IdString> urlLists(this->getUrlLists(websiteId));

					while(!urlLists.empty()) {
						// update URLs
						Database::sqlExecute(
							urlStatement,
							"UPDATE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "`"
							" SET url = CONCAT(\'" + oldDomain + "\', url)"
						);

						urlLists.pop();
					}
				}
			}

			// check whether namespace has changed
			if(websiteProperties.nameSpace != oldNamespace) {
				// create SQL statement for renaming
				SqlStatementPtr renameStatement(this->connection->createStatement());

				// rename tables
				std::queue<IdString> urlLists(this->getUrlLists(websiteId));
				std::queue<IdString> tables;

				while(!urlLists.empty()) {
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "`"
							" RENAME TO `crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "`"
					);

					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "_crawled`"
							" RENAME TO `crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_crawled`"
					);

					// rename crawling table
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "_crawling`"
							" RENAME TO `crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_crawling`"
					);

					// rename parsing tables
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "_parsing`"
							" RENAME TO `crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_parsing`"
					);

					tables = this->getTargetTables("parsed", urlLists.front().first);

					while(!tables.empty()) {
						Database::sqlExecute(
								renameStatement,
								"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "_parsed_"
								+ tables.front().second + "`"
								" RENAME TO `" + "crawlserv_" + websiteProperties.nameSpace + "_"
								+ urlLists.front().second + "_parsed_" + tables.front().second + "`"
						);

						tables.pop();
					}

					// rename extracting tables
					Database::sqlExecute(renameStatement,
							"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "_extracting`"
							" RENAME TO"
							" `crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_extracting`"
					);

					tables = this->getTargetTables("extracted", urlLists.front().first);

					while(!tables.empty()) {
						Database::sqlExecute(renameStatement,
								"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "_extracted_"
								+ tables.front().second + "`"
								" RENAME TO `" + "crawlserv_" + websiteProperties.nameSpace + "_"
								+ urlLists.front().second + "_extracted_" + tables.front().second + "`"
						);

						tables.pop();
					}

					// rename analyzing tables
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "_analyzing`"
							" RENAME TO"
							+ " `crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_analyzing`"
					);

					tables = this->getTargetTables("analyzed", urlLists.front().first);

					while(!tables.empty()) {
						Database::sqlExecute(renameStatement,
								"ALTER TABLE `crawlserv_" + oldNamespace + "_" + urlLists.front().second + "_analyzed_"
								+ tables.front().second + "`"
								" RENAME TO `" + "crawlserv_" + websiteProperties.nameSpace + "_"
								+ urlLists.front().second + "_analyzed_" + tables.front().second + "`"
						);

						tables.pop();
					}

					// URL list done
					urlLists.pop();
				}

				// prepare SQL statement for updating
				SqlPreparedStatementPtr updateStatement(
						this->connection->prepareStatement(
								"UPDATE crawlserv_websites"
								" SET domain = ?, namespace = ?, name = ?"
								" WHERE id = ?"
								" LIMIT 1"
						)
				);

				// execute SQL statement for updating
				if(websiteProperties.domain.empty())
					updateStatement->setNull(1, 0);
				else
					updateStatement->setString(1, websiteProperties.domain);

				updateStatement->setString(2, websiteProperties.nameSpace);
				updateStatement->setString(3, websiteProperties.name);
				updateStatement->setUInt64(4, websiteId);

				Database::sqlExecute(updateStatement);
			}
			else {
				// prepare SQL statement for updating
				SqlPreparedStatementPtr updateStatement(
						this->connection->prepareStatement(
								"UPDATE crawlserv_websites"
								" SET domain = ?, name = ?"
								" WHERE id = ?"
								" LIMIT 1"
						)
				);

				// execute SQL statement for updating
				if(websiteProperties.domain.empty())
					updateStatement->setNull(1, 0);
				else
					updateStatement->setString(1, websiteProperties.domain);

				updateStatement->setString(2, websiteProperties.name);
				updateStatement->setUInt64(3, websiteId);

				Database::sqlExecute(updateStatement);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateWebsite", e); }

		// check whether data directory has to be changed
		if(this->getWebsiteDataDirectory(websiteId) != websiteProperties.dir)
			this->moveWebsite(websiteId, websiteProperties);
	}

	// delete website (and all associated data) from the database by its ID, throws Database::Exception
	void Database::deleteWebsite(std::uint64_t websiteId) {
		// check argument
		if(!websiteId)
			throw Database::Exception("Main::Database::deleteWebsite(): No website ID specified");

		// get website namespace
		std::string websiteNamespace(this->getWebsiteNamespace(websiteId));

		try {
			// delete URL lists
			std::queue<IdString> urlLists(this->getUrlLists(websiteId));

			while(!urlLists.empty()) {
				// delete URL list
				this->deleteUrlList(urlLists.front().first);

				urlLists.pop();
			}

			// check connection
			this->checkConnection();

			// prepare SQL statement for deletion of website
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"DELETE FROM `crawlserv_websites`"
							" WHERE id = ? LIMIT 1"
					)
			);

			// execute SQL statements for deletion of website
			sqlStatement->setUInt64(1, websiteId);

			Database::sqlExecute(sqlStatement);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_websites"))
				this->resetAutoIncrement("crawlserv_websites");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteWebsite", e); }
	}

	// duplicate website in the database by its ID (no processed data will be duplicated),
	//  throws Database::Exception
	std::uint64_t Database::duplicateWebsite(std::uint64_t websiteId, const Queries& queries) {
		std::uint64_t newId = 0;

		// check argument
		if(!websiteId)
			throw Database::Exception("Main::Database::duplicateWebsite(): No website ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for geting website info
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT name, namespace, domain, dir"
							" FROM `crawlserv_websites`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement for geting website info
			sqlStatement->setUInt64(1, websiteId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				const std::string websiteNamespace(sqlResultSet->getString("namespace"));
				const std::string websiteName(sqlResultSet->getString("name"));

				std::string websiteDomain;
				std::string websiteDir;

				if(!(sqlResultSet->isNull("domain")))
					websiteDomain = sqlResultSet->getString("domain");

				if(!(sqlResultSet->isNull("dir")))
					websiteDir = sqlResultSet->getString("dir");

				// create new namespace and new name
				const std::string newNamespace(Database::duplicateWebsiteNamespace(websiteNamespace));
				const std::string newName(websiteName + " (copy)");

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
				sqlStatement->setUInt64(1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					// add URL lists with same name (except for "default", which has already been created)
					const std::string urlListName(sqlResultSet->getString("namespace"));

					if(urlListName != "default")
						this->addUrlList(
								newId,
								UrlListProperties(
										sqlResultSet->getString("namespace"),
										urlListName
								)
						);
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
				sqlStatement->setUInt64(1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					// copy query
					const std::uint64_t oldQueryId = sqlResultSet->getUInt64("id");

					const std::uint64_t newQueryId = this->addQuery(
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
					);

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
				sqlStatement->setUInt64(1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					const std::string module(sqlResultSet->getString("module"));
					std::string config(sqlResultSet->getString("config"));

					// find module in queries
					auto modIt = std::find_if(queries.begin(), queries.end(), [&module](const auto& p) {
						return p.first == module;
					});

					if(modIt != queries.end()) {
						// update queries in configuration
						rapidjson::Document jsonConfig;

						try {
							jsonConfig = Helper::Json::parseRapid(config);
						}
						catch(const JsonException& e) {
							throw Exception("Main::Database::duplicateWebsite(): Could not parse configuration (" + e.whatStr() + ")");
						}

						if(!jsonConfig.IsArray())
							throw Exception(
									"Main::Database::duplicateWebsite(): Configuration is no valid JSON array: \'"
									+ Helper::Json::stringify(jsonConfig)
									+ "\'"
							);

						for(auto& configEntry : jsonConfig.GetArray()) {
							if(!configEntry.IsObject())
								throw Exception(
										"Main::Database::duplicateWebsite(): Configuration contains invalid entry \'"
										+ Helper::Json::stringify(configEntry)
										+ "\'"
								);

							if(!configEntry.HasMember("name"))
								throw Exception(
										"Main::Database::duplicateWebsite(): Configuration entry \'"
										+ Helper::Json::stringify(configEntry)
										+ "\' does not include \'name\'"
								);

							if(!configEntry["name"].IsString())
								throw Exception(
										"Main::Database::duplicateWebsite(): Configuration entry \'"
										+ Helper::Json::stringify(configEntry)
										+ "\' does not include valid string for \'name\'"
								);

							if(!configEntry.HasMember("value"))
								throw Exception(
										"Main::Database::duplicateWebsite(): Configuration entry \'"
										+ Helper::Json::stringify(configEntry)
										+ "\' does not include \'value\'"
								);

							std::string cat;
							const std::string name(
									configEntry["name"].GetString(),
									configEntry["name"].GetStringLength()
							);

							if(name != "_algo") {
								if(!configEntry.HasMember("cat"))
									throw Exception(
											"Main::Database::duplicateWebsite(): Configuration entry \'"
											+ Helper::Json::stringify(configEntry)
											+ "\' does not include \'cat\'"
									);

								if(!configEntry["cat"].IsString())
									throw Exception(
											"Main::Database::duplicateWebsite(): Configuration entry \'"
											+ Helper::Json::stringify(configEntry)
											+ "\' does not include valid string for \'cat\'"
									);

								std::string(
										configEntry["cat"].GetString(),
										configEntry["cat"].GetStringLength()
								).swap(cat);
							}

							const auto queryIt = std::find_if(
									modIt->second.begin(),
									modIt->second.end(),
									[&cat, &name](const auto& p) {
										return p.first == cat && p.second == name;
									}
							);

							if(queryIt != modIt->second.end()) {
								if(configEntry["value"].IsArray()) {
									for(auto& arrayElement : configEntry["value"].GetArray()) {
										if(!arrayElement.IsUint64())
											throw Exception(
													"Main::Database::duplicateWebsite(): Configuration entry \'"
													+ Helper::Json::stringify(configEntry)
													+ "\' includes invalid query ID \'"
													+ Helper::Json::stringify(arrayElement)
													+ "\'"
											);

										const std::uint64_t queryId = arrayElement.GetUint64();

										const auto idsIt = std::find_if(
												ids.begin(),
												ids.end(),
												[&queryId](const auto& p) {
													return p.first == queryId;
												}
										);

										if(idsIt != ids.end()) {
											rapidjson::Value newValue(idsIt->second);

											arrayElement.Swap(newValue);
										}
									}
								}
								else {
									if(!configEntry["value"].IsUint64())
										throw Exception(
												"Main::Database::duplicateWebsite(): Configuration entry \'"
												+ Helper::Json::stringify(configEntry)
												+ "\' includes invalid query ID \'"
												+ Helper::Json::stringify(configEntry["value"])
												+ "\'"
										);

									const std::uint64_t queryId = configEntry["value"].GetUint64();

									const auto idsIt = std::find_if(
											ids.begin(),
											ids.end(),
											[&queryId](const auto& p) {
												return p.first == queryId;
											}
									);

									if(idsIt != ids.end()) {
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
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::duplicateWebsite", e); }

		return newId;
	}

	// move website (and all associated data) to another data directory
	void Database::moveWebsite(std::uint64_t websiteId, const WebsiteProperties& websiteProperties) {
#ifdef MAIN_DATABASE_LOG_MOVING
		Timer::Simple timer;

		std::cout	<< "\n\nMOVING website "
					<< websiteProperties.name
					<< " to \'"
					<< websiteProperties.dir
					<< "\'..."
					<< std::flush;
#endif
		// create table list
		std::vector<std::string> tables;

		// go through all affected URL lists
		std::queue<IdString> urlLists(this->getUrlLists(websiteId));

		while(!urlLists.empty()) {
			// get tables with parsed, extracted and analyzed content
			std::queue<IdString> parsedTables(this->getTargetTables("parsed", urlLists.front().first));
			std::queue<IdString> extractedTables(this->getTargetTables("extracted", urlLists.front().first));
			std::queue<IdString> analyzedTables(this->getTargetTables("analyzed", urlLists.front().first));

			// reserve additional memory for all table names of the current URL list
			tables.reserve(tables.size() + 6 + parsedTables.size() + extractedTables.size() + analyzedTables.size());

			// add main table for URL list
			tables.emplace_back("crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second);

			// add status tables for URL list
			tables.emplace_back("crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_crawling");
			tables.emplace_back("crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_parsing");
			tables.emplace_back("crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_extracting");
			tables.emplace_back("crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_analyzing");

			// add table with crawled content for URL list
			tables.emplace_back("crawlserv_" + websiteProperties.nameSpace + "_" + urlLists.front().second + "_crawled");

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
		for(std::vector<std::string>::const_reverse_iterator i = tables.rbegin(); i != tables.rend(); ++i)
			this->dropTable(*i + "_tmp");

		// clone tables to new data directory (without data or constraints)
		std::queue<StringQueueOfStrings> constraints;

		for(const auto& table : tables) {
#ifdef MAIN_DATABASE_LOG_MOVING
			std::cout << "\n Cloning: `" << table << "`" << std::flush;
#endif

			constraints.emplace(table, this->cloneTable(table, websiteProperties.dir));
		}

		// check connection
		this->checkConnection();

		try { // start first transaction in-scope (copying data)
			Transaction transaction(*this, "READ UNCOMMITTED");

			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// add constraints to tables
			while(!constraints.empty()) {
				if(constraints.front().second.empty()) {
					constraints.pop();

					continue;
				}

				std::string toAdd;

				while(!constraints.front().second.empty()) {
					std::string constraint = constraints.front().second.front();

					// check reference and use temporary table if the table lies inside the namespace of the website
					const auto pos = constraint.find(" `");
					const auto end = constraint.find("` ");

					if(pos != std::string::npos && end != std::string::npos) {
						const std::string nspace("crawlserv_" + websiteProperties.nameSpace + "_");
						const std::string referenced(constraint, pos + 2, end - pos - 2);

						if(referenced.substr(0, nspace.length()) == nspace)
							constraint.insert(end, "_tmp");

						toAdd += " ADD " + constraint;

						if(constraint.back() != ',')
							toAdd += ',';
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
						"ALTER TABLE `" + constraints.front().first + "_tmp`" + toAdd
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
				std::uint64_t count = 0;

				SqlResultSetPtr result1(
						Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT COUNT(*) AS count FROM `" + table + "`"
						)
				);

				if(result1->next() && !(result1->isNull("count")))
					count = result1->getUInt64("count");

				// get names of columns to copy
				std::string columns;

				SqlResultSetPtr result2(
						Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT "
							" COLUMN_NAME AS name "
							"FROM INFORMATION_SCHEMA.COLUMNS "
							"WHERE"
							" TABLE_SCHEMA = '" + this->settings.name + "'"
							" AND TABLE_NAME = '" + table + "'"
						)
				);

				while(result2->next())
					if(!(result2->isNull("name")))
						columns += "`" + result2->getString("name") + "`, ";

				if(columns.empty())
					continue;

				columns.pop_back();
				columns.pop_back();

#ifdef MAIN_DATABASE_LOG_MOVING
				if(count < 100)
#endif
					// copy all at once
					Database::sqlExecute(
							sqlStatement,
							"INSERT INTO `" + table + "_tmp`(" + columns + ")"
							" SELECT " + columns +
							" FROM `" + table + "`"
					);
#ifdef MAIN_DATABASE_LOG_MOVING
				else {
					// copy in 100 (or 101) steps for logging the progress
					std::cout << "     " << std::flush;

					const auto step = count / 100;

					for(auto n = 0; n <= 100; ++n) {
						Database::sqlExecute(
								sqlStatement,
								"INSERT INTO `"
								+ table
								+ "_tmp`(" + columns + ")"
								  " SELECT "
								+    columns
								+ " FROM `"
								+ table
								+ "` AS t"
								  " JOIN "
								  " ("
								   " SELECT"
								   " COALESCE(MAX(id), 0) AS offset"
								   " FROM `"
								+    table
								+    "_tmp`"
								  " ) AS m"
								" ON t.id > m.offset"
								" ORDER BY t.id"
								" LIMIT "
								+ std::to_string(step)
						);

						std::cout << "\b\b\b\b";

						if(n < 100)
							std::cout << ' ';

						if(n < 10)
							std::cout << ' ';

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

			this->sqlException("Main::Database::moveWebsite", e);
		}
#ifdef MAIN_DATABASE_LOG_MOVING
		catch(const Exception& e) {
			std::cout << "\n " << e.what() << std::endl;

			throw;
		}
#endif
		// </EXCEPTION HANDLING>

		// check connection
		this->checkConnection();

		try { // start second transaction in-scope (replacing tables)
			Transaction transaction(*this);

			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// delete old tables in reverse order (to avoid problems with constraints)
			for(std::vector<std::string>::const_reverse_iterator i = tables.rbegin(); i != tables.rend(); ++i) {
#ifdef MAIN_DATABASE_LOG_MOVING
				std::cout << "\n Deleting: `" << *i << "`" << std::flush;
#endif

				Database::sqlExecute(
						sqlStatement,
						"DROP TABLE IF EXISTS `" + *i + "`"
				);
			}

			// rename new tables
			for(const auto& table : tables) {
#ifdef MAIN_DATABASE_LOG_MOVING
				std::cout << "\n Renaming: `" << table << "_tmp`" << std::flush;
#endif

				Database::sqlExecute(
						sqlStatement,
						"RENAME TABLE `" + table + "_tmp` TO `" + table + "`"
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

			this->sqlException("Main::Database::moveWebsite", e);
		}
#ifdef MAIN_DATABASE_LOG_MOVING
		catch(const Exception& e) {
			std::cout << "\n " << e.what() << std::endl;

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

	// add a URL list to the database and return its new ID, throws Database::Exception
	std::uint64_t Database::addUrlList(std::uint64_t websiteId, const UrlListProperties& listProperties) {
		std::uint64_t newId = 0;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::addUrlList(): No website ID specified");

		if(listProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::addUrlList(): No URL list namespace specified");

		if(listProperties.name.empty())
			throw Database::Exception("Main::Database::addUrlList(): No URL list name specified");

		// get website namespace and data directory
		const std::string websiteNamespace(this->getWebsiteNamespace(websiteId));
		const std::string websiteDataDirectory(this->getWebsiteDataDirectory(websiteId));

		// check URL list namespace
		if(this->isUrlListNamespace(websiteId, listProperties.nameSpace))
			throw Database::Exception("Main::Database::addUrlList(): URL list namespace already exists");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for adding URL list
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"INSERT INTO crawlserv_urllists(website, namespace, name)"
							" VALUES (?, ?, ?)"
					)
			);

			// execute SQL query for adding URL list
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setString(2, listProperties.nameSpace);
			sqlStatement->setString(3, listProperties.name);

			Database::sqlExecute(sqlStatement);

			// get ID of newly added URL list
			newId = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addUrlList", e); }

		// create table for URL list
		std::vector<TableColumn> columns;

		columns.reserve(11);

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
		columns.emplace_back("crawltime", "DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP NOT NULL", true);
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
						"crawlserv_" + websiteNamespace  + "_" + listProperties.nameSpace + "_crawling",
						columns,
						websiteDataDirectory,
						false
				)
		);

		columns.clear();

		// create table for parsing
		columns.emplace_back("target", "BIGINT UNSIGNED NOT NULL", "crawlserv_parsedtables", "id");
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
		columns.emplace_back("target", "BIGINT UNSIGNED NOT NULL", "crawlserv_extractedtables", "id");
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
		columns.emplace_back("target", "BIGINT UNSIGNED NOT NULL", "crawlserv_analyzedtables", "id");
		columns.emplace_back("chunk_id", "BIGINT UNSIGNED DEFAULT NULL");
		columns.emplace_back("chunk_label", "TINYTEXT DEFAULT NULL");
		columns.emplace_back("algo", "TINYTEXT NOT NULL");
		columns.emplace_back("locktime", "DATETIME DEFAULT NULL");
		columns.emplace_back("success", "BOOLEAN DEFAULT FALSE NOT NULL");

		this->createTable(
				TableProperties(
						"crawlserv_" + websiteNamespace  + "_" + listProperties.nameSpace + "_analyzing",
						columns,
						websiteDataDirectory,
						false
				)
		);

		columns.clear();

		return newId;
	}

	// get URL lists for ID-specified website from the database, throws Database::Exception
	std::queue<Database::IdString> Database::getUrlLists(std::uint64_t websiteId) {
		std::queue<IdString> result;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::getUrlLists(): No website ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT id, namespace"
							" FROM `crawlserv_urllists`"
							" WHERE website = ?"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get results
			if(sqlResultSet) {
				while(sqlResultSet->next())
					result.emplace(sqlResultSet->getUInt64("id"), sqlResultSet->getString("namespace"));
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getUrlLists", e); }

		return result;
	}

	// insert URLs that do not already exist into the specified URL list and return the number of added URLs,
	//  throws Database::Exception
	std::size_t Database::mergeUrls(std::uint64_t listId, std::queue<std::string>& urls) {
		std::size_t urlsAdded = 0;

		// check arguments
		if(!listId)
			throw Database::Exception("Main::Database::mergeUrls(): No URL list ID specified");

		if(urls.empty())
			return 0;

		// get ID and namespaces of website
		const IdString website(this->getWebsiteNamespaceFromUrlList(listId));

		// get namespace of URL list and generate name of URL list table
		const std::string urlListTable(
				"crawlserv_"
				+ website.second
				+ "_"
				+ this->getUrlListNamespace(listId)
		);

		// generate SQL string for URL hashing
		std::string hashQuery("CRC32( ");

		if(this->isUrlListCaseSensitive(listId))
			hashQuery += "?";
		else
			hashQuery += "LOWER( ? )";

		hashQuery += " )";

		// generate query for each 1,000 (or less) URLs
		while(!urls.empty()) {
			// generate INSERT INTO ... VALUES clause
			std::string sqlQueryStr(
					"INSERT IGNORE INTO `"
					+ urlListTable
					+ "`(id, url, hash) VALUES "
			);

			// generate placeholders
			for(std::size_t n = 0; n < (urls.size() > 1000 ? 1000 : urls.size()); ++n)
				sqlQueryStr += "(" // begin of VALUES arguments
								" ("
									"SELECT id FROM"
									" ("
										"SELECT id, url"
										" FROM `" + urlListTable + "`"
										" AS `a" + std::to_string(n + 1) + "`"
										" WHERE hash = " + hashQuery +
									" ) AS tmp2 WHERE url = ? LIMIT 1"
								" ),"
								"?, " +
								hashQuery +
							"), "; // end of VALUES arguments

			// remove last comma and space
			sqlQueryStr.pop_back();
			sqlQueryStr.pop_back();

			// check connection
			this->checkConnection();

			try {
				// prepare SQL statement
				SqlPreparedStatementPtr sqlStatement(
						this->connection->prepareStatement(
								sqlQueryStr
						)
				);

				// execute SQL query
				const std::size_t max = urls.size() > 1000 ? 1000 : urls.size();

				for(std::size_t n = 0; n < max; ++n) {
					sqlStatement->setString((n * 4) + 1, urls.front());
					sqlStatement->setString((n * 4) + 2, urls.front());
					sqlStatement->setString((n * 4) + 3, urls.front());
					sqlStatement->setString((n * 4) + 4, urls.front());

					urls.pop();
				}

				auto added = Database::sqlExecuteUpdate(sqlStatement);

				if(added > 0)
					urlsAdded += added;
			}
			catch(const sql::SQLException &e) { this->sqlException("Main::Database::mergeUrls", e); }
		}

		return urlsAdded;
	}

	// get all URLs from the specified URL list, throws Database::Exception
	std::queue<std::string> Database::getUrls(std::uint64_t listId) {
		std::queue<std::string> result;

		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::getUrls(): No URL list ID specified");

		// get ID and namespaces of website
		const IdString website(this->getWebsiteNamespaceFromUrlList(listId));

		// get namespace of URL list and generate name of URL list table
		const std::string urlListTable(
				"crawlserv_"
				+ website.second
				+ "_"
				+ this->getUrlListNamespace(listId)
		);

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(
					sqlStatement,
					"SELECT url FROM `" + urlListTable + "`"
			));

			// get results
			if(sqlResultSet)
				while(sqlResultSet->next())
					result.emplace(sqlResultSet->getString("url"));
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getUrls", e); }

		return result;
	}

	// get all URLs with their IDs from the specified URL list, throws Database::Exception
	std::queue<Database::IdString> Database::getUrlsWithIds(std::uint64_t listId) {
		std::queue<IdString> result;

		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::getUrlsWithIds(): No URL list ID specified");

		// get ID and namespaces of website
		const IdString website(this->getWebsiteNamespaceFromUrlList(listId));

		// get namespace of URL list and generate name of URL list table
		const std::string urlListTable(
				"crawlserv_"
				+ website.second
				+ "_"
				+ this->getUrlListNamespace(listId)
		);

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(
					sqlStatement,
					"SELECT id, url FROM `" + urlListTable + "`"
			));

			// get results
			if(sqlResultSet)
				while(sqlResultSet->next())
					result.emplace(sqlResultSet->getUInt64("id"), sqlResultSet->getString("url"));
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getUrlsWithIds", e); }

		return result;
	}

	// get namespace of URL list by its ID, throws Database::Exception
	std::string Database::getUrlListNamespace(std::uint64_t listId) {
		std::string result;

		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::getUrlListNamespace(): No URL list ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT namespace"
							" FROM `crawlserv_urllists`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL query
			sqlStatement->setUInt64(1, listId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("namespace");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getUrlListNamespace", e); }

		return result;
	}

	// get ID and namespace of URL list from the database using a target table type and ID,
	//  throws Database::Exception
	Database::IdString Database::getUrlListNamespaceFromTargetTable(
			const std::string& type,
			std::uint64_t tableId
	) {
		std::uint64_t urlListId = 0;

		// check arguments
		if(type.empty())
			throw Database::Exception("Main::Database::getUrlListNamespaceFromCustomTable(): No table type specified");

		if(!tableId)
			throw Database::Exception("Main::Database::getUrlListNamespaceFromCustomTable(): No table ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT urllist"
							" FROM `crawlserv_" + type + "tables`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL query
			sqlStatement->setUInt64(1, tableId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				urlListId = sqlResultSet->getUInt64("urllist");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getUrlListNamespaceFromCustomTable", e); }

		return IdString(urlListId, this->getUrlListNamespace(urlListId));
	}

	// check whether a URL list namespace for an ID-specified website exists in the database,
	//  throws Database::Exception
	bool Database::isUrlListNamespace(std::uint64_t websiteId, const std::string& nameSpace) {
		bool result = false;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::isUrlListNamespace(): No website ID specified");

		if(nameSpace.empty())
			throw Database::Exception("Main::Database::isUrlListNamespace(): No namespace specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
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
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setString(2, nameSpace);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isUrlListNamespace", e); }

		return result;
	}

	// update URL list (and all associated tables) in the database, throws Database::Exception
	void Database::updateUrlList(std::uint64_t listId, const UrlListProperties& listProperties) {
		// check arguments
		if(!listId)
			throw Database::Exception("Main::Database::updateUrlList(): No website ID specified");

		if(listProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::updateUrlList(): No URL list namespace specified");

		if(listProperties.name.empty())
			throw Database::Exception("Main::Database::updateUrlList(): No URL list name specified");

		// get website namespace and URL list name
		const IdString websiteNamespace(this->getWebsiteNamespaceFromUrlList(listId));
		const std::string oldListNamespace(this->getUrlListNamespace(listId));

		// check website namespace if necessary
		if(listProperties.nameSpace != oldListNamespace)
			if(this->isUrlListNamespace(websiteNamespace.first, listProperties.nameSpace))
				throw Database::Exception("Main::Database::updateUrlList(): URL list namespace already exists");

		// check connection
		this->checkConnection();

		try {
			if(listProperties.nameSpace != oldListNamespace) {
				// create SQL statement for renaming
				SqlStatementPtr renameStatement(this->connection->createStatement());

				// rename URL list table
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "`"
						" RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace + "`"
				);

				// rename crawling tables
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "_crawled`"
						" RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace + "_crawled`"
				);

				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "_crawling`"
						" RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace	+ "_crawling`"
				);

				// rename parsing tables
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "_parsing`"
						" RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace + "_parsing`"
				);

				std::queue<IdString> tables(this->getTargetTables("parsed", listId));

				while(!tables.empty()) {
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "_parsed_"
								+ tables.front().second + "`"
							" RENAME TO `crawlserv_" + websiteNamespace.second + "_"
								+ listProperties.nameSpace	+ "_parsed_" + tables.front().second + "`"
					);

					tables.pop();
				}

				// rename extracting tables
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "_extracting`"
						" RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace + "_extracting`"
				);

				tables = this->getTargetTables("extracted", listId);

				while(!tables.empty()) {
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "_extracted_"
								+ tables.front().second + "`"
							" RENAME TO `crawlserv_" + websiteNamespace.second + "_"
								+ listProperties.nameSpace + "_extracted_"	+ tables.front().second + "`"
					);

					tables.pop();
				}

				// rename analyzing tables
				Database::sqlExecute(
						renameStatement,
						"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "_analyzing`"
						" RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace	+ "_analyzing`"
				);

				tables = this->getTargetTables("analyzed", listId);

				while(!tables.empty()) {
					Database::sqlExecute(
							renameStatement,
							"ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace + "_analyzed_"
								+ tables.front().second + "`"
							" RENAME TO `crawlserv_" + websiteNamespace.second + "_"
								+ listProperties.nameSpace + "_analyzed_" + tables.front().second + "`"
					);

					tables.pop();
				}

				// prepare SQL statement for updating
				SqlPreparedStatementPtr updateStatement(
						this->connection->prepareStatement(
						"UPDATE crawlserv_urllists"
						" SET namespace = ?, name = ?"
						" WHERE id = ?"
						" LIMIT 1"
				));

				// execute SQL statement for updating
				updateStatement->setString(1, listProperties.nameSpace);
				updateStatement->setString(2, listProperties.name);
				updateStatement->setUInt64(3, listId);

				Database::sqlExecute(updateStatement);
			}
			else {
				// prepare SQL statement for updating
				SqlPreparedStatementPtr updateStatement(this->connection->prepareStatement(
						"UPDATE crawlserv_urllists SET name = ? WHERE id = ? LIMIT 1"
				));

				// execute SQL statement for updating
				updateStatement->setString(1, listProperties.name);
				updateStatement->setUInt64(2, listId);

				Database::sqlExecute(updateStatement);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateUrlList", e); }
	}

	// delete URL list (and all associated data) from the database by its ID, throws Database::Exception
	void Database::deleteUrlList(std::uint64_t listId) {
		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::deleteUrlList(): No URL list ID specified");

		// get website namespace and URL list name
		const IdString websiteNamespace(this->getWebsiteNamespaceFromUrlList(listId));
		const std::string listNamespace(this->getUrlListNamespace(listId));

		// delete parsing tables
		std::queue<IdString> tables(this->getTargetTables("parsed", listId));

		while(!tables.empty()) {
			this->deleteTargetTable("parsed", tables.front().first);

			tables.pop();
		}

		// delete extracting tables
		tables = this->getTargetTables("extracted", listId);

		while(!tables.empty()) {
			this->deleteTargetTable("extracted", tables.front().first);

			tables.pop();
			}

		// delete analyzing tables
		tables = this->getTargetTables("analyzed", listId);

		while(!tables.empty()) {
			this->deleteTargetTable("analyzed", tables.front().first);

			tables.pop();
		}

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for deleting URL list
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
						"DELETE FROM `crawlserv_urllists`"
						" WHERE id = ?"
						" LIMIT 1"
					)
			);

			// execute SQL statement for deleting URL list
			sqlStatement->setUInt64(1, listId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteUrlList", e); }

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_urllists"))
			this->resetAutoIncrement("crawlserv_urllists");

		// delete tables
		this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_crawled");
		this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_crawling");
		this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_parsing");
		this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_extracting");
		this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_analyzing");
		this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace);
	}

	// delete URLs with the specified IDs from the URL list specified by its ID, return the number of deleted URLs
	std::size_t Database::deleteUrls(std::uint64_t listId, std::queue<std::uint64_t>& urlIds) {
		// check arguments
		if(!listId)
			throw Database::Exception("Main::Database::deleteUrlList(): No URL list ID specified");

		if(urlIds.empty())
			return 0;

		// get website namespace and URL list name
		const IdString websiteNamespace(this->getWebsiteNamespaceFromUrlList(listId));
		const std::string listNamespace(this->getUrlListNamespace(listId));

		// get maximum length of SQL query
		const auto maxLength = this->getMaxAllowedPacketSize();

		// check connection
		this->checkConnection();

		std::size_t result = 0;

		try {
			while(!urlIds.empty()) {
				// create SQL query
				std::string sqlQuery(
						"DELETE FROM `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "`"
						" WHERE"
				);

				// add maximum possible number of URLs to the SQL query
				while(true) {
					// check whether there are more IDs to process
					if(urlIds.empty())
						break;

					// convert ID to string
					std::string idString(std::to_string(urlIds.front()));

					// check whether maximum length of SQL query will be exceeded
					if(sqlQuery.length() + 4 + idString.length() >= maxLength)
						break;

					// add ID to SQL query
					sqlQuery += " id=";
					sqlQuery += idString;
					sqlQuery += " OR";

					// remove ID from queue
					urlIds.pop();
				}

				// remove last " OR" from SQL query
				sqlQuery.pop_back();
				sqlQuery.pop_back();
				sqlQuery.pop_back();

				// execute SQL query and get number of deleted URLs
				const auto removed = this->executeUpdate(sqlQuery);

				if(removed > 0)
					result += removed;
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteUrls", e); }

		return result;
	}


	// reset parsing status of ID-specified URL list, throws Database::Exception
	void Database::resetParsingStatus(std::uint64_t listId) {
		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::resetParsingStatus(): No URL list ID specified");

		// get website namespace and URL list name
		const IdString websiteNamespace(this->getWebsiteNamespaceFromUrlList(listId));
		const std::string listNamespace(this->getUrlListNamespace(listId));

		// check connection
		this->checkConnection();

		try {
			// update parsing table
			this->execute(
					"UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_parsing`"
					" SET success = FALSE, locktime = NULL"
			);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetParsingStatus", e); }
	}

	// reset extracting status of ID-specified URL list, throws Database::Exception
	void Database::resetExtractingStatus(std::uint64_t listId) {
		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::resetExtractingStatus(): No URL list ID specified");

		// get website namespace and URL list name
		const IdString websiteNamespace(this->getWebsiteNamespaceFromUrlList(listId));
		const std::string listNamespace(this->getUrlListNamespace(listId));

		// check connection
		this->checkConnection();

		try {
			// update extracting table
			this->execute(
					"UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_extracting`"
					" SET success = FALSE, locktime = NULL"
			);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetExtractingStatus", e); }
	}

	// reset analyzing status of ID-specified URL list, throws Database::Exception
	void Database::resetAnalyzingStatus(std::uint64_t listId) {
		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::resetAnalyzingStatus(): No URL list ID specified");

		// get website namespace and URL list name
		const IdString websiteNamespace(this->getWebsiteNamespaceFromUrlList(listId));
		const std::string listNamespace(this->getUrlListNamespace(listId));

		// check connection
		this->checkConnection();

		try {
			// update URL list
			this->execute(
					"UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_analyzing`"
					" SET success = FALSE, locktime = NULL"
			);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetAnalyzingStatus", e); }
	}

	/*
	 * QUERY FUNCTIONS
	 */

	// add a query to the database and return its new ID (add global query when websiteId is zero),
	//  throws Database::Exception
	std::uint64_t Database::addQuery(std::uint64_t websiteId, const QueryProperties& queryProperties) {
		std::uint64_t newId = 0;

		// check arguments
		if(queryProperties.name.empty())
			throw Database::Exception("Main::Database::addQuery(): No query name specified");

		if(queryProperties.text.empty())
			throw Database::Exception("Main::Database::addQuery(): No query text specified");

		if(queryProperties.type.empty())
			throw Database::Exception("Main::Database::addQuery(): No query type specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for adding query
			SqlPreparedStatementPtr sqlStatement(
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
			));

			// execute SQL query for adding query
			if(websiteId)
				sqlStatement->setUInt64(1, websiteId);
			else
				sqlStatement->setNull(1, 0);

			sqlStatement->setString(2, queryProperties.name);
			sqlStatement->setString(3, queryProperties.text);
			sqlStatement->setString(4, queryProperties.type);
			sqlStatement->setBoolean(5, queryProperties.resultBool);
			sqlStatement->setBoolean(6, queryProperties.resultSingle);
			sqlStatement->setBoolean(7, queryProperties.resultMulti);
			sqlStatement->setBoolean(8, queryProperties.resultSubSets);
			sqlStatement->setBoolean(9, queryProperties.textOnly);

			Database::sqlExecute(sqlStatement);

			// get ID
			newId = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addQuery", e); }

		return newId;
	}

	// get the properties of a query from the database by its ID, throws Database::Exception
	void Database::getQueryProperties(std::uint64_t queryId, QueryProperties& queryPropertiesTo) {
		// check argument
		if(!queryId)
			throw Database::Exception("Main::Database::getQueryProperties(): No query ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT name, query, type, resultbool, resultsingle, resultmulti, resultsubsets, textonly"
							" FROM `crawlserv_queries` WHERE id = ? LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, queryId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

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
			else
				queryPropertiesTo = QueryProperties();
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getQueryProperties", e); }
	}

	// edit query in the database, throws Database::Exception
	void Database::updateQuery(std::uint64_t queryId, const QueryProperties& queryProperties) {
		// check arguments
		if(!queryId)
			throw Database::Exception("Main::Database::updateQuery(): No query ID specified");

		if(queryProperties.name.empty())
			throw Database::Exception("Main::Database::updateQuery(): No query name specified");

		if(queryProperties.text.empty())
			throw Database::Exception("Main::Database::updateQuery(): No query text specified");

		if(queryProperties.type.empty())
			throw Database::Exception("Main::Database::updateQuery(): No query type specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for updating
			SqlPreparedStatementPtr sqlStatement(
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
			);

			// execute SQL statement for updating
			sqlStatement->setString(1, queryProperties.name);
			sqlStatement->setString(2, queryProperties.text);
			sqlStatement->setString(3, queryProperties.type);
			sqlStatement->setBoolean(4, queryProperties.resultBool);
			sqlStatement->setBoolean(5, queryProperties.resultSingle);
			sqlStatement->setBoolean(6, queryProperties.resultMulti);
			sqlStatement->setBoolean(7, queryProperties.resultSubSets);
			sqlStatement->setBoolean(8, queryProperties.textOnly);
			sqlStatement->setUInt64(9, queryId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateQuery", e); }
	}

	// move query to another website by their IDs, throws Database::Exception
	void Database::moveQuery(std::uint64_t queryId, std::uint64_t toWebsiteId) {
		// check argument
		if(!queryId)
			throw Database::Exception("Main::Database::moveQuery(): No query ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"UPDATE `crawlserv_queries` SET website = ? WHERE id = ? LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, toWebsiteId);
			sqlStatement->setUInt64(2, queryId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteQuery", e); }
	}

	// delete query from the database by its ID, throws Database::Exception
	void Database::deleteQuery(std::uint64_t queryId) {
		// check argument
		if(!queryId)
			throw Database::Exception("Main::Database::deleteQuery(): No query ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"DELETE FROM `crawlserv_queries`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, queryId);

			Database::sqlExecute(sqlStatement);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_queries"))
				this->resetAutoIncrement("crawlserv_queries");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteQuery", e); }
	}

	// duplicate query in the database by its ID, throws Database::Exception
	std::uint64_t Database::duplicateQuery(std::uint64_t queryId) {
		std::uint64_t newId = 0;

		// check argument
		if(!queryId)
			throw Database::Exception("Main::Database::duplicateQuery(): No query ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for getting query info
			SqlPreparedStatementPtr sqlStatement(
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
			);

			// execute SQL statement for getting query info
			sqlStatement->setUInt64(1, queryId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

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
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::duplicateQuery", e); }

		return newId;
	}

	/*
	 * CONFIGURATION FUNCTIONS
	 */

	// add a configuration to the database and return its new ID (add global configuration when websiteId is 0),
	//  throws Database::Exception
	std::uint64_t Database::addConfiguration(
			std::uint64_t websiteId,
			const ConfigProperties& configProperties
	) {
		std::uint64_t newId = 0;

		// check arguments
		if(configProperties.module.empty())
			throw Database::Exception("Main::Database::addConfiguration(): No configuration module specified");

		if(configProperties.name.empty())
			throw Database::Exception("Main::Database::addConfiguration(): No configuration name specified");

		if(configProperties.config.empty())
			throw Database::Exception("Main::Database::addConfiguration(): No configuration specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for adding configuration
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"INSERT INTO crawlserv_configs(website, module, name, config)"
							" VALUES (?, ?, ?, ?)"
					)
			);

			// execute SQL query for adding website
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setString(2, configProperties.module);
			sqlStatement->setString(3, configProperties.name);
			sqlStatement->setString(4, configProperties.config);

			Database::sqlExecute(sqlStatement);

			// get ID
			newId = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addConfiguration", e); }

		return newId;
	}

	// get a configuration from the database by its ID, throws Database::Exception
	const std::string Database::getConfiguration(std::uint64_t configId) {
		std::string result;

		// check argument
		if(!configId)
			throw Database::Exception("Main::Database::getConfiguration(): No configuration ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT config"
							" FROM `crawlserv_configs`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, configId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("config");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getConfiguration", e); }

		return result;
	}

	// update configuration in the database (NOTE: module will not be updated!), throws Database::Exception
	void Database::updateConfiguration(std::uint64_t configId, const ConfigProperties& configProperties) {
		// check arguments
		if(!configId)
			throw Database::Exception("Main::Database::updateConfiguration(): No configuration ID specified");

		if(configProperties.name.empty())
			throw Database::Exception("Main::Database::updateConfiguration(): No configuration name specified");

		if(configProperties.config.empty())
			throw Database::Exception("Main::Database::updateConfiguration(): No configuration specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for updating
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"UPDATE crawlserv_configs"
							" SET name = ?,"
							" config = ?"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement for updating
			sqlStatement->setString(1, configProperties.name);
			sqlStatement->setString(2, configProperties.config);
			sqlStatement->setUInt64(3, configId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateConfiguration", e); }
	}

	// delete configuration from the database by its ID, throws Database::Exception
	void Database::deleteConfiguration(std::uint64_t configId) {
		// check argument
		if(!configId)
			throw Database::Exception("Main::Database::deleteConfiguration(): No configuration ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"DELETE FROM `crawlserv_configs`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, configId);

			Database::sqlExecute(sqlStatement);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_configs"))
				this->resetAutoIncrement("crawlserv_configs");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteConfiguration", e); }
	}

	// duplicate configuration in the database by its ID, throws Database::Exception
	std::uint64_t Database::duplicateConfiguration(std::uint64_t configId) {
		std::uint64_t newId = 0;

		// check argument
		if(!configId)
			throw Database::Exception("Main::Database::duplicateConfiguration(): No configuration ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for getting configuration info
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT website, module, name, config"
							" FROM `crawlserv_configs`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement for getting configuration info
			sqlStatement->setUInt64(1, configId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				// add configuration
				newId = this->addConfiguration(sqlResultSet->getUInt64("website"),
						ConfigProperties(
								sqlResultSet->getString("module"),
								sqlResultSet->getString("name") + " (copy)",
								sqlResultSet->getString("config")
						));
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::duplicateConfiguration", e); }

		return newId;
	}

	/*
	 * TARGET TABLE FUNCTIONS
	 */

	// add a target table of the specified type to the database if such a table does not exist already, return table ID,
	//  throws Database::Exception
	std::uint64_t Database::addTargetTable(const TargetTableProperties& properties) {
		std::uint64_t newId = 0;

		// check arguments
		if(properties.type.empty())
			throw Database::Exception("Main::Database::addTargetTable(): No table type specified");

		if(!properties.website)
			throw Database::Exception("Main::Database::addTargetTable(): No website ID specified");

		if(!properties.urlList)
			throw Database::Exception("Main::Database::addTargetTable(): No URL list ID specified");

		if(properties.name.empty())
			throw Database::Exception("Main::Database::addTargetTable(): No table name specified");

		if(properties.columns.empty())
			throw Database::Exception("Main::Database::addTargetTable(): No columns specified");

		// check connection
		this->checkConnection();

		try {
			// check whether table exists
			if(this->isTableExists(properties.fullName)) {
				// table exists already: add columns that do not exist yet and check columns that do exist
				for(const auto& column : properties.columns) {
					if(!column.name.empty()) {
						std::string columnName(column.name);

						if(this->isColumnExists(properties.fullName, columnName)) {
							// column does exist: check types (does not check specifiers like 'UNSIGNED'!)
							std::string columnType(column.type, 0, column.type.find(' '));
							std::string targetType(this->getColumnType(properties.fullName, columnName));

							std::transform(columnType.begin(), columnType.end(), columnType.begin(), ::tolower);
							std::transform(targetType.begin(), targetType.end(), targetType.begin(), ::tolower);

							if(columnType != targetType)
								throw Database::Exception(
										"Main::Database::addTargetTable(): Cannot overwrite column of type \'"
										+ columnType + "\' with data of type \'" + targetType + "\'"
								);
						}
						else {
							// column does not exist: add column
							this->addColumn(properties.fullName, TableColumn(columnName, column));
						}
					}
				}

				// compress table if necessary
				if(properties.compressed)
					this->compressTable(properties.fullName);
			}
			else {
				// table does not exist: get data directory and create table
				const std::string dataDirectory(this->getWebsiteDataDirectory(properties.website));

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
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT id"
							" FROM `crawlserv_" + properties.type + "tables`"
							" WHERE website = ?"
							" AND urllist = ?"
							" AND name = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement for checking for entry
			sqlStatement->setUInt64(1, properties.website);
			sqlStatement->setUInt64(2, properties.urlList);
			sqlStatement->setString(3, properties.name);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			if(sqlResultSet && sqlResultSet->next())
				// entry exists: return ID
				newId = sqlResultSet->getUInt64("id");
			else {
				// entry does not exist already: prepare SQL statement for adding table
				sqlStatement.reset(
						this->connection->prepareStatement(
								"INSERT INTO `crawlserv_" + properties.type + "tables`(website, urllist, name)"
								" VALUES (?, ?, ?)"
						)
				);

				// execute SQL statement for adding table
				sqlStatement->setUInt64(1, properties.website);
				sqlStatement->setUInt64(2, properties.urlList);
				sqlStatement->setString(3, properties.name);

				Database::sqlExecute(sqlStatement);

				// return ID of newly inserted table
				newId = this->getLastInsertedId();
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addTargetTable", e); }

		return newId;
	}

	// get target tables of the specified type for an ID-specified URL list from the database,
	//  throws Database::Exception
	std::queue<Database::IdString> Database::getTargetTables(
			const std::string& type,
			std::uint64_t listId
	) {
		std::queue<IdString> result;

		// check arguments
		if(type.empty())
			throw Database::Exception("Main::Database::getTargetTables(): No table type specified");

		if(!listId)
			throw Database::Exception("Main::Database::getTargetTables(): No URL list ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT id, name"
							" FROM `crawlserv_" + type + "tables`"
							" WHERE urllist = ?"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, listId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get results
			if(sqlResultSet)
				while(sqlResultSet->next())
					result.emplace(sqlResultSet->getUInt64("id"), sqlResultSet->getString("name"));
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getTargetTables", e); }

		return result;
	}

	// get the ID of a target table of the specified type from the database by its website ID, URL list ID and table name,
	//  throws Database::Exception
	std::uint64_t Database::getTargetTableId(
			const std::string& type,
			std::uint64_t websiteId,
			std::uint64_t listId,
			const std::string& tableName
	) {
		std::uint64_t result = 0;

		// check arguments
		if(type.empty())
			throw Database::Exception("Main::Database::getTargetTableId(): No table type specified");

		if(!websiteId)
			throw Database::Exception("Main::Database::getTargetTableId(): No website ID specified");

		if(!listId)
			throw Database::Exception("Main::Database::getTargetTableId(): No URL list ID specified");

		if(tableName.empty())
			throw Database::Exception("Main::Database::getTargetTableId(): No table name specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT id"
							" FROM `crawlserv_" + type + "tables`"
							" WHERE website = ?"
							" AND urllist = ?"
							" AND name = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setUInt64(2, listId);
			sqlStatement->setString(3, tableName);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("id");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getTargetTableId", e); }

		return result;
	}

	// get the name of a target table of the specified type from the database by its ID,
	//  throws Database::Exception
	std::string Database::getTargetTableName(const std::string& type, std::uint64_t tableId) {
		std::string result;

		// check arguments
		if(type.empty())
			throw Database::Exception("Main::Database::getTargetTableName(): No table type specified");

		if(!tableId)
			throw Database::Exception("Main::Database::getTargetTableName(): No table ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT name"
							" FROM `crawlserv_" + type + "tables`"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, tableId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("name");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getTargetTableName", e); }

		return result;
	}

	// delete target table of the specified type from the database by its ID, throws Database::Exception
	void Database::deleteTargetTable(const std::string& type, std::uint64_t tableId) {
		// check arguments
		if(type.empty())
			throw Database::Exception("Main::Database::deleteTargetTable(): No table type specified");

		if(!tableId)
			throw Database::Exception("Main::Database::deleteTargetTable(): No table ID specified");

		// get namespace, URL list name and table name
		const IdString websiteNamespace(this->getWebsiteNamespaceFromTargetTable(type, tableId));
		const IdString listNamespace(this->getUrlListNamespaceFromTargetTable(type, tableId));
		const std::string tableName(this->getTargetTableName(type, tableId));

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement for deletion
			SqlPreparedStatementPtr deleteStatement(this->connection->prepareStatement(
					"DELETE FROM `crawlserv_" + type + "tables` WHERE id = ? LIMIT 1"
			));

			// execute SQL statement for deletion
			deleteStatement->setUInt64(1, tableId);

			Database::sqlExecute(deleteStatement);

			// create SQL statement for dropping table
			SqlStatementPtr dropStatement(this->connection->createStatement());

			// execute SQL query for dropping table
			Database::sqlExecute(
					dropStatement,
					"DROP TABLE IF EXISTS"
					" `"
						"crawlserv_" +
						websiteNamespace.second + "_" +
						listNamespace.second + "_" +
						type + "_"
						+ tableName +
					"`"
			);

			// reset auto-increment if table is empty
			if(this->isTableEmpty("crawlserv_" + type + "tables"))
				this->resetAutoIncrement("crawlserv_" + type + "tables");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteTargetTable", e); }
	}

	/*
	 * VALIDATION FUNCTIONS
	 */

	// check whether the connection to the database is still valid and try to re-connect if necesssary,
	//  throws Database::Exception
	// WARNING: Afterwards, old references to prepared SQL statements may be invalid!
	void Database::checkConnection() {
		unsigned long long milliseconds = 0;

		// check driver
		if(!(this->driver))
			throw Database::Exception("Main::Database::checkConnection(): MySQL driver not loaded");

		try {
			// check connection
			if(this->connection) {
				// check whether re-connect should be performed anyway
				milliseconds = this->reconnectTimer.tick();

				if(milliseconds < MAIN_DATABASE_RECONNECT_AFTER_IDLE_SEC * 1000) {
					// check whether connection is valid
					if(this->connection->isValid())
						return;

					milliseconds = 0;
				}

				// try to re-connect
				if(!(this->connection->reconnect())) {
					// simple re-connect failed: try to reset connection after sleeping over it for some time
					this->connection.reset();

					try {
						this->connect();
					}
					catch(const Database::Exception& e) {
						if(this->sleepOnError)
							std::this_thread::sleep_for(std::chrono::seconds(this->sleepOnError));

						this->connect();
					}
				}
			}
			else this->connect();

			// recover prepared SQL statements
			for(auto& preparedStatement : this->preparedStatements)
				preparedStatement.refresh(this->connection.get());

			// log re-connect on idle if necessary
			if(milliseconds)
				this->log(
					"re-connected to database after idling for "
					+ Helper::DateTime::secondsToString(std::round(static_cast<float>(milliseconds / 1000)))
					+ "."
				);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::checkConnection", e); }
	}

	// check whether a website ID is valid, throws Database::Exception
	bool Database::isWebsite(std::uint64_t websiteId) {
		bool result = false;

		// check argument
		if(!websiteId)
			throw Database::Exception("Main::Database::isWebsite(): No website ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT EXISTS"
							" ("
								" SELECT *"
								" FROM `crawlserv_websites`"
								" WHERE id = ?"
							" )"
							" AS result"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isWebsite", e); }

		return result;
	}

	// check whether a URL list ID is valid, throws Database::Exception
	bool Database::isUrlList(std::uint64_t urlListId) {
		bool result = false;

		// check argument
		if(!urlListId)
			throw Database::Exception("Main::Database::isUrlList(): No URL list ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT EXISTS"
							" ("
								" SELECT *"
								" FROM `crawlserv_urllists`"
								" WHERE id = ?"
							" )"
							" AS result"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, urlListId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isUrlList", e); }

		return result;
	}

	// check whether a URL list ID is valid for the ID-specified website, throws Database::Exception
	bool Database::isUrlList(std::uint64_t websiteId, std::uint64_t urlListId) {
		bool result = false;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::isUrlList(): No website ID specified");

		if(!urlListId)
			throw Database::Exception("Main::Database::isUrlList(): No URL list ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
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
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setUInt64(2, urlListId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isUrlList", e); }

		return result;
	}

	// check whether a query ID is valid, throws Database::Exception
	bool Database::isQuery(std::uint64_t queryId) {
		bool result = false;

		// check argument
		if(!queryId)
			throw Database::Exception("Main::Database::isQuery(): No query ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT EXISTS"
							" ("
								" SELECT *"
								" FROM `crawlserv_queries`"
								" WHERE id = ?"
							" )"
							" AS result"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, queryId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isQuery", e); }

		return result;
	}

	// check whether a query ID is valid for the ID-specified website (including global queries),
	//  throws Database::Exception
	bool Database::isQuery(std::uint64_t websiteId, std::uint64_t queryId) {
		bool result = false;

		// check arguments
		if(!queryId)
			throw Database::Exception("Main::Database::isQuery(): No query ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
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
			));

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setUInt64(2, queryId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isQuery", e); }

		return result;
	}

	// check whether a configuration ID is valid, throws Database::Exception
	bool Database::isConfiguration(std::uint64_t configId) {
		bool result = false;

		// check argument
		if(!configId)
			throw Database::Exception("Main::Database::isConfiguration(): No configuration ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT EXISTS"
							" ("
								" SELECT *"
								" FROM `crawlserv_configs`"
								" WHERE id = ?"
							" )"
							" AS result"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, configId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isConfiguration", e); }

		return result;
	}

	// check whether a configuration ID is valid for the ID-specified website (look for global configurations if websiteId is 0),
	//  throws Database::Exception
	bool Database::isConfiguration(std::uint64_t websiteId, std::uint64_t configId) {
		bool result = false;

		// check arguments
		if(!configId)
			throw Database::Exception("Main::Database::isConfiguration(): No configuration ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
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
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setUInt64(2, configId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isConfiguration", e); }

		return result;
	}

	/*
	 * DATABASE FUNCTIONS
	 */

	// disable locking
	void Database::beginNoLock() {
		try {
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			sqlStatement->execute("SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::beginNoLock", e); }
	}

	// re-enable locking by ending (non-existing) transaction
	void Database::endNoLock() {
		try {
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			sqlStatement->execute("COMMIT");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::endNoLock", e); }
	}

	// check whether a data directory is known to the database
	bool Database::checkDataDir(const std::string& dir) {
		return std::find(this->dirs.begin(), this->dirs.end(), dir) != this->dirs.end();
	}

	/*
	 * GENERAL TABLE FUNCTIONS
	 */

	// check whether a name-specified table in the database is empty, throws Database::Exception
	bool Database::isTableEmpty(const std::string& tableName) {
		bool result = false;

		// check argument
		if(tableName.empty())
			throw Database::Exception("Main::Database::isTableEmpty(): No table name specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT NOT EXISTS"
							" ("
									" SELECT *"
									" FROM `" + tableName + "`"
							" ) "
							" AS result"
					)
			);

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isTableEmpty", e); }

		return result;
	}

	// check whether a specific table exists in the database, throws Database::Exception
	bool Database::isTableExists(const std::string& tableName) {
		bool result = false;

		// check argument
		if(tableName.empty())
			throw Database::Exception("Main::Database::isTableExists(): No table name specified");

		// check connection
		this->checkConnection();

		try {
			// create and execute SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT COUNT(*)"
							" AS result"
							" FROM INFORMATION_SCHEMA.TABLES"
							" WHERE TABLE_SCHEMA = ?"
							" AND TABLE_NAME = ?"
							" LIMIT 1"
					)
			);

			// get result
			sqlStatement->setString(1, this->settings.name);
			sqlStatement->setString(2, tableName);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isTableExists", e); }

		return result;
	}

	// check whether a specific column of a specific table exists in the database, throws Database::Exception
	bool Database::isColumnExists(const std::string& tableName, const std::string& columnName) {
		bool result = false;

		// check arguments
		if(tableName.empty())
			throw Database::Exception("Main::Database::isColumnExists(): No table name specified");

		if(columnName.empty())
			throw Database::Exception("Main::Database::isColumnExists(): No column name specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT COUNT(*)"
							" AS result"
							" FROM INFORMATION_SCHEMA.COLUMNS"
							" WHERE TABLE_SCHEMA = ?"
							" AND TABLE_NAME = ?"
							" AND COLUMN_NAME = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setString(1, this->settings.name);
			sqlStatement->setString(2, tableName);
			sqlStatement->setString(3, columnName);

			// get result
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isColumnExists", e); }

		return result;
	}

	// get the type of a specific column of a specific table in the database, throws Database::Exception
	std::string Database::getColumnType(const std::string& tableName, const std::string& columnName) {
		std::string result;

		// check arguments
		if(tableName.empty())
			throw Database::Exception("Main::Database::getColumnType(): No table name specified");

		if(columnName.empty())
			throw Database::Exception("Main::Database::getColumnType(): No column name specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT DATA_TYPE"
							" FROM INFORMATION_SCHEMA.COLUMNS"
							" WHERE TABLE_SCHEMA = ?"
							" AND TABLE_NAME = ?"
							" AND COLUMN_NAME = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setString(1, this->settings.name);
			sqlStatement->setString(2, tableName);
			sqlStatement->setString(3, columnName);

			// get result
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("DATA_TYPE");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getColumnType", e); }

		return result;
	}

	// lock tables
	void Database::lockTables(std::queue<TableNameWriteAccess>& locks) {
		// check argument
		if(locks.empty())
			return;

		// create lock string
		std::string lockString;

		while(!locks.empty()) {
			lockString += "`" + locks.front().first + "` ";

			if(locks.front().second)
				lockString += "WRITE";
			else
				lockString += "READ";

			lockString += ", ";

			locks.pop();
		}

		lockString.pop_back();
		lockString.pop_back();

		// execute SQL query
		try {
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			sqlStatement->execute("LOCK TABLES " + lockString);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::lockTables", e); }
	}

	// unlock tables
	void Database::unlockTables() {
		// execute SQL query
		try {
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			sqlStatement->execute("UNLOCK TABLES");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::unlockTables", e); }
	}

	// start a new transaction with the specified isolation level
	//  NOTE: the default isolation level will be used if isolationLevel is an empty string
	void Database::startTransaction(const std::string& isolationLevel) {
		// check connection
		this->checkConnection();

		// execute SQL query
		try {
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			if(!isolationLevel.empty())
				sqlStatement->execute("SET TRANSACTION ISOLATION LEVEL " + isolationLevel);

			sqlStatement->execute("START TRANSACTION");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::startTransaction", e); }
	}

	// end the current transaction, commit the commands
	void Database::endTransaction(bool success) {
		// check connection
		this->checkConnection();

		// execute SQL query
		try {
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			if(success)
				sqlStatement->execute("COMMIT");
			else
				sqlStatement->execute("ROLLBACK");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::endTransaction", e); }
	}

	/*
	 * CUSTOM DATA FUNCTIONS FOR ALGORITHMS
	 */

	// get one custom value from one field of a row in the database, throws Database::Exception
	void Database::getCustomData(Data::GetValue& data) {
		// check argument
		if(data.column.empty())
			throw Database::Exception("Main::Database::getCustomData(): No column name specified");

		if(data.type == Data::Type::_unknown)
			throw Database::Exception("Main::Database::getCustomData(): No column type specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT `" + data.column + "`"
							" FROM `" + data.table + "`"
							" WHERE (" + data.condition + ")"
					)
			);

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				if(sqlResultSet->isNull(data.column))
					data.value = Data::Value();
				else {
					switch(data.type) {
					case Data::Type::_bool:
						data.value = Data::Value(sqlResultSet->getBoolean(data.column));

						break;

					case Data::Type::_double:
						data.value = Data::Value(static_cast<double>(sqlResultSet->getDouble(data.column)));

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
								"Main::Database::getCustomData(): Invalid data type when getting custom data."
						);
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from multiple fields of a row in the database, throws Database::Exception
	void Database::getCustomData(Data::GetFields& data) {
		// check argument
		if(data.columns.empty())
			throw Database::Exception("Main::Database::getCustomData(): No column names specified");

		if(data.type == Data::Type::_unknown)
			throw Database::Exception("Main::Database::getCustomData(): No column type specified");

		// clear and reserve memory
		data.values.clear();
		data.values.reserve(data.columns.size());

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// create SQL query
			std::string sqlQuery("SELECT ");

			for(const auto& column : data.columns)
				sqlQuery += "`" + column + "`, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `" + data.table + "` WHERE (" + data.condition + ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement, sqlQuery));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				switch(data.type) {
				case Data::Type::_bool:
					for(const auto& column : data.columns)
						if(sqlResultSet->isNull(column))
							data.values.emplace_back();
						else
							data.values.emplace_back(sqlResultSet->getBoolean(column));

					break;

				case Data::Type::_double:
					for(const auto& column : data.columns)
						if(sqlResultSet->isNull(column))
							data.values.emplace_back();
						else
							data.values.emplace_back(static_cast<double>(sqlResultSet->getDouble(column)));

					break;

				case Data::Type::_int32:
					for(const auto& column : data.columns)
						if(sqlResultSet->isNull(column))
							data.values.emplace_back();
						else
							data.values.emplace_back(sqlResultSet->getInt(column));

					break;

				case Data::Type::_int64:
					for(const auto& column : data.columns)
						if(sqlResultSet->isNull(column))
							data.values.emplace_back();
						else
							data.values.emplace_back(sqlResultSet->getInt64(column));

					break;

				case Data::Type::_string:
					for(const auto& column : data.columns)
						if(sqlResultSet->isNull(column))
							data.values.emplace_back();
						else
							data.values.emplace_back(sqlResultSet->getString(column));

					break;

				case Data::Type::_uint32:
					for(const auto& column : data.columns)
						if(sqlResultSet->isNull(column))
							data.values.emplace_back();
						else
							data.values.emplace_back(sqlResultSet->getUInt(column));

					break;

				case Data::Type::_uint64:
					for(const auto& column : data.columns)
						if(sqlResultSet->isNull(column))
							data.values.emplace_back();
						else
							data.values.emplace_back(sqlResultSet->getUInt64(column));

					break;

				default:
					throw Database::Exception(
							"Main::Database::getCustomData(): Invalid data type when getting custom data."
					);
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from multiple fields of a row with different types in the database, throws Database::Exception
	void Database::getCustomData(Data::GetFieldsMixed& data) {
		// check argument
		if(data.columns_types.empty())
			throw Database::Exception("Main::Database::getCustomData(): No columns specified");

		// clear and reserve memory
		data.values.clear();
		data.values.reserve(data.columns_types.size());

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// create SQL query
			std::string sqlQuery("SELECT ");

			for(const auto& column_type : data.columns_types)
				sqlQuery += "`" + column_type.first + "`, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `" + data.table + "` WHERE (" + data.condition + ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement, sqlQuery));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				for(const auto& column_type : data.columns_types) {
					if(sqlResultSet->isNull(column_type.first))
						data.values.emplace_back();
					else {
						switch(column_type.second) {
						case Data::Type::_bool:
							data.values.emplace_back(sqlResultSet->getBoolean(column_type.first));

							break;

						case Data::Type::_double:
							data.values.emplace_back(static_cast<double>(sqlResultSet->getDouble(column_type.first)));

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
									"Main::Database::getCustomData(): Invalid data type when getting custom data."
							);
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from one column in the database, throws Database::Exception
	void Database::getCustomData(Data::GetColumn& data) {
		// check argument
		if(data.column.empty())
			throw Database::Exception("Main::Database::getCustomData(): No columns specified");

		if(data.type == Data::Type::_unknown)
			throw Database::Exception("Main::Database::getCustomData(): No column type specified");

		// clear memory
		data.values.clear();

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// create SQL query
			std::string sqlQuery("SELECT `" + data.column + "` FROM `" + data.table + "`");

			if(!data.condition.empty())
				sqlQuery += " WHERE (" + data.condition + ")";

			if(!data.order.empty())
				sqlQuery += " ORDER BY (" + data.order + ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement, sqlQuery));

			// get result
			if(sqlResultSet) {
				data.values.reserve(sqlResultSet->rowsCount());

				while(sqlResultSet->next()) {
					if(sqlResultSet->isNull(data.column))
						data.values.emplace_back();
					else {
						switch(data.type) {
						case Data::Type::_bool:
							data.values.emplace_back(sqlResultSet->getBoolean(data.column));

							break;

						case Data::Type::_double:
							data.values.emplace_back(static_cast<double>(sqlResultSet->getDouble(data.column)));

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
									"Main::Database::getCustomData(): Invalid data type when getting custom data."
							);
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from multiple columns of the same type in the database, throws Database::Exception
	void Database::getCustomData(Data::GetColumns& data) {
		// check argument
		if(data.columns.empty())
			throw Database::Exception("Main::Database::getCustomData(): No column name specified");

		if(data.type == Data::Type::_unknown)
			throw Database::Exception("Main::Database::getCustomData(): No column type specified");

		// clear and reserve memory
		data.values.clear();

		data.values.reserve(data.columns.size());

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// create SQL query
			std::string sqlQuery("SELECT ");

			for(const auto& column : data.columns) {
				sqlQuery += "`" + column + "`, ";

				// add column to result vector
				data.values.emplace_back();
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `" + data.table + "`";

			if(!data.condition.empty())
				sqlQuery += " WHERE (" + data.condition + ")";

			if(!data.order.empty())
				sqlQuery += " ORDER BY (" + data.order + ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement, sqlQuery));

			if(sqlResultSet) {
				// reserve memory for results
				for(auto& value : data.values)
					value.reserve(sqlResultSet->rowsCount());

				// get results
				while(sqlResultSet->next()) {
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i) {
						auto& column = data.values.at(i - data.columns.begin());

						if(sqlResultSet->isNull(*i))
							column.emplace_back();
						else {
							switch(data.type) {
							case Data::Type::_bool:
								column.emplace_back(sqlResultSet->getBoolean(*i));

								break;

							case Data::Type::_double:
								column.emplace_back(static_cast<double>(sqlResultSet->getDouble(*i)));

								break;

							case Data::Type::_int32:
								column.emplace_back(sqlResultSet->getInt(*i));

								break;

							case Data::Type::_int64:
								column.emplace_back(sqlResultSet->getInt64(*i));

								break;

							case Data::Type::_string:
								column.emplace_back(sqlResultSet->getString(*i));

								break;

							case Data::Type::_uint32:
								column.emplace_back(sqlResultSet->getUInt(*i));

								break;

							case Data::Type::_uint64:
								column.emplace_back(sqlResultSet->getUInt64(*i));

								break;

							default:
								throw Database::Exception(
										"Main::Database::getCustomData(): Invalid data type when getting custom data."
								);
							}
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from multiple columns of different types in the database, throws Database::Exception
	void Database::getCustomData(Data::GetColumnsMixed& data) {
		// check argument
		if(data.columns_types.empty())
			throw Database::Exception("Main::Database::getCustomData(): No columns specified");

		// clear and reserve memory
		data.values.clear();

		data.values.reserve(data.columns_types.size());

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// create SQL query
			std::string sqlQuery("SELECT ");

			for(const auto& column_type : data.columns_types) {
				sqlQuery += "`" + column_type.first + "`, ";

				// add column to result vector
				data.values.emplace_back();
			}

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `" + data.table + "`";

			if(!data.condition.empty())
				sqlQuery += " WHERE (" + data.condition + ")";

			if(!data.order.empty())
				sqlQuery += " ORDER BY (" + data.order + ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement, sqlQuery));

			if(sqlResultSet) {
				// reserve memory for results
				for(auto& value : data.values)
					value.reserve(sqlResultSet->rowsCount());

				// get results
				while(sqlResultSet->next()) {
					for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
						auto& column = data.values.at(i - data.columns_types.begin());

						if(sqlResultSet->isNull(i->first))
							column.emplace_back();
						else {
							switch(i->second) {
							case Data::Type::_bool:
								column.emplace_back(sqlResultSet->getBoolean(i->first));

								break;

							case Data::Type::_double:
								column.emplace_back(static_cast<double>(sqlResultSet->getDouble(i->first)));

								break;

							case Data::Type::_int32:
								column.emplace_back(sqlResultSet->getInt(i->first));

								break;

							case Data::Type::_int64:
								column.emplace_back(sqlResultSet->getInt64(i->first));

								break;

							case Data::Type::_string:
								column.emplace_back(sqlResultSet->getString(i->first));

								break;

							case Data::Type::_uint32:
								column.emplace_back(sqlResultSet->getUInt(i->first));

								break;

							case Data::Type::_uint64:
								column.emplace_back(sqlResultSet->getUInt64(i->first));

								break;

							default:
								throw Database::Exception(
										"Main::Database::getCustomData(): Invalid data type when getting custom data."
								);
							}
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// insert one custom value into a row in the database, throws Database::Exception
	void Database::insertCustomData(const Data::InsertValue& data) {
		// check argument
		if(data.column.empty())
			throw Database::Exception("Main::Database::insertCustomData(): No column name specified");

		if(data.type == Data::Type::_unknown)
			throw Database::Exception("Main::Database::insertCustomData(): No column type specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"INSERT INTO `" + data.table + "` (`" + data.column + "`)"
							" VALUES (?)"
					)
			);

			// set value
			if(data.value._isnull)
				sqlStatement->setNull(1, 0);
			else {
				switch(data.type) {
				case Data::Type::_bool:
					sqlStatement->setBoolean(1, data.value._b);

					break;

				case Data::Type::_double:
					sqlStatement->setDouble(1, data.value._d);

					break;

				case Data::Type::_int32:
					sqlStatement->setInt(1, data.value._i32);

					break;

				case Data::Type::_int64:
					sqlStatement->setInt64(1, data.value._i64);

					break;

				case Data::Type::_string:
					if(data.value._s.size() > this->getMaxAllowedPacketSize()) {
						switch(data.value._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(1, data.value._s.substr(0, this->getMaxAllowedPacketSize()));

							break;

						case Data::Value::_if_too_large::_empty:
							sqlStatement->setString(1, "");

							break;

						case Data::Value::_if_too_large::_null:
							sqlStatement->setNull(1, 0);

							break;

						default:
							std::ostringstream errStrStr;

							errStrStr.imbue(std::locale(""));

							errStrStr	<< "Main::Database::insertCustomData(): Size ("
										<< data.value._s.size()
										<< " bytes) of custom value for `"
										<< data.table
										<< "`.`"
										<< data.column
										<< "` exceeds the ";

							if(data.value._s.size() > 1073741824)
								errStrStr << "MySQL data limit of 1 GiB";
							else
								errStrStr	<< "current MySQL server limit of "
											<< this->getMaxAllowedPacketSize()
											<< " bytes - adjust the \'max_allowed_packet\'"
												" setting on the server accordingly"
												" (to max. 1 GiB).";

							throw Database::Exception(errStrStr.str());
						}
					}
					else
						sqlStatement->setString(1, data.value._s);

					break;

				case Data::Type::_uint32:
					sqlStatement->setUInt(1, data.value._ui32);

					break;

				case Data::Type::_uint64:
					sqlStatement->setUInt64(1, data.value._ui64);

					break;

				default:
					throw Database::Exception(
							"Main::Database::insertCustomData(): Invalid data type when inserting custom data."
					);
				}
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::insertCustomData", e); }
	}

	// insert custom values into multiple fields of the same type into a row in the database,
	//  throws Database::Exception
	void Database::insertCustomData(const Data::InsertFields& data) {
		// check argument
		if(data.columns_values.empty())
			throw Database::Exception("Main::Database::insertCustomData(): No columns specified");

		if(data.type == Data::Type::_unknown)
			throw Database::Exception("Main::Database::insertCustomData(): No column type specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery("INSERT INTO `" + data.table + "` (");

			for(const auto& column_value : data.columns_values)
				sqlQuery += "`" + column_value.first + "`, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += ") VALUES(";

			for(std::size_t n = 0; n < data.columns_values.size() - 1; ++n)
				sqlQuery += "?, ";

			sqlQuery += "?)";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// set values
			unsigned int counter = 1;

			switch(data.type) {
			case Data::Type::_bool:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setBoolean(counter, column_value.second._b);

					++counter;
				}

				break;

			case Data::Type::_double:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setDouble(counter, column_value.second._d);

					++counter;
				}

				break;

			case Data::Type::_int32:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setInt(counter, column_value.second._i32);

					++counter;
				}

				break;

			case Data::Type::_int64:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setInt64(counter, column_value.second._i64);

					++counter;
				}

				break;

			case Data::Type::_string:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull) sqlStatement->setNull(counter, 0);
					else if(column_value.second._s.size() > this->getMaxAllowedPacketSize()) {
						switch(column_value.second._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(1, column_value.second._s.substr(0, this->getMaxAllowedPacketSize()));

							break;

						case Data::Value::_if_too_large::_empty:
							sqlStatement->setString(1, "");

							break;

						case Data::Value::_if_too_large::_null:
							sqlStatement->setNull(1, 0);

							break;

						default:
							std::ostringstream errStrStr;

							errStrStr.imbue(std::locale(""));

							errStrStr	<< "Main::Database::insertCustomData(): Size ("
										<< column_value.second._s.size()
										<< " bytes) of custom value for `"
										<< data.table << "`.`"
										<< column_value.first
										<< "` exceeds the ";

							if(column_value.second._s.size() > 1073741824)
								errStrStr << "MySQL data limit of 1 GiB";
							else
								errStrStr	<< "current MySQL server limit of "
											<< this->getMaxAllowedPacketSize()
											<< " bytes - adjust the \'max_allowed_packet\'"
												" setting on the server accordingly"
												" (to max. 1 GiB).";

							throw Database::Exception(errStrStr.str());
						}
					}
					else
						sqlStatement->setString(counter, column_value.second._s);

					++counter;
				}

				break;

			case Data::Type::_uint32:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setUInt(counter, column_value.second._ui32);

					++counter;
				}

				break;

			case Data::Type::_uint64:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setUInt64(counter, column_value.second._ui64);

					++counter;
				}

				break;

			default:
				throw Database::Exception(
						"Main::Database::insertCustomData(): Invalid data type when inserting custom data."
				);
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::insertCustomData", e); }
	}

	// insert custom values into multiple fields of different types into a row in the database,
	//  throws Database::Exception
	void Database::insertCustomData(const Data::InsertFieldsMixed& data) {
		// check argument
		if(data.columns_types_values.empty())
			throw Database::Exception("Main::Database::insertCustomData(): No columns specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery("INSERT INTO `" + data.table + "` (");

			for(const auto& column_type_value : data.columns_types_values)
				sqlQuery += "`" + std::get<0>(column_type_value) + "`, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += ") VALUES(";

			for(std::size_t n = 0; n < data.columns_types_values.size() - 1; ++n)
				sqlQuery += "?, ";

			sqlQuery += "?)";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// set values
			unsigned int counter = 1;

			for(const auto& column_type_value : data.columns_types_values) {
				if(std::get<2>(column_type_value)._isnull)
					sqlStatement->setNull(counter, 0);
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
										1,
										std::get<2>(column_type_value)._s.substr(
												0,
												this->getMaxAllowedPacketSize()
										)
								);

								break;

							case Data::Value::_if_too_large::_empty:
								sqlStatement->setString(1, "");

								break;

							case Data::Value::_if_too_large::_null:
								sqlStatement->setNull(1, 0);

								break;

							default:
								std::ostringstream errStrStr;

								errStrStr.imbue(std::locale(""));

								errStrStr	<< "Main::Database::insertCustomData(): Size ("
											<< std::get<2>(column_type_value)._s.size()
											<< " bytes) of custom value for `"
											<< data.table
											<< "`.`"
											<< std::get<0>(column_type_value)
											<< "` exceeds the ";

								if(std::get<2>(column_type_value)._s.size() > 1073741824)
									errStrStr << "MySQL data limit of 1 GiB";
								else
									errStrStr	<< "current MySQL server limit of "
												<< this->getMaxAllowedPacketSize()
												<< " bytes - adjust the \'max_allowed_packet\'"
													" setting on the server accordingly"
												 	" (to max. 1 GiB).";

								throw Database::Exception(errStrStr.str());
							}
						}
						else
							sqlStatement->setString(counter, std::get<2>(column_type_value)._s);

						break;

					case Data::Type::_uint32:
						sqlStatement->setUInt(counter, std::get<2>(column_type_value)._ui32);

						break;

					case Data::Type::_uint64:
						sqlStatement->setUInt64(counter, std::get<2>(column_type_value)._ui64);

						break;

					default:
						throw Database::Exception(
								"Main::Database::insertCustomData(): Invalid data type when inserting custom data."
						);
					}
				}

				++counter;
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::insertCustomData", e); }
	}

	// update one custom value in one field of a row in the database, throws Database::Exception
	void Database::updateCustomData(const Data::UpdateValue& data) {
		// check argument
		if(data.column.empty())
			throw Database::Exception("Main::Database::updateCustomData(): No column name specified");

		if(data.type == Data::Type::_unknown)
			throw Database::Exception("Main::Database::updateCustomData(): No column type specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"UPDATE `" + data.table + "`"
							" SET `" + data.column + "` = ?"
							" WHERE (" + data.condition + ")"
					)
			);

			// set value
			if(data.value._isnull)
				sqlStatement->setNull(1, 0);
			else {
				switch(data.type) {
				case Data::Type::_bool:
					sqlStatement->setBoolean(1, data.value._b);

					break;

				case Data::Type::_double:
					sqlStatement->setDouble(1, data.value._d);

					break;

				case Data::Type::_int32:
					sqlStatement->setInt(1, data.value._i32);

					break;

				case Data::Type::_int64:
					sqlStatement->setInt64(1, data.value._i64);

					break;

				case Data::Type::_string:
					if(data.value._s.size() > this->getMaxAllowedPacketSize()) {
						switch(data.value._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(1, data.value._s.substr(0, this->getMaxAllowedPacketSize()));

							break;

						case Data::Value::_if_too_large::_empty:
							sqlStatement->setString(1, "");

							break;

						case Data::Value::_if_too_large::_null:
							sqlStatement->setNull(1, 0);

							break;

						default:
							std::ostringstream errStrStr;

							errStrStr.imbue(std::locale(""));

							errStrStr	<< "Main::Database::updateCustomData(): Size ("
										<< data.value._s.size()
										<< " bytes) of custom value for `"
										<< data.table
										<< "`.`"
										<< data.column
										<< "` exceeds the ";

							if(data.value._s.size() > 1073741824)
								errStrStr << "MySQL data limit of 1 GiB";
							else
								errStrStr	<< "current MySQL server limit of "
											<< this->getMaxAllowedPacketSize()
											<< " bytes - adjust the \'max_allowed_packet\'"
												" setting on the server accordingly"
												" (to max. 1 GiB).";

							throw Database::Exception(errStrStr.str());
						}
					}
					else sqlStatement->setString(1, data.value._s);

					break;

				case Data::Type::_uint32:
					sqlStatement->setUInt(1, data.value._ui32);

					break;

				case Data::Type::_uint64:
					sqlStatement->setUInt64(1, data.value._ui64);

					break;

				default:
					throw Database::Exception("Main::Database::updateCustomData(): Invalid data type when updating custom data.");
				}
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateCustomData", e); }
	}

	// update custom values in multiple fields of a row with the same type in the database,
	//  throws Database::Exception
	void Database::updateCustomData(const Data::UpdateFields& data) {
		// check argument
		if(data.columns_values.empty())
			throw Database::Exception("Main::Database::updateCustomData(): No columns specified");

		if(data.type == Data::Type::_unknown)
			throw Database::Exception("Main::Database::updateCustomData(): No column type specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery("UPDATE `" + data.table + "` SET ");

			for(const auto& column_value : data.columns_values)
				sqlQuery += "`" + column_value.first + "` = ?, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " WHERE (" + data.condition + ")";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// set values
			unsigned int counter = 1;

			switch(data.type) {
			case Data::Type::_bool:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setBoolean(counter, column_value.second._b);

					++counter;
				}

				break;

			case Data::Type::_double:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setDouble(counter, column_value.second._d);

					++counter;
				}

				break;

			case Data::Type::_int32:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setInt(counter, column_value.second._i32);

					++counter;
				}

				break;

			case Data::Type::_int64:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setInt64(counter, column_value.second._i64);

					++counter;
				}

				break;

			case Data::Type::_string:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else if(column_value.second._s.size() > this->getMaxAllowedPacketSize()) {
						switch(column_value.second._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(1, column_value.second._s.substr(0, this->getMaxAllowedPacketSize()));

							break;

						case Data::Value::_if_too_large::_empty:
							sqlStatement->setString(1, "");

							break;

						case Data::Value::_if_too_large::_null:
							sqlStatement->setNull(1, 0);

							break;

						default:
							std::ostringstream errStrStr;

							errStrStr.imbue(std::locale(""));

							errStrStr	<< "Main::Database::updateCustomData(): Size ("
										<< column_value.second._s.size()
										<< " bytes) of custom value for `"
										<< data.table
										<< "`.`"
										<< column_value.first
										<< "` exceeds the ";

							if(column_value.second._s.size() > 1073741824)
								errStrStr << "MySQL data limit of 1 GiB";
							else
								errStrStr	<< "current MySQL server limit of "
											<< this->getMaxAllowedPacketSize()
											<< " bytes - adjust the \'max_allowed_packet\'"
												" setting on the server accordingly"
											 	" (to max. 1 GiB).";

							throw Database::Exception(errStrStr.str());
						}
					}
					else
						sqlStatement->setString(counter, column_value.second._s);

					++counter;
				}

				break;

			case Data::Type::_uint32:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setUInt(counter, column_value.second._ui32);

					++counter;
				}

				break;

			case Data::Type::_uint64:
				for(const auto& column_value : data.columns_values) {
					if(column_value.second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setUInt64(counter, column_value.second._ui64);

					++counter;
				}

				break;

			default:
				throw Database::Exception("Main::Database::updateCustomData(): Invalid data type when updating custom data.");
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateCustomData", e); }
	}

	// update custom values in multiple fields of a row with different types in the database,
	//  throws Database::Exception
	void Database::updateCustomData(const Data::UpdateFieldsMixed& data) {
		// check argument
		if(data.columns_types_values.empty())
			throw Database::Exception("Main::Database::updateCustomData(): No columns specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery("UPDATE `" + data.table + "` SET ");

			for(const auto& column_type_value : data.columns_types_values)
				sqlQuery += "`" + std::get<0>(column_type_value) + "` = ?, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " WHERE (" + data.condition + ")";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// set values
			unsigned int counter = 1;

			for(const auto& column_type_value : data.columns_types_values) {
				if(std::get<2>(column_type_value)._isnull)
					sqlStatement->setNull(counter, 0);
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
										1,
										std::get<2>(column_type_value)._s.substr(
												0,
												this->getMaxAllowedPacketSize()
										)
								);

								break;

							case Data::Value::_if_too_large::_empty:
								sqlStatement->setString(1, "");

								break;

							case Data::Value::_if_too_large::_null:
								sqlStatement->setNull(1, 0);

								break;

							default:
								std::ostringstream errStrStr;

								errStrStr.imbue(std::locale(""));

								errStrStr	<< "Main::Database::updateCustomData(): Size ("
											<< std::get<2>(column_type_value)._s.size()
											<< " bytes) of custom value for `"
											<< data.table
											<< "`.`"
											<< std::get<0>(column_type_value)
											<< "` exceeds the ";

								if(std::get<2>(column_type_value)._s.size() > 1073741824)
									errStrStr << "MySQL data limit of 1 GiB";
								else
									errStrStr	<< "current MySQL server limit of "
												<< this->getMaxAllowedPacketSize()
												<< " bytes - adjust the \'max_allowed_packet\'"
													" setting on the server accordingly"
												 	" (to max. 1 GiB).";

								throw Database::Exception(errStrStr.str());
							}
						}
						else
							sqlStatement->setString(counter, std::get<2>(column_type_value)._s);

						break;

					case Data::Type::_uint32:
						sqlStatement->setUInt(counter, std::get<2>(column_type_value)._ui32);

						break;

					case Data::Type::_uint64:
						sqlStatement->setUInt64(counter, std::get<2>(column_type_value)._ui64);

						break;

					default:
						throw Database::Exception(
								"Main::Database::updateCustomData(): Invalid data type when updating custom data."
						);
					}
				}

				++counter;
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateCustomData", e); }
	}

	/*
	 * HELPER FUNCTIONS FOR PREPARED SQL STATEMENTS (protected)
	 */

	// reserve memory for a specific number of SQL statements to additionally prepare
	void Database::reserveForPreparedStatements(std::size_t numberOfAdditionalPreparedStatements) {
		this->preparedStatements.reserve(this->preparedStatements.size() + numberOfAdditionalPreparedStatements);
	}

	// prepare additional SQL statement, return the ID of the newly prepared SQL statement
	std::size_t Database::addPreparedStatement(const std::string& sqlQuery) {
		// check connection
		this->checkConnection();

		// try to prepare SQL statement
		try {
			this->preparedStatements.emplace_back(this->connection.get(), sqlQuery);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addPreparedStatement", e); }

		return this->preparedStatements.size();
	}

	// get reference to prepared SQL statement by its ID
	//  WARNING: Do not run Database::checkConnection() while using this reference!
	sql::PreparedStatement& Database::getPreparedStatement(std::size_t id) {
		try {
			return this->preparedStatements.at(id - 1).get();
		}
		catch(const sql::SQLException &e) {
			this->sqlException("Main::Database::getPreparedStatement", e);

			throw;// will not be used
		}
	}

	/*
	 * DATABASE HELPER FUNCTIONS (protected)
	 */

	// get the last inserted ID from the database, throws Database::Exception
	std::uint64_t Database::getLastInsertedId() {
		std::uint64_t result = 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.lastId))
			throw Database::Exception(
					"Main::Database::getLastInsertedId: Missing prepared SQL statement for last inserted ID"
			);

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.lastId));

		try {
			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("id");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getLastInsertedId", e); }

		return result;
	}

	// reset the auto increment of an (empty) table in the database, throws Database::Exception
	void Database::resetAutoIncrement(const std::string& tableName) {
		// check argument
		if(tableName.empty())
			throw Database::Exception("Main::Database::resetAutoIncrement(): No table name specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			Database::sqlExecute(sqlStatement, "ALTER TABLE `" + tableName + "` AUTO_INCREMENT = 1");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetAutoIncrement", e); }
	}

	// add a lock with a specific name to the database, wait if lock already exists
	void Database::addDatabaseLock(const std::string& name, IsRunningCallback isRunningCallback) {
		while(isRunningCallback()) {
			{ // lock access to locks
				std::lock_guard<std::mutex> accessLock(Database::lockAccess);

				// check whether lock with specified name already exists
				if(
						std::find(
								Database::locks.begin(),
								Database::locks.end(),
								name
						) == Database::locks.end()
				) {
					// lock does not exist: add lock and return true
					Database::locks.emplace_back(name);

					break;
				}
			}

			// sleep before re-attempting to add database lock
			std::this_thread::sleep_for(std::chrono::milliseconds(MAIN_DATABASE_SLEEP_ON_LOCK_MS));
		}
	}

	// remove lock with specific name from database
	void Database::removeDatabaseLock(const std::string& name) {
		std::lock_guard<std::mutex> accessLock(Database::lockAccess);

		Database::locks.erase(
				std::remove(
						Database::locks.begin(),
						Database::locks.end(),
						name
				),
				Database::locks.end()
		);
	}

	// add a table to the database (the primary key 'id' will be created automatically), throws Database::Exception
	void Database::createTable(const TableProperties& properties) {
		// check argument
		if(properties.name.empty())
			throw Database::Exception("Main::Database::createTable(): No table name specified");

		if(properties.columns.empty())
			throw Database::Exception("Main::Database::createTable(): No columns specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery(
					"CREATE TABLE IF NOT EXISTS `" + properties.name + "`"
						"(id SERIAL"
			);

			std::string propertiesString;

			for(const auto& column : properties.columns) {
				// check values
				if(column.name.empty())
					throw Database::Exception("Main::Database::createTable(): A column is missing its name");

				if(column.type.empty())
					throw Database::Exception("Main::Database::createTable(): A column is missing its type");

				// add to SQL query
				sqlQuery += ", `" + column.name + "` " + column.type;

				// add indices and references
				if(column.indexed)
					propertiesString += ", INDEX(`" + column.name + "`)";

				if(!column.referenceTable.empty()) {
					if(column.referenceColumn.empty())
						throw Database::Exception(
								"Main::Database::createTable(): A column reference is missing its source column name"
						);

					propertiesString	+= ", FOREIGN KEY(`"
										+ column.name
										+ "`) REFERENCES `"
										+ column.referenceTable
										+ "` (`"
										+ column.referenceColumn
										+ "`) ON UPDATE RESTRICT ON DELETE CASCADE";
				}
			}

			sqlQuery += ", PRIMARY KEY(id)" + propertiesString + ")"
						" CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci,"
						" ENGINE=InnoDB";

			if(properties.compressed)
				sqlQuery += ", ROW_FORMAT=COMPRESSED";

			if(!properties.dataDirectory.empty())
				sqlQuery += ", DATA DIRECTORY='" + properties.dataDirectory + "'";

			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			Database::sqlExecute(sqlStatement, sqlQuery);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::createTable", e); }
	}

	// delete table from database if it exists, throws Database::Exception
	void Database::dropTable(const std::string& tableName) {
		// check argument
		if(tableName.empty())
			throw Database::Exception("Main::Database::dropTable(): No table name specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			Database::sqlExecute(sqlStatement, "DROP TABLE IF EXISTS `" + tableName + "`");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::dropTable", e); }
	}

	// add a column to a table in the database, throws Database::Exception
	void Database::addColumn(const std::string& tableName, const TableColumn& column) {
		// check arguments
		if(tableName.empty())
			throw Database::Exception("Main::Database::addColumn(): No table name specified");

		if(column.name.empty())
			throw Database::Exception("Main::Database::addColumn(): No column name specified");

		if(column.type.empty())
			throw Database::Exception("Main::Database::addColumn(): No column type specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery(
					"ALTER TABLE `"
					+ tableName
					+ "` ADD COLUMN `"
					+ column.name
					+ "` "
					+ column.type
			);

			if(!column.referenceTable.empty()) {
				if(column.referenceColumn.empty())
					throw Database::Exception(
							"Main::Database::addColumn(): A column reference is missing its source column name"
					);

				sqlQuery += ", ADD FOREIGN KEY(`" + column.name + "`)"
							" REFERENCES `" + column.referenceTable + "`(`" + column.referenceColumn + "`)"
							" ON UPDATE RESTRICT ON DELETE CASCADE";
			}

			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			Database::sqlExecute(sqlStatement, sqlQuery);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addColumn", e); }
	}

	// compress a table in the database, throws Database::Exception
	void Database::compressTable(const std::string& tableName) {
		bool compressed = false;

		// check argument
		if(tableName.empty())
			throw Database::Exception("Main::Database::compressTable(): No table name specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement for checking whether the table is already compressed
			SqlResultSetPtr sqlResultSet(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SELECT LOWER(row_format) = 'compressed'"
							" AS result FROM information_schema.tables"
							" WHERE table_schema = '" + this->settings.name + "'"
							" AND table_name = '" + tableName + "'"
							" LIMIT 1"
					)
			);

			// get result
			if(sqlResultSet && sqlResultSet->next())
				compressed = sqlResultSet->getBoolean("result");
			else
				throw Database::Exception(
						"Main::Database::compressTable(): Could not determine row format of \'" + tableName + "\'"
				);

			// execute SQL statement for compressing the table if table is not already compressed
			if(!compressed)
				Database::sqlExecute(
						sqlStatement,
						"ALTER TABLE `" + tableName + "` ROW_FORMAT=COMPRESSED"
				);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::compressTable", e); }
	}

	// delete a table from the database if it exists, throws Database::Exception
	void Database::deleteTable(const std::string& tableName) {
		// check argument
		if(tableName.empty())
			throw Database::Exception("Main::Database::deleteTableIfExists(): No table name specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE IF EXISTS `" + tableName + "`"
			);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteTable", e); }
	}

	// check access to an external data directory, throws Database::Exception
	void Database::checkDirectory(const std::string& dir) {
		// check argument
		if(dir.empty())
			throw Database::Exception("Main::Database::checkDirectory(): No external directory specified.");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL commands
			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE IF EXISTS `crawlserv_testaccess`"
			);

			Database::sqlExecute(
					sqlStatement,
					"CREATE TABLE `crawlserv_testaccess(id SERIAL)` DATA DIRECTORY=`" + dir + "`"
			);

			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE `crawlserv_testaccess`"
			);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::checkDirectory", e); }
	}

	// clone table as '<tablename>_tmp' into different data directory (without data or constraints),
	// 	return the constraints that have been dropped - NOTE: <tablename>_tmp may not exist!
	std::queue<std::string> Database::cloneTable(const std::string& tableName, const std::string& dataDir) {
		std::queue<std::string> constraints;

		// check argument
		if(tableName.empty())
			throw Database::Exception("Main::Database::cloneTable(): No table specified.");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// drop temporary table if necessary
			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE IF EXISTS `crawlserv_tmp`"
			);

			// get constraints that will be dropped
			std::string result;

			SqlResultSetPtr sqlResultSet1(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SHOW CREATE TABLE `" + tableName + "`"
					)
			);

			if(sqlResultSet1 && sqlResultSet1->next())
				result = sqlResultSet1->getString("Create Table");
			else
				throw Database::Exception(
						"Main::Database::cloneTable(): Could not get properties of table `"
						+ tableName
						+ "`"
				);

			std::istringstream stream(result);
			std::string line;

			// parse single constraints
			while(std::getline(stream, line)) {
				Helper::Strings::trim(line);

				if(line.length() > 11 && line.substr(0, 11) == "CONSTRAINT ") {
					line.erase(0, 11);

					const auto pos = line.find("` ");

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
			SqlResultSetPtr sqlResultSet2(
					Database::sqlExecuteQuery(
							sqlStatement,
							"SHOW CREATE TABLE `crawlserv_tmp`"
					)
			);

			if(sqlResultSet2 && sqlResultSet2->next())
				result = sqlResultSet2->getString("Create Table");
			else
				throw Database::Exception(
						"Main::Database::cloneTable(): Could not get properties of table `crawlserv_tmp`"
				);

			// drop temporary table
			Database::sqlExecute(
					sqlStatement,
					"DROP TABLE `crawlserv_tmp`"
			);

			// replace table name and add new data directory
			const auto pos = result.find("` ") + 2;

			result = "CREATE TABLE `" + tableName + "_tmp` " + result.substr(pos);

			result += " DATA DIRECTORY=\'" + dataDir + "\'";

			// create new table
			Database::sqlExecute(sqlStatement, result);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::cloneTable", e); }

		return constraints;
	}

	/*
	 * URL LIST HELPER FUNCTIONS (protected)
	 */

	// get whether the specified URL list is case-sensitive, throws Database::Exception
	bool Database::isUrlListCaseSensitive(std::uint64_t listId) {
		bool result = true;

		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::isUrlListCaseSensitive(): No URL list specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(
					"SELECT case_sensitive"
					" FROM `crawlserv_urllists`"
					" WHERE id = ?"
					" LIMIT 1"
			));

			// execute SQL statement
			sqlStatement->setUInt64(1, listId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("case_sensitive");
			else
				throw Database::Exception(
						"Main::Database::isUrlListCaseSensitive():"
						" Could not get case sensitivity for URL list #"
						+ std::to_string(listId)
				);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::isUrlListCaseSensitive", e); }

		return result;
	}

	// set whether the specified URL list is case-sensitive, throws Database::Exception
	void Database::setUrlListCaseSensitive(std::size_t listId, bool isCaseSensitive) {
		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::setUrlListCaseSensitive(): No URL list specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(
					"UPDATE `crawlserv_urllists`"
					" SET case_sensitive = ?"
					" WHERE id = ?"
					" LIMIT 1"
			));

			// execute SQL statement
			sqlStatement->setBoolean(1, isCaseSensitive);
			sqlStatement->setUInt64(2, listId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::setUrlListCaseSensitive", e); }
	}

	/*
	 * EXCEPTION HELPER FUNCTION (protected)
	 */

	// catch SQL exception and re-throw it as specific Database::[X]Exception or generic Database::Exception
	void Database::sqlException(const std::string& function, const sql::SQLException& e) {
		// get error code and create error string
		const int error = e.getErrorCode();
		std::string errorStr(
				function
				+ "() SQL Error #"
				+ std::to_string(error)
				+ " (State "
				+ e.getSQLState()
				+ "): "
				+ std::string(e.what())
		);

		switch(error) {
		// check for connection error
		case 1027: // Sort aborted
		case 1040: // Too many connections
		case 1042: // Can't get hostname for your address
		case 1043: // Bad handshake
		case 1053: // Server shutdown in progress
		case 1077: // Normal shutdown
		case 1078: // Got signal
		case 1079: // Shutdown complete
		case 1080: // Forcing close of thread
		case 1081: // Can't create IP socket
		case 1152: // Aborted connection
		case 1154: // Got a read error from connection pipe
		case 1156: // Got packets out of order
		case 1157: // Couldn't uncompress communication packet
		case 1158: // Got an error reading communication packets
		case 1159: // Got timeout reading communication packets
		case 1160: // Got an error writing communication packets
		case 1161: // Got timeout writing communication packets
		case 1184: // Aborted connection
		case 1189: // Net error reading from master
		case 1190: // Net error writing to master
		case 1203: // User already has more than 'max_user_connections' active connections
		case 1205: // Lock wait timeout exceeded
		case 1206: // The total number of locks exceeds the lock table size
		case 1218: // Error connecting to master
		case 1317: // Query execution was interrupted
		case 1429: // Unable to connect to foreign data source
		case 2002: // Can't connect to MySQL server throught socket
		case 2003: // Can't connect to MySQL server
		case 2005: // Unknown MySQL server host
		case 2006: // MySQL server has gone away
		case 2011: // TCP error
		case 2012: // Error in server handshake
		case 2013: // Lost connection to MySQL server during query
		case 2024: // Error connecting to slave
		case 2025: // Error connecting to master
		case 2026: // SSL connection error
		case 2027: // Malformed packet
		case 2048: // Invalid connection handle
			// throw connection exception
			throw Database::ConnectionException(errorStr);

		// check for storage engine error
		case 1030:
			throw Database::StorageEngineException(errorStr);

		// check for insufficient privileges error
		case 1045:
			throw Database::PrivilegesException(errorStr);

		// check for wrong arguments error
		case 1210:
			throw Database::WrongArgumentsException(errorStr);

		// check for incorrect path value error
		case 1525:
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
		if(sqlFile.empty())
			throw Database::Exception("Main::Database::run(): No SQL file specified");

		// open SQL file
		std::ifstream initSQLFile(sqlFile);

		if(initSQLFile.is_open()) {
			// check connection
			this->checkConnection();

			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());
			std::string line;

			if(!sqlStatement)
				throw Database::Exception("Main::Database::run(): Could not create SQL statement");

			// execute lines in SQL file
			while(std::getline(initSQLFile, line)) {
				try {
					if(!line.empty())
						Database::sqlExecute(sqlStatement, line);
				}
				catch(const sql::SQLException &e) { this->sqlException("(in " + sqlFile + ")", e); }
			}

			// close SQL file
			initSQLFile.close();
		}
		else
			throw Database::Exception("Main::Database::run(): Could not open \'" + sqlFile + "\' for reading");
	}

	// execute a SQL query, throws Database::Exception
	void Database::execute(const std::string& sqlQuery) {
		// check argument
		if(sqlQuery.empty())
			throw Database::Exception("Main::Database::execute(): No SQL query specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			Database::sqlExecute(sqlStatement, sqlQuery);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::execute", e); }
	}

	// execute a SQL query and return updated rows, throws Database::Exception
	int Database::executeUpdate(const std::string& sqlQuery) {
		int result = -1;

		// check argument
		if(sqlQuery.empty())
			throw Database::Exception("Main::Database::execute(): No SQL query specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL statement
			result = Database::sqlExecuteUpdate(sqlStatement, sqlQuery);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::execute", e); }

		return result;
	}

	// escape string for usage in SQL commands
	std::string Database::sqlEscapeString(const std::string& in) {
		// check connection
		this->checkConnection();

		// escape string
		return static_cast<sql::mysql::MySQL_Connection *>(this->connection.get())->escapeString(in);
	}

} /* crawlservpp::Main */
