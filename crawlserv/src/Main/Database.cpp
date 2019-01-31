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

#include "../Main/Database.h"

namespace crawlservpp::Main {

sql::Driver * Database::driver = NULL;

// constructor: save settings and set default values
Database::Database(const crawlservpp::Struct::DatabaseSettings& dbSettings) : settings(dbSettings) {
	// set default values
	this->connection = NULL;
	this->tablesLocked = false;
	this->maxAllowedPacketSize = 0;
	this->sleepOnError = 0;

	this->psLog = 0;
	this->psLastId = 0;
	this->psSetThreadStatus = 0;
	this->psSetThreadStatusMessage = 0;
	this->psStrlen = 0;

	// get driver instance if necessary
	if(!Database::driver)
		Database::driver = get_driver_instance();
}

// destructor
Database::~Database() {
	// clear prepared SQL statements
	for(auto i = this->preparedStatements.begin(); i != this->preparedStatements.end(); ++i)
		MAIN_DATABASE_DELETE(i->statement);
	this->preparedStatements.clear();

	// clear connection
	if(this->connection) {
		if(this->connection->isValid()) this->connection->close();
		delete this->connection;
		this->connection = NULL;
	}

	this->tablesLocked = false;
}

/*
 * SETTERS
 */

// set the number of seconds to wait before (first and last) re-try on connection loss to mySQL server
void Database::setSleepOnError(unsigned long seconds) {
	this->sleepOnError = seconds;
}

/*
 * GETTERS
 */

// get settings of the database
const crawlservpp::Struct::DatabaseSettings& Database::getSettings() const {
	return this->settings;
}

// get the last error message
const std::string& Database::getErrorMessage() const {
	return this->errorMessage;
}

/*
 * INITIALIZING FUNCTIONS
 */

// connect to the database
bool Database::connect() {
	sql::Statement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool success = true;

	// connect to database
	try {
		// set options for connecting
		sql::ConnectOptionsMap connectOptions;
		connectOptions["hostName"] = this->settings.host;
		connectOptions["userName"] = this->settings.user;
		connectOptions["password"] = this->settings.password;
		connectOptions["schema"] = this->settings.name;
		connectOptions["port"] = this->settings.port;
		connectOptions["OPT_RECONNECT"] = false; // do not automatically reconnect (need to manually recover prepared statements instead)
		connectOptions["OPT_CHARSET_NAME"] = "utf8mb4";
		connectOptions["characterSetResults"] = "utf8mb4";
		connectOptions["preInit"] = "SET NAMES utf8mb4";

		// get driver if necessary
		if(!Database::Database::driver) {
			this->errorMessage = "MySQL driver not loaded";
			return false;
		}

		// connect
		this->connection = this->driver->connect(connectOptions);
		if(!(this->connection)) {
			this->errorMessage = "Could not connect to database";
			return false;
		}
		if(!(this->connection->isValid())) {
			this->errorMessage = "Connection to database is invalid";
			return false;
		}

		// set max_allowed_packet to highest possible value (1 GiB)
		int maxAllowedPacket = 1073741824;
		this->connection->setClientOption("OPT_MAX_ALLOWED_PACKET", (void *) &maxAllowedPacket);

		// run initializing session commands
		sqlStatement = this->connection->createStatement();
		if(!sqlStatement) {
			this->errorMessage = "Could not create SQL statement";
			return false;
		}

		// set lock timeout to 10min
		sqlStatement->execute("SET SESSION innodb_lock_wait_timeout = 600");

		// get maximum allowed package size
		sqlResultSet = sqlStatement->executeQuery("SHOW VARIABLES LIKE 'max_allowed_packet'");
		if(sqlResultSet && sqlResultSet->next()){
			this->maxAllowedPacketSize = sqlResultSet->getUInt64("Value");
			if(!(this->maxAllowedPacketSize)) {
				this->errorMessage = "\'max_allowed_packet\' is zero";
				success = false;
			}
		}
		else {
			this->errorMessage = "Could not get \'max_allowed_packet\'";
			success = false;
		}

		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// free memory
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);

		// set error message
		std::ostringstream errorStrStr;
		errorStrStr << "connect() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();

		// clear connection
		if(this->connection) {
			if(this->connection->isValid()) this->connection->close();
			delete this->connection;
			this->connection = NULL;
		}

		return false;
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return success;
}

// run initializing SQL commands by processing all .sql files in the "sql" sub-folder
bool Database::initializeSql() {
	// read 'sql' directory
	std::vector<std::string> sqlFiles;
	try {
		sqlFiles = crawlservpp::Helper::FileSystem::listFilesInPath("sql", ".sql");
	}
	catch(std::exception& e) {
		this->errorMessage = e.what();
		return false;
	}

	// execute all SQL files
	for(auto i = sqlFiles.begin(); i != sqlFiles.end(); ++i)
		if(!(this->run(*i))) return false;
	return true;
}

// prepare basic SQL statements (getting last id, logging, thread status and strlen)
bool Database::prepare() {
	// check connection
	if(!(this->checkConnection())) return false;

	// reserve memory for SQL statements
	this->preparedStatements.reserve(5);

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

		// prepare thread statements
		if(!(this->psSetThreadStatus)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET status = ?, paused = ? WHERE id = ? LIMIT 1";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psSetThreadStatus = this->preparedStatements.size();
		}
		if(!(this->psSetThreadStatusMessage)) {
			PreparedSqlStatement statement;
			statement.string = "UPDATE crawlserv_threads SET status = ? WHERE id = ? LIMIT 1";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psSetThreadStatusMessage = this->preparedStatements.size();
		}
		
		// prepare strlen statement
		if(!(this->psStrlen)) {
			PreparedSqlStatement statement;
			statement.string = "SELECT LENGTH(?) AS l";
			statement.statement = this->connection->prepareStatement(statement.string);
			this->preparedStatements.push_back(statement);
			this->psStrlen = this->preparedStatements.size();
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "prepare() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();
		return false;
	}

	return true;
}

/*
 * LOGGING FUNCTIONS
 */

// add a log entry to the database
void Database::log(const std::string& logModule, const std::string& logEntry) {
	// check table lock
	if(this->tablesLocked) {
		std::cout << std::endl << "[WARNING] Logging not possible while a table is locked - re-routing to stdout:";
		std::cout << std::endl << " " << logModule << ": " << logEntry << std::flush;
		return;
	}

	// check prepared SQL statement
	if(!(this->psLog)) throw Database::Exception("Missing prepared SQL statement for Database::log(...)");
	sql::PreparedStatement * sqlStatement = this->preparedStatements.at(this->psLog - 1).statement;
	if(!sqlStatement) throw Database::Exception("Prepared SQL statement for Database::log(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	// add entry to database
	try {
		// execute SQL query
		if(logModule.empty()) sqlStatement->setString(1, "[unknown]");
		else sqlStatement->setString(1, logModule);
		if(logEntry.empty()) sqlStatement->setString(2, "[empty]");
		else sqlStatement->setString(2, logEntry);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "log() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") - " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
}

// get the number of log entries for a specific module from the database (or for all modules if logModule is an empty string)
unsigned long Database::getNumberOfLogEntries(const std::string& logModule) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	// create SQL query string
	std::string sqlQuery = "SELECT COUNT(*) FROM crawlserv_log";
	if(!logModule.empty()) sqlQuery += " WHERE module = ?";

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement(sqlQuery);

		// execute SQL statement
		if(!logModule.empty()) sqlStatement->setString(1, logModule);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64(1);

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getNumberOfLogEntries() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// remove the log entries of a specific module from the database (or all log entries if logModule is an empty string)
void Database::clearLogs(const std::string& logModule) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	// create SQL query string
	std::string sqlQuery = "DELETE FROM crawlserv_log";
	if(!logModule.empty()) sqlQuery += " WHERE module = ?";

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement(sqlQuery);

		// execute SQL statement
		if(!logModule.empty()) sqlStatement->setString(1, logModule);
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);

	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "clearLogs() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

/*
 * THREAD FUNCTIONS
 */

// get all threads from the database
std::vector<crawlservpp::Struct::ThreadDatabaseEntry> Database::getThreads() {
	sql::Statement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<crawlservpp::Struct::ThreadDatabaseEntry> result;

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL query
		sqlResultSet = sqlStatement->executeQuery("SELECT id, module, status, paused, website, urllist, config, last"
				" FROM crawlserv_threads");

		// get results
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next()) {
				crawlservpp::Struct::ThreadDatabaseEntry entry;
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
		}

