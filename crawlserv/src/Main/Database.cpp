/*
 * Database.cpp
 *
 * A class to handle database access for crawlserv and its threads.
 * Thread-specific functionality is not implemented in this (parent) class.
 *
 * NOT THREAD-SAFE!
 * Use instances of the child class Module::Database for thread-specific functionality functionality instead.
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

	// constructor: save settings and set default values
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
				throw std::runtime_error("Could not get database instance");
		}
	}

	// destructor
	Database::~Database() {
		if(this->module == "server") {
			// log SQL request counter (if available)
			unsigned long long requests = this->getRequestCounter();

			if(requests) {
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr << "performed " << requests << " SQL requests.";

				try {
					this->log(logStrStr.str());
				}
				catch(...) { // could not log -> write to stdout
					std::cout.imbue(std::locale(""));

					std::cout << '\n' << requests << " SQL requests performed." << std::flush;
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
	 * SETTER
	 */

	// set the number of seconds to wait before (first and last) re-try on connection loss to mySQL server
	void Database::setSleepOnError(unsigned long seconds) {
		this->sleepOnError = seconds;
	}

	/*
	 * GETTER
	 */

	// get settings of the database
	const Database::DatabaseSettings& Database::getSettings() const {
		return this->settings;
	}

	/*
	 * INITIALIZING FUNCTIONS
	 */

	// connect to the database
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
			connectOptions["OPT_RECONNECT"] = false; // do not automatically re-connect to manually recover prepared statements
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
			//  NOTE: needs to be set independently, because setting in connection options somehow leads to invalid read of size 8
			int maxAllowedPacket = 1073741824;

			this->connection->setClientOption("OPT_MAX_ALLOWED_PACKET", (void *) &maxAllowedPacket);

			// run initializing session commands
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			if(!sqlStatement)
				throw Database::Exception("Main::Database::connect(): Could not create SQL statement");

			// set lock timeout
			std::ostringstream executeStr;

			executeStr << "SET SESSION innodb_lock_wait_timeout = ";
			executeStr << MAIN_DATABASE_LOCK_TIMEOUT_SEC;

			Database::sqlExecute(sqlStatement, executeStr.str());

			// request maximum allowed package size
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement,
					"SHOW VARIABLES LIKE 'max_allowed_packet'"
			));

			// get result
			if(sqlResultSet && sqlResultSet->next()){
				this->maxAllowedPacketSize = sqlResultSet->getUInt64("Value");

				if(!(this->maxAllowedPacketSize))
					throw Database::Exception("Main::Database::connect(): \'max_allowed_packet\' is zero");
			}
			else
				throw Database::Exception("Main::Database::connect(): Could not get \'max_allowed_packet\'");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::connect", e); }
	}

	// run initializing SQL commands by processing all .sql files in the "sql" sub-folder
	void Database::initializeSql() {
		// read 'sql' directory
		std::vector<std::string> sqlFiles;
		sqlFiles = Helper::FileSystem::listFilesInPath("sql", ".sql");

		// execute all SQL files
		for(auto i = sqlFiles.begin(); i != sqlFiles.end(); ++i)
			this->run(*i);
	}

	// prepare basic SQL statements (getting last ID, logging and thread status)
	void Database::prepare() {
		// reserve memory for SQL statements
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(unsigned short));

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
		std::vector<std::string> locales = Helper::Portability::enumLocales();

		if(!locales.empty()) {
			std::string sqlQuery("INSERT INTO `crawlserv_locales`(name) VALUES");

			for(unsigned long n = 0; n < locales.size(); ++n)
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
				unsigned long counter = 1;

				for(auto i = locales.begin(); i != locales.end(); ++i) {
					sqlStatement->setString(counter, *i);

					counter++;
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
		std::vector<std::pair<std::string, std::string>> versions(Helper::Versions::getLibraryVersions());

		if(!versions.empty()) {
			std::string sqlQuery("INSERT INTO `crawlserv_versions`(name, version) VALUES");

			for(unsigned long n = 0; n < versions.size(); ++n)
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
				unsigned long counter = 1;

				for(auto i = versions.begin(); i != versions.end(); ++i) {
					sqlStatement->setString(counter, i->first);
					sqlStatement->setString(counter + 1, i->second);

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

	// add a log entry to the database (for any module), remove invalid UTF-8 characters if needed
	void Database::log(const std::string& logModule, const std::string& logEntry) {
		bool repaired = false;
		std::string repairedEntry;

		// check argument
		repaired = Helper::Utf8::repairUtf8(logEntry, repairedEntry);

		if(repaired)
			repairedEntry += " [invalid UTF-8 character(s) removed from log]";

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.log))
			throw Database::Exception("Missing prepared SQL statement for Main::Database::log(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.log);

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

	// get the number of log entries for a specific module from the database (or for all modules if logModule is an empty string)
	unsigned long Database::getNumberOfLogEntries(const std::string& logModule) {
		unsigned long result = 0;

		// check connection
		this->checkConnection();

		// create SQL query string
		std::string sqlQuery("SELECT COUNT(*) FROM crawlserv_log");
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

	// remove the log entries of a specific module from the database (or all log entries if logModule is an empty string)
	void Database::clearLogs(const std::string& logModule) {
		// check connection
		this->checkConnection();

		// create SQL query string
		std::string sqlQuery("DELETE FROM crawlserv_log");

		if(!logModule.empty())
			sqlQuery += " WHERE module = ?";

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// execute SQL statement
			if(!logModule.empty())
				sqlStatement->setString(1, logModule);

			Database::sqlExecute(sqlStatement);

		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::clearLogs", e); }
	}

	/*
	 * THREAD FUNCTIONS
	 */

	// get all threads from the database
	std::vector<Database::ThreadDatabaseEntry> Database::getThreads() {
		std::vector<ThreadDatabaseEntry> result;

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlStatementPtr sqlStatement(this->connection->createStatement());

			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement,
					"SELECT id, module, status, paused, website, urllist, config, last FROM crawlserv_threads"
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

	// add a thread to the database and return its new ID
	unsigned long Database::addThread(const ThreadOptions& threadOptions) {
		unsigned long result = 0;

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

	// get run time of thread (in seconds) from the database by its ID
	unsigned long Database::getThreadRunTime(unsigned long threadId) {
		unsigned long result = 0;

		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::getThreadRunTime(): No thread ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(
					"SELECT runtime FROM crawlserv_threads WHERE id = ? LIMIT 1"
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

	// get pause time of thread (in seconds) from the database by its ID
	unsigned long Database::getThreadPauseTime(unsigned long threadId) {
		unsigned long result = 0;

		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::getThreadPauseTime(): No thread ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(
					"SELECT pausetime FROM crawlserv_threads WHERE id = ? LIMIT 1"
			));

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

	// update thread status in the database (and add the pause state to the status message if necessary)
	void Database::setThreadStatus(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage) {
		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::setThreadStatus(): No thread ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadStatus))
			throw Database::Exception("Missing prepared SQL statement for Main::Database::setThreadStatus(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setThreadStatus);

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

	// update thread status in the database (without using or changing the pause state)
	void Database::setThreadStatus(unsigned long threadId, const std::string& threadStatusMessage) {
		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::setThreadStatus(): No thread ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setThreadStatusMessage))
			throw Database::Exception("Missing prepared SQL statement for Main::Database::setThreadStatus(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setThreadStatusMessage);

		// create status message
		std::string statusMessage;

		statusMessage = threadStatusMessage;

		try {
			// execute SQL statement
			sqlStatement.setString(1, statusMessage);
			sqlStatement.setUInt64(2, threadId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadStatus", e); }
	}

	// set run time of thread (in seconds) in the database
	void Database::setThreadRunTime(unsigned long threadId, unsigned long threadRunTime) {
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

	// set pause time of thread (in seconds) in the database
	void Database::setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime) {
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

	// remove thread from the database by its ID
	void Database::deleteThread(unsigned long threadId) {
		// check argument
		if(!threadId)
			throw Database::Exception("Main::Database::deleteThread(): No thread ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"DELETE FROM crawlserv_threads"
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

	// add a website to the database and return its new ID
	unsigned long Database::addWebsite(const WebsiteProperties& websiteProperties) {
		unsigned long result = 0;
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

	// get website domain from the database by its ID
	std::string Database::getWebsiteDomain(unsigned long websiteId) {
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
							" FROM crawlserv_websites"
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

	// get namespace of website from the database by its ID
	std::string Database::getWebsiteNamespace(unsigned long int websiteId) {
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
							" FROM crawlserv_websites"
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

	// get ID and namespace of website from the database by URL list ID
	Database::IdString Database::getWebsiteNamespaceFromUrlList(unsigned long listId) {
		unsigned long websiteId = 0;

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
							" FROM crawlserv_urllists"
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

	// get ID and namespace of website from the database by configuration ID
	Database::IdString Database::getWebsiteNamespaceFromConfig(unsigned long configId) {
		unsigned long websiteId = 0;

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
							" FROM crawlserv_configs"
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

	// get ID and namespace of website from the database by target table ID of specified type
	Database::IdString Database::getWebsiteNamespaceFromTargetTable(const std::string& type, unsigned long tableId) {
		unsigned long websiteId = 0;

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

	// check whether website namespace exists in the database
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
								" FROM crawlserv_websites"
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

	// create new website namespace for duplicated website
	std::string Database::duplicateWebsiteNamespace(const std::string& websiteNamespace) {
		// check argument
		if(websiteNamespace.empty())
			throw Database::Exception("Main::Database::duplicateWebsiteNamespace(): No namespace specified");

		std::string nameString, numberString;
		unsigned long end = websiteNamespace.find_last_not_of("0123456789");

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

		unsigned long n = 1;
		std::string result;

		if(!numberString.empty())
			n = std::stoul(numberString, nullptr);

		// check whether number needs to be incremented
		while(true) {
			// increment number at the end of the string
			std::ostringstream resultStrStr;

			++n;

			resultStrStr << nameString << n;

			result = resultStrStr.str();

			if(!(this->isWebsiteNamespace(result)))
				break;
		}

		return result;
	}

	// get data directory of website or empty string if default directory is used
	std::string Database::getWebsiteDataDirectory(unsigned long websiteId) {
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
							" FROM crawlserv_websites"
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

	// get number of URLs that will be modified by updating the website
	unsigned long Database::getChangedUrlsByWebsiteUpdate(unsigned long websiteId, const WebsiteProperties& websiteProperties) {
		unsigned long result = 0;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::getChangedUrlsByWebsiteUpdate(): No website ID specified");

		if(websiteProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::getChangedUrlsByWebsiteUpdate(): No website namespace specified");

		if(websiteProperties.name.empty())
			throw Database::Exception("Main::Database::getChangedUrlsByWebsiteUpdate(): No website name specified");

		// get old namespace and domain
		std::string oldNamespace = this->getWebsiteNamespace(websiteId);
		std::string oldDomain = this->getWebsiteDomain(websiteId);

		// check connection
		this->checkConnection();

		try {
			if(oldDomain.empty() != websiteProperties.domain.empty()) {
				// get URL lists
				std::queue<IdString> urlLists = this->getUrlLists(websiteId);

				// create SQL statement
				SqlStatementPtr sqlStatement(this->connection->createStatement());

				if(oldDomain.empty() && !websiteProperties.domain.empty()) {
					// type changed from cross-domain to specific domain:
					// 	change URLs from absolute (URLs without protocol) to relative (sub-URLs),
					// 	discarding URLs with different domain
					while(!urlLists.empty()) {
						// count URLs of same domain
						std::string comparison = "url LIKE '" + websiteProperties.domain + "/%'";

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

	// get number of URLs that will be lost by updating the website
	unsigned long Database::getLostUrlsByWebsiteUpdate(unsigned long websiteId, const WebsiteProperties& websiteProperties) {
		unsigned long result = 0;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::getLostUrlsByWebsiteUpdate(): No website ID specified");

		if(websiteProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::getLostUrlsByWebsiteUpdate(): No website namespace specified");

		if(websiteProperties.name.empty())
			throw Database::Exception("Main::Database::getLostUrlsByWebsiteUpdate(): No website name specified");

		// get old namespace and domain
		std::string oldNamespace = this->getWebsiteNamespace(websiteId);
		std::string oldDomain = this->getWebsiteDomain(websiteId);

		// check connection
		this->checkConnection();

		try {
			if(oldDomain.empty() && !websiteProperties.domain.empty()) {
				// get URL lists
				std::queue<IdString> urlLists = this->getUrlLists(websiteId);

				// create SQL statement
				SqlStatementPtr sqlStatement(this->connection->createStatement());

				while(!urlLists.empty()) {
					// count URLs of different domain
					std::string comparison = "url NOT LIKE '" + websiteProperties.domain + "/%'";

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

	// update website (and all associated tables) in the database
	void Database::updateWebsite(unsigned long websiteId, const WebsiteProperties& websiteProperties) {
		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::updateWebsite(): No website ID specified");

		if(websiteProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::updateWebsite(): No website namespace specified");

		if(websiteProperties.name.empty())
			throw Database::Exception("Main::Database::updateWebsite(): No website name specified");

		// get old namespace and domain
		std::string oldNamespace = this->getWebsiteNamespace(websiteId);
		std::string oldDomain = this->getWebsiteDomain(websiteId);

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
				std::queue<IdString> urlLists = this->getUrlLists(websiteId);

				// create SQL statement for modifying and deleting URLs
				SqlStatementPtr urlStatement(this->connection->createStatement());

				if(oldDomain.empty() && !websiteProperties.domain.empty()) {
					// type changed from cross-domain to specific domain:
					// 	change URLs from absolute (URLs without protocol) to relative (sub-URLs),
					// 	discarding URLs with different domain
					while(!urlLists.empty()) {
						// update URLs of same domain
						std::string comparison = "url LIKE '" + websiteProperties.domain + "/%'";

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
					std::queue<IdString> urlLists = this->getUrlLists(websiteId);

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
				std::queue<IdString> tables;

				// create SQL statement for renaming
				SqlStatementPtr renameStatement(this->connection->createStatement());

				// rename sub-tables
				std::queue<IdString> urlLists = this->getUrlLists(websiteId);

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
	}

	// delete website (and all associated data) from the database by its ID
	void Database::deleteWebsite(unsigned long websiteId) {
		// check argument
		if(!websiteId)
			throw Database::Exception("Main::Database::deleteWebsite(): No website ID specified");

		// get website namespace
		std::string websiteNamespace = this->getWebsiteNamespace(websiteId);

		try {
			// delete URL lists
			std::queue<IdString> urlLists = this->getUrlLists(websiteId);

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
							"DELETE FROM crawlserv_websites"
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

	// duplicate website in the database by its ID (no processed data will be duplicated)
	unsigned long Database::duplicateWebsite(unsigned long websiteId) {
		unsigned long result = 0;

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
							" FROM crawlserv_websites"
							" WHERE id = ?"
							" LIMIT 1"
					)
			);

			// execute SQL statement for geting website info
			sqlStatement->setUInt64(1, websiteId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				std::string websiteNamespace = sqlResultSet->getString("namespace");
				std::string websiteName = sqlResultSet->getString("name");
				std::string websiteDomain;
				std::string websiteDir;

				if(!(sqlResultSet->isNull("domain")))
					websiteDomain = sqlResultSet->getString("domain");

				if(!(sqlResultSet->isNull("dir")))
					websiteDir = sqlResultSet->getString("dir");

				// create new namespace and new name
				std::string newNamespace = Database::duplicateWebsiteNamespace(websiteNamespace);

				std::string newName = websiteName + " (copy)";

				// add website
				result = this->addWebsite(WebsiteProperties(websiteDomain, newNamespace, newName, websiteDir));

				// prepare SQL statement for geting URL list info
				sqlStatement.reset(
						this->connection->prepareStatement(
								"SELECT name, namespace"
								" FROM crawlserv_urllists"
								" WHERE website = ?"
						)
				);

				// execute SQL statement for geting URL list info
				sqlStatement->setUInt64(1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					// add URL lists with same name (except for "default", which has already been created)
					std::string urlListName = sqlResultSet->getString("namespace");

					if(urlListName != "default")
						this->addUrlList(result, UrlListProperties(sqlResultSet->getString("namespace"), urlListName));
				}

				// prepare SQL statement for getting queries
				sqlStatement.reset(
						this->connection->prepareStatement(
							"SELECT name, query, type, resultbool, resultsingle, resultmulti, textonly"
							" FROM crawlserv_queries"
							" WHERE website = ?"
						)
				);

				// execute SQL statement for getting queries
				sqlStatement->setUInt64(1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					// add query
					this->addQuery(
							result,
							QueryProperties(sqlResultSet->getString("name"),
							sqlResultSet->getString("query"),
							sqlResultSet->getString("type"),
							sqlResultSet->getBoolean("resultbool"),
							sqlResultSet->getBoolean("resultsingle"),
							sqlResultSet->getBoolean("resultmulti"),
							sqlResultSet->getBoolean("textonly"))
					);

				}

				// prepare SQL statement for getting configurations
				sqlStatement.reset(
						this->connection->prepareStatement(
								"SELECT module, name, config"
								" FROM crawlserv_configs"
								" WHERE website = ?"
						)
				);

				// execute SQL statement for getting configurations
				sqlStatement->setUInt64(1, websiteId);

				sqlResultSet.reset(Database::sqlExecuteQuery(sqlStatement));

				// get results
				while(sqlResultSet && sqlResultSet->next()) {
					// add configuration
					this->addConfiguration(
							result,
							ConfigProperties(
									sqlResultSet->getString("module"),
									sqlResultSet->getString("name"),
									sqlResultSet->getString("config")
							)
					);
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::duplicateWebsite", e); }

		return result;
	}

	/*
	 * URL LIST FUNCTIONS
	 */

	// add a URL list to the database and return its new ID
	unsigned long Database::addUrlList(unsigned long websiteId, const UrlListProperties& listProperties) {
		unsigned long result = 0;

		// check arguments
		if(!websiteId)
			throw Database::Exception("Main::Database::addUrlList(): No website ID specified");

		if(listProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::addUrlList(): No URL list namespace specified");

		if(listProperties.name.empty())
			throw Database::Exception("Main::Database::addUrlList(): No URL list name specified");

		// get website namespace and data directory
		std::string websiteNamespace = this->getWebsiteNamespace(websiteId);
		std::string websiteDataDirectory = this->getWebsiteDataDirectory(websiteId);

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
			result = this->getLastInsertedId();
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

		return result;
	}

	// get URL lists for ID-specified website from the database
	std::queue<Database::IdString> Database::getUrlLists(unsigned long websiteId) {
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
							" FROM crawlserv_urllists"
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

	// get namespace of URL list by its ID
	std::string Database::getUrlListNamespace(unsigned long listId) {
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
							" FROM crawlserv_urllists"
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

	// get ID and namespace of URL list from the database using a target table type and ID
	Database::IdString Database::getUrlListNamespaceFromTargetTable(const std::string& type, unsigned long tableId) {
		unsigned long urlListId = 0;

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

	// check whether a URL list namespace for an ID-specified website exists in the database
	bool Database::isUrlListNamespace(unsigned long websiteId, const std::string& nameSpace) {
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
								" FROM crawlserv_urllists"
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

	// update URL list (and all associated tables) in the database
	void Database::updateUrlList(unsigned long listId, const UrlListProperties& listProperties) {
		// check arguments
		if(!listId)
			throw Database::Exception("Main::Database::updateUrlList(): No website ID specified");

		if(listProperties.nameSpace.empty())
			throw Database::Exception("Main::Database::updateUrlList(): No URL list namespace specified");

		if(listProperties.name.empty())
			throw Database::Exception("Main::Database::updateUrlList(): No URL list name specified");

		// get website namespace and URL list name
		IdString websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
		std::string oldListNamespace = this->getUrlListNamespace(listId);

		// check website namespace if necessary
		if(listProperties.nameSpace != oldListNamespace)
			if(this->isUrlListNamespace(websiteNamespace.first, listProperties.nameSpace))
				throw Database::Exception("Main::Database::updateUrlList(): URL list namespace already exists");

		// check connection
		this->checkConnection();

		try {
			if(listProperties.nameSpace != oldListNamespace) {
				std::queue<IdString> tables;

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

				tables = this->getTargetTables("parsed", listId);

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

	// delete URL list (and all associated data) from the database by its ID
	void Database::deleteUrlList(unsigned long listId) {
		std::queue<IdString> tables;

		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::deleteUrlList(): No URL list ID specified");

		// get website namespace and URL list name
		IdString websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
		std::string listNamespace = this->getUrlListNamespace(listId);

		// delete parsing tables
		tables = this->getTargetTables("parsed", listId);

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
						"DELETE FROM crawlserv_urllists"
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

	// reset parsing status of ID-specified URL list
	void Database::resetParsingStatus(unsigned long listId) {
		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::resetParsingStatus(): No URL list ID specified");

		// get website namespace and URL list name
		IdString websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
		std::string listNamespace = this->getUrlListNamespace(listId);

		try {
			// update parsing table
			this->execute(
					"UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_parsing`"
					" SET success = FALSE, locktime = NULL"
			);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetParsingStatus", e); }
	}

	// reset extracting status of ID-specified URL list
	void Database::resetExtractingStatus(unsigned long listId) {
		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::resetExtractingStatus(): No URL list ID specified");

		// get website namespace and URL list name
		IdString websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
		std::string listNamespace = this->getUrlListNamespace(listId);

		try {
			// update extracting table
			this->execute(
					"UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_extracting`"
					" SET success = FALSE, locktime = NULL"
			);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetExtractingStatus", e); }
	}

	// reset analyzing status of ID-specified URL list
	void Database::resetAnalyzingStatus(unsigned long listId) {
		// check argument
		if(!listId)
			throw Database::Exception("Main::Database::resetAnalyzingStatus(): No URL list ID specified");

		// get website namespace and URL list name
		IdString websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
		std::string listNamespace = this->getUrlListNamespace(listId);

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

	// add a query to the database and return its new ID (add global query when websiteId is zero)
	unsigned long Database::addQuery(unsigned long websiteId, const QueryProperties& queryProperties) {
		unsigned long result = 0;

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
								" textonly"
							" )"
							" VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
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
			sqlStatement->setBoolean(8, queryProperties.textOnly);

			Database::sqlExecute(sqlStatement);

			// get ID
			result = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addQuery", e); }

		return result;
	}

	// get the properties of a query from the database by its ID
	void Database::getQueryProperties(unsigned long queryId, QueryProperties& queryPropertiesTo) {
		// check argument
		if(!queryId)
			throw Database::Exception("Main::Database::getQueryProperties(): No query ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"SELECT name, query, type, resultbool, resultsingle, resultmulti, textonly"
							" FROM crawlserv_queries WHERE id = ? LIMIT 1"
					)
			);

			// execute SQL statement
			sqlStatement->setUInt64(1, queryId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				queryPropertiesTo.name = sqlResultSet->getString("name");
				queryPropertiesTo.text = sqlResultSet->getString("query");
				queryPropertiesTo.type = sqlResultSet->getString("type");
				queryPropertiesTo.resultBool = sqlResultSet->getBoolean("resultbool");
				queryPropertiesTo.resultSingle = sqlResultSet->getBoolean("resultsingle");
				queryPropertiesTo.resultMulti = sqlResultSet->getBoolean("resultmulti");
				queryPropertiesTo.textOnly = sqlResultSet->getBoolean("textonly");
			}
			else {
				queryPropertiesTo.name = "";
				queryPropertiesTo.text = "";
				queryPropertiesTo.type = "";
				queryPropertiesTo.resultBool = false;
				queryPropertiesTo.resultSingle = false;
				queryPropertiesTo.resultMulti = false;
				queryPropertiesTo.textOnly = false;
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getQueryProperties", e); }
	}

	// edit query in the database
	void Database::updateQuery(unsigned long queryId, const QueryProperties& queryProperties) {
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
			sqlStatement->setBoolean(7, queryProperties.textOnly);
			sqlStatement->setUInt64(8, queryId);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateQuery", e); }
	}

	// delete query from the database by its ID
	void Database::deleteQuery(unsigned long queryId) {
		// check argument
		if(!queryId)
			throw Database::Exception("Main::Database::deleteQuery(): No query ID specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"DELETE FROM crawlserv_queries"
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

	// duplicate query in the database by its ID
	unsigned long Database::duplicateQuery(unsigned long queryId) {
		unsigned long result = 0;

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
						" textonly"
						" FROM crawlserv_queries"
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
				result = this->addQuery(
						sqlResultSet->getUInt64("website"),
						QueryProperties(
								sqlResultSet->getString("name") + " (copy)",
								sqlResultSet->getString("query"),
								sqlResultSet->getString("type"),
								sqlResultSet->getBoolean("resultbool"),
								sqlResultSet->getBoolean("resultsingle"),
								sqlResultSet->getBoolean("resultmulti"),
								sqlResultSet->getBoolean("textonly")
						)
				);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::duplicateQuery", e); }

		return result;
	}

	/*
	 * CONFIGURATION FUNCTIONS
	 */

	// add a configuration to the database and return its new ID (add global configuration when websiteId is zero)
	unsigned long Database::addConfiguration(unsigned long websiteId, const ConfigProperties& configProperties) {
		unsigned long result = 0;

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
			result = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addConfiguration", e); }

		return result;
	}

	// get a configuration from the database by its ID
	const std::string Database::getConfiguration(unsigned long configId) {
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
							" FROM crawlserv_configs"
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

	// update configuration in the database (NOTE: module will not be updated!)
	void Database::updateConfiguration(unsigned long configId, const ConfigProperties& configProperties) {
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
			// create SQL statement for updating
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

	// delete configuration from the database by its ID
	void Database::deleteConfiguration(unsigned long configId) {
		// check argument
		if(!configId)
			throw Database::Exception("Main::Database::deleteConfiguration(): No configuration ID specified");

		// check connection
		this->checkConnection();

		try {
			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(
					this->connection->prepareStatement(
							"DELETE FROM crawlserv_configs"
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

	// duplicate configuration in the database by its ID
	unsigned long Database::duplicateConfiguration(unsigned long configId) {
		unsigned long result = 0;

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
							" FROM crawlserv_configs"
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
				result = this->addConfiguration(sqlResultSet->getUInt64("website"),
						ConfigProperties(
								sqlResultSet->getString("module"), sqlResultSet->getString("name") + " (copy)",
								sqlResultSet->getString("config")
						));
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::duplicateConfiguration", e); }

		return result;
	}

	/*
	 * TARGET TABLE FUNCTIONS
	 */

	// add a target table of the specified type to the database if such a table does not exist already, return table ID
	unsigned long Database::addTargetTable(const TargetTableProperties& properties) {
		unsigned long result = 0;

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
				for(auto i = properties.columns.begin(); i != properties.columns.end(); ++i) {
					if(!(i->name.empty())) {
						std::string columnName = i->name;

						if(this->isColumnExists(properties.fullName, columnName)) {
							// column does exist: check types (does not check specifiers like 'UNSIGNED'!)
							std::string columnType(i->type, 0, i->type.find(' '));
							std::string targetType(this->getColumnType(properties.fullName, columnName));

							std::transform(columnType.begin(), columnType.end(), columnType.begin(), ::tolower);
							std::transform(targetType.begin(), targetType.end(), targetType.begin(), ::tolower);

							if(columnType != targetType)
								throw Database::Exception("Main::Database::addTargetTable(): Cannot overwrite column of type \'"
										+ columnType + "\' with data of type \'" + targetType + "\'");
						}
						else {
							// column does not exist: add column
							this->addColumn(properties.fullName, TableColumn(columnName, *i));
						}
					}
				}

				// compress table if necessary
				if(properties.compressed)
					this->compressTable(properties.fullName);
			}
			else {
				// table does not exist: get data directory and create table
				std::string dataDirectory = this->getWebsiteDataDirectory(properties.website);

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
				result = sqlResultSet->getUInt64("id");
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
				result = this->getLastInsertedId();
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::addTargetTable", e); }

		return result;
	}

	// get target tables of the specified type for an ID-specified URL list from the database
	std::queue<Database::IdString> Database::getTargetTables(const std::string& type, unsigned long listId) {
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

	// get the ID of a target table of the specified type from the database by its website ID, URL list ID and table name
	unsigned long Database::getTargetTableId(
			const std::string& type,
			unsigned long websiteId,
			unsigned long listId,
			const std::string& tableName
	) {
		unsigned long result = 0;

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

	// get the name of a target table of the specified type from the database by its ID
	std::string Database::getTargetTableName(const std::string& type, unsigned long tableId) {
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

	// delete target table of the specified type from the database by its ID
	void Database::deleteTargetTable(const std::string& type, unsigned long tableId) {
		// check arguments
		if(type.empty())
			throw Database::Exception("Main::Database::deleteTargetTable(): No table type specified");

		if(!tableId)
			throw Database::Exception("Main::Database::deleteTargetTable(): No table ID specified");

		// get namespace, URL list name and table name
		IdString websiteNamespace = this->getWebsiteNamespaceFromTargetTable(type, tableId);
		IdString listNamespace = this->getUrlListNamespaceFromTargetTable(type, tableId);
		std::string tableName = this->getTargetTableName(type, tableId);

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

	// check whether the connection to the database is still valid and try to re-connect if necesssary
	//  WARNING: afterwards, old references to prepared SQL statements may be invalid!
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
			for(auto i = this->preparedStatements.begin(); i != this->preparedStatements.end(); ++i)
				i->refresh(this->connection.get());

			// log re-connect on idle if necessary
			if(milliseconds)
				this->log(
					"re-connected to database after idling for " +
					Helper::DateTime::secondsToString(std::round(static_cast<float>(milliseconds / 1000))) +
					"."
				);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::checkConnection", e); }
	}

	// check whether a website ID is valid
	bool Database::isWebsite(unsigned long websiteId) {
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
								" FROM crawlserv_websites"
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

	// check whether a URL list ID is valid
	bool Database::isUrlList(unsigned long urlListId) {
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
								" FROM crawlserv_urllists"
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

	// check whether a URL list ID is valid for the ID-specified website
	bool Database::isUrlList(unsigned long websiteId, unsigned long urlListId) {
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
								" FROM crawlserv_urllists"
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

	// check whether a query ID is valid
	bool Database::isQuery(unsigned long queryId) {
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
								" FROM crawlserv_queries"
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

	// check whether a query ID is valid for the ID-specified website (look for global queries if websiteID is zero)
	bool Database::isQuery(unsigned long websiteId, unsigned long queryId) {
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
								" FROM crawlserv_queries"
								" WHERE"
								" ("
									" website = ?"
									" OR website = 0"
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

	// check whether a configuration ID is valid
	bool Database::isConfiguration(unsigned long configId) {
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
								" FROM crawlserv_configs"
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

	// check whether a configuration ID is valid for the ID-specified website (look for global configurations if websiteId is zero)
	bool Database::isConfiguration(unsigned long websiteId, unsigned long configId) {
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
								" FROM crawlserv_configs"
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
		SqlStatementPtr sqlStatement(this->connection->createStatement());

		sqlStatement->execute("SET TRANSACTION ISOLATION LEVEL READ UNCOMMITTED");
	}

	// re-enable locking by ending (non-existing) transaction
	void Database::endNoLock() {
		SqlStatementPtr sqlStatement(this->connection->createStatement());

		sqlStatement->execute("COMMIT");
	}

	/*
	 * GENERAL TABLE FUNCTIONS
	 */

	// check whether a name-specified table in the database is empty
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

	// check whether a specific table exists in the database
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

	// check whether a specific column of a specific table exists in the database
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

	// get the type of a specific column of a specific table in the database
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

	/*
	 * CUSTOM DATA FUNCTIONS FOR ALGORITHMS
	 */

	// get one custom value from one field of a row in the database
	void Database::getCustomData(Data::GetValue& data) {
		// check argument
		if(data.column.empty())
			throw Database::Exception("Main::Database::getCustomData(): No column name specified");

		if(data.type == Main::Data::Type::_unknown)
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

					case Data::Type::_int:
						data.value = Data::Value(static_cast<int>(sqlResultSet->getInt(data.column)));

						break;

					case Data::Type::_long:
						data.value = Data::Value(static_cast<long>(sqlResultSet->getInt64(data.column)));

						break;

					case Data::Type::_string:
						data.value = Data::Value(sqlResultSet->getString(data.column));

						break;

					case Data::Type::_uint:
						data.value = Data::Value(static_cast<unsigned int>(sqlResultSet->getUInt(data.column)));

						break;

					case Data::Type::_ulong:
						data.value = Data::Value(static_cast<unsigned long>(sqlResultSet->getUInt64(data.column)));

						break;

					default:
						throw Database::Exception("Main::Database::getCustomData(): Invalid data type when getting custom data.");
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from multiple fields of a row in the database
	void Database::getCustomData(Data::GetFields& data) {
		// check argument
		if(data.columns.empty())
			throw Database::Exception("Main::Database::getCustomData(): No column names specified");

		if(data.type == Main::Data::Type::_unknown)
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

			for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
				sqlQuery += "`" + *i + "`, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `" + data.table + "` WHERE (" + data.condition + ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement, sqlQuery));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				switch(data.type) {
				case Data::Type::_bool:
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
						if(sqlResultSet->isNull(*i))
							data.values.emplace_back();
						else
							data.values.emplace_back(sqlResultSet->getBoolean(*i));

					break;

				case Data::Type::_double:
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
						if(sqlResultSet->isNull(*i))
							data.values.emplace_back();
						else
							data.values.emplace_back(static_cast<double>(sqlResultSet->getDouble(*i)));

					break;

				case Data::Type::_int:
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
						if(sqlResultSet->isNull(*i))
							data.values.emplace_back();
						else
							data.values.emplace_back(static_cast<int>(sqlResultSet->getInt(*i)));

					break;

				case Data::Type::_long:
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
						if(sqlResultSet->isNull(*i))
							data.values.emplace_back();
						else
							data.values.emplace_back(static_cast<long>(sqlResultSet->getInt64(*i)));

					break;

				case Data::Type::_string:
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
						if(sqlResultSet->isNull(*i))
							data.values.emplace_back();
						else
							data.values.emplace_back(sqlResultSet->getString(*i));

					break;

				case Data::Type::_uint:
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
						if(sqlResultSet->isNull(*i))
							data.values.emplace_back();
						else
							data.values.emplace_back(static_cast<unsigned int>(sqlResultSet->getUInt(*i)));

					break;

				case Data::Type::_ulong:
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
						if(sqlResultSet->isNull(*i))
							data.values.emplace_back();
						else
							data.values.emplace_back(static_cast<unsigned long>(sqlResultSet->getUInt64(*i)));

					break;

				default:
					throw Database::Exception("Main::Database::getCustomData(): Invalid data type when getting custom data.");
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from multiple fields of a row with different types in the database
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

			for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i)
				sqlQuery += "`" + i->first + "`, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " FROM `" + data.table + "` WHERE (" + data.condition + ")";

			// execute SQL statement
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement, sqlQuery));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
					if(sqlResultSet->isNull(i->first))
						data.values.emplace_back();
					else {
						switch(i->second) {
						case Data::Type::_bool:
							data.values.emplace_back(sqlResultSet->getBoolean(i->first));
							break;

						case Data::Type::_double:
							data.values.emplace_back(static_cast<double>(sqlResultSet->getDouble(i->first)));
							break;

						case Data::Type::_int:
							data.values.emplace_back(static_cast<int>(sqlResultSet->getInt(i->first)));
							break;

						case Data::Type::_long:
							data.values.emplace_back(static_cast<long>(sqlResultSet->getInt64(i->first)));
							break;

						case Data::Type::_string:
							data.values.emplace_back(sqlResultSet->getString(i->first));
							break;

						case Data::Type::_uint:
							data.values.emplace_back(static_cast<unsigned int>(sqlResultSet->getUInt(i->first)));
							break;

						case Data::Type::_ulong:
							data.values.emplace_back(static_cast<unsigned long>(sqlResultSet->getUInt64(i->first)));
							break;

						default:
							throw Database::Exception("Main::Database::getCustomData(): Invalid data type when getting custom data.");
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from one column in the database
	void Database::getCustomData(Data::GetColumn& data) {
		// check argument
		if(data.column.empty())
			throw Database::Exception("Main::Database::getCustomData(): No columns specified");

		if(data.type == Main::Data::Type::_unknown)
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

						case Data::Type::_int:
							data.values.emplace_back(static_cast<int>(sqlResultSet->getInt(data.column)));

							break;

						case Data::Type::_long:
							data.values.emplace_back(static_cast<long>(sqlResultSet->getInt64(data.column)));

							break;

						case Data::Type::_string:
							data.values.emplace_back(sqlResultSet->getString(data.column));

							break;

						case Data::Type::_uint:
							data.values.emplace_back(static_cast<unsigned int>(sqlResultSet->getUInt(data.column)));

							break;

						case Data::Type::_ulong:
							data.values.emplace_back(static_cast<unsigned long>(sqlResultSet->getUInt64(data.column)));

							break;

						default:
							throw Database::Exception("Main::Database::getCustomData(): Invalid data type when getting custom data.");
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from multiple columns of the same type in the database
	void Database::getCustomData(Data::GetColumns& data) {
		// check argument
		if(data.columns.empty())
			throw Database::Exception("Main::Database::getCustomData(): No column name specified");

		if(data.type == Main::Data::Type::_unknown)
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

			for(auto i = data.columns.begin(); i != data.columns.end(); ++i) {
				sqlQuery += "`" + *i + "`, ";

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
				for(auto i = data.values.begin(); i != data.values.end(); ++i)
					i->reserve(sqlResultSet->rowsCount());

				// get results
				while(sqlResultSet->next()) {
					for(auto i = data.columns.begin(); i != data.columns.end(); ++i) {
						auto column = data.values.begin() + (i - data.columns.begin());

						if(sqlResultSet->isNull(*i))
							column->emplace_back();
						else {
							switch(data.type) {
							case Data::Type::_bool:
								column->emplace_back(sqlResultSet->getBoolean(*i));

								break;

							case Data::Type::_double:
								column->emplace_back(static_cast<double>(sqlResultSet->getDouble(*i)));

								break;

							case Data::Type::_int:
								column->emplace_back(static_cast<int>(sqlResultSet->getInt(*i)));

								break;

							case Data::Type::_long:
								column->emplace_back(static_cast<long>(sqlResultSet->getInt64(*i)));

								break;

							case Data::Type::_string:
								column->emplace_back(sqlResultSet->getString(*i));

								break;

							case Data::Type::_uint:
								column->emplace_back(static_cast<unsigned int>(sqlResultSet->getUInt(*i)));

								break;

							case Data::Type::_ulong:
								column->emplace_back(static_cast<unsigned long>(sqlResultSet->getUInt64(*i)));

								break;

							default:
								throw Database::Exception("Main::Database::getCustomData(): Invalid data type when getting custom data.");
							}
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// get custom values from multiple columns of different types in the database
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

			for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
				sqlQuery += "`" + i->first + "`, ";

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
				for(auto i = data.values.begin(); i != data.values.end(); ++i)
					i->reserve(sqlResultSet->rowsCount());

				// get results
				while(sqlResultSet->next()) {
					for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
						auto column = data.values.begin() + (i - data.columns_types.begin());

						if(sqlResultSet->isNull(i->first))
							column->emplace_back();
						else {
							switch(i->second) {
							case Data::Type::_bool:
								column->emplace_back(sqlResultSet->getBoolean(i->first));

								break;

							case Data::Type::_double:
								column->emplace_back(static_cast<double>(sqlResultSet->getDouble(i->first)));

								break;

							case Data::Type::_int:
								column->emplace_back(static_cast<int>(sqlResultSet->getInt(i->first)));

								break;

							case Data::Type::_long:
								column->emplace_back(static_cast<long>(sqlResultSet->getInt64(i->first)));

								break;

							case Data::Type::_string:
								column->emplace_back(sqlResultSet->getString(i->first));

								break;

							case Data::Type::_uint:
								column->emplace_back(static_cast<unsigned int>(sqlResultSet->getUInt(i->first)));

								break;

							case Data::Type::_ulong:
								column->emplace_back(static_cast<unsigned long>(sqlResultSet->getUInt64(i->first)));

								break;

							default:
								throw Database::Exception("Main::Database::getCustomData(): Invalid data type when getting custom data.");
							}
						}
					}
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
	}

	// insert one custom value into a row in the database
	void Database::insertCustomData(const Data::InsertValue& data) {
		// check argument
		if(data.column.empty())
			throw Database::Exception("Main::Database::insertCustomData(): No column name specified");

		if(data.type == Main::Data::Type::_unknown)
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

				case Data::Type::_int:
					sqlStatement->setInt(1, data.value._i);

					break;

				case Data::Type::_long:
					sqlStatement->setInt64(1, data.value._l);

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

							errStrStr <<	"Main::Database::insertCustomData():"
											" Size (" << data.value._s.size() << " bytes)"
											" of custom value for `" << data.table << "`.`" << data.column << "`"
											" exceeds the ";

							if(data.value._s.size() > 1073741824)
								errStrStr << "mySQL data limit of 1 GiB";
							else
								errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
											 " - adjust the \'max_allowed_packet\' setting on the server accordingly"
											 " (to max. 1 GiB).";

							throw Database::Exception(errStrStr.str());
						}
					}
					else
						sqlStatement->setString(1, data.value._s);

					break;

				case Data::Type::_uint:
					sqlStatement->setUInt(1, data.value._ui);

					break;

				case Data::Type::_ulong:
					sqlStatement->setUInt64(1, data.value._ul);

					break;

				default:
					throw Database::Exception("Main::Database::insertCustomData(): Invalid data type when inserting custom data.");
				}
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::insertCustomData", e); }
	}

	// insert custom values into multiple fields of the same type into a row in the database
	void Database::insertCustomData(const Data::InsertFields& data) {
		// check argument
		if(data.columns_values.empty())
			throw Database::Exception("Main::Database::insertCustomData(): No columns specified");

		if(data.type == Main::Data::Type::_unknown)
			throw Database::Exception("Main::Database::insertCustomData(): No column type specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery("INSERT INTO `" + data.table + "` (");

			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i)
				sqlQuery += "`" + i->first + "`, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += ") VALUES(";

			for(unsigned long n = 0; n < data.columns_values.size() - 1; ++n)
				sqlQuery += "?, ";

			sqlQuery += "?)";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// set values
			unsigned int counter = 1;

			switch(data.type) {
			case Data::Type::_bool:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setBoolean(counter, i->second._b);

					++counter;
				}

				break;

			case Data::Type::_double:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setDouble(counter, i->second._d);

					++counter;
				}

				break;

			case Data::Type::_int:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setInt(counter, i->second._i);

					++counter;
				}

				break;

			case Data::Type::_long:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setInt64(counter, i->second._l);

					++counter;
				}

				break;

			case Data::Type::_string:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull) sqlStatement->setNull(counter, 0);
					else if(i->second._s.size() > this->getMaxAllowedPacketSize()) {
						switch(i->second._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(1, i->second._s.substr(0, this->getMaxAllowedPacketSize()));
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
							errStrStr	<< "Main::Database::insertCustomData(): Size (" << i->second._s.size()
										<< " bytes) of custom value for `" << data.table << "`.`" << i->first
										<< "` exceeds the ";

							if(i->second._s.size() > 1073741824)
								errStrStr << "mySQL data limit of 1 GiB";
							else
								errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
										" - adjust the \'max_allowed_packet\' setting on the server accordingly (to max. 1 GiB).";

							throw Database::Exception(errStrStr.str());
						}
					}
					else
						sqlStatement->setString(counter, i->second._s);

					++counter;
				}

				break;

			case Data::Type::_uint:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setUInt(counter, i->second._ui);

					++counter;
				}

				break;

			case Data::Type::_ulong:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setUInt64(counter, i->second._ul);

					++counter;
				}

				break;

			default:
				throw Database::Exception("Main::Database::insertCustomData(): Invalid data type when inserting custom data.");
			}

			// execute SQL statement
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::insertCustomData", e); }
	}

	// insert custom values into multiple fields of different types into a row in the database
	void Database::insertCustomData(const Data::InsertFieldsMixed& data) {
		// check argument
		if(data.columns_types_values.empty())
			throw Database::Exception("Main::Database::insertCustomData(): No columns specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery("INSERT INTO `" + data.table + "` (");

			for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i)
				sqlQuery += "`" + std::get<0>(*i) + "`, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += ") VALUES(";

			for(unsigned long n = 0; n < data.columns_types_values.size() - 1; ++n)
				sqlQuery += "?, ";

			sqlQuery += "?)";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// set values
			unsigned int counter = 1;

			for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i) {
				if(std::get<2>(*i)._isnull)
					sqlStatement->setNull(counter, 0);
				else {
					switch(std::get<1>(*i)) {
					case Data::Type::_bool:
						sqlStatement->setBoolean(counter, std::get<2>(*i)._b);

						break;

					case Data::Type::_double:
						sqlStatement->setDouble(counter, std::get<2>(*i)._d);

						break;

					case Data::Type::_int:
						sqlStatement->setInt(counter, std::get<2>(*i)._i);

						break;
					case Data::Type::_long:
						sqlStatement->setInt64(counter, std::get<2>(*i)._l);

						break;

					case Data::Type::_string:
						if(std::get<2>(*i)._s.size() > this->getMaxAllowedPacketSize()) {
							switch(std::get<2>(*i)._overflow) {
							case Data::Value::_if_too_large::_trim:
								sqlStatement->setString(1, std::get<2>(*i)._s.substr(0, this->getMaxAllowedPacketSize()));

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

								errStrStr <<	"Main::Database::insertCustomData():"
												" Size (" << std::get<2>(*i)._s.size()<<  " bytes)"
												" of custom value for `" << data.table << "`.`" << std::get<0>(*i) << "`"
												" exceeds the ";

								if(std::get<2>(*i)._s.size() > 1073741824)
									errStrStr << "mySQL data limit of 1 GiB";
								else
									errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
												 " - adjust the \'max_allowed_packet\' setting on the server accordingly"
												 " (to max. 1 GiB).";

								throw Database::Exception(errStrStr.str());
							}
						}
						else
							sqlStatement->setString(counter, std::get<2>(*i)._s);

						break;

					case Data::Type::_uint:
						sqlStatement->setUInt(counter, std::get<2>(*i)._ui);

						break;

					case Data::Type::_ulong:
						sqlStatement->setUInt64(counter, std::get<2>(*i)._ul);

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

	// update one custom value in one field of a row in the database
	void Database::updateCustomData(const Data::UpdateValue& data) {
		// check argument
		if(data.column.empty())
			throw Database::Exception("Main::Database::updateCustomData(): No column name specified");

		if(data.type == Main::Data::Type::_unknown)
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

				case Data::Type::_int:
					sqlStatement->setInt(1, data.value._i);

					break;

				case Data::Type::_long:
					sqlStatement->setInt64(1, data.value._l);

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

							errStrStr <<	"Main::Database::updateCustomData():"
											" Size (" << data.value._s.size() << " bytes) of custom value"
											" for `" << data.table << "`.`" << data.column << "`"
											" exceeds the ";

							if(data.value._s.size() > 1073741824)
								errStrStr << "mySQL data limit of 1 GiB";
							else
								errStrStr << "current mySQL server limit"
										" of " << this->getMaxAllowedPacketSize() << " bytes"
										" - adjust the \'max_allowed_packet\' setting on the server accordingly"
										" (to max. 1 GiB).";

							throw Database::Exception(errStrStr.str());
						}
					}
					else sqlStatement->setString(1, data.value._s);

					break;

				case Data::Type::_uint:
					sqlStatement->setUInt(1, data.value._ui);

					break;

				case Data::Type::_ulong:
					sqlStatement->setUInt64(1, data.value._ul);

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

	// update custom values in multiple fields of a row with the same type in the database
	void Database::updateCustomData(const Data::UpdateFields& data) {
		// check argument
		if(data.columns_values.empty())
			throw Database::Exception("Main::Database::updateCustomData(): No columns specified");

		if(data.type == Main::Data::Type::_unknown)
			throw Database::Exception("Main::Database::updateCustomData(): No column type specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery("UPDATE `" + data.table + "` SET ");

			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i)
				sqlQuery += "`" + i->first + "` = ?, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " WHERE (" + data.condition + ")";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// set values
			unsigned int counter = 1;

			switch(data.type) {
			case Data::Type::_bool:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setBoolean(counter, i->second._b);

					++counter;
				}

				break;

			case Data::Type::_double:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setDouble(counter, i->second._d);

					++counter;
				}

				break;

			case Data::Type::_int:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setInt(counter, i->second._i);

					++counter;
				}

				break;

			case Data::Type::_long:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setInt64(counter, i->second._l);

					++counter;
				}

				break;

			case Data::Type::_string:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else if(i->second._s.size() > this->getMaxAllowedPacketSize()) {
						switch(i->second._overflow) {
						case Data::Value::_if_too_large::_trim:
							sqlStatement->setString(1, i->second._s.substr(0, this->getMaxAllowedPacketSize()));

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

							errStrStr <<	"Main::Database::updateCustomData():"
											" Size (" << i->second._s.size() << " bytes)"
											" of custom value for `" << data.table << "`.`" << i->first << "`"
											" exceeds the ";

							if(i->second._s.size() > 1073741824)
								errStrStr << "mySQL data limit of 1 GiB";
							else
								errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
											 " - adjust the \'max_allowed_packet\' setting on the server accordingly"
											 " (to max. 1 GiB).";

							throw Database::Exception(errStrStr.str());
						}
					}
					else
						sqlStatement->setString(counter, i->second._s);

					++counter;
				}

				break;

			case Data::Type::_uint:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setUInt(counter, i->second._ui);

					++counter;
				}

				break;

			case Data::Type::_ulong:
				for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
					if(i->second._isnull)
						sqlStatement->setNull(counter, 0);
					else
						sqlStatement->setUInt64(counter, i->second._ul);

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

	// update custom values in multiple fields of a row with different types in the database
	void Database::updateCustomData(const Data::UpdateFieldsMixed& data) {
		// check argument
		if(data.columns_types_values.empty())
			throw Database::Exception("Main::Database::updateCustomData(): No columns specified");

		// check connection
		this->checkConnection();

		try {
			// create SQL query
			std::string sqlQuery("UPDATE `" + data.table + "` SET ");

			for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i)
				sqlQuery += "`" + std::get<0>(*i) + "` = ?, ";

			sqlQuery.pop_back();
			sqlQuery.pop_back();

			sqlQuery += " WHERE (" + data.condition + ")";

			// prepare SQL statement
			SqlPreparedStatementPtr sqlStatement(this->connection->prepareStatement(sqlQuery));

			// set values
			unsigned int counter = 1;

			for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i) {
				if(std::get<2>(*i)._isnull)
					sqlStatement->setNull(counter, 0);
				else {
					switch(std::get<1>(*i)) {
					case Data::Type::_bool:
						sqlStatement->setBoolean(counter, std::get<2>(*i)._b);

						break;

					case Data::Type::_double:
						sqlStatement->setDouble(counter, std::get<2>(*i)._d);

						break;

					case Data::Type::_int:
						sqlStatement->setInt(counter, std::get<2>(*i)._i);

						break;

					case Data::Type::_long:
						sqlStatement->setInt64(counter, std::get<2>(*i)._l);

						break;

					case Data::Type::_string:
						if(std::get<2>(*i)._s.size() > this->getMaxAllowedPacketSize()) {
							switch(std::get<2>(*i)._overflow) {
							case Data::Value::_if_too_large::_trim:
								sqlStatement->setString(1, std::get<2>(*i)._s.substr(0, this->getMaxAllowedPacketSize()));

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

								errStrStr <<	"Main::Database::updateCustomData():"
												" Size (" << std::get<2>(*i)._s.size() << " bytes)"
												" of custom value for `" << data.table << "`.`" << std::get<0>(*i) << "`"
												" exceeds the ";

								if(std::get<2>(*i)._s.size() > 1073741824)
									errStrStr << "mySQL data limit of 1 GiB";
								else
									errStrStr << "current mySQL server limit"
												 " of " << this->getMaxAllowedPacketSize() << " bytes"
												 " - adjust the \'max_allowed_packet\' setting on the server accordingly"
												 " (to max. 1 GiB).";

								throw Database::Exception(errStrStr.str());
							}
						}
						else
							sqlStatement->setString(counter, std::get<2>(*i)._s);

						break;

					case Data::Type::_uint:
						sqlStatement->setUInt(counter, std::get<2>(*i)._ui);

						break;

					case Data::Type::_ulong:
						sqlStatement->setUInt64(counter, std::get<2>(*i)._ul);

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
	 * PROTECTED GETTER (protected)
	 */

	// get the maximum allowed packet size
	unsigned long Database::getMaxAllowedPacketSize() const {
		return this->maxAllowedPacketSize;
	}

	/*
	 * HELPER FUNCTIONS FOR PREPARED SQL STATEMENTS (protected)
	 */

	// reserve memory for a specific number of SQL statements to additionally prepare
	void Database::reserveForPreparedStatements(unsigned short numberOfAdditionalPreparedStatements) {
		this->preparedStatements.reserve(this->preparedStatements.size() + numberOfAdditionalPreparedStatements);
	}

	// prepare additional SQL statement, return ID of newly prepared SQL statement
	unsigned short Database::addPreparedStatement(const std::string& sqlQuery) {
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
	sql::PreparedStatement& Database::getPreparedStatement(unsigned short id) {
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

	// get the last inserted ID from the database
	unsigned long Database::getLastInsertedId() {
		unsigned long result = 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.lastId))
			throw Database::Exception("Main::Database::getLastInsertedId: Missing prepared SQL statement for last inserted ID");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.lastId);

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

	// reset the auto increment of an (empty) table in the database
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
				auto databaseLock = std::find(Database::locks.begin(), Database::locks.end(), name);

				if(databaseLock == Database::locks.end()) {
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
		Database::locks.erase(std::remove(Database::locks.begin(), Database::locks.end(), name), Database::locks.end());
	}

	// add a table to the database (the primary key 'id' will be created automatically)
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

			for(auto i = properties.columns.begin(); i != properties.columns.end(); ++i) {
				// check values
				if(i->name.empty())
					throw Database::Exception("Main::Database::createTable(): A column is missing its name");

				if(i->type.empty())
					throw Database::Exception("Main::Database::createTable(): A column is missing its type");

				// add to SQL query
				sqlQuery += ", `" + i->name + "` " + i->type;

				// add indices and references
				if(i->indexed)
					propertiesString += ", INDEX(`" + i->name + "`)";

				if(!(i->referenceTable.empty())) {
					if(i->referenceColumn.empty())
						throw Database::Exception(
								"Main::Database::createTable(): A column reference is missing its source column name"
						);

					propertiesString +=	", FOREIGN KEY(`" + i->name + "`)"
										" REFERENCES `" + i->referenceTable + "`"
										" (`" + i->referenceColumn + "`)"
										" ON UPDATE RESTRICT ON DELETE CASCADE";
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

	// delete table from database if it exists
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

	// add a column to a table in the database
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
			std::string sqlQuery("ALTER TABLE `" + tableName + "` ADD COLUMN `" + column.name + "` " + column.type);

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

	// compress a table in the database
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
				Database::sqlExecute(sqlStatement, "ALTER TABLE `" + tableName + "` ROW_FORMAT=COMPRESSED");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::compressTable", e); }
	}

	// delete a table from the database if it exists
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
			Database::sqlExecute(sqlStatement, "DROP TABLE IF EXISTS `" + tableName + "`");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteTable", e); }
	}

	// check access to an external data directory
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
			Database::sqlExecute(sqlStatement, "DROP TABLE IF EXISTS `crawlserv_testaccess`");
			Database::sqlExecute(sqlStatement, "CREATE TABLE `crawlserv_testaccess(id SERIAL)` DATA DIRECTORY=`" + dir + "`");
			Database::sqlExecute(sqlStatement, "DROP TABLE `crawlserv_testaccess`");
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::checkDirectory", e); }
	}

	/*
	 * EXCEPTION HELPER FUNCTION (protected)
	 */

	// catch SQL exception and re-throw it as ConnectionException or generic Exception
	void Database::sqlException(const std::string& function, const sql::SQLException& e) {
		// create error string
		std::ostringstream errorStrStr;
		int error = e.getErrorCode();

		errorStrStr << function << "() SQL Error #" << error << " (State " << e.getSQLState() << "): " << e.what();

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
			throw Database::ConnectionException(errorStrStr.str());

		// check for storage engine error
		case 1030:
			throw Database::StorageEngineException(errorStrStr.str());

		// check for insufficient privileges error
		case 1045:
			throw Database::PrivilegesException(errorStrStr.str());

		// check for incorrect path value error
		case 1525:
			throw Database::IncorrectPathException(errorStrStr.str());

		default:
			// throw general database exception
			throw Database::Exception(errorStrStr.str());
		}
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// run file with SQL commands
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
				catch(const sql::SQLException &e) { this->sqlException("Main::Database::run", e); }
			}

			// close SQL file
			initSQLFile.close();
		}
		else
			throw Database::Exception("Main::Database::run(): Could not open \'" + sqlFile + "\' for reading");
	}

	// execute a SQL query
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

	// escape string for usage in SQL commands
	std::string Database::sqlEscapeString(const std::string& in) {
		auto mySqlConnection = dynamic_cast<sql::mysql::MySQL_Connection*>(this->connection.get());

		return mySqlConnection->escapeString(in);
	}

} /* crawlservpp::Main */
