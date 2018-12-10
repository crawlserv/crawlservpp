/*
 * Database.cpp
 *
 * A class to handle database access for crawlserv and its threads.
 * Thread-specific functionality is not implemented in this (parent) class.
 *
 * NOT THREAD-SAFE!
 * Use instances of the child class DatabaseThread for thread-specific functionality
 * and child classes of DatabaseModule for module-specific functionality instead.
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#include "Database.h"

sql::Driver * Database::driver = NULL;

// constructor: save settings and set default values
Database::Database(const DatabaseSettings& dbSettings) : settings(dbSettings) {
	// set default values
	this->connection = NULL;
	this->tablesLocked = false;
	this->sleepOnError = 0;

	this->psLog = 0;
	this->psLastId = 0;
	this->psSetThreadStatus = 0;

	// get driver instance if necessary
	if(!Database::driver) Database::driver = get_driver_instance();
}

// destructor
Database::~Database() {
	// clear prepared SQL statements
	for(auto i = this->preparedStatements.begin(); i != this->preparedStatements.end(); ++i) {
		DATABASE_DELETE(i->statement);
	}
	this->preparedStatements.clear();

	// clear connection
	if(this->connection) {
		if(this->connection->isValid()) this->connection->close();
		delete this->connection;
		this->connection = NULL;
	}

	this->tablesLocked = false;
}

// connect to database
bool Database::connect() {
	// connect to database
	try {
		// set options for connecting
		sql::ConnectOptionsMap connectOptions;
		connectOptions["hostName"] = this->settings.host;
		connectOptions["userName"] = this->settings.user;
		connectOptions["password"] = this->settings.password;
		connectOptions["schema"] = this->settings.name;
		connectOptions["port"] = this->settings.port;
		connectOptions["OPT_RECONNECT"] = false; // do not automatically reconnect (need to recover prepared statements instead)
		connectOptions["OPT_CHARSET_NAME"] = "utf8mb4";
		connectOptions["characterSetResults"] = "utf8mb4";
		connectOptions["preInit"] = "SET NAMES utf8mb4";

		// get driver if necessary
		if(!Database::driver) {
			this->errorMessage = "MySQL driver not loaded";
			return false;
		}

		// connect
		this->connection = this->driver->connect(connectOptions);
		if(!this->connection) {
			this->errorMessage = "Could not connect to database";
			return false;
		}
		if(!this->connection->isValid()) {
			this->errorMessage = "Connection to database is invalid";
			return false;
		}

		// run initializing session commands
		sql::Statement * sqlStatement = this->connection->createStatement();
		if(!sqlStatement) {
			this->errorMessage = "Could not create SQL statement";
			DATABASE_DELETE(sqlStatement);
			return false;
		}

		// set lock timeout to 10min
		sqlStatement->execute("SET SESSION innodb_lock_wait_timeout = 600");
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// set error message
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();

		// clear connection
		if(this->connection) {
			if(this->connection->isValid()) this->connection->close();
			delete this->connection;
			this->connection = NULL;
		}

		return false;
	}

	return true;
}

// run initializing SQL commands by processing all files in the "sql" sub-folder
bool Database::initializeSql() {
	// read 'sql' directory
	std::vector<std::string> sqlFiles;
	try {
		sqlFiles = FileSystem::listFilesInPath("sql", ".sql");
	}
	catch(std::exception& e) {
		this->errorMessage = e.what();
		return false;
	}

	// execute all SQL files
	for(auto i = sqlFiles.begin(); i != sqlFiles.end(); ++i) {
		if(!(this->run(*i))) return false;
	}
	return true;
}

// prepare basic SQL statements and logging
bool Database::prepare() {
	// check connection
	if(!this->checkConnection()) return false;

	try {
		// prepare basic SQL statements
		if(!(this->psLastId)) {
			PreparedSqlStatement statement;
			statement.string = "SELECT LAST_INSERT_ID() AS id";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psLastId = this->preparedStatements.size();
		}
		if(!(this->psLog)) {
			PreparedSqlStatement statement;
			statement.string = "INSERT INTO crawlserv_log(module, entry) VALUES (?, ?)";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psLog = this->preparedStatements.size();
		}

		// prepare thread statement
		if(!(this->psSetThreadStatus)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET status = ?, paused = ? WHERE id = ? LIMIT 1";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psSetThreadStatus = this->preparedStatements.size();
		}
	}
	catch(sql::SQLException &e) {
		// set error message
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();
		return false;
	}

	return true;
}

// set the number of seconds to wait before last re-try on connection loss to mySQL server
void Database::setSleepOnError(unsigned long seconds) {
	this->sleepOnError = seconds;
}

// get database settings
const DatabaseSettings& Database::getSettings() const {
	return this->settings;
}

// get last error message
const std::string& Database::getErrorMessage() const {
	return this->errorMessage;
}

// add log entry to database
void Database::log(const std::string& logModule, const std::string& logEntry) {
	// check table lock
	if(this->tablesLocked) {
		std::cout << std::endl << "[ERROR] Logging not possible while a table is locked - re-routing to stdout:";
		std::cout << std::endl << " " << logModule << ": " << logEntry << std::flush;
		return;
	}

	// check prepared SQL statement
	if(!this->psLog) throw std::runtime_error("Missing prepared SQL statement for Database::log(...)");
	sql::PreparedStatement * sqlStatement = this->preparedStatements.at(this->psLog - 1).statement;
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Database::log(...) is NULL");

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	// add entry to database
	try {
		// execute SQL query
		sqlStatement->setString(1, logModule);
		sqlStatement->setString(2, logEntry);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") - " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// get number of log entries for a specific module (or for all modules if module is an empty string) from database
unsigned long Database::getNumberOfLogEntries(const std::string& logModule) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	// create SQL query string
	std::string sqlQuery = "SELECT COUNT(*) FROM crawlserv_log";
	if(logModule.length()) sqlQuery += " WHERE module = ?";

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement(sqlQuery);

		// execute SQL statement
		if(logModule.length()) sqlStatement->setString(1, logModule);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getUInt64(1);

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// remove log entries of a specific module (or all log entries if module is an empty string) from database
void Database::clearLogs(const std::string& logModule) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	// create SQL query string
	std::string sqlQuery = "DELETE FROM crawlserv_log";
	if(logModule.length() > 0) sqlQuery += " WHERE module = ?";

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement(sqlQuery);

		// execute SQL statement
		if(logModule.length() > 0) sqlStatement->setString(1, logModule);
		sqlStatement->execute();

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);

	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// get all threads from database
std::vector<ThreadDatabaseEntry> Database::getThreads() {
	sql::Statement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<ThreadDatabaseEntry> result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL query
		sqlResultSet = sqlStatement->executeQuery("SELECT id, module, status, paused, website, urllist, config, last"
				" FROM crawlserv_threads");

		// get results
		while(sqlResultSet->next()) {
			ThreadDatabaseEntry entry;
			entry.id = sqlResultSet->getUInt64("id");
			entry.module = sqlResultSet->getString("module");
			entry.status = sqlResultSet->getString("status");
			entry.paused = sqlResultSet->getBoolean("paused");
			entry.options.website = sqlResultSet->getUInt64("website");
			entry.options.urlList = sqlResultSet->getUInt64("urllist");
			entry.options.config = sqlResultSet->getUInt64("config");
			entry.last = sqlResultSet->getUInt64("last");
			result.push_back(entry);
		}

		// delete results and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// add a thread to the database and return its new ID
unsigned long Database::addThread(const std::string& threadModule, const ThreadOptions& threadOptions) {
	sql::PreparedStatement * addStatement = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		addStatement = this->connection->prepareStatement("INSERT INTO crawlserv_threads(module, website, urllist, config)"
				" VALUES (?, ?, ?, ?)");

		// execute SQL statement
		addStatement->setString(1, threadModule);
		addStatement->setUInt64(2, threadOptions.website);
		addStatement->setUInt64(3, threadOptions.urlList);
		addStatement->setUInt64(4, threadOptions.config);
		addStatement->execute();

		// get id
		result = this->getLastInsertedId();

		// delete result and SQL statement
		DATABASE_DELETE(addStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(addStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get run time of thread (in seconds) from database by its ID
unsigned long Database::getThreadRunTime(unsigned long threadId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT runtime FROM crawlserv_threads WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getUInt64("runtime");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get pause time of thread (in seconds) from database by its ID
unsigned long Database::getThreadPauseTime(unsigned long threadId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT pausetime FROM crawlserv_threads WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getUInt64("pausetime");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// update thread status in database (and add the pause state to the status message if necessary)
void Database::setThreadStatus(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage) {
	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	// check prepared SQL statement
	if(!this->psSetThreadStatus) throw std::runtime_error("Missing prepared SQL statement for Database::setThreadStatus(...)");
	sql::PreparedStatement * sqlStatement = this->preparedStatements.at(this->psSetThreadStatus - 1).statement;
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Database::setThreadStatus(...) is NULL");

	// create status message
	std::string statusMessage;
	if(threadPaused) {
		if(threadStatusMessage.length()) statusMessage = "PAUSED " + threadStatusMessage;
		else statusMessage = "PAUSED";
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
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// set run time of thread (in seconds) in database
void Database::setThreadRunTime(unsigned long threadId, unsigned long threadRunTime) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("UPDATE crawlserv_threads SET runtime = ? WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadRunTime);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// set pause time of thread (in seconds) in database
void Database::setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("UPDATE crawlserv_threads SET pausetime = ? WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadPauseTime);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// remove thread from database by its ID
void Database::deleteThread(unsigned long threadId) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("DELETE FROM crawlserv_threads WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		sqlStatement->execute();

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_threads")) this->resetAutoIncrement("crawlserv_threads");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// add website to database and return its new ID
unsigned long Database::addWebsite(const std::string& websiteName, const std::string& websiteNameSpace,
		const std::string& websiteDomain) {
	sql::PreparedStatement * addStatement = NULL;
	sql::Statement * createStatement = NULL;
	unsigned long result = 0;
	std::string timeStamp;

	// check website namespace
	if(this->isWebsiteNameSpace(websiteNameSpace)) throw std::runtime_error("Website namespace already exists");

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for adding website
		addStatement =
				this->connection->prepareStatement("INSERT INTO crawlserv_websites(name, namespace, domain) VALUES (?, ?, ?)");

		// execute SQL statement for adding website
		addStatement->setString(1, websiteName);
		addStatement->setString(2, websiteNameSpace);
		addStatement->setString(3, websiteDomain);
		addStatement->execute();

		// delete SQL statement for adding website
		DATABASE_DELETE(addStatement);

		// get id
		result = this->getLastInsertedId();

		// create SQL statement for table creation
		createStatement = this->connection->createStatement();

		// delete SQL statement for table creation
		DATABASE_DELETE(createStatement);

		// add default URL list
		this->addUrlList(result, "Default URL list", "default");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(addStatement);
		DATABASE_DELETE(createStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get website domain from database by its ID
std::string Database::getWebsiteDomain(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT domain FROM crawlserv_websites WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getString("domain");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get namespace of website from database by its ID
std::string Database::getWebsiteNameSpace(unsigned long int websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT namespace FROM crawlserv_websites WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getString("namespace");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get ID and namespace of website from database by URL list ID
IdString Database::getWebsiteNameSpaceFromUrlList(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_urllists WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return IdString(websiteId, this->getWebsiteNameSpace(websiteId));
}

// get ID and namespace of website from database by configuration ID
IdString Database::getWebsiteNameSpaceFromConfig(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_configs WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return IdString(websiteId, this->getWebsiteNameSpace(websiteId));
}

// get ID and namespace of website from database by parsing table ID
IdString Database::getWebsiteNameSpaceFromParsedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_parsedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return IdString(websiteId, this->getWebsiteNameSpace(websiteId));
}

// get ID and namespace of website from database by extracting table ID
IdString Database::getWebsiteNameSpaceFromExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_extractedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return IdString(websiteId, this->getWebsiteNameSpace(websiteId));
}

// get ID and namespace of website from database by analyzing table ID
IdString Database::getWebsiteNameSpaceFromAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return IdString(websiteId, this->getWebsiteNameSpace(websiteId));
}

// check whether website namespace exists in database
bool Database::isWebsiteNameSpace(const std::string& nameSpace) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS (SELECT 1 FROM crawlserv_websites WHERE namespace = ? LIMIT 1)"
				"AS result");

		// execute SQL statement
		sqlStatement->setString(1, nameSpace);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// create new website namespace for duplicated website
std::string Database::duplicateWebsiteNameSpace(const std::string& websiteNameSpace) {
	std::string nameString, numberString;
	unsigned long end = websiteNameSpace.find_last_not_of("0123456789");

	// separate number at the end of string from the rest of the string
	if(end == std::string::npos) {
		// string is number
		numberString = websiteNameSpace;
	}
	else if(end == websiteNameSpace.length() - 1) {
		// no number at the end of the string
		nameString = websiteNameSpace;
	}
	else {
		// number at the end of the string
		nameString = websiteNameSpace.substr(0, end + 1);
		numberString = websiteNameSpace.substr(end + 1);
	}

	unsigned long n = 1;
	std::string result;

	if(numberString.length()) n = std::stoul(numberString, NULL);

	// check whether number needs to be incremented
	while(true) {
		// increment number at the end of the string
		std::ostringstream resultStrStr;
		n++;
		resultStrStr << nameString << n;
		result = resultStrStr.str();
		if(!this->isWebsiteNameSpace(result)) break;
	}
	return result;
}

// update website (and all associated tables) in database
void Database::updateWebsite(unsigned long websiteId, const std::string& websiteName, const std::string& websiteNameSpace,
		const std::string& websiteDomain) {
	sql::Statement * renameStatement = NULL;
	sql::PreparedStatement * updateStatement = NULL;

	// get old namespace
	std::string oldNameSpace = this->getWebsiteNameSpace(websiteId);

	// check website namespace if necessary
	if(websiteNameSpace != oldNameSpace)
		if(this->isWebsiteNameSpace(websiteNameSpace))
			throw std::runtime_error("Webspace namespace already exists");

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// check whether namespace has changed
		if(websiteNameSpace != oldNameSpace) {
			// create SQL statement for renaming
			renameStatement = this->connection->createStatement();

			// rename sub tables
			std::vector<IdString> urlLists = this->getUrlLists(websiteId);
			for(auto liI = urlLists.begin(); liI != urlLists.end(); ++liI) {
				renameStatement->execute("ALTER TABLE crawlserv_" + oldNameSpace + "_" + liI->string + " RENAME TO "
						+ "crawlserv_" + websiteNameSpace + "_" + liI->string);
				renameStatement->execute("ALTER TABLE crawlserv_" + oldNameSpace + "_" + liI->string + "_crawled RENAME TO "
						+ "crawlserv_" + websiteNameSpace + "_" + liI->string + "_crawled");
				renameStatement->execute("ALTER TABLE crawlserv_" + oldNameSpace + "_" + liI->string + "_links RENAME TO "
						+ "crawlserv_" + websiteNameSpace + "_" + liI->string + "_links");

				// rename parsing tables
				std::vector<IdString> parsedTables = this->getParsedTables(liI->id);
				for(auto taI = parsedTables.begin(); taI != parsedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE crawlserv_" + oldNameSpace + "_" + liI->string + "_parsed_"
							+ taI->string + " RENAME TO " + "crawlserv_" + oldNameSpace + "_" + liI->string + "_parsed_"
							+ taI->string);
				}

				// rename extracting tables
				std::vector<IdString> extractedTables = this->getExtractedTables(liI->id);
				for(auto taI = extractedTables.begin(); taI != extractedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE crawlserv_" + oldNameSpace + "_" + liI->string + "_extracted_"
							+ taI->string + " RENAME TO " + "crawlserv_" + oldNameSpace + "_" + liI->string + "_extracted_"
							+ taI->string);
				}

				// rename analyzing tables
				std::vector<IdString> analyzedTables = this->getAnalyzedTables(liI->id);
				for(auto taI = analyzedTables.begin(); taI != analyzedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE crawlserv_" + oldNameSpace + "_" + liI->string + "_analyzed_"
							+ taI->string + " RENAME TO " + "crawlserv_" + oldNameSpace + "_" + liI->string + "_analyzed_"
							+ taI->string);
				}
			}

			// delete SQL statement for renaming
			DATABASE_DELETE(renameStatement);

			// create SQL statement for updating
			updateStatement = this->connection->prepareStatement("UPDATE crawlserv_websites SET name = ?, namespace = ?,"
					" domain = ? WHERE id = ? LIMIT 1");

			// execute SQL statement for updating
			updateStatement->setString(1, websiteName);
			updateStatement->setString(2, websiteNameSpace);
			updateStatement->setString(3, websiteDomain);
			updateStatement->setUInt64(4, websiteId);
			updateStatement->execute();

			// delete SQL statement for updating
			DATABASE_DELETE(updateStatement);
		}
		else {
			// create SQL statement for updating
			updateStatement = this->connection->prepareStatement("UPDATE crawlserv_websites SET name = ?, domain = ?"
					" WHERE id = ? LIMIT 1");

			// execute SQL statement for updating
			updateStatement->setString(1, websiteName);
			updateStatement->setString(2, websiteDomain);
			updateStatement->setUInt64(3, websiteId);
			updateStatement->execute();

			// delete SQL statement for updating
			DATABASE_DELETE(updateStatement);
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(renameStatement);
		DATABASE_DELETE(updateStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// delete website (and all associated data) from database by its ID
void Database::deleteWebsite(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;

	// get website namespace
	std::string websiteNameSpace = this->getWebsiteNameSpace(websiteId);

	try {
		// delete URL lists
		std::vector<IdString> urlLists = this->getUrlLists(websiteId);
		for(auto liI = urlLists.begin(); liI != urlLists.end(); ++liI) {
			// delete URL list
			this->deleteUrlList(liI->id);
		}

		// check connection
		if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

		// create SQL statement for deletion of website
		sqlStatement = this->connection->prepareStatement("DELETE FROM crawlserv_websites WHERE id = ? LIMIT 1");

		// execute SQL statements for deletion of website
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->execute();

		// delete SQL statements for deletion of website
		DATABASE_DELETE(sqlStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_websites")) this->resetAutoIncrement("crawlserv_websites");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// duplicate website in database by its ID (no processed data will be duplicated)
unsigned long Database::duplicateWebsite(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for geting website info
		sqlStatement =
				this->connection->prepareStatement("SELECT name, namespace, domain FROM crawlserv_websites WHERE id = ? LIMIT 1");

		// execute SQL statement for geting website info
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		std::string websiteName = sqlResultSet->getString("name");
		std::string websiteNameSpace =sqlResultSet->getString("namespace");
		std::string websiteDomain =sqlResultSet->getString("domain");

		// delete result and SQL statement for geting website info
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);

		// create new name and new namespace
		std::string newName = websiteName + " (copy)";
		std::string newNameSpace = Database::duplicateWebsiteNameSpace(websiteNameSpace);

		// add website
		result = this->addWebsite(newName, newNameSpace, websiteDomain);

		// create SQL statement for geting URL list info
		sqlStatement =
				this->connection->prepareStatement("SELECT name, namespace FROM crawlserv_urllists WHERE website = ?");

		// execute SQL statement for geting URL list info
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get results
		while(sqlResultSet->next()) {
			// add URL list
			std::string urlListName = sqlResultSet->getString("namespace");

			// add empty URL lists with same name
			if(urlListName != "default") this->addUrlList(result, urlListName, sqlResultSet->getString("namespace"));
		}

		// delete result and SQL statement for getting URL list info
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);

		// create SQL statement for getting queries
		sqlStatement = this->connection->prepareStatement("SELECT name, query, type, resultbool, resultsingle, resultmulti,"
				" textonly FROM crawlserv_queries WHERE website = ?");

		// execute SQL statement for getting queries
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get results
		while(sqlResultSet->next()) {
			// add query
			this->addQuery(result, sqlResultSet->getString("name"), sqlResultSet->getString("query"),
					sqlResultSet->getString("type"), sqlResultSet->getBoolean("resultbool"),
					sqlResultSet->getBoolean("resultsingle"), sqlResultSet->getBoolean("resultmulti"),
					sqlResultSet->getBoolean("textonly"));

		}

		// delete result and SQL statement for getting queries
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);

		// create SQL statement for getting configurations
		sqlStatement = this->connection->prepareStatement("SELECT module, name, config FROM crawlserv_configs WHERE website = ?");

		// execute SQL statement for getting configurations
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get results
		while(sqlResultSet->next()) {
			// add configuration
			this->addConfiguration(result, sqlResultSet->getString("module"), sqlResultSet->getString("name"),
					sqlResultSet->getString("config"));
		}

		// delete result and SQL statement for getting configurations
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// add URL list to database and return its new ID
unsigned long Database::addUrlList(unsigned long websiteId, const std::string& listName, const std::string& listNameSpace) {
	sql::PreparedStatement * addStatement = NULL;
	sql::Statement * createStatement = NULL;
	unsigned long result = 0;

	// get website namespace
	std::string websiteNameSpace = this->getWebsiteNameSpace(websiteId);

	// check URL list namespace
	if(this->isUrlListNameSpace(websiteId, listNameSpace)) throw std::runtime_error("URL list namespace already exists");

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for adding URL list
		addStatement =
				this->connection->prepareStatement("INSERT INTO crawlserv_urllists(website, name, namespace) VALUES (?, ?, ?)");

		// execute SQL query for adding URL list
		addStatement->setUInt64(1, websiteId);
		addStatement->setString(2, listName);
		addStatement->setString(3, listNameSpace);
		addStatement->execute();

		// delete SQL statement for adding URL list
		DATABASE_DELETE(addStatement);

		// get id
		result = this->getLastInsertedId();

		// create SQL statement for table creation
		createStatement = this->connection->createStatement();

		// execute SQL queries for table creation
		createStatement->execute("CREATE TABLE IF NOT EXISTS crawlserv_" + websiteNameSpace + "_" + listNameSpace
				+ "(id SERIAL, manual BOOLEAN DEFAULT false NOT NULL, url VARCHAR(2000) NOT NULL, hash INT UNSIGNED DEFAULT 0 NOT NULL,"
				" crawled BOOLEAN DEFAULT false NOT NULL, parsed BOOLEAN DEFAULT false NOT NULL,"
				" extracted BOOLEAN DEFAULT false NOT NULL, analyzed BOOLEAN DEFAULT false NOT NULL, crawllock DATETIME DEFAULT NULL,"
				" parselock DATETIME DEFAULT NULL, extractlock DATETIME DEFAULT NULL, analyzelock DATETIME DEFAULT NULL,"
				" PRIMARY KEY(id), INDEX(hash)) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");
		createStatement->execute("CREATE TABLE IF NOT EXISTS crawlserv_" + websiteNameSpace + "_" + listNameSpace + "_crawled("
				"id SERIAL, url BIGINT UNSIGNED NOT NULL,"
				" crawltime DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP NOT NULL,"
				" archived BOOLEAN DEFAULT false NOT NULL, response SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
				" type TINYTEXT NOT NULL, content LONGTEXT NOT NULL, PRIMARY KEY(id), FOREIGN KEY(url) REFERENCES crawlserv_"
				+ websiteNameSpace + "_" + listNameSpace + "(id) ON UPDATE RESTRICT ON DELETE CASCADE, INDEX(crawltime))"
				" CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ROW_FORMAT=COMPRESSED");
		createStatement->execute("CREATE TABLE IF NOT EXISTS crawlserv_" + websiteNameSpace + "_" + listNameSpace + "_links("
				"id SERIAL, fromurl BIGINT UNSIGNED NOT NULL, tourl BIGINT UNSIGNED NOT NULL, archived BOOLEAN DEFAULT FALSE NOT NULL,"
				" PRIMARY KEY(id), FOREIGN KEY(fromurl) REFERENCES crawlserv_" + websiteNameSpace + "_" + listNameSpace + "(id) ON"
				" UPDATE RESTRICT ON DELETE CASCADE, FOREIGN KEY(tourl) REFERENCES crawlserv_" + websiteNameSpace + "_"
				+ listNameSpace + "(id) ON UPDATE RESTRICT ON DELETE CASCADE)");

		// delete SQL statement for table creation
		DATABASE_DELETE(createStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(addStatement);
		DATABASE_DELETE(createStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get URL lists for ID-specified website from database
std::vector<IdString> Database::getUrlLists(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<IdString> result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT id, namespace FROM crawlserv_urllists WHERE website = ?");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get results
		while(sqlResultSet->next()) result.push_back(IdString(sqlResultSet->getUInt64("id"), sqlResultSet->getString("namespace")));

		// delete results and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get namespace of URL list by its ID
std::string Database::getUrlListNameSpace(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT namespace FROM crawlserv_urllists WHERE id = ? LIMIT 1");

		// execute SQL query
		sqlStatement->setUInt64(1, listId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getString("namespace");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get ID and namespace of URL list from database by parsing table ID
IdString Database::getUrlListNameSpaceFromParsedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long urlListId = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT urllist FROM crawlserv_parsedtables WHERE id = ? LIMIT 1");

		// execute SQL query
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		urlListId = sqlResultSet->getUInt64("urllist");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return IdString(urlListId, this->getUrlListNameSpace(urlListId));
}

// get ID and namespace of URL list from database by extracting table ID
IdString Database::getUrlListNameSpaceFromExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long urlListId = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT urllist FROM crawlserv_extractedtables WHERE id = ? LIMIT 1");

		// execute SQL query
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		urlListId = sqlResultSet->getUInt64("urllist");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return IdString(urlListId, this->getUrlListNameSpace(urlListId));
}

// get ID and namespace of URL list from database by analyzing table ID
IdString Database::getUrlListNameSpaceFromAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long urlListId = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT urllist FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1");

		// execute SQL query
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		urlListId = sqlResultSet->getUInt64("urllist");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return IdString(urlListId, this->getUrlListNameSpace(urlListId));
}

// check whether URL list namespace for an ID-specified website exists in database
bool Database::isUrlListNameSpace(unsigned long websiteId, const std::string& nameSpace) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS (SELECT 1 FROM crawlserv_urllists"
				" WHERE website = ? AND namespace = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setString(2, nameSpace);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// update URL list (and all associated tables) in database
void Database::updateUrlList(unsigned long listId, const std::string& listName, const std::string& listNameSpace) {
	sql::Statement * renameStatement = NULL;
	sql::PreparedStatement * updateStatement = NULL;

	// get website namespace and URL list name
	IdString websiteNameSpace = this->getWebsiteNameSpaceFromUrlList(listId);
	std::string oldListNameSpace = this->getUrlListNameSpace(listId);

	// check website namespace if necessary
	if(listNameSpace != oldListNameSpace)
		if(this->isUrlListNameSpace(websiteNameSpace.id, listNameSpace))
			throw std::runtime_error("Webspace namespace already exists");

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		if(listNameSpace != oldListNameSpace) {
			// create SQL statement for renaming
			renameStatement = this->connection->createStatement();

			// rename main tables
			renameStatement->execute("ALTER TABLE crawlserv_" + websiteNameSpace.string + "_" + oldListNameSpace
					+ " RENAME TO crawlserv_" + websiteNameSpace.string + "_" + listNameSpace);
			renameStatement->execute("ALTER TABLE crawlserv_" + websiteNameSpace.string + "_" + oldListNameSpace
					+ "_crawled RENAME TO crawlserv_" + websiteNameSpace.string + "_" + listNameSpace + "_crawled");
			renameStatement->execute("ALTER TABLE crawlserv_" + websiteNameSpace.string + "_" + oldListNameSpace
					+ "_links RENAME TO crawlserv_" + websiteNameSpace.string + "_" + listNameSpace + "_links");

			// rename parsing tables
			std::vector<IdString> parsedTables = this->getParsedTables(listId);
			for(auto taI = parsedTables.begin(); taI != parsedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE crawlserv_" + websiteNameSpace.string + "_" + oldListNameSpace + "_parsed_"
						+ taI->string + " RENAME TO " + "crawlserv_" + websiteNameSpace.string + "_" + listNameSpace + "_parsed_"
						+ taI->string);
			}

			// rename extracting tables
			std::vector<IdString> extractedTables = this->getExtractedTables(listId);
			for(auto taI = extractedTables.begin(); taI != extractedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE crawlserv_" + websiteNameSpace.string + "_" + oldListNameSpace + "_extracted_"
						+ taI->string + " RENAME TO " + "crawlserv_" + websiteNameSpace.string + "_" + listNameSpace + "_extracted_"
						+ taI->string);
			}

			// rename analyzing tables
			std::vector<IdString> analyzedTables = this->getAnalyzedTables(listId);
			for(auto taI = analyzedTables.begin(); taI != analyzedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE crawlserv_" + websiteNameSpace.string + "_" + oldListNameSpace + "_analyzed_"
						+ taI->string + " RENAME TO " + "crawlserv_" + websiteNameSpace.string + "_" + listNameSpace + "_analyzed_"
						+ taI->string);
			}

			// delete SQL statement for renaming
			DATABASE_DELETE(renameStatement);

			// create SQL statement for updating
			updateStatement = this->connection->prepareStatement("UPDATE crawlserv_urllists SET name = ?, namespace = ?"
					" WHERE id = ? LIMIT 1");

			// execute SQL statement for updating
			updateStatement->setString(1, listName);
			updateStatement->setString(2, listNameSpace);
			updateStatement->setUInt64(3, listId);
			updateStatement->execute();

			// delete SQL statement for updating
			DATABASE_DELETE(updateStatement);
		}
		else {
			// create SQL statement for updating
			updateStatement = this->connection->prepareStatement("UPDATE crawlserv_urllists SET name = ? WHERE id = ? LIMIT 1");

			// execute SQL statement for updating
			updateStatement->setString(1, listName);
			updateStatement->setUInt64(2, listId);
			updateStatement->execute();

			// delete SQL statement for updating
			DATABASE_DELETE(updateStatement);
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(renameStatement);
		DATABASE_DELETE(updateStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// delete URL list (and all associated data) from database by its ID
void Database::deleteUrlList(unsigned long listId) {
	sql::PreparedStatement * deleteStatement = NULL;
	sql::Statement * dropStatement = NULL;

	// get website namespace and URL list name
	IdString websiteNameSpace = this->getWebsiteNameSpaceFromUrlList(listId);
	std::string listNameSpace = this->getUrlListNameSpace(listId);

	try {
		// delete parsing tables
		std::vector<IdString> parsedTables = this->getParsedTables(listId);
		for(auto taI = parsedTables.begin(); taI != parsedTables.end(); ++taI) {
			this->deleteParsedTable(taI->id);
		}

		// delete extracting tables
		std::vector<IdString> extractedTables = this->getExtractedTables(listId);
		for(auto taI = extractedTables.begin(); taI != extractedTables.end(); ++taI) {
			this->deleteParsedTable(taI->id);
		}

		// delete analyzing tables
		std::vector<IdString> analyzedTables = this->getAnalyzedTables(listId);
		for(auto taI = analyzedTables.begin(); taI != analyzedTables.end(); ++taI) {
			this->deleteParsedTable(taI->id);
		}

		// check connection
		if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

		// create SQL statement for deleting URL list
		deleteStatement = this->connection->prepareStatement("DELETE FROM crawlserv_urllists WHERE id = ? LIMIT 1");

		// execute SQL statement for deleting URL list
		deleteStatement->setUInt64(1, listId);
		deleteStatement->execute();

		// delete SQL statement for deleting URL list
		DATABASE_DELETE(deleteStatement);

		// create SQL statement for dropping tables
		dropStatement = this->connection->createStatement();

		// execute SQL queries for dropping tables
		dropStatement->execute("DROP TABLE IF EXISTS crawlserv_" + websiteNameSpace.string + "_" + listNameSpace + "_links");
		dropStatement->execute("DROP TABLE IF EXISTS crawlserv_" + websiteNameSpace.string + "_" + listNameSpace + "_crawled");
		dropStatement->execute("DROP TABLE IF EXISTS crawlserv_" + websiteNameSpace.string + "_" + listNameSpace);

		// delete SQL statement for dropping tables
		DATABASE_DELETE(dropStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_urllists")) this->resetAutoIncrement("crawlserv_urllists");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(deleteStatement);
		DATABASE_DELETE(dropStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// add query to database
unsigned long Database::addQuery(unsigned long websiteId, const std::string& queryName, const std::string& queryText,
		const std::string& queryType, bool queryResultBool, bool queryResultSingle, bool queryResultMulti, bool queryTextOnly) {
	sql::PreparedStatement * sqlStatement = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for adding query
		sqlStatement =
				this->connection->prepareStatement("INSERT INTO crawlserv_queries(website, name, query, type, resultbool,"
						" resultsingle, resultmulti, textonly) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

		// execute SQL query for adding query
		if(websiteId) sqlStatement->setUInt64(1, websiteId);
		else sqlStatement->setNull(1, 0);
		sqlStatement->setString(2, queryName);
		sqlStatement->setString(3, queryText);
		sqlStatement->setString(4, queryType);
		sqlStatement->setBoolean(5, queryResultBool);
		sqlStatement->setBoolean(6, queryResultSingle);
		sqlStatement->setBoolean(7, queryResultMulti);
		sqlStatement->setBoolean(8, queryTextOnly);
		sqlStatement->execute();

		// delete SQL statement for adding query
		DATABASE_DELETE(sqlStatement);

		// get id
		result = this->getLastInsertedId();
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get query properties from database by its ID
void Database::getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo, bool& queryResultBoolTo,
		bool& queryResultSingleTo, bool& queryResultMultiTo, bool queryTextOnlyTo) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT query, type, resultbool, resultsingle, resultmulti, textonly"
				" FROM crawlserv_queries WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		queryTextTo = sqlResultSet->getString("query");
		queryTypeTo = sqlResultSet->getString("type");
		queryResultBoolTo = sqlResultSet->getBoolean("resultbool");
		queryResultSingleTo = sqlResultSet->getBoolean("resultsingle");
		queryResultMultiTo = sqlResultSet->getBoolean("resultmulti");
		queryTextOnlyTo = sqlResultSet->getBoolean("textonly");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// edit query in database
void Database::updateQuery(unsigned long queryId, const std::string& queryName, const std::string& queryText,
		const std::string& queryType, bool queryResultBool, bool queryResultSingle, bool queryResultMulti, bool queryTextOnly) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for updating
		sqlStatement = this->connection->prepareStatement("UPDATE crawlserv_queries SET name = ?, query = ?, type = ?,"
				" resultbool = ?, resultsingle = ?, resultmulti = ?, textonly = ? WHERE id = ? LIMIT 1");

		// execute SQL statement for updating
		sqlStatement->setString(1, queryName);
		sqlStatement->setString(2, queryText);
		sqlStatement->setString(3, queryType);
		sqlStatement->setBoolean(4, queryResultBool);
		sqlStatement->setBoolean(5, queryResultSingle);
		sqlStatement->setBoolean(6, queryResultMulti);
		sqlStatement->setBoolean(7, queryTextOnly);
		sqlStatement->setUInt64(8, queryId);
		sqlStatement->execute();

		// delete SQL statement for updating
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// delete query from database by its ID
void Database::deleteQuery(unsigned long queryId) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("DELETE FROM crawlserv_queries WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		sqlStatement->execute();

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_queries")) this->resetAutoIncrement("crawlserv_queries");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// duplicate query in database by its ID
unsigned long Database::duplicateQuery(unsigned long queryId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for getting query info
		sqlStatement = this->connection->prepareStatement("SELECT website, name, query, type, resultbool, resultsingle,"
				" resultmulti, textonly FROM crawlserv_queries WHERE id = ? LIMIT 1");

		// execute SQL statement for getting query info
		sqlStatement->setUInt64(1, queryId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();

		// add query
		result = this->addQuery(sqlResultSet->getUInt64("website"), sqlResultSet->getString("name") + " (copy)",
				sqlResultSet->getString("query"), sqlResultSet->getString("type"), sqlResultSet->getBoolean("resultbool"),
				sqlResultSet->getBoolean("resultsingle"), sqlResultSet->getBoolean("resultmulti"),
				sqlResultSet->getBoolean("textonly"));

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// add configuration to database
unsigned long Database::addConfiguration(unsigned long websiteId, const std::string& configModule, const std::string& configName,
		const std::string& config) {
	sql::PreparedStatement * sqlStatement = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for adding configuration
		sqlStatement = this->connection->prepareStatement("INSERT INTO crawlserv_configs(website, module, name, config)"
						" VALUES (?, ?, ?, ?)");

		// execute SQL query for adding website
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setString(2, configModule);
		sqlStatement->setString(3, configName);
		sqlStatement->setString(4, config);
		sqlStatement->execute();

		// delete SQL statement for adding website
		DATABASE_DELETE(sqlStatement);

		// get id
		result = this->getLastInsertedId();
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get configuration from database by its ID
const std::string Database::getConfiguration(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT config FROM crawlserv_configs WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getString("config");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// update configuration in database
void Database::updateConfiguration(unsigned long configId, const std::string& configName, const std::string& config) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for updating
		sqlStatement =
				this->connection->prepareStatement("UPDATE crawlserv_configs SET name = ?, config = ? WHERE id = ? LIMIT 1");

		// execute SQL statement for updating
		sqlStatement->setString(1, configName);
		sqlStatement->setString(2, config);
		sqlStatement->setUInt64(3, configId);
		sqlStatement->execute();

		// delete SQL statement for updating
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// delete configuration from database by its ID
void Database::deleteConfiguration(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("DELETE FROM crawlserv_configs WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlStatement->execute();

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_configs")) this->resetAutoIncrement("crawlserv_configs");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// duplicate configuration in database by its ID
unsigned long Database::duplicateConfiguration(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for getting configuration info
		sqlStatement = this->connection->prepareStatement("SELECT website, module, name, config FROM crawlserv_configs"
				" WHERE id = ? LIMIT 1");

		// execute SQL statement for getting configuration info
		sqlStatement->setUInt64(1, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();

		// add configuration
		result = this->addConfiguration(sqlResultSet->getUInt64("website"), sqlResultSet->getString("module"),
				sqlResultSet->getString("name") + " (copy)", sqlResultSet->getString("config"));

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get parsed tables for ID-specified URL list from database
std::vector<IdString> Database::getParsedTables(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<IdString> result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT id, name FROM crawlserv_parsedtables WHERE urllist = ?");

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		sql::ResultSet * sqlResultSet = sqlStatement->executeQuery();

		// get results
		while(sqlResultSet->next()) result.push_back(IdString(sqlResultSet->getUInt64("id"), sqlResultSet->getString("name")));

		// delete results and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get name of parsing table from database by its ID
std::string Database::getParsedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT name FROM crawlserv_parsedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getString("name");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// delete parsing table from database by its ID
void Database::deleteParsedTable(unsigned long tableId) {
	sql::PreparedStatement * deleteStatement = NULL;
	sql::Statement * dropStatement = NULL;

	// get namespace, URL list name and table name
	IdString websiteNameSpace = this->getWebsiteNameSpaceFromParsedTable(tableId);
	IdString listNameSpace = this->getUrlListNameSpaceFromParsedTable(tableId);
	std::string tableName = this->getParsedTable(tableId);

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for deletion
		deleteStatement =
				this->connection->prepareStatement("DELETE FROM crawlserv_parsedtables WHERE id = ? LIMIT 1");

		// execute SQL statement for deletion
		deleteStatement->setUInt64(1, tableId);
		deleteStatement->execute();

		// delete SQL statement for deletion
		DATABASE_DELETE(deleteStatement);

		// create SQL statement for dropping table
		dropStatement = this->connection->createStatement();

		// execute SQL query for dropping table
		dropStatement->execute("DROP TABLE IF EXISTS crawlserv_" + websiteNameSpace.string + "_" + listNameSpace.string + "_parsed_"
				+ tableName);

		// delete SQL statement for dropping table
		DATABASE_DELETE(dropStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_parsedtables")) this->resetAutoIncrement("crawlserv_parsedtables");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(deleteStatement);
		DATABASE_DELETE(dropStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// get extracted tables for ID-specified URL list from database
std::vector<IdString> Database::getExtractedTables(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<IdString> result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT id, name FROM crawlserv_extractedtables WHERE urllist = ?");

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		sql::ResultSet * sqlResultSet = sqlStatement->executeQuery();

		// get results
		while(sqlResultSet->next()) result.push_back(IdString(sqlResultSet->getUInt64("id"), sqlResultSet->getString("name")));

		// delete results and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get name extracting table from database by its ID
std::string Database::getExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT name FROM crawlserv_extractedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getString("name");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// delete extracting table from database by its ID
void Database::deleteExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * deleteStatement = NULL;
	sql::Statement * dropStatement = NULL;

	// get namespace, URL list name and table name
	IdString websiteNameSpace = this->getWebsiteNameSpaceFromExtractedTable(tableId);
	IdString listNameSpace = this->getUrlListNameSpaceFromExtractedTable(tableId);
	std::string tableName = this->getExtractedTable(tableId);

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for deletion
		deleteStatement =
				this->connection->prepareStatement("DELETE FROM crawlserv_extractedtables WHERE id = ? LIMIT 1");

		// execute SQL statement for deletion
		deleteStatement->setUInt64(1, tableId);
		deleteStatement->execute();

		// delete SQL statement for deletion
		DATABASE_DELETE(deleteStatement);

		// create SQL statement for dropping table
		dropStatement = this->connection->createStatement();

		// execute SQL query for dropping table
		dropStatement->execute("DROP TABLE IF EXISTS crawlserv_" + websiteNameSpace.string + "_" + listNameSpace.string + "_extracted_"
				+ tableName);

		// delete SQL statement for dropping table
		DATABASE_DELETE(dropStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_extractedtables")) this->resetAutoIncrement("crawlserv_extractedtables");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(deleteStatement);
		DATABASE_DELETE(dropStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// get analyzed tables for ID-specified URL list from database
std::vector<IdString> Database::getAnalyzedTables(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<IdString> result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT id, name FROM crawlserv_analyzedtables WHERE urllist = ?");

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		sql::ResultSet * sqlResultSet = sqlStatement->executeQuery();

		// get results
		while(sqlResultSet->next()) result.push_back(IdString(sqlResultSet->getUInt64("id"), sqlResultSet->getString("name")));

		// delete results and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get name of analyzing table from database by its ID
std::string Database::getAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT name FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getString("name");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// delete analyzing from database by its ID
void Database::deleteAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * deleteStatement = NULL;
	sql::Statement * dropStatement = NULL;

	// get namespace, URL list name and table name
	IdString websiteNameSpace = this->getWebsiteNameSpaceFromAnalyzedTable(tableId);
	IdString listNameSpace = this->getUrlListNameSpaceFromAnalyzedTable(tableId);
	std::string tableName = this->getParsedTable(tableId);

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement for deletion
		deleteStatement =
				this->connection->prepareStatement("DELETE FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1");

		// execute SQL statement for deletion
		deleteStatement->setUInt64(1, tableId);
		deleteStatement->execute();

		// delete SQL statement for deletion
		DATABASE_DELETE(deleteStatement);

		// create SQL statement for dropping table
		dropStatement = this->connection->createStatement();

		// execute SQL query for dropping table
		dropStatement->execute("DROP TABLE IF EXISTS crawlserv_" + websiteNameSpace.string + "_" + listNameSpace.string + "_analyzed_"
				+ tableName);

		// delete SQL statement for dropping table
		DATABASE_DELETE(dropStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_analyzedtables")) this->resetAutoIncrement("crawlserv_analyzedtables");
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(deleteStatement);
		DATABASE_DELETE(dropStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// release table locks in database if necessary
void Database::releaseLocks() {
	if(this->tablesLocked) this->unlockTables();
}

// check whether website ID is valid
bool Database::isWebsite(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_websites WHERE id = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether URL list ID is valid
bool Database::isUrlList(unsigned long urlListId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_urllists WHERE id = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, urlListId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether URL list ID is valid for the ID-specified website
bool Database::isUrlList(unsigned long websiteId, unsigned long urlListId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_urllists WHERE website = ? AND id = ?"
				" LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, urlListId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether query ID is valid
bool Database::isQuery(unsigned long queryId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_queries WHERE id = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether query ID is valid for the ID-specified website
bool Database::isQuery(unsigned long websiteId, unsigned long queryId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_queries"
				" WHERE (website = ? OR website = 0) AND id = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, queryId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether configuration ID is valid
bool Database::isConfiguration(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_configs WHERE id = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether configuration ID is valid for the ID-specified website
bool Database::isConfiguration(unsigned long websiteId, unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_configs WHERE website = ? AND id = ?"
				" LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether parsing table ID is valid
bool Database::isParsedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_parsedtables WHERE id = ? LIMIT 1)"
				" AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether extracting table ID is valid
bool Database::isExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_extractedtables WHERE id = ? LIMIT 1)"
				" AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether analyzing table ID is valid
bool Database::isAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1)"
				" AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether connection is valid and try to reconnect if necesssary (sets error message on failure)
bool Database::checkConnection() {
	// check driver
	if(!(this->driver)) {
		this->errorMessage = "MySQL driver not loaded";
		return false;
	}

	// check connection
	if(!(this->connection)) {
		this->errorMessage = "No connection to database";
		return false;
	}

	try {
		// check whether connection is valid
		if(this->connection->isValid()) return true;

		// try to reconnect
		if(!this->connection->reconnect()) {
			// simple reconnect failed: try to reset connection
			delete this->connection;
			this->connection = NULL;
			if(!(this->connect())) {
				if(this->sleepOnError) std::this_thread::sleep_for(std::chrono::seconds(this->sleepOnError));
				if(!(this->connect())) {
					this->errorMessage = "Could not re-connect to mySQL database after connection loss";
					return false;
				}
			}
		}

		// recover prepared statements
		for(auto i = this->preparedStatements.begin(); i != this->preparedStatements.end(); ++i) {
			if(i->statement) {
				delete i->statement;
				i->statement = NULL;
			}
			i->statement = this->connection->prepareStatement(i->string);
		}
	}
	catch(sql::SQLException &e) {
		// SQL error while recovering prepared statements: set error message and return
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();

		// clear connection
		if(this->connection) {
			delete this->connection;
			this->connection = NULL;
		}

		return false;
	}

	return true;
}

// get last inserted ID from database
unsigned long Database::getLastInsertedId() {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!this->psLastId) throw std::runtime_error("Missing prepared SQL statement for last inserted id");
	sqlStatement = this->preparedStatements.at(this->psLastId - 1).statement;
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for last inserted id is NULL");

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getUInt64("id");

		DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	// return result
	return result;
}

// check whether a name-specified table is empty
bool Database::isTableEmpty(const std::string& tableName) {
	sql::Statement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery("SELECT NOT EXISTS (SELECT 1 FROM " + tableName + " LIMIT 1) AS result");

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlResultSet);
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// reset the auto increment of an (empty) table in database
void Database::resetAutoIncrement(const std::string& tableName) {
	sql::Statement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlStatement->execute("ALTER TABLE " + tableName + " AUTO_INCREMENT = 1");

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// lock table in database
void Database::lockTable(const std::string& tableName) {
	sql::Statement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		this->tablesLocked = true;
		sqlStatement->execute("LOCK TABLES " + tableName + " WRITE");

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// lock two tables in database
void Database::lockTables(const std::string& tableName1, const std::string& tableName2) {
	sql::Statement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		this->tablesLocked = true;
		sqlStatement->execute("LOCK TABLES " + tableName1 + " WRITE, " + tableName2 + " WRITE");

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// unlock tables in database
void Database::unlockTables() {
	sql::Statement * sqlStatement = NULL;

	// check connection
	if(!this->checkConnection()) throw std::runtime_error(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlStatement->execute("UNLOCK TABLES");
		this->tablesLocked = false;

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "SQL Error " << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// run file with SQL commands
bool Database::run(const std::string& sqlFile) {
	// open SQL file
	std::ifstream initSQLFile(sqlFile);

	if(initSQLFile.is_open()) {
		// check connection
		if(!this->checkConnection()) return false;

		// create SQL statement
		sql::Statement * sqlStatement = this->connection->createStatement();
		std::string line;

		if(!sqlStatement) {
			this->errorMessage = "Could not create SQL statement";
			return false;
		}

		// execute lines in SQL file
		unsigned long lineCounter = 1;
		while(std::getline(initSQLFile, line)) {
			try {
				if(line.length()) sqlStatement->execute(line);
				lineCounter++;
			}
			catch(sql::SQLException &e) {
				// SQL error
				DATABASE_DELETE(sqlStatement);
				std::ostringstream errorStrStr;
				errorStrStr << "SQL Error #" << e.getErrorCode() << " on line #" << lineCounter
						<< " (State " << e.getSQLState() << "): " << e.what();
				this->errorMessage = errorStrStr.str();
				return false;
			}
		}

		// delete SQL statement
		DATABASE_DELETE(sqlStatement);

		// close SQL file
		initSQLFile.close();
	}
	else {
		this->errorMessage = "Could not open \'" + sqlFile + "\' for reading";
		return false;
	}

	return true;
}