		// delete results and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getThreads() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// add a thread to the database and return its new ID
unsigned long Database::addThread(const std::string& threadModule, const crawlservpp::Struct::ThreadOptions& threadOptions) {
	sql::PreparedStatement * sqlStatement = NULL;
	unsigned long result = 0;

	// check arguments
	if(threadModule.empty()) throw Database::Exception("addThread(): No thread module specified");
	if(!threadOptions.website) throw Database::Exception("addThread(): No website specified");
	if(!threadOptions.urlList) throw Database::Exception("addThread(): No URL list specified");
	if(!threadOptions.config) throw Database::Exception("addThread(): No configuration specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("INSERT INTO crawlserv_threads(module, website, urllist, config)"
				" VALUES (?, ?, ?, ?)");

		// execute SQL statement
		sqlStatement->setString(1, threadModule);
		sqlStatement->setUInt64(2, threadOptions.website);
		sqlStatement->setUInt64(3, threadOptions.urlList);
		sqlStatement->setUInt64(4, threadOptions.config);
		sqlStatement->execute();

		// get id
		result = this->getLastInsertedId();

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addThread() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get run time of thread (in seconds) from the database by its ID
unsigned long Database::getThreadRunTime(unsigned long threadId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check argument
	if(!threadId) throw Database::Exception("getThreadRunTime(): No thread ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT runtime FROM crawlserv_threads WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("runtime");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getThreadRunTime() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get pause time of thread (in seconds) from the database by its ID
unsigned long Database::getThreadPauseTime(unsigned long threadId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check argument
	if(!threadId) throw Database::Exception("getThreadPauseTime(): No thread ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT pausetime FROM crawlserv_threads WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("pausetime");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getThreadPauseTime() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// update thread status in the database (and add the pause state to the status message if necessary)
void Database::setThreadStatus(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage) {
	// check argument
	if(!threadId) throw Database::Exception("setThreadStatus(): No thread ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	// check prepared SQL statement
	if(!(this->psSetThreadStatus)) throw Database::Exception("Missing prepared SQL statement for Database::setThreadStatus(...)");
	sql::PreparedStatement * sqlStatement = this->preparedStatements.at(this->psSetThreadStatus - 1).statement;
	if(!sqlStatement) throw Database::Exception("Prepared SQL statement for Database::setThreadStatus(...) is NULL");

	// create status message
	std::string statusMessage;
	if(threadPaused) {
		if(!threadStatusMessage.empty()) statusMessage = "PAUSED " + threadStatusMessage;
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
		errorStrStr << "setThreadStatus() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
}

// update thread status in the database (without using or changing the pause state)
void Database::setThreadStatus(unsigned long threadId, const std::string& threadStatusMessage) {
	// check argument
	if(!threadId) throw Database::Exception("setThreadStatus(): No thread ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	// check prepared SQL statement
	if(!(this->psSetThreadStatusMessage))
		throw Database::Exception("Missing prepared SQL statement for Database::setThreadStatus(...)");
	sql::PreparedStatement * sqlStatement = this->preparedStatements.at(this->psSetThreadStatusMessage - 1).statement;
	if(!sqlStatement)
		throw Database::Exception("Prepared SQL statement for Database::setThreadStatus(...) is NULL");

	// create status message
	std::string statusMessage;
	statusMessage = threadStatusMessage;

	try {
		// execute SQL statement
		sqlStatement->setString(1, statusMessage);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadStatus() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
}

// set run time of thread (in seconds) in the database
void Database::setThreadRunTime(unsigned long threadId, unsigned long threadRunTime) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check argument
	if(!threadId) throw Database::Exception("setThreadRunTime(): No thread ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("UPDATE crawlserv_threads SET runtime = ? WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadRunTime);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadRunTime() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// set pause time of thread (in seconds) in the database
void Database::setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check argument
	if(!threadId) throw Database::Exception("setThreadPauseTime(): No thread ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("UPDATE crawlserv_threads SET pausetime = ? WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadPauseTime);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "setThreadPauseTime() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// remove thread from the database by its ID
void Database::deleteThread(unsigned long threadId) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check argument
	if(!threadId) throw Database::Exception("deleteThread(): No thread ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("DELETE FROM crawlserv_threads WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_threads")) this->resetAutoIncrement("crawlserv_threads");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteThread() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

/*
 * WEBSITE FUNCTIONS
 */

// add a website to the database and return its new ID
unsigned long Database::addWebsite(const std::string& websiteName, const std::string& websiteNamespace, const std::string& websiteDomain) {
	sql::PreparedStatement * addStatement = NULL;
	sql::Statement * createStatement = NULL;
	unsigned long result = 0;
	std::string timeStamp;

	// check arguments
	if(websiteName.empty()) throw Database::Exception("addWebsite(): No website name specified");
	if(websiteNamespace.empty()) throw Database::Exception("addWebsite(): No website namespace specified");
	if(websiteDomain.empty()) throw Database::Exception("addWebsite(): No domain specified");

	// check website namespace
	if(this->isWebsiteNamespace(websiteNamespace)) throw Database::Exception("Website namespace already exists");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for adding website
		addStatement =
				this->connection->prepareStatement("INSERT INTO crawlserv_websites(name, namespace, domain) VALUES (?, ?, ?)");

		// execute SQL statement for adding website
		addStatement->setString(1, websiteName);
		addStatement->setString(2, websiteNamespace);
		addStatement->setString(3, websiteDomain);
		addStatement->execute();

		// delete SQL statement for adding website
		MAIN_DATABASE_DELETE(addStatement);

		// get id
		result = this->getLastInsertedId();

		// create SQL statement for table creation
		createStatement = this->connection->createStatement();

		// delete SQL statement for table creation
		MAIN_DATABASE_DELETE(createStatement);

		// add default URL list
		this->addUrlList(result, "Default URL list", "default");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(addStatement);
		MAIN_DATABASE_DELETE(createStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addWebsite() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(addStatement);
		MAIN_DATABASE_DELETE(createStatement);
		throw;
	}

	return result;
}

// get website domain from the database by its ID
std::string Database::getWebsiteDomain(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check argument
	if(!websiteId) throw Database::Exception("getWebsiteDomain(): No website ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT domain FROM crawlserv_websites WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("domain");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getWebsiteDomain() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get namespace of website from the database by its ID
std::string Database::getWebsiteNamespace(unsigned long int websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check argument
	if(!websiteId) throw Database::Exception("getWebsiteNamespace(): No website ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT namespace FROM crawlserv_websites WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("namespace");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getWebsiteNamespace() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get ID and namespace of website from the database by URL list ID
std::pair<unsigned long, std::string> Database::getWebsiteNamespaceFromUrlList(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check argument
	if(!listId) throw Database::Exception("getWebsiteNamespaceFromUrlList(): No URL list ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_urllists WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getWebsiteNamespaceFromUrlList() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState()
				<< "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return std::pair<unsigned long, std::string>(websiteId, this->getWebsiteNamespace(websiteId));
}

// get ID and namespace of website from the database by configuration ID
std::pair<unsigned long, std::string> Database::getWebsiteNamespaceFromConfig(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check argument
	if(!configId) throw Database::Exception("getWebsiteNamespaceFromConfig(): No configuration ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_configs WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getWebsiteNamespaceFromConfig() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState()
				<< "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return std::pair<unsigned long, std::string>(websiteId, this->getWebsiteNamespace(websiteId));
}

// get ID and namespace of website from the database by parsing table ID
std::pair<unsigned long, std::string> Database::getWebsiteNamespaceFromParsedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check argument
	if(!tableId) throw Database::Exception("getWebsiteNamespaceFromParsedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_parsedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getWebsiteNamespaceFromParsedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState()
				<< "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return std::pair<unsigned long, std::string>(websiteId, this->getWebsiteNamespace(websiteId));
}

// get ID and namespace of website from the database by extracting table ID
std::pair<unsigned long, std::string> Database::getWebsiteNamespaceFromExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check argument
	if(!tableId) throw Database::Exception("getWebsiteNamespaceFromExtractedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_extractedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getWebsiteNamespaceFromExtractedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState()
				<< "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return std::pair<unsigned long, std::string>(websiteId, this->getWebsiteNamespace(websiteId));
}

// get ID and namespace of website from the database by analyzing table ID
std::pair<unsigned long, std::string> Database::getWebsiteNamespaceFromAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long websiteId = 0;

	// check argument
	if(!tableId) throw Database::Exception("getWebsiteNamespaceFromAnalyzedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT website FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) websiteId = sqlResultSet->getUInt64("website");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getWebsiteNamespaceFromAnalyzedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState()
				<< "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return std::pair<unsigned long, std::string>(websiteId, this->getWebsiteNamespace(websiteId));
}

// check whether website namespace exists in the database
bool Database::isWebsiteNamespace(const std::string& nameSpace) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check argument
	if(nameSpace.empty()) throw Database::Exception("isWebsiteNamespace(): No namespace specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS (SELECT 1 FROM crawlserv_websites WHERE namespace = ?"
				" LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setString(1, nameSpace);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isWebsiteNamespace() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): "	<< e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// create new website namespace for duplicated website
std::string Database::duplicateWebsiteNamespace(const std::string& websiteNamespace) {
	// check argument
	if(websiteNamespace.empty()) throw Database::Exception("duplicateWebsiteNamespace(): No namespace specified");

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

	if(!numberString.empty()) n = std::stoul(numberString, NULL);

	// check whether number needs to be incremented
	while(true) {
		// increment number at the end of the string
		std::ostringstream resultStrStr;
		n++;
		resultStrStr << nameString << n;
		result = resultStrStr.str();
		if(!(this->isWebsiteNamespace(result))) break;
	}
	return result;
}

// update website (and all associated tables) in the database
void Database::updateWebsite(unsigned long websiteId, const std::string& websiteName, const std::string& websiteNamespace,
		const std::string& websiteDomain) {
	sql::Statement * renameStatement = NULL;
	sql::PreparedStatement * updateStatement = NULL;

	// check arguments
	if(!websiteId) throw Database::Exception("updateWebsite(): No website ID specified");
	if(websiteName.empty()) throw Database::Exception("updateWebsite(): No website name specified");
	if(websiteNamespace.empty()) throw Database::Exception("updateWebsite(): No website namespace specified");
	if(websiteDomain.empty()) throw Database::Exception("updateWebsite(): No domain specified");

	// get old namespace
	std::string oldNamespace = this->getWebsiteNamespace(websiteId);

	// check website namespace if necessary
	if(websiteNamespace != oldNamespace)
		if(this->isWebsiteNamespace(websiteNamespace))
			throw Database::Exception("Webspace namespace already exists");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// check whether namespace has changed
		if(websiteNamespace != oldNamespace) {
			// create SQL statement for renaming
			renameStatement = this->connection->createStatement();

			// rename sub tables
			std::vector<std::pair<unsigned long, std::string>> urlLists = this->getUrlLists(websiteId);
			for(auto liI = urlLists.begin(); liI != urlLists.end(); ++liI) {
				renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "` RENAME TO "
						+ "`crawlserv_" + websiteNamespace + "_" + liI->second + "`");
				renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_crawled` RENAME TO "
						+ "`crawlserv_" + websiteNamespace + "_" + liI->second + "_crawled`");
				renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_links` RENAME TO "
						+ "`crawlserv_" + websiteNamespace + "_" + liI->second + "_links`");

				// rename parsing tables
				std::vector<std::pair<unsigned long, std::string>> parsedTables = this->getParsedTables(liI->first);
				for(auto taI = parsedTables.begin(); taI != parsedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_parsed_"
							+ taI->second + "` RENAME TO `" + "crawlserv_" + oldNamespace + "_" + liI->second + "_parsed_"
							+ taI->second + "`");
				}

				// rename extracting tables
				std::vector<std::pair<unsigned long, std::string>> extractedTables = this->getExtractedTables(liI->first);
				for(auto taI = extractedTables.begin(); taI != extractedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_extracted_"
							+ taI->second + "` RENAME TO `" + "crawlserv_" + oldNamespace + "_" + liI->second + "_extracted_"
							+ taI->second + "`");
				}

				// rename analyzing tables
				std::vector<std::pair<unsigned long, std::string>> analyzedTables = this->getAnalyzedTables(liI->first);
				for(auto taI = analyzedTables.begin(); taI != analyzedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_analyzed_"
							+ taI->second + "` RENAME TO `" + "crawlserv_" + oldNamespace + "_" + liI->second + "_analyzed_"
							+ taI->second + "`");
				}
			}

			// delete SQL statement for renaming
			MAIN_DATABASE_DELETE(renameStatement);

			// create SQL statement for updating
			updateStatement = this->connection->prepareStatement("UPDATE crawlserv_websites SET name = ?, namespace = ?,"
					" domain = ? WHERE id = ? LIMIT 1");

			// execute SQL statement for updating
			updateStatement->setString(1, websiteName);
			updateStatement->setString(2, websiteNamespace);
			updateStatement->setString(3, websiteDomain);
			updateStatement->setUInt64(4, websiteId);
			updateStatement->execute();

			// delete SQL statement for updating
			MAIN_DATABASE_DELETE(updateStatement);
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
			MAIN_DATABASE_DELETE(updateStatement);
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(renameStatement);
		MAIN_DATABASE_DELETE(updateStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "updateWebsite() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(renameStatement);
		MAIN_DATABASE_DELETE(updateStatement);
		throw;
	}
}

// delete website (and all associated data) from the database by its ID
void Database::deleteWebsite(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check argument
	if(!websiteId) throw Database::Exception("deleteWebsite(): No website ID specified");

	// get website namespace
	std::string websiteNamespace = this->getWebsiteNamespace(websiteId);

	try {
		// delete URL lists
		std::vector<std::pair<unsigned long, std::string>> urlLists = this->getUrlLists(websiteId);
		for(auto liI = urlLists.begin(); liI != urlLists.end(); ++liI) {
			// delete URL list
			this->deleteUrlList(liI->first);
		}

		// check connection
		if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

		// create SQL statement for deletion of website
		sqlStatement = this->connection->prepareStatement("DELETE FROM crawlserv_websites WHERE id = ? LIMIT 1");

		// execute SQL statements for deletion of website
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->execute();

		// delete SQL statements for deletion of website
		MAIN_DATABASE_DELETE(sqlStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_websites")) this->resetAutoIncrement("crawlserv_websites");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteWebsite() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// duplicate website in the database by its ID (no processed data will be duplicated)
unsigned long Database::duplicateWebsite(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check argument
	if(!websiteId) throw Database::Exception("duplicateWebsite(): No website ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for geting website info
		sqlStatement =
				this->connection->prepareStatement("SELECT name, namespace, domain FROM crawlserv_websites WHERE id = ? LIMIT 1");

		// execute SQL statement for geting website info
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			std::string websiteName = sqlResultSet->getString("name");
			std::string websiteNamespace =sqlResultSet->getString("namespace");
			std::string websiteDomain =sqlResultSet->getString("domain");

			// delete result and SQL statement for geting website info
			MAIN_DATABASE_DELETE(sqlResultSet);
			MAIN_DATABASE_DELETE(sqlStatement);

			// create new name and new namespace
			std::string newName = websiteName + " (copy)";
			std::string newNamespace = Database::duplicateWebsiteNamespace(websiteNamespace);

			// add website
			result = this->addWebsite(newName, newNamespace, websiteDomain);

			// create SQL statement for geting URL list info
			sqlStatement =
					this->connection->prepareStatement("SELECT name, namespace FROM crawlserv_urllists WHERE website = ?");

			// execute SQL statement for geting URL list info
			sqlStatement->setUInt64(1, websiteId);
			sqlResultSet = sqlStatement->executeQuery();

			// get results
			while(sqlResultSet && sqlResultSet->next()) {
				// add URL list
				std::string urlListName = sqlResultSet->getString("namespace");

				// add empty URL lists with same name
				if(urlListName != "default") this->addUrlList(result, urlListName, sqlResultSet->getString("namespace"));
			}

			// delete result and SQL statement for getting URL list info
			MAIN_DATABASE_DELETE(sqlResultSet);
			MAIN_DATABASE_DELETE(sqlStatement);

			// create SQL statement for getting queries
			sqlStatement = this->connection->prepareStatement("SELECT name, query, type, resultbool, resultsingle, resultmulti,"
					" textonly FROM crawlserv_queries WHERE website = ?");

			// execute SQL statement for getting queries
			sqlStatement->setUInt64(1, websiteId);
			sqlResultSet = sqlStatement->executeQuery();

			// get results
			while(sqlResultSet && sqlResultSet->next()) {
				// add query
				this->addQuery(result, sqlResultSet->getString("name"), sqlResultSet->getString("query"),
						sqlResultSet->getString("type"), sqlResultSet->getBoolean("resultbool"),
						sqlResultSet->getBoolean("resultsingle"), sqlResultSet->getBoolean("resultmulti"),
						sqlResultSet->getBoolean("textonly"));

			}

			// delete result and SQL statement for getting queries
			MAIN_DATABASE_DELETE(sqlResultSet);
			MAIN_DATABASE_DELETE(sqlStatement);

			// create SQL statement for getting configurations
			sqlStatement = this->connection->prepareStatement("SELECT module, name, config FROM crawlserv_configs WHERE website = ?");

			// execute SQL statement for getting configurations
			sqlStatement->setUInt64(1, websiteId);
			sqlResultSet = sqlStatement->executeQuery();

			// get results
			while(sqlResultSet && sqlResultSet->next()) {
				// add configuration
				this->addConfiguration(result, sqlResultSet->getString("module"), sqlResultSet->getString("name"),
						sqlResultSet->getString("config"));
			}
		}

		// delete result and SQL statement for getting configurations
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "duplicateWebsite() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

/*
 * URL LIST FUNCTIONS
 */

// add a URL list to the database and return its new ID
unsigned long Database::addUrlList(unsigned long websiteId, const std::string& listName, const std::string& listNamespace) {
	sql::PreparedStatement * addStatement = NULL;
	sql::Statement * createStatement = NULL;
	unsigned long result = 0;

	// check arguments
	if(!websiteId) throw Database::Exception("addUrlList(): No website ID specified");
	if(listName.empty()) throw Database::Exception("addUrlList(): No URL list name specified");
	if(listNamespace.empty()) throw Database::Exception("addUrlList(): No URL list namespace specified");

	// get website namespace
	std::string websiteNamespace = this->getWebsiteNamespace(websiteId);

	// check URL list namespace
	if(this->isUrlListNamespace(websiteId, listNamespace)) throw Database::Exception("URL list namespace already exists");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for adding URL list
		addStatement =
				this->connection->prepareStatement("INSERT INTO crawlserv_urllists(website, name, namespace) VALUES (?, ?, ?)");

		// execute SQL query for adding URL list
		addStatement->setUInt64(1, websiteId);
		addStatement->setString(2, listName);
		addStatement->setString(3, listNamespace);
		addStatement->execute();

		// delete SQL statement for adding URL list
		MAIN_DATABASE_DELETE(addStatement);

		// get id
		result = this->getLastInsertedId();

		// create SQL statement for table creation
		createStatement = this->connection->createStatement();

		// execute SQL queries for table creation
		createStatement->execute("CREATE TABLE IF NOT EXISTS `crawlserv_" + websiteNamespace + "_" + listNamespace
				+ "`(id SERIAL, manual BOOLEAN DEFAULT false NOT NULL, url VARCHAR(2000) NOT NULL,"
				" hash INT UNSIGNED DEFAULT 0 NOT NULL, crawled BOOLEAN DEFAULT false NOT NULL,"
				" parsed BOOLEAN DEFAULT false NOT NULL, extracted BOOLEAN DEFAULT false NOT NULL,"
				" analyzed BOOLEAN DEFAULT false NOT NULL, crawllock DATETIME DEFAULT NULL,"
				" parselock DATETIME DEFAULT NULL, extractlock DATETIME DEFAULT NULL, analyzelock DATETIME DEFAULT NULL,"
				" PRIMARY KEY(id), INDEX(hash)) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");
		createStatement->execute("CREATE TABLE IF NOT EXISTS `crawlserv_" + websiteNamespace + "_" + listNamespace
				+ "_crawled`(id SERIAL, url BIGINT UNSIGNED NOT NULL,"
				" crawltime DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP NOT NULL,"
				" archived BOOLEAN DEFAULT false NOT NULL, response SMALLINT UNSIGNED NOT NULL DEFAULT 0,"
				" type TINYTEXT NOT NULL, content LONGTEXT NOT NULL, PRIMARY KEY(id), FOREIGN KEY(url) REFERENCES crawlserv_"
				+ websiteNamespace + "_" + listNamespace + "(id) ON UPDATE RESTRICT ON DELETE CASCADE, INDEX(crawltime))"
				" CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ROW_FORMAT=COMPRESSED");
		createStatement->execute("CREATE TABLE IF NOT EXISTS `crawlserv_" + websiteNamespace + "_" + listNamespace + "_links`("
				"id SERIAL, fromurl BIGINT UNSIGNED NOT NULL, tourl BIGINT UNSIGNED NOT NULL,"
				" archived BOOLEAN DEFAULT FALSE NOT NULL, PRIMARY KEY(id), FOREIGN KEY(fromurl) REFERENCES crawlserv_"
				+ websiteNamespace + "_" + listNamespace + "(id) ON UPDATE RESTRICT ON DELETE CASCADE, "
				"FOREIGN KEY(tourl) REFERENCES crawlserv_" + websiteNamespace + "_"	+ listNamespace
				+ "(id) ON UPDATE RESTRICT ON DELETE CASCADE)");

		// delete SQL statement for table creation
		MAIN_DATABASE_DELETE(createStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(addStatement);
		MAIN_DATABASE_DELETE(createStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addUrlList() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(addStatement);
		MAIN_DATABASE_DELETE(createStatement);
		throw;
	}

	return result;
}

// get URL lists for ID-specified website from the database
std::vector<std::pair<unsigned long, std::string>> Database::getUrlLists(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<std::pair<unsigned long, std::string>> result;

	// check arguments
	if(!websiteId) throw Database::Exception("getUrlLists(): No website ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT id, namespace FROM crawlserv_urllists WHERE website = ?");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get results
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next())
				result.push_back(std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("namespace")));
		}

		// delete results and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlLists() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get namespace of URL list by its ID
std::string Database::getUrlListNamespace(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check argument
	if(!listId) throw Database::Exception("getUrlListNamespace(): No URL list ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT namespace FROM crawlserv_urllists WHERE id = ? LIMIT 1");

		// execute SQL query
		sqlStatement->setUInt64(1, listId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("namespace");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlListNamespace() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get ID and namespace of URL list from the database using a parsing table ID
std::pair<unsigned long, std::string> Database::getUrlListNamespaceFromParsedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long urlListId = 0;

	// check argument
	if(!tableId) throw Database::Exception("getUrlListNamespaceFromParsedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT urllist FROM crawlserv_parsedtables WHERE id = ? LIMIT 1");

		// execute SQL query
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) urlListId = sqlResultSet->getUInt64("urllist");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlListNamespaceFromParsedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState()
				<< "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return std::pair<unsigned long, std::string>(urlListId, this->getUrlListNamespace(urlListId));
}

// get ID and namespace of URL list from the database using an extracting table ID
std::pair<unsigned long, std::string> Database::getUrlListNamespaceFromExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long urlListId = 0;

	// check argument
	if(!tableId) throw Database::Exception("getUrlListNamespaceFromExtractedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT urllist FROM crawlserv_extractedtables WHERE id = ? LIMIT 1");

		// execute SQL query
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) urlListId = sqlResultSet->getUInt64("urllist");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlListNamespaceFromExtractedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState()
				<< "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return std::pair<unsigned long, std::string>(urlListId, this->getUrlListNamespace(urlListId));
}

// get ID and namespace of URL list from the database using an analyzing table ID
std::pair<unsigned long, std::string> Database::getUrlListNamespaceFromAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long urlListId = 0;

	// check argument
	if(!tableId) throw Database::Exception("getUrlListNamespaceFromAnalyzedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT urllist FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1");

		// execute SQL query
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) urlListId = sqlResultSet->getUInt64("urllist");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlListNamespaceFromAnalyzedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState()
				<< "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return std::pair<unsigned long, std::string>(urlListId, this->getUrlListNamespace(urlListId));
}

// check whether a URL list namespace for an ID-specified website exists in the database
bool Database::isUrlListNamespace(unsigned long websiteId, const std::string& nameSpace) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check arguments
	if(!websiteId) throw Database::Exception("isUrlListNamespace(): No website ID specified");
	if(nameSpace.empty()) throw Database::Exception("isUrlListNamespace(): No namespace specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS (SELECT 1 FROM crawlserv_urllists"
				" WHERE website = ? AND namespace = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setString(2, nameSpace);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlListNamespace() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// update URL list (and all associated tables) in the database
void Database::updateUrlList(unsigned long listId, const std::string& listName, const std::string& listNamespace) {
	sql::Statement * renameStatement = NULL;
	sql::PreparedStatement * updateStatement = NULL;

	// check arguments
	if(!listId) throw Database::Exception("updateUrlList(): No website ID specified");
	if(listName.empty()) throw Database::Exception("updateUrlList(): No URL list name specified");
	if(listNamespace.empty()) throw Database::Exception("updateUrlList(): No URL list namespace specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string oldListNamespace = this->getUrlListNamespace(listId);

	// check website namespace if necessary
	if(listNamespace != oldListNamespace)
		if(this->isUrlListNamespace(websiteNamespace.first, listNamespace))
			throw Database::Exception("Webspace namespace already exists");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		if(listNamespace != oldListNamespace) {
			// create SQL statement for renaming
			renameStatement = this->connection->createStatement();

			// rename main tables
			renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
					+ "` RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "`");
			renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
					+ "_crawled` RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_crawled`");
			renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
					+ "_links` RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_links`");

			// rename parsing tables
			std::vector<std::pair<unsigned long, std::string>> parsedTables = this->getParsedTables(listId);
			for(auto taI = parsedTables.begin(); taI != parsedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
						+ "_parsed_" + taI->second + "` RENAME TO `crawlserv_" + websiteNamespace.second + "_"
						+ listNamespace	+ "_parsed_" + taI->second + "`");
			}

			// rename extracting tables
			std::vector<std::pair<unsigned long, std::string>> extractedTables = this->getExtractedTables(listId);
			for(auto taI = extractedTables.begin(); taI != extractedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
						+ "_extracted_"	+ taI->second + "` RENAME TO `crawlserv_" + websiteNamespace.second + "_"
						+ listNamespace + "_extracted_"	+ taI->second + "`");
			}

			// rename analyzing tables
			std::vector<std::pair<unsigned long, std::string>> analyzedTables = this->getAnalyzedTables(listId);
			for(auto taI = analyzedTables.begin(); taI != analyzedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
						+ "_analyzed_" + taI->second + "` RENAME TO `crawlserv_" + websiteNamespace.second + "_"
						+ listNamespace + "_analyzed_" + taI->second + "`");
			}

			// delete SQL statement for renaming
			MAIN_DATABASE_DELETE(renameStatement);

			// create SQL statement for updating
			updateStatement = this->connection->prepareStatement("UPDATE crawlserv_urllists SET name = ?, namespace = ?"
					" WHERE id = ? LIMIT 1");

			// execute SQL statement for updating
			updateStatement->setString(1, listName);
			updateStatement->setString(2, listNamespace);
			updateStatement->setUInt64(3, listId);
			updateStatement->execute();

			// delete SQL statement for updating
			MAIN_DATABASE_DELETE(updateStatement);
		}
		else {
			// create SQL statement for updating
			updateStatement = this->connection->prepareStatement("UPDATE crawlserv_urllists SET name = ? WHERE id = ? LIMIT 1");

			// execute SQL statement for updating
			updateStatement->setString(1, listName);
			updateStatement->setUInt64(2, listId);
			updateStatement->execute();

			// delete SQL statement for updating
			MAIN_DATABASE_DELETE(updateStatement);
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(renameStatement);
		MAIN_DATABASE_DELETE(updateStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "updateUrlList() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(renameStatement);
		MAIN_DATABASE_DELETE(updateStatement);
		throw;
	}
}

// delete URL list (and all associated data) from the database by its ID
void Database::deleteUrlList(unsigned long listId) {
	sql::PreparedStatement * deleteStatement = NULL;
	sql::Statement * dropStatement = NULL;

	// check argument
	if(!listId) throw Database::Exception("deleteUrlList(): No URL list ID specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string listNamespace = this->getUrlListNamespace(listId);

	try {
		// delete parsing tables
		std::vector<std::pair<unsigned long, std::string>> parsedTables = this->getParsedTables(listId);
		for(auto taI = parsedTables.begin(); taI != parsedTables.end(); ++taI)
			this->deleteParsedTable(taI->first);

		// delete extracting tables
		std::vector<std::pair<unsigned long, std::string>> extractedTables = this->getExtractedTables(listId);
		for(auto taI = extractedTables.begin(); taI != extractedTables.end(); ++taI)
			this->deleteParsedTable(taI->first);

		// delete analyzing tables
		std::vector<std::pair<unsigned long, std::string>> analyzedTables = this->getAnalyzedTables(listId);
		for(auto taI = analyzedTables.begin(); taI != analyzedTables.end(); ++taI)
			this->deleteParsedTable(taI->first);

		// check connection
		if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

		// create SQL statement for deleting URL list
		deleteStatement = this->connection->prepareStatement("DELETE FROM crawlserv_urllists WHERE id = ? LIMIT 1");

		// execute SQL statement for deleting URL list
		deleteStatement->setUInt64(1, listId);
		deleteStatement->execute();

		// delete SQL statement for deleting URL list
		MAIN_DATABASE_DELETE(deleteStatement);

		// create SQL statement for dropping tables
		dropStatement = this->connection->createStatement();

		// execute SQL queries for dropping tables
		dropStatement->execute("DROP TABLE IF EXISTS `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_links`");
		dropStatement->execute("DROP TABLE IF EXISTS `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_crawled`");
		dropStatement->execute("DROP TABLE IF EXISTS `crawlserv_" + websiteNamespace.second + "_" + listNamespace + "`");

		// delete SQL statement for dropping tables
		MAIN_DATABASE_DELETE(dropStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_urllists")) this->resetAutoIncrement("crawlserv_urllists");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(deleteStatement);
		MAIN_DATABASE_DELETE(dropStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteUrlList() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(deleteStatement);
		MAIN_DATABASE_DELETE(dropStatement);
		throw;
	}
}

// reset parsing status of ID-specified URL list
void Database::resetParsingStatus(unsigned long listId) {
	// check argument
	if(!listId) throw Database::Exception("resetParsingStatus(): No URL list ID specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string listNamespace = this->getUrlListNamespace(listId);

	// lock URL list
	this->lockTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace);

	try {
		// update URL list
		this->execute("UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace
				+ "` SET parsed = FALSE, parselock = NULL");
	}
	catch(...) {
		// any exeption: try to release table lock and re-throw
		try { this->releaseLocks(); }
		catch(...) {}
		throw;
	}

	// release lock
	this->releaseLocks();
}

// reset extracting status of ID-specified URL list
void Database::resetExtractingStatus(unsigned long listId) {
	// check argument
	if(!listId) throw Database::Exception("resetExtractingStatus(): No URL list ID specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string listNamespace = this->getUrlListNamespace(listId);

	// lock URL list
	this->lockTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace);

	try {
		// update URL list
		this->execute("UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace
				+ "` SET extracted = FALSE, extractlock = NULL");
	}
	catch(...) {
		// any exeption: try to release table lock and re-throw
		try { this->releaseLocks(); }
		catch(...) {}
		throw;
	}

	// release lock
	this->releaseLocks();
}

// reset analyzing status of ID-specified URL list
void Database::resetAnalyzingStatus(unsigned long listId) {
	// check argument
	if(!listId) throw Database::Exception("resetAnalyzingStatus(): No URL list ID specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string listNamespace = this->getUrlListNamespace(listId);

	// lock URL list
	this->lockTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace);

	try {
		// update URL list
		this->execute("UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace
				+ "` SET analyzed = FALSE, analyzelock = NULL");
	}
	catch(...) {
		// any exeption: try to release table lock and re-throw
		try { this->releaseLocks(); }
		catch(...) {}
		throw;
	}

	// release lock
	this->releaseLocks();
}

/*
 * QUERY FUNCTIONS
 */

// add a query to the database and return its new ID (add global query when websiteId is zero)
unsigned long Database::addQuery(unsigned long websiteId, const std::string& queryName, const std::string& queryText,
		const std::string& queryType, bool queryResultBool, bool queryResultSingle, bool queryResultMulti, bool queryTextOnly) {
	sql::PreparedStatement * sqlStatement = NULL;
	unsigned long result = 0;

	// check arguments
	if(queryName.empty()) throw Database::Exception("addQuery(): No query name specified");
	if(queryText.empty()) throw Database::Exception("addQuery(): No query text specified");
	if(queryType.empty()) throw Database::Exception("addQuery(): No query type specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

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
		MAIN_DATABASE_DELETE(sqlStatement);

		// get id
		result = this->getLastInsertedId();
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addQuery() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get the properties of a query from the database by its ID
void Database::getQueryProperties(unsigned long queryId, std::string& queryTextTo, std::string& queryTypeTo, bool& queryResultBoolTo,
		bool& queryResultSingleTo, bool& queryResultMultiTo, bool& queryTextOnlyTo) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;

	// check argument
	if(!queryId) throw Database::Exception("getQueryProperties(): No query ID specified");

	// check ID
	if(!queryId) {
		queryTextTo = "";
		queryTypeTo = "";
		queryResultBoolTo = false;
		queryResultSingleTo = false;
		queryResultMultiTo = false;
		queryTextOnlyTo = false;
		return;
	}

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT query, type, resultbool, resultsingle, resultmulti, textonly"
				" FROM crawlserv_queries WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			queryTextTo = sqlResultSet->getString("query");
			queryTypeTo = sqlResultSet->getString("type");
			queryResultBoolTo = sqlResultSet->getBoolean("resultbool");
			queryResultSingleTo = sqlResultSet->getBoolean("resultsingle");
			queryResultMultiTo = sqlResultSet->getBoolean("resultmulti");
			queryTextOnlyTo = sqlResultSet->getBoolean("textonly");
		}
		else {
			queryTextTo = "";
			queryTypeTo = "";
			queryResultBoolTo = false;
			queryResultSingleTo = false;
			queryResultMultiTo = false;
			queryTextOnlyTo = false;
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getQueryProperties() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// edit query in the database
void Database::updateQuery(unsigned long queryId, const std::string& queryName, const std::string& queryText,
		const std::string& queryType, bool queryResultBool, bool queryResultSingle, bool queryResultMulti, bool queryTextOnly) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check arguments
	if(!queryId) throw Database::Exception("updateQuery(): No query ID specified");
	if(queryName.empty()) throw Database::Exception("updateQuery(): No query name specified");
	if(queryText.empty()) throw Database::Exception("updateQuery(): No query text specified");
	if(queryType.empty()) throw Database::Exception("updateQuery(): No query type specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

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
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "updateQuery() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// delete query from the database by its ID
void Database::deleteQuery(unsigned long queryId) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check argument
	if(!queryId) throw Database::Exception("deleteQuery(): No query ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("DELETE FROM crawlserv_queries WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_queries")) this->resetAutoIncrement("crawlserv_queries");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteQuery() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// duplicate query in the database by its ID
unsigned long Database::duplicateQuery(unsigned long queryId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check argument
	if(!queryId) throw Database::Exception("duplicateQuery(): No query ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for getting query info
		sqlStatement = this->connection->prepareStatement("SELECT website, name, query, type, resultbool, resultsingle,"
				" resultmulti, textonly FROM crawlserv_queries WHERE id = ? LIMIT 1");

		// execute SQL statement for getting query info
		sqlStatement->setUInt64(1, queryId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			// add query
			result = this->addQuery(sqlResultSet->getUInt64("website"), sqlResultSet->getString("name") + " (copy)",
					sqlResultSet->getString("query"), sqlResultSet->getString("type"), sqlResultSet->getBoolean("resultbool"),
					sqlResultSet->getBoolean("resultsingle"), sqlResultSet->getBoolean("resultmulti"),
					sqlResultSet->getBoolean("textonly"));
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "duplicateQuery() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

/*
 * CONFIGURATION FUNCTIONS
 */

// add a configuration to the database and return its new ID (add global configuration when websiteId is zero)
unsigned long Database::addConfiguration(unsigned long websiteId, const std::string& configModule, const std::string& configName,
		const std::string& config) {
	sql::PreparedStatement * sqlStatement = NULL;
	unsigned long result = 0;

	// check arguments
	if(configModule.empty()) throw Database::Exception("addConfiguration(): No configuration module specified");
	if(configName.empty()) throw Database::Exception("addConfiguration(): No configuration name specified");
	if(config.empty()) throw Database::Exception("addConfiguration(): No configuration specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for adding configuration
		sqlStatement = this->connection->prepareStatement("INSERT INTO crawlserv_configs(website, module, name, config) VALUES (?, ?, ?, ?)");

		// execute SQL query for adding website
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setString(2, configModule);
		sqlStatement->setString(3, configName);
		sqlStatement->setString(4, config);
		sqlStatement->execute();

		// delete SQL statement for adding website
		MAIN_DATABASE_DELETE(sqlStatement);

		// get id
		result = this->getLastInsertedId();
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addConfiguration() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get a configuration from the database by its ID
const std::string Database::getConfiguration(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check argument
	if(!configId) throw Database::Exception("getConfiguration(): No configuration ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT config FROM crawlserv_configs WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("config");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getConfiguration() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// update configuration in the database
void Database::updateConfiguration(unsigned long configId, const std::string& configName, const std::string& config) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check arguments
	if(!configId) throw Database::Exception("updateConfiguration(): No configuration ID specified");
	if(configName.empty()) throw Database::Exception("updateConfiguration(): No configuration name specified");
	if(config.empty()) throw Database::Exception("updateConfiguration(): No configuration specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

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
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "updateConfiguration() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// delete configuration from the database by its ID
void Database::deleteConfiguration(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check argument
	if(!configId) throw Database::Exception("deleteConfiguration(): No configuration ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("DELETE FROM crawlserv_configs WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_configs")) this->resetAutoIncrement("crawlserv_configs");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteConfiguration() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// duplicate configuration in the database by its ID
unsigned long Database::duplicateConfiguration(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check argument
	if(!configId) throw Database::Exception("duplicateConfiguration(): No configuration ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for getting configuration info
		sqlStatement = this->connection->prepareStatement("SELECT website, module, name, config FROM crawlserv_configs"
				" WHERE id = ? LIMIT 1");

		// execute SQL statement for getting configuration info
		sqlStatement->setUInt64(1, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			// add configuration
			result = this->addConfiguration(sqlResultSet->getUInt64("website"), sqlResultSet->getString("module"),
					sqlResultSet->getString("name") + " (copy)", sqlResultSet->getString("config"));
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "duplicateConfiguration() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

/*
 * TABLE INDEXING FUNCTIONS
 */

// add a parsed table to the database if a such a table does not exist already
void Database::addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;

	// check arguments
	if(!websiteId) throw Database::Exception("addParsedTable(): No website ID specified");
	if(!listId) throw Database::Exception("addParsedTable(): No URL list ID specified");
	if(tableName.empty()) throw Database::Exception("addParsedTable(): No table name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for checking for entry
		sqlStatement = this->connection->prepareStatement("SELECT COUNT(id) AS result FROM crawlserv_parsedtables"
				" WHERE website = ? AND urllist = ? AND name = ? LIMIT 1");

		// execute SQL statement for checking for entry
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, listId);
		sqlStatement->setString(3, tableName);
		sqlResultSet = sqlStatement->executeQuery();

		if(sqlResultSet && sqlResultSet->next() && !(sqlResultSet->getBoolean("result"))) {
			// delete result and SQL statement
			MAIN_DATABASE_DELETE(sqlResultSet);
			MAIN_DATABASE_DELETE(sqlStatement);

			// entry does not exist already: create SQL statement for adding table
			sqlStatement =
					this->connection->prepareStatement("INSERT INTO crawlserv_parsedtables(website, urllist, name)"
							" VALUES (?, ?, ?)");

			// execute SQL statement for adding table
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setUInt64(2, listId);
			sqlStatement->setString(3, tableName);
			sqlStatement->execute();
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addParsedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// get parsed tables for an ID-specified URL list from the database
std::vector<std::pair<unsigned long, std::string>> Database::getParsedTables(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<std::pair<unsigned long, std::string>> result;

	// check argument
	if(!listId) throw Database::Exception("getParsedTables(): No URL list ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT id, name FROM crawlserv_parsedtables WHERE urllist = ?");

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		sql::ResultSet * sqlResultSet = sqlStatement->executeQuery();

		// get results
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next())
				result.push_back(std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("name")));
		}

		// delete results and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getParsedTables() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get the name of a parsing table from the database by its ID
std::string Database::getParsedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check argument
	if(!tableId) throw Database::Exception("getParsedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT name FROM crawlserv_parsedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("name");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getParsedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// delete parsing table from the database by its ID
void Database::deleteParsedTable(unsigned long tableId) {
	sql::PreparedStatement * deleteStatement = NULL;
	sql::Statement * dropStatement = NULL;

	// get namespace, URL list name and table name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromParsedTable(tableId);
	std::pair<unsigned long, std::string> listNamespace = this->getUrlListNamespaceFromParsedTable(tableId);
	std::string tableName = this->getParsedTable(tableId);

	// check argument
	if(!tableId) throw Database::Exception("deleteParsedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for deletion
		deleteStatement =
				this->connection->prepareStatement("DELETE FROM crawlserv_parsedtables WHERE id = ? LIMIT 1");

		// execute SQL statement for deletion
		deleteStatement->setUInt64(1, tableId);
		deleteStatement->execute();

		// delete SQL statement for deletion
		MAIN_DATABASE_DELETE(deleteStatement);

		// create SQL statement for dropping table
		dropStatement = this->connection->createStatement();

		// execute SQL query for dropping table
		dropStatement->execute("DROP TABLE IF EXISTS `crawlserv_" + websiteNamespace.second + "_" + listNamespace.second
				+ "_parsed_" + tableName + "`");

		// delete SQL statement for dropping table
		MAIN_DATABASE_DELETE(dropStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_parsedtables")) this->resetAutoIncrement("crawlserv_parsedtables");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(deleteStatement);
		MAIN_DATABASE_DELETE(dropStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteParsedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(deleteStatement);
		MAIN_DATABASE_DELETE(dropStatement);
		throw;
	}
}

// add an extracted table to the database if such a table does not exist already
void Database::addExtractedTable(unsigned long websiteId, unsigned long listId,
		const std::string& tableName) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;

	// check arguments
	if(!websiteId) throw Database::Exception("addExtractedTable(): No website ID specified");
	if(!listId) throw Database::Exception("addExtractedTable(): No URL list ID specified");
	if(tableName.empty()) throw Database::Exception("addExtractedTable(): No table name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for checking for entry
		sqlStatement = this->connection->prepareStatement("SELECT COUNT(id) AS result FROM crawlserv_extractedtables"
				" WHERE website = ? AND urllist = ? AND name = ? LIMIT 1");

		// execute SQL statement for checking for entry
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, listId);
		sqlStatement->setString(3, tableName);
		sqlResultSet = sqlStatement->executeQuery();

		if(sqlResultSet && sqlResultSet->next() && !(sqlResultSet->getBoolean("result"))) {
			// delete result and SQL statement
			MAIN_DATABASE_DELETE(sqlResultSet);
			MAIN_DATABASE_DELETE(sqlStatement);

			// create SQL statement for adding table
			sqlStatement = this->connection->prepareStatement("INSERT INTO crawlserv_extractedtables(website, urllist, name)"
					" VALUES (?, ?, ?)");

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setUInt64(2, listId);
			sqlStatement->setString(3, tableName);
			sqlStatement->execute();
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addExtractedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// get the extracted tables for an ID-specified URL list from the database
std::vector<std::pair<unsigned long, std::string>> Database::getExtractedTables(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<std::pair<unsigned long, std::string>> result;

	// check argument
	if(!listId) throw Database::Exception("getExtractedTables(): No URL list ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT id, name FROM crawlserv_extractedtables WHERE urllist = ?");

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		sql::ResultSet * sqlResultSet = sqlStatement->executeQuery();

		// get results
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next())
				result.push_back(std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("name")));
		}

		// delete results and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getExtractedTables() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get the name of an extracting table from the database by its ID
std::string Database::getExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check argument
	if(!tableId) throw Database::Exception("getExtractedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT name FROM crawlserv_extractedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("name");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getExtractedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// delete an extracting table from the database by its ID
void Database::deleteExtractedTable(unsigned long tableId) {
	sql::PreparedStatement * deleteStatement = NULL;
	sql::Statement * dropStatement = NULL;

	// check argument
	if(!tableId) throw Database::Exception("deleteExtractedTable(): No table ID specified");

	// get namespace, URL list name and table name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromExtractedTable(tableId);
	std::pair<unsigned long, std::string> listNamespace = this->getUrlListNamespaceFromExtractedTable(tableId);
	std::string tableName = this->getExtractedTable(tableId);

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for deletion
		deleteStatement =
				this->connection->prepareStatement("DELETE FROM crawlserv_extractedtables WHERE id = ? LIMIT 1");

		// execute SQL statement for deletion
		deleteStatement->setUInt64(1, tableId);
		deleteStatement->execute();

		// delete SQL statement for deletion
		MAIN_DATABASE_DELETE(deleteStatement);

		// create SQL statement for dropping table
		dropStatement = this->connection->createStatement();

		// execute SQL query for dropping table
		dropStatement->execute("DROP TABLE IF EXISTS `crawlserv_" + websiteNamespace.second + "_" + listNamespace.second
				+ "_extracted_"	+ tableName + "`");

		// delete SQL statement for dropping table
		MAIN_DATABASE_DELETE(dropStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_extractedtables")) this->resetAutoIncrement("crawlserv_extractedtables");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(deleteStatement);
		MAIN_DATABASE_DELETE(dropStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteExtractedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(deleteStatement);
		MAIN_DATABASE_DELETE(dropStatement);
		throw;
	}
}

// add an analyzed table to the database if such a table does not exist already
void Database::addAnalyzedTable(unsigned long websiteId, unsigned long listId,
		const std::string& tableName) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;

	// check arguments
	if(!websiteId) throw Database::Exception("addAnalyzedTable(): No website ID specified");
	if(!listId) throw Database::Exception("addAnalyzedTable(): No URL list ID specified");
	if(tableName.empty()) throw Database::Exception("addAnalyzedTable(): No table name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for checking for entry
		sqlStatement = this->connection->prepareStatement("SELECT COUNT(id) AS result FROM crawlserv_analyzedtables"
				" WHERE website = ? AND urllist = ? AND name = ? LIMIT 1");

		// execute SQL statement for checking for entry
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, listId);
		sqlStatement->setString(3, tableName);
		sqlResultSet = sqlStatement->executeQuery();

		if(sqlResultSet && sqlResultSet->next() && !(sqlResultSet->getBoolean("result"))) {
			// delete result and SQL statement
			MAIN_DATABASE_DELETE(sqlResultSet);
			MAIN_DATABASE_DELETE(sqlStatement);

			// create SQL statement for adding table
			sqlStatement =
					this->connection->prepareStatement("INSERT INTO crawlserv_analyzedtables(website, urllist, name)"
							" VALUES (?, ?, ?)");

			// execute SQL statement
			sqlStatement->setUInt64(1, websiteId);
			sqlStatement->setUInt64(2, listId);
			sqlStatement->setString(3, tableName);
			sqlStatement->execute();
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addAnalyzedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// get analyzed tables for an ID-specified URL list from the database
std::vector<std::pair<unsigned long, std::string>> Database::getAnalyzedTables(unsigned long listId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<std::pair<unsigned long, std::string>> result;

	// check argument
	if(!listId) throw Database::Exception("getAnalyzedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT id, name FROM crawlserv_analyzedtables WHERE urllist = ?");

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		sql::ResultSet * sqlResultSet = sqlStatement->executeQuery();

		// get results
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next())
				result.push_back(std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("name")));
		}

		// delete results and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getAnalyzedTables() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// get the name of an analyzing table from the database by its ID
std::string Database::getAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check argument
	if(!tableId) throw Database::Exception("getAnalyzedTable(): No table ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT name FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1");

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("name");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getAnalyzedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// delete an analyzing table from the database by its ID
void Database::deleteAnalyzedTable(unsigned long tableId) {
	sql::PreparedStatement * deleteStatement = NULL;
	sql::Statement * dropStatement = NULL;

	// check argument
	if(!tableId) throw Database::Exception("deleteAnalyzedTable(): No table ID specified");

	// get namespace, URL list name and table name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromAnalyzedTable(tableId);
	std::pair<unsigned long, std::string> listNamespace = this->getUrlListNamespaceFromAnalyzedTable(tableId);
	std::string tableName = this->getParsedTable(tableId);

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement for deletion
		deleteStatement =
				this->connection->prepareStatement("DELETE FROM crawlserv_analyzedtables WHERE id = ? LIMIT 1");

		// execute SQL statement for deletion
		deleteStatement->setUInt64(1, tableId);
		deleteStatement->execute();

		// delete SQL statement for deletion
		MAIN_DATABASE_DELETE(deleteStatement);

		// create SQL statement for dropping table
		dropStatement = this->connection->createStatement();

		// execute SQL query for dropping table
		dropStatement->execute("DROP TABLE IF EXISTS `crawlserv_" + websiteNamespace.second + "_" + listNamespace.second
				+ "_analyzed_" + tableName + "`");

		// delete SQL statement for dropping table
		MAIN_DATABASE_DELETE(dropStatement);

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_analyzedtables")) this->resetAutoIncrement("crawlserv_analyzedtables");
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(deleteStatement);
		MAIN_DATABASE_DELETE(dropStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteAnalyzedTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(deleteStatement);
		MAIN_DATABASE_DELETE(dropStatement);
		throw;
	}
}

/*
 * TABLE LOCKING FUNCTION
 */

// release table locks in the database (if necessary)
void Database::releaseLocks() {
	if(this->tablesLocked) this->unlockTables();
}

/*
 * VALIDATION FUNCTIONS
 */

// check whether a website ID is valid
bool Database::isWebsite(unsigned long websiteId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check argument
	if(!websiteId) throw Database::Exception("isWebsite(): No website ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_websites WHERE id = ? LIMIT 1)"
						" AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isWebsite() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// check whether a URL list ID is valid
bool Database::isUrlList(unsigned long urlListId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check argument
	if(!urlListId) throw Database::Exception("isUrlList(): No URL list ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_urllists WHERE id = ? LIMIT 1)"
						" AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, urlListId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlList() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// check whether a URL list ID is valid for the ID-specified website
bool Database::isUrlList(unsigned long websiteId, unsigned long urlListId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check arguments
	if(!websiteId) throw Database::Exception("isUrlList(): No website ID specified");
	if(!urlListId) throw Database::Exception("isUrlList(): No URL list ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_urllists WHERE website = ?"
				" AND id = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, urlListId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlList() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// check whether a query ID is valid
bool Database::isQuery(unsigned long queryId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check argument
	if(!queryId) throw Database::Exception("isQuery(): No query ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_queries WHERE id = ? LIMIT 1)"
						" AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isQuery() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// check whether a query ID is valid for the ID-specified website (look for global queries if websiteID is zero)
bool Database::isQuery(unsigned long websiteId, unsigned long queryId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check arguments
	if(!queryId) throw Database::Exception("isQuery(): No query ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_queries"
				" WHERE (website = ? OR website = 0) AND id = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, queryId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isQuery() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// check whether a configuration ID is valid
bool Database::isConfiguration(unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check argument
	if(!configId) throw Database::Exception("isConfiguration(): No configuration ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement =
				this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_configs WHERE id = ? LIMIT 1)"
						" AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isConfiguration() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// check whether a configuration ID is valid for the ID-specified website (look for global configurations if websiteId is zero)
bool Database::isConfiguration(unsigned long websiteId, unsigned long configId) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check arguments
	if(!configId) throw Database::Exception("isConfiguration(): No configuration ID specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT EXISTS(SELECT 1 FROM crawlserv_configs WHERE website = ?"
				" AND id = ? LIMIT 1) AS result");

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, configId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isConfiguration() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

/*
 * CUSTOM DATA FUNCTIONS FOR ALGORITHMS
 */

// get one custom value from one field of a row in the database
void Database::getCustomData(Data::GetValue& data) {
	sql::ResultSet * sqlResultSet = NULL;
	sql::Statement * sqlStatement = NULL;

	// check argument
	if(data.column.empty()) throw Database::Exception("getCustomData(): No column name specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown) throw Database::Exception("getCustomData(): No column type specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery("SELECT `" + data.column + "` FROM `" + data.table + "` WHERE (" + data.condition + ")");

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			if(sqlResultSet->isNull(data.column)) data.value = Data::Value();
			else {
				switch(data.type) {
				case Data::Type::_bool:
					data.value = Data::Value(sqlResultSet->getBoolean(data.column));
					break;
				case Data::Type::_double:
					data.value = Data::Value((double) sqlResultSet->getDouble(data.column));
					break;
				case Data::Type::_int:
					data.value = Data::Value((int) sqlResultSet->getInt(data.column));
					break;
				case Data::Type::_long:
					data.value = Data::Value((long) sqlResultSet->getInt64(data.column));
					break;
				case Data::Type::_string:
					data.value = Data::Value(sqlResultSet->getString(data.column));
					break;
				case Data::Type::_uint:
					data.value = Data::Value((unsigned int) sqlResultSet->getUInt(data.column));
					break;
				case Data::Type::_ulong:
					data.value = Data::Value((unsigned long) sqlResultSet->getUInt64(data.column));
					break;
				default:
					throw Database::Exception("getCustomData(): Invalid data type when getting custom data.");
				}
			}
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// get custom values from multiple fields of a row in the database
void Database::getCustomData(Data::GetFields& data) {
	sql::ResultSet * sqlResultSet = NULL;
	sql::Statement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.columns.empty()) throw Database::Exception("getCustomData(): No column names specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown) throw Database::Exception("getCustomData(): No column type specified");
	data.values.clear();
	data.values.reserve(data.columns.size());

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// create SQL query
		sqlQuery = "SELECT ";
		for(auto i = data.columns.begin(); i != data.columns.end(); ++i) {
			sqlQuery += "`" + *i + "`, ";
		}
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " FROM `" + data.table + "` WHERE (" + data.condition + ")";

		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery(sqlQuery);

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			switch(data.type) {
			case Data::Type::_bool:
				for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
					if(sqlResultSet->isNull(*i)) data.values.push_back(Data::Value());
					else data.values.push_back(Data::Value(sqlResultSet->getBoolean(*i)));
				break;
			case Data::Type::_double:
				for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
					if(sqlResultSet->isNull(*i)) data.values.push_back(Data::Value());
					else data.values.push_back(Data::Value((double) sqlResultSet->getDouble(*i)));
				break;
			case Data::Type::_int:
				for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
					if(sqlResultSet->isNull(*i)) data.values.push_back(Data::Value());
					else data.values.push_back(Data::Value((int) sqlResultSet->getInt(*i)));
				break;
			case Data::Type::_long:
				for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
					if(sqlResultSet->isNull(*i)) data.values.push_back(Data::Value());
					else data.values.push_back(Data::Value((long) sqlResultSet->getInt64(*i)));
				break;
			case Data::Type::_string:
				for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
					if(sqlResultSet->isNull(*i)) data.values.push_back(Data::Value());
					else data.values.push_back(Data::Value(sqlResultSet->getString(*i)));
				break;
			case Data::Type::_uint:
				for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
					if(sqlResultSet->isNull(*i)) data.values.push_back(Data::Value());
					else data.values.push_back(Data::Value((unsigned int) sqlResultSet->getUInt(*i)));
				break;
			case Data::Type::_ulong:
				for(auto i = data.columns.begin(); i != data.columns.end(); ++i)
					if(sqlResultSet->isNull(*i)) data.values.push_back(Data::Value());
					else data.values.push_back(Data::Value((unsigned long) sqlResultSet->getUInt64(*i)));
				break;
			default:
				throw Database::Exception("getCustomData(): Invalid data type when getting custom data.");
			}
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// get custom values from multiple fields of a row with different types in the database
void Database::getCustomData(Data::GetFieldsMixed& data) {
	sql::ResultSet * sqlResultSet = NULL;
	sql::Statement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.columns_types.empty()) throw Database::Exception("getCustomData(): No columns specified");
	data.values.clear();
	data.values.reserve(data.columns_types.size());

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// create SQL query
		sqlQuery = "SELECT ";
		for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
			sqlQuery += "`" + i->first + "`, ";
		}
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " FROM `" + data.table + "` WHERE (" + data.condition + ")";

		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery(sqlQuery);

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
				if(sqlResultSet->isNull(i->first)) data.values.push_back(Data::Value());
				else {
					switch(i->second) {
					case Data::Type::_bool:
						data.values.push_back(Data::Value(sqlResultSet->getBoolean(i->first)));
						break;
					case Data::Type::_double:
						data.values.push_back(Data::Value((double) sqlResultSet->getDouble(i->first)));
						break;
					case Data::Type::_int:
						data.values.push_back(Data::Value((int) sqlResultSet->getInt(i->first)));
						break;
					case Data::Type::_long:
						data.values.push_back(Data::Value((long) sqlResultSet->getInt64(i->first)));
						break;
					case Data::Type::_string:
						data.values.push_back(Data::Value(sqlResultSet->getString(i->first)));
						break;
					case Data::Type::_uint:
						data.values.push_back(Data::Value((unsigned int) sqlResultSet->getUInt(i->first)));
						break;
					case Data::Type::_ulong:
						data.values.push_back(Data::Value((unsigned long) sqlResultSet->getUInt64(i->first)));
						break;
					default:
						throw Database::Exception("getCustomData(): Invalid data type when getting custom data.");
					}
				}
			}
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// get custom values from one column in the database
void Database::getCustomData(Data::GetColumn& data) {
	sql::ResultSet * sqlResultSet = NULL;
	sql::Statement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.column.empty()) throw Database::Exception("getCustomData(): No columns specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown) throw Database::Exception("getCustomData(): No column type specified");
	data.values.clear();

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// create SQL query
		sqlQuery = "SELECT `" + data.column + "` FROM `" + data.table + "`";
		if(!data.condition.empty()) sqlQuery += " WHERE (" + data.condition + ")";
		if(!data.order.empty()) sqlQuery += " ORDER BY (" + data.order + ")";

		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery(sqlQuery);

		// get result
		if(sqlResultSet) {
			data.values.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next()) {
				if(sqlResultSet->isNull(data.column)) data.values.push_back(Data::Value());
				else {
					switch(data.type) {
					case Data::Type::_bool:
						data.values.push_back(Data::Value(sqlResultSet->getBoolean(data.column)));
						break;
					case Data::Type::_double:
						data.values.push_back(Data::Value((double) sqlResultSet->getDouble(data.column)));
						break;
					case Data::Type::_int:
						data.values.push_back(Data::Value((int) sqlResultSet->getInt(data.column)));
						break;
					case Data::Type::_long:
						data.values.push_back(Data::Value((long) sqlResultSet->getInt64(data.column)));
						break;
					case Data::Type::_string:
						data.values.push_back(Data::Value(sqlResultSet->getString(data.column)));
						break;
					case Data::Type::_uint:
						data.values.push_back(Data::Value((unsigned int) sqlResultSet->getUInt(data.column)));
						break;
					case Data::Type::_ulong:
						data.values.push_back(Data::Value((unsigned long) sqlResultSet->getUInt64(data.column)));
						break;
					default:
						throw Database::Exception("getCustomData(): Invalid data type when getting custom data.");
					}
				}
			}
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// get custom values from multiple columns of the same type in the database
void Database::getCustomData(Data::GetColumns& data) {
	sql::ResultSet * sqlResultSet = NULL;
	sql::Statement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.columns.empty()) throw Database::Exception("getCustomData(): No column name specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown) throw Database::Exception("getCustomData(): No column type specified");
	data.values.clear();
	data.values.reserve(data.columns.size());

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// create SQL query
		sqlQuery = "SELECT ";
		for(auto i = data.columns.begin(); i != data.columns.end(); ++i) {
			sqlQuery += "`" + *i + "`, ";

			// add column to result vector
			data.values.push_back(std::vector<Data::Value>());
		}
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " FROM `" + data.table + "`";
		if(!data.condition.empty()) sqlQuery += " WHERE (" + data.condition + ")";
		if(!data.order.empty()) sqlQuery += " ORDER BY (" + data.order + ")";

		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery(sqlQuery);

		if(sqlResultSet) {
			// reserve memory for results
			for(auto i = data.values.begin(); i != data.values.end(); ++i) i->reserve(sqlResultSet->rowsCount());

			// get results
			while(sqlResultSet->next()) {
				for(auto i = data.columns.begin(); i != data.columns.end(); ++i) {
					auto column = data.values.begin() + (i - data.columns.begin());
					if(sqlResultSet->isNull(*i)) column->push_back(Data::Value());
					else {
						switch(data.type) {
						case Data::Type::_bool:
							column->push_back(Data::Value(sqlResultSet->getBoolean(*i)));
							break;
						case Data::Type::_double:
							column->push_back(Data::Value((double) sqlResultSet->getDouble(*i)));
							break;
						case Data::Type::_int:
							column->push_back(Data::Value((int) sqlResultSet->getInt(*i)));
							break;
						case Data::Type::_long:
							column->push_back(Data::Value((long) sqlResultSet->getInt64(*i)));
							break;
						case Data::Type::_string:
							column->push_back(Data::Value(sqlResultSet->getString(*i)));
							break;
						case Data::Type::_uint:
							column->push_back(Data::Value((unsigned int) sqlResultSet->getUInt(*i)));
							break;
						case Data::Type::_ulong:
							column->push_back(Data::Value((unsigned long) sqlResultSet->getUInt64(*i)));
							break;
						default:
							throw Database::Exception("getCustomData(): Invalid data type when getting custom data.");
						}
					}
				}
			}
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// get custom values from multiple columns of different types in the database
void Database::getCustomData(Data::GetColumnsMixed& data) {
	sql::ResultSet * sqlResultSet = NULL;
	sql::Statement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.columns_types.empty()) throw Database::Exception("getCustomData(): No columns specified");
	data.values.clear();
	data.values.reserve(data.columns_types.size());

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// create SQL query
		sqlQuery = "SELECT ";
		for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
			sqlQuery += "`" + i->first + "`, ";

			// add column to result vector
			data.values.push_back(std::vector<Data::Value>());
		}
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " FROM `" + data.table + "`";
		if(!data.condition.empty()) sqlQuery += " WHERE (" + data.condition + ")";
		if(!data.order.empty()) sqlQuery += " ORDER BY (" + data.order + ")";

		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery(sqlQuery);

		if(sqlResultSet) {
			// reserve memory for results
			for(auto i = data.values.begin(); i != data.values.end(); ++i) i->reserve(sqlResultSet->rowsCount());

			// get results
			while(sqlResultSet->next()) {
				for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
					auto column = data.values.begin() + (i - data.columns_types.begin());
					if(sqlResultSet->isNull(i->first)) column->push_back(Data::Value());
					else {
						switch(i->second) {
						case Data::Type::_bool:
							column->push_back(Data::Value(sqlResultSet->getBoolean(i->first)));
							break;
						case Data::Type::_double:
							column->push_back(Data::Value((double) sqlResultSet->getDouble(i->first)));
							break;
						case Data::Type::_int:
							column->push_back(Data::Value((int) sqlResultSet->getInt(i->first)));
							break;
						case Data::Type::_long:
							column->push_back(Data::Value((long) sqlResultSet->getInt64(i->first)));
							break;
						case Data::Type::_string:
							column->push_back(Data::Value(sqlResultSet->getString(i->first)));
							break;
						case Data::Type::_uint:
							column->push_back(Data::Value((unsigned int) sqlResultSet->getUInt(i->first)));
							break;
						case Data::Type::_ulong:
							column->push_back(Data::Value((unsigned long) sqlResultSet->getUInt64(i->first)));
							break;
						default:
							throw Database::Exception("getCustomData(): Invalid data type when getting custom data.");
						}
					}
				}
			}
		}

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "getCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// insert one custom value into a row in the database
void Database::insertCustomData(const Data::InsertValue& data) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check argument
	if(data.column.empty()) throw Database::Exception("insertCustomData(): No column name specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown) throw Database::Exception("insertCustomData(): No column type specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("INSERT INTO `" + data.table + "` (`" + data.column + "`) VALUES (?)");

		// set value
		if(data.value._isnull) sqlStatement->setNull(1, 0);
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
						MAIN_DATABASE_DELETE(sqlStatement);
						std::ostringstream errStrStr;
						errStrStr.imbue(std::locale(""));
						errStrStr << "Size (" << data.value._s.size() << " bytes) of custom value for `" << data.table << "`.`"
								<< data.column << "` exceeds the ";
						if(data.value._s.size() > 1073741824) errStrStr << "mySQL data limit of 1 GiB";
						else errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
								" - adjust the \'max_allowed_packet\' setting on the server accordingly (to max. 1 GiB).";
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
				throw Database::Exception("insertCustomData(): Invalid data type when inserting custom data.");
			}
		}

		// execute SQL statement
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "insertCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// insert custom values into multiple fields of the same type into a row in the database
void Database::insertCustomData(const Data::InsertFields& data) {
	sql::PreparedStatement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.columns_values.empty()) throw Database::Exception("insertCustomData(): No columns specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown) throw Database::Exception("insertCustomData(): No column type specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL query
		sqlQuery = "INSERT INTO `" + data.table + "` (";
		for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) sqlQuery += "`" + i->first + "`, ";
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += ") VALUES(";
		for(unsigned long n = 0; n < data.columns_values.size() - 1; n++) sqlQuery += "?, ";
		sqlQuery += "?)";

		// create SQL statement
		sqlStatement = this->connection->prepareStatement(sqlQuery);

		// set values
		unsigned int counter = 1;
		switch(data.type) {
		case Data::Type::_bool:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setBoolean(counter, i->second._b);
				counter++;
			}
			break;
		case Data::Type::_double:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setDouble(counter, i->second._d);
				counter++;
			}
			break;
		case Data::Type::_int:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setInt(counter, i->second._i);
				counter++;
			}
			break;
		case Data::Type::_long:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setInt64(counter, i->second._l);
				counter++;
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
						MAIN_DATABASE_DELETE(sqlStatement);
						std::ostringstream errStrStr;
						errStrStr.imbue(std::locale(""));
						errStrStr << "Size (" << i->second._s.size() << " bytes) of custom value for `" << data.table << "`.`"
								<< i->first << "` exceeds the ";
						if(i->second._s.size() > 1073741824) errStrStr << "mySQL data limit of 1 GiB";
						else errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
								" - adjust the \'max_allowed_packet\' setting on the server accordingly (to max. 1 GiB).";
						throw Database::Exception(errStrStr.str());
					}
				}
				else sqlStatement->setString(counter, i->second._s);
				counter++;
			}
			break;
		case Data::Type::_uint:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setUInt(counter, i->second._ui);
				counter++;
			}
			break;
		case Data::Type::_ulong:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setUInt64(counter, i->second._ul);
				counter++;
			}
			break;
		default:
			throw Database::Exception("insertCustomData(): Invalid data type when inserting custom data.");
		}

		// execute SQL statement
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "insertCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// insert custom values into multiple fields of different types into a row in the database
void Database::insertCustomData(const Data::InsertFieldsMixed& data) {
	sql::PreparedStatement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.columns_types_values.empty()) throw Database::Exception("insertCustomData(): No columns specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL query
		sqlQuery = "INSERT INTO `" + data.table + "` (";
		for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i)
			sqlQuery += "`" + std::get<0>(*i) + "`, ";
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += ") VALUES(";
		for(unsigned long n = 0; n < data.columns_types_values.size() - 1; n++) sqlQuery += "?, ";
		sqlQuery += "?)";

		// create SQL statement
		sqlStatement = this->connection->prepareStatement(sqlQuery);

		// set values
		unsigned int counter = 1;
		for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i) {
			if(std::get<2>(*i)._isnull) sqlStatement->setNull(counter, 0);
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
							MAIN_DATABASE_DELETE(sqlStatement);
							std::ostringstream errStrStr;
							errStrStr.imbue(std::locale(""));
							errStrStr << "Size (" << std::get<2>(*i)._s.size() << " bytes) of custom value for `" << data.table << "`.`"
									<< std::get<0>(*i) << "` exceeds the ";
							if(std::get<2>(*i)._s.size() > 1073741824) errStrStr << "mySQL data limit of 1 GiB";
							else errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
									" - adjust the \'max_allowed_packet\' setting on the server accordingly (to max. 1 GiB).";
							throw Database::Exception(errStrStr.str());
						}
					}
					else sqlStatement->setString(counter, std::get<2>(*i)._s);
					break;
				case Data::Type::_uint:
					sqlStatement->setUInt(counter, std::get<2>(*i)._ui);
					break;
				case Data::Type::_ulong:
					sqlStatement->setUInt64(counter, std::get<2>(*i)._ul);
					break;
				default:
					throw Database::Exception("insertCustomData(): Invalid data type when inserting custom data.");
				}
			}
			counter++;
		}

		// execute SQL statement
		sqlStatement->execute();

		// delete SQL statetement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "insertCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// update one custom value in one field of a row in the database
void Database::updateCustomData(const Data::UpdateValue& data) {
	sql::PreparedStatement * sqlStatement = NULL;

	// check argument
	if(data.column.empty()) throw Database::Exception("updateCustomData(): No column name specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown) throw Database::Exception("updateCustomData(): No column type specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->prepareStatement("UPDATE `" + data.table + "` SET `" + data.column
				+ "` = ? WHERE (" + data.condition + ")");

		// set value
		if(data.value._isnull) sqlStatement->setNull(1, 0);
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
						MAIN_DATABASE_DELETE(sqlStatement);
						std::ostringstream errStrStr;
						errStrStr.imbue(std::locale(""));
						errStrStr << "Size (" << data.value._s.size() << " bytes) of custom value for `" << data.table << "`.`"
								<< data.column << "` exceeds the ";
						if(data.value._s.size() > 1073741824) errStrStr << "mySQL data limit of 1 GiB";
						else errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
								" - adjust the \'max_allowed_packet\' setting on the server accordingly (to max. 1 GiB).";
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
				throw Database::Exception("updateCustomData(): Invalid data type when updating custom data.");
			}
		}

		// execute SQL statement
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "updateCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// update custom values in multiple fields of a row with the same type in the database
void Database::updateCustomData(const Data::UpdateFields& data) {
	sql::PreparedStatement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.columns_values.empty()) throw Database::Exception("updateCustomData(): No columns specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown) throw Database::Exception("updateCustomData(): No column type specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL query
		sqlQuery = "UPDATE `" + data.table + "` SET ";
		for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) sqlQuery += "`" + i->first + "` = ?, ";
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " WHERE (" + data.condition + ")";

		// create SQL statement
		sqlStatement = this->connection->prepareStatement(sqlQuery);

		// set values
		unsigned int counter = 1;
		switch(data.type) {
		case Data::Type::_bool:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setBoolean(counter, i->second._b);
				counter++;
			}
			break;
		case Data::Type::_double:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setDouble(counter, i->second._d);
				counter++;
			}
			break;
		case Data::Type::_int:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setInt(counter, i->second._i);
				counter++;
			}
			break;
		case Data::Type::_long:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setInt64(counter, i->second._l);
				counter++;
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
						MAIN_DATABASE_DELETE(sqlStatement);
						std::ostringstream errStrStr;
						errStrStr.imbue(std::locale(""));
						errStrStr << "Size (" << i->second._s.size() << " bytes) of custom value for `" << data.table << "`.`"
								<< i->first << "` exceeds the ";
						if(i->second._s.size() > 1073741824) errStrStr << "mySQL data limit of 1 GiB";
						else errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
								" - adjust the \'max_allowed_packet\' setting on the server accordingly (to max. 1 GiB).";
						throw Database::Exception(errStrStr.str());
					}
				}
				else sqlStatement->setString(counter, i->second._s);
				counter++;
			}
			break;
		case Data::Type::_uint:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setUInt(counter, i->second._ui);
				counter++;
			}
			break;
		case Data::Type::_ulong:
			for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) {
				if(i->second._isnull) sqlStatement->setNull(counter, 0);
				else sqlStatement->setUInt64(counter, i->second._ul);
				counter++;
			}
			break;
		default:
			throw Database::Exception("updateCustomData(): Invalid data type when updating custom data.");
		}

		// execute SQL statement
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "updateCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// update custom values in multiple fields of a row with different types in the database
void Database::updateCustomData(const Data::UpdateFieldsMixed& data) {
	sql::PreparedStatement * sqlStatement = NULL;
	std::string sqlQuery;

	// check argument
	if(data.columns_types_values.empty()) throw Database::Exception("updateCustomData(): No columns specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL query
		sqlQuery = "UPDATE `" + data.table + "` SET ";
		for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i)
			sqlQuery += "`" + std::get<0>(*i) + "` = ?, ";
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " WHERE (" + data.condition + ")";

		// create SQL statement
		sqlStatement = this->connection->prepareStatement(sqlQuery);

		// set values
		unsigned int counter = 1;
		for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i) {
			if(std::get<2>(*i)._isnull) sqlStatement->setNull(counter, 0);
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
							MAIN_DATABASE_DELETE(sqlStatement);
							std::ostringstream errStrStr;
							errStrStr.imbue(std::locale(""));
							errStrStr << "Size (" << std::get<2>(*i)._s.size() << " bytes) of custom value for `" << data.table << "`.`"
									<< std::get<0>(*i) << "` exceeds the ";
							if(std::get<2>(*i)._s.size() > 1073741824) errStrStr << "mySQL data limit of 1 GiB";
							else errStrStr << "current mySQL server limit of " << this->getMaxAllowedPacketSize() << " bytes"
									" - adjust the \'max_allowed_packet\' setting on the server accordingly (to max. 1 GiB).";
							throw Database::Exception(errStrStr.str());
						}
					}
					else sqlStatement->setString(counter, std::get<2>(*i)._s);
					break;
				case Data::Type::_uint:
					sqlStatement->setUInt(counter, std::get<2>(*i)._ui);
					break;
				case Data::Type::_ulong:
					sqlStatement->setUInt64(counter, std::get<2>(*i)._ul);
					break;
				default:
					throw Database::Exception("updateCustomData(): Invalid data type when updating custom data.");
				}
			}
			counter++;
		}

		// execute SQL statement
		sqlStatement->execute();

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "updateCustomData() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		if(!sqlQuery.empty()) errorStrStr << " [QUERY: " << sqlQuery << "]";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

/*
 * PROTECTED GETTER (protected)
 */

// get the maximum allowed packet size
unsigned long Database::getMaxAllowedPacketSize() const {
	return this->maxAllowedPacketSize;
}

/*
 * DATABASE HELPER FUNCTIONS (protected)
 */

// check whether the connection to the database is still valid and try to reconnect if necesssary (set error message on failure)
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
		if(!(this->connection->reconnect())) {
			// simple reconnect failed: try to reset connection after sleeping over it for some time
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

		// recover prepared SQL statements
		for(auto i = this->preparedStatements.begin(); i != this->preparedStatements.end(); ++i) {
			if(i->statement) {
				delete i->statement;
				i->statement = NULL;
			}
			i->statement = this->connection->prepareStatement(i->string);
		}
	}
	catch(sql::SQLException &e) {
		// SQL error while recovering prepared statements
		std::ostringstream errorStrStr;
		errorStrStr << "checkConnection() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();

		// clear connection
		delete this->connection;
		this->connection = NULL;

		return false;
	}
	catch(...) {
		// any other exception: clear connection and re-throw
		delete this->connection;
		this->connection = NULL;
		throw;
	}

	return true;
}

// get the last inserted ID from the database
unsigned long Database::getLastInsertedId() {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psLastId)) throw Database::Exception("Missing prepared SQL statement for last inserted id");
	sqlStatement = this->preparedStatements.at(this->psLastId - 1).statement;
	if(!sqlStatement) throw Database::Exception("Prepared SQL statement for last inserted id is NULL");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("id");

		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getLastInsertedId() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// reset the auto increment of an (empty) table in the database
void Database::resetAutoIncrement(const std::string& tableName) {
	sql::Statement * sqlStatement = NULL;

	// check argument
	if(tableName.empty()) throw Database::Exception("resetAutoIncrement(): No table name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlStatement->execute("ALTER TABLE `" + tableName + "` AUTO_INCREMENT = 1");

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "resetAutoIncrement() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// lock a table in the database for writing
void Database::lockTable(const std::string& tableName) {
	sql::Statement * sqlStatement = NULL;

	// check argument
	if(tableName.empty()) throw Database::Exception("lockTable(): No table name specified");

	// check for lock and unlock if necessary
	if(this->tablesLocked) this->unlockTables();

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		this->tablesLocked = true;
		sqlStatement->execute("LOCK TABLES `" + tableName + "` WRITE");

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "lockTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// lock two tables in the database for writing (plus the alias 'a' for reading the first and the alias 'b' for reading the second table)
void Database::lockTables(const std::string& tableName1, const std::string& tableName2) {
	sql::Statement * sqlStatement = NULL;

	// check arguments
	if(tableName1.empty()) throw Database::Exception("lockTables(): Table name #1 missing");
	if(tableName2.empty()) throw Database::Exception("lockTables(): Table name #2 missing");

	// check for lock and unlock if necessary
	if(this->tablesLocked) this->unlockTables();

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		this->tablesLocked = true;
		sqlStatement->execute("LOCK TABLES `" + tableName1 + "` WRITE, `" + tableName1 + "` AS a READ, `" + tableName2 + "` WRITE,"
				"`" + tableName2 + "` AS b READ");

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "lockTables() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// unlock tables in the database
void Database::unlockTables() {
	sql::Statement * sqlStatement = NULL;

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlStatement->execute("UNLOCK TABLES");
		this->tablesLocked = false;

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "unlockTables() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// check whether a name-specified table in the database is empty
bool Database::isTableEmpty(const std::string& tableName) {
	sql::Statement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check argument
	if(tableName.empty()) throw Database::Exception("isTableEmpty(): No table name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlResultSet = sqlStatement->executeQuery("SELECT NOT EXISTS (SELECT 1 FROM `" + tableName + "` LIMIT 1) AS result");

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isTableEmpty() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// check whether a specific table exists in the database
bool Database::isTableExists(const std::string& tableName) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check argument
	if(tableName.empty()) throw Database::Exception("isTableExists(): No table name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create and execute SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT COUNT(*) AS result FROM INFORMATION_SCHEMA.TABLES"
				" WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ? LIMIT 1");

		// get result
		sqlStatement->setString(1, this->settings.name);
		sqlStatement->setString(2, tableName);
		sqlResultSet = sqlStatement->executeQuery();
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isTableExists() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// check whether a specific column of a specific table exists in the database
bool Database::isColumnExists(const std::string& tableName, const std::string& columnName) {
	sql::PreparedStatement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check arguments
	if(tableName.empty()) throw Database::Exception("isColumnExists(): No table name specified");
	if(columnName.empty()) throw Database::Exception("isColumnExists(): No column name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create and execute SQL statement
		sqlStatement = this->connection->prepareStatement("SELECT COUNT(*) AS result FROM INFORMATION_SCHEMA.COLUMNS"
				" WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ? AND COLUMN_NAME = ? LIMIT 1");

		// get result
		sqlStatement->setString(1, this->settings.name);
		sqlStatement->setString(2, tableName);
		sqlStatement->setString(3, columnName);
		sqlResultSet = sqlStatement->executeQuery();
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result and SQL statement
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "isColumnExists() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}

	return result;
}

// add a table to the database (the primary key 'id' will be created automatically; WARNING: check existence beforehand!)
void Database::createTable(const std::string& tableName, const std::vector<Column>& columns, bool compressed) {
	sql::Statement * sqlStatement = NULL;

	// check arguments
	if(tableName.empty()) throw Database::Exception("addTable(): No table name specified");
	if(columns.empty()) throw Database::Exception("addTable(): No columns specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL query
		std::string sqlQuery = "CREATE TABLE `" + tableName + "`(id SERIAL";
		std::string references;
		for(auto i = columns.begin(); i != columns.end(); ++i) {
			// check values
			if(i->_name.empty()) throw Database::Exception("addTable(): A column is missing its name");
			if(i->_type.empty()) throw Database::Exception("addTable(): A column is missing its type");

			// add to SQL query
			sqlQuery += ", `" + i->_name + "` " + i->_type;
			if(!(i->_referenceTable.empty())) {
				if(i->_referenceColumn.empty())
					throw Database::Exception("addTable(): A column reference is missing its source column name");
				references += ", FOREIGN KEY(`" + i->_name + "`) REFERENCES `" + i->_referenceTable + "`(`" + i->_referenceColumn
						+ "`) ON UPDATE RESTRICT ON DELETE CASCADE";
			}
		}
		sqlQuery += ", PRIMARY KEY(id)" + references + ") CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
		if(compressed) sqlQuery += ", ROW_FORMAT=COMPRESSED";

		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlStatement->execute(sqlQuery);

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// add a column to a table in the database
void Database::addColumn(const std::string& tableName, const Column& column) {
	sql::Statement * sqlStatement = NULL;

	// check arguments
	if(tableName.empty()) throw Database::Exception("addColumn(): No table name specified");
	if(column._name.empty()) throw Database::Exception("addColumn(): No column name specified");
	if(column._type.empty()) throw Database::Exception("addColumn(): No column type specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL query
		std::string sqlQuery = "ALTER TABLE `" + tableName + "` ADD COLUMN `" + column._name + "` " + column._type;
		if(!column._referenceTable.empty()) {
			if(column._referenceColumn.empty())
				throw Database::Exception("addColumn(): A column reference is missing its source column name");
			sqlQuery += ", ADD FOREIGN KEY(`" + column._name + "`) REFERENCES `" + column._referenceTable + "`(`"
					+ column._referenceColumn + "`) ON UPDATE RESTRICT ON DELETE CASCADE";
		}

		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlStatement->execute(sqlQuery);

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "addColumn() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// compress a table in the database
void Database::compressTable(const std::string& tableName) {
	sql::Statement * sqlStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;
	bool compressed = false;

	// check argument
	if(tableName.empty()) throw Database::Exception("compressTable(): No table name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement for checking whether the table is already compressed
		sqlResultSet = sqlStatement->executeQuery("SELECT LOWER(row_format) = 'compressed' AS result FROM information_schema.tables"
				" WHERE table_schema = '" + this->settings.name + "' AND table_name = '" + tableName + "' LIMIT 1;");

		// get result
		if(sqlResultSet && sqlResultSet->next()) compressed = sqlResultSet->getBoolean("result");
		else {
			MAIN_DATABASE_DELETE(sqlResultSet);
			MAIN_DATABASE_DELETE(sqlStatement);
			throw Database::Exception("compressTable(): Could not determine row format of table \'" + tableName + "\'");
		}

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);

		// execute SQL statement for compressing the table if table is not already compressed
		if(!compressed) sqlStatement->execute("ALTER TABLE `" + tableName + "` ROW_FORMAT=COMPRESSED");

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "compressTable() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

// delete a table from the database if it exists
void Database::deleteTableIfExists(const std::string& tableName) {
	sql::Statement * sqlStatement = NULL;

	// check arguments
	if(tableName.empty()) throw Database::Exception("deleteTableIfExists(): No table name specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlStatement->execute("DELETE TABLE IF EXISTS `" + tableName + "`");

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "deleteTableIfExists() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

/*
 * INTERNAL HELPER FUNCTION (private)
 */

// run file with SQL commands
bool Database::run(const std::string& sqlFile) {
	// check argument
	if(sqlFile.empty()) throw Database::Exception("run(): No SQL file specified");

	// open SQL file
	std::ifstream initSQLFile(sqlFile);

	if(initSQLFile.is_open()) {
		// check connection
		if(!(this->checkConnection())) return false;

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
				if(!line.empty()) sqlStatement->execute(line);
				lineCounter++;
			}
			catch(sql::SQLException &e) {
				// SQL error
				MAIN_DATABASE_DELETE(sqlStatement);
				std::ostringstream errorStrStr;
				errorStrStr << "run() SQL Error #" << e.getErrorCode() << " on line #" << lineCounter
						<< " (State " << e.getSQLState() << "): " << e.what();
				this->errorMessage = errorStrStr.str();
				return false;
			}
			catch(...) {
				// any other exception: free memory and re-throw
				MAIN_DATABASE_DELETE(sqlStatement);
				throw;
			}
		}

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);

		// close SQL file
		initSQLFile.close();
	}
	else {
		this->errorMessage = "Could not open \'" + sqlFile + "\' for reading";
		return false;
	}

	return true;
}

// execute a SQL query
void Database::execute(const std::string& sqlQuery) {
	sql::Statement * sqlStatement = NULL;

	// check argument
	if(sqlQuery.empty()) throw Database::Exception("execute(): No SQL query specified");

	// check connection
	if(!(this->checkConnection())) throw Database::Exception(this->errorMessage);

	try {
		// create SQL statement
		sqlStatement = this->connection->createStatement();

		// execute SQL statement
		sqlStatement->execute(sqlQuery);

		// delete SQL statement
		MAIN_DATABASE_DELETE(sqlStatement);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlStatement);
		std::ostringstream errorStrStr;
		errorStrStr << "execute() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		errorStrStr << " [QUERY: \'" << sqlQuery << "\']";
		throw Database::Exception(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlStatement);
		throw;
	}
}

}
