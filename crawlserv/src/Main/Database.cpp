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
Database::Database(const crawlservpp::Struct::DatabaseSettings& dbSettings, const std::string& dbModule)
		: tablesLocked(false), settings(dbSettings), maxAllowedPacketSize(0), sleepOnError(0), module(dbModule), ps({0}) {
	// get driver instance if necessary
	if(!Database::driver) {
		Database::driver = get_driver_instance();
		if(!Database::driver) throw std::runtime_error("Could not get database instance");
	}
}

// destructor
Database::~Database() {
	// clear prepared SQL statements
	this->preparedStatements.clear();

	// clear connection
	if(this->connection) if(this->connection->isValid()) this->connection->close();

	// unset table lock
	this->tablesLocked = false;
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
const crawlservpp::Struct::DatabaseSettings& Database::getSettings() const {
	return this->settings;
}

/*
 * INITIALIZING FUNCTIONS
 */

// connect to the database
void Database::connect() {
	// check driver
	if(!Database::driver) throw Database::Exception("Main::Database::connect(): MySQL driver not loaded");

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
		if(this->settings.compression) connectOptions["CLIENT_COMPRESS"] = true; // enable server-client compression

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
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());
		if(!sqlStatement)
			throw Database::Exception("Main::Database::connect(): Could not create SQL statement");

		// set lock timeout
		std::ostringstream executeStr;
		executeStr << "SET SESSION innodb_lock_wait_timeout = ";
		executeStr << MAIN_DATABASE_LOCK_TIMEOUT_SECONDS;
		sqlStatement->execute(executeStr.str());

		// get maximum allowed package size
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery("SHOW VARIABLES LIKE 'max_allowed_packet'"));
		if(sqlResultSet && sqlResultSet->next()){
			this->maxAllowedPacketSize = sqlResultSet->getUInt64("Value");
			if(!(this->maxAllowedPacketSize))
				throw Database::Exception("Main::Database::connect(): \'max_allowed_packet\' is zero");
		}
		else throw Database::Exception("Main::Database::connect(): Could not get \'max_allowed_packet\'");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::connect", e); }
}

// run initializing SQL commands by processing all .sql files in the "sql" sub-folder
void Database::initializeSql() {
	// read 'sql' directory
	std::vector<std::string> sqlFiles;
	sqlFiles = crawlservpp::Helper::FileSystem::listFilesInPath("sql", ".sql");

	// execute all SQL files
	for(auto i = sqlFiles.begin(); i != sqlFiles.end(); ++i) this->run(*i);
}

// prepare basic SQL statements (getting last id, logging and thread status)
void Database::prepare() {
	// reserve memory for SQL statements
	this->reserveForPreparedStatements(5);

	try {
		// prepare basic SQL statements
		if(!(this->ps.lastId)) this->ps.lastId = this->addPreparedStatement("SELECT LAST_INSERT_ID() AS id");
		if(!(this->ps.log)) this->ps.log = this->addPreparedStatement("INSERT INTO crawlserv_log(module, entry) VALUES (?, ?)");

		// prepare thread statements
		if(!(this->ps.setThreadStatus)) this->ps.setThreadStatus = this->addPreparedStatement(
				"UPDATE crawlserv_threads SET status = ?, paused = ? WHERE id = ? LIMIT 1");
		if(!(this->ps.setThreadStatusMessage)) this->ps.setThreadStatusMessage = this->addPreparedStatement(
				"UPDATE crawlserv_threads SET status = ? WHERE id = ? LIMIT 1");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::prepare", e); }
}

/*
 * LOGGING FUNCTIONS
 */

// add a log entry to the database (for any module)
void Database::log(const std::string& logModule, const std::string& logEntry) {
	// check table lock
	if(this->tablesLocked) {
		std::cout << std::endl << "[WARNING] Logging not possible while a table is locked - re-routing to stdout:";
		std::cout << std::endl << " " << logModule << ": " << logEntry << std::flush;
		return;
	}

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.log)) throw Database::Exception("Missing prepared SQL statement for Main::Database::log(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.log);

	// add entry to database
	try {
		// execute SQL query
		if(logModule.empty()) sqlStatement.setString(1, "[unknown]");
		else sqlStatement.setString(1, logModule);
		if(logEntry.empty()) sqlStatement.setString(2, "[empty]");
		else sqlStatement.setString(2, logEntry);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::log", e); }
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
	std::string sqlQuery = "SELECT COUNT(*) FROM crawlserv_log";
	if(!logModule.empty()) sqlQuery += " WHERE module = ?";

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(sqlQuery));

		// execute SQL statement
		if(!logModule.empty()) sqlStatement->setString(1, logModule);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64(1);
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getNumberOfLogEntries", e); }

	return result;
}

// remove the log entries of a specific module from the database (or all log entries if logModule is an empty string)
void Database::clearLogs(const std::string& logModule) {
	// check connection
	this->checkConnection();

	// create SQL query string
	std::string sqlQuery = "DELETE FROM crawlserv_log";
	if(!logModule.empty()) sqlQuery += " WHERE module = ?";

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(sqlQuery));

		// execute SQL statement
		if(!logModule.empty()) sqlStatement->setString(1, logModule);
		sqlStatement->execute();

	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::clearLogs", e); }
}

/*
 * THREAD FUNCTIONS
 */

// get all threads from the database
std::vector<crawlservpp::Struct::ThreadDatabaseEntry> Database::getThreads() {
	std::vector<crawlservpp::Struct::ThreadDatabaseEntry> result;

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL query
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(
				"SELECT id, module, status, paused, website, urllist, config, last FROM crawlserv_threads"));

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
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getThreads", e); }

	return result;
}

// add a thread to the database and return its new ID
unsigned long Database::addThread(const std::string& threadModule, const crawlservpp::Struct::ThreadOptions& threadOptions) {
	unsigned long result = 0;

	// check arguments
	if(threadModule.empty()) throw Database::Exception("Main::Database::addThread(): No thread module specified");
	if(!threadOptions.website) throw Database::Exception("Main::Database::addThread(): No website specified");
	if(!threadOptions.urlList) throw Database::Exception("Main::Database::addThread(): No URL list specified");
	if(!threadOptions.config) throw Database::Exception("Main::Database::addThread(): No configuration specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"INSERT INTO crawlserv_threads(module, website, urllist, config) VALUES (?, ?, ?, ?)"));

		// execute SQL statement
		sqlStatement->setString(1, threadModule);
		sqlStatement->setUInt64(2, threadOptions.website);
		sqlStatement->setUInt64(3, threadOptions.urlList);
		sqlStatement->setUInt64(4, threadOptions.config);
		sqlStatement->execute();

		// get id
		result = this->getLastInsertedId();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::addThread", e); }

	return result;
}

// get run time of thread (in seconds) from the database by its ID
unsigned long Database::getThreadRunTime(unsigned long threadId) {
	unsigned long result = 0;

	// check argument
	if(!threadId) throw Database::Exception("Main::Database::getThreadRunTime(): No thread ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT runtime FROM crawlserv_threads WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("runtime");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getThreadRunTime", e); }

	return result;
}

// get pause time of thread (in seconds) from the database by its ID
unsigned long Database::getThreadPauseTime(unsigned long threadId) {
	unsigned long result = 0;

	// check argument
	if(!threadId) throw Database::Exception("Main::Database::getThreadPauseTime(): No thread ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT pausetime FROM crawlserv_threads WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("pausetime");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getThreadPauseTime", e); }

	return result;
}

// update thread status in the database (and add the pause state to the status message if necessary)
void Database::setThreadStatus(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage) {
	// check argument
	if(!threadId) throw Database::Exception("Main::Database::setThreadStatus(): No thread ID specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.setThreadStatus))
		throw Database::Exception("Missing prepared SQL statement for Main::Database::setThreadStatus(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setThreadStatus);

	// create status message
	std::string statusMessage;
	if(threadPaused) {
		if(!threadStatusMessage.empty()) statusMessage = "PAUSED " + threadStatusMessage;
		else statusMessage = "PAUSED";
	}
	else statusMessage = threadStatusMessage;

	try {
		// execute SQL statement
		sqlStatement.setString(1, statusMessage);
		sqlStatement.setBoolean(2, threadPaused);
		sqlStatement.setUInt64(3, threadId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadStatus", e); }
}

// update thread status in the database (without using or changing the pause state)
void Database::setThreadStatus(unsigned long threadId, const std::string& threadStatusMessage) {
	// check argument
	if(!threadId) throw Database::Exception("Main::Database::setThreadStatus(): No thread ID specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.setThreadStatusMessage))
		throw Database::Exception("Missing prepared SQL statement for Main::Database::setThreadStatus(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setThreadStatusMessage);

	// create status message
	std::string statusMessage;
	statusMessage = threadStatusMessage;

	try {
		// execute SQL statement
		sqlStatement.setString(1, statusMessage);
		sqlStatement.setUInt64(2, threadId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadStatus", e); }
}

// set run time of thread (in seconds) in the database
void Database::setThreadRunTime(unsigned long threadId, unsigned long threadRunTime) {
	// check argument
	if(!threadId) throw Database::Exception("Main::Database::setThreadRunTime(): No thread ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"UPDATE crawlserv_threads SET runtime = ? WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, threadRunTime);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadRunTime", e); }
}

// set pause time of thread (in seconds) in the database
void Database::setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime) {
	// check argument
	if(!threadId) throw Database::Exception("Main::Database::setThreadPauseTime(): No thread ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"UPDATE crawlserv_threads SET pausetime = ? WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, threadPauseTime);
		sqlStatement->setUInt64(2, threadId);
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::setThreadPauseTime", e); }
}

// remove thread from the database by its ID
void Database::deleteThread(unsigned long threadId) {
	// check argument
	if(!threadId) throw Database::Exception("Main::Database::deleteThread(): No thread ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"DELETE FROM crawlserv_threads WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, threadId);
		sqlStatement->execute();

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_threads")) this->resetAutoIncrement("crawlserv_threads");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteThread", e); }
}

/*
 * WEBSITE FUNCTIONS
 */

// add a website to the database and return its new ID
unsigned long Database::addWebsite(const crawlservpp::Struct::WebsiteProperties& websiteProperties) {
	unsigned long result = 0;
	std::string timeStamp;

	// check arguments
	if(websiteProperties.domain.empty())
		throw Database::Exception("Main::Database::addWebsite(): No domain specified");
	if(websiteProperties.nameSpace.empty())
		throw Database::Exception("Main::Database::addWebsite(): No website namespace specified");
	if(websiteProperties.name.empty())
		throw Database::Exception("Main::Database::addWebsite(): No website name specified");

	// check website namespace
	if(this->isWebsiteNamespace(websiteProperties.nameSpace))
		throw Database::Exception("Main::Database::addWebsite(): Website namespace already exists");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement for adding website
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"INSERT INTO crawlserv_websites(domain, namespace, name) VALUES (?, ?, ?)"));

		// execute SQL statement for adding website
		sqlStatement->setString(1, websiteProperties.domain);
		sqlStatement->setString(2, websiteProperties.nameSpace);
		sqlStatement->setString(3, websiteProperties.name);
		sqlStatement->execute();

		// get id
		result = this->getLastInsertedId();

		// add default URL list
		this->addUrlList(result, crawlservpp::Struct::UrlListProperties("default", "Default URL list"));
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::addWebsite", e); }

	return result;
}

// get website domain from the database by its ID
std::string Database::getWebsiteDomain(unsigned long websiteId) {
	std::string result;

	// check argument
	if(!websiteId) throw Database::Exception("Main::Database::getWebsiteDomain(): No website ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT domain FROM crawlserv_websites WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("domain");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteDomain", e); }

	return result;
}

// get namespace of website from the database by its ID
std::string Database::getWebsiteNamespace(unsigned long int websiteId) {
	std::string result;

	// check argument
	if(!websiteId) throw Database::Exception("Main::Database::getWebsiteNamespace(): No website ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT namespace FROM crawlserv_websites WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("namespace");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespace", e); }

	return result;
}

// get ID and namespace of website from the database by URL list ID
std::pair<unsigned long, std::string> Database::getWebsiteNamespaceFromUrlList(unsigned long listId) {
	unsigned long websiteId = 0;

	// check argument
	if(!listId) throw Database::Exception("Main::Database::getWebsiteNamespaceFromUrlList(): No URL list ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT website FROM crawlserv_urllists WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) websiteId = sqlResultSet->getUInt64("website");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespaceFromUrlList", e); }

	return std::pair<unsigned long, std::string>(websiteId, this->getWebsiteNamespace(websiteId));
}

// get ID and namespace of website from the database by configuration ID
std::pair<unsigned long, std::string> Database::getWebsiteNamespaceFromConfig(unsigned long configId) {
	unsigned long websiteId = 0;

	// check argument
	if(!configId) throw Database::Exception("Main::Database::getWebsiteNamespaceFromConfig(): No configuration ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT website FROM crawlserv_configs WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) websiteId = sqlResultSet->getUInt64("website");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespaceFromConfig", e); }

	return std::pair<unsigned long, std::string>(websiteId, this->getWebsiteNamespace(websiteId));
}

// get ID and namespace of website from the database by custom table ID of specified type
std::pair<unsigned long, std::string> Database::getWebsiteNamespaceFromCustomTable(const std::string& type, unsigned long tableId) {
	unsigned long websiteId = 0;

	// check argument
	if(type.empty())
		throw Database::Exception("Main::Database::getWebsiteNamespaceFromCustomTable(): No table type specified");
	if(!tableId)
		throw Database::Exception("Main::Database::getWebsiteNamespaceFromCustomTable(): No table ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT website FROM `crawlserv_" + type + "tables` WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) websiteId = sqlResultSet->getUInt64("website");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getWebsiteNamespaceFromCustomTable", e); }

	return std::pair<unsigned long, std::string>(websiteId, this->getWebsiteNamespace(websiteId));
}

// check whether website namespace exists in the database
bool Database::isWebsiteNamespace(const std::string& nameSpace) {
	bool result = false;

	// check argument
	if(nameSpace.empty()) throw Database::Exception("Main::Database::isWebsiteNamespace(): No namespace specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS (SELECT 1 FROM crawlserv_websites WHERE namespace = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setString(1, nameSpace);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
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
void Database::updateWebsite(unsigned long websiteId, const crawlservpp::Struct::WebsiteProperties& websiteProperties) {
	// check arguments
	if(!websiteId)
		throw Database::Exception("Main::Database::updateWebsite(): No website ID specified");
	if(websiteProperties.domain.empty())
		throw Database::Exception("Main::Database::updateWebsite(): No domain specified");
	if(websiteProperties.nameSpace.empty())
		throw Database::Exception("Main::Database::updateWebsite(): No website namespace specified");
	if(websiteProperties.name.empty())
		throw Database::Exception("Main::Database::updateWebsite(): No website name specified");

	// get old namespace
	std::string oldNamespace = this->getWebsiteNamespace(websiteId);

	// check website namespace if necessary
	if(websiteProperties.nameSpace != oldNamespace)
		if(this->isWebsiteNamespace(websiteProperties.nameSpace))
			throw Database::Exception("Main::Database::Website namespace already exists");

	// check connection
	this->checkConnection();

	try {
		// check whether namespace has changed
		if(websiteProperties.nameSpace != oldNamespace) {
			// create SQL statement for renaming
			std::unique_ptr<sql::Statement> renameStatement(this->connection->createStatement());

			// rename sub tables
			std::vector<std::pair<unsigned long, std::string>> urlLists = this->getUrlLists(websiteId);
			for(auto liI = urlLists.begin(); liI != urlLists.end(); ++liI) {
				renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "` RENAME TO "
						+ "`crawlserv_" + websiteProperties.nameSpace + "_" + liI->second + "`");
				renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_crawled` RENAME TO "
						+ "`crawlserv_" + websiteProperties.nameSpace + "_" + liI->second + "_crawled`");
				renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_links` RENAME TO "
						+ "`crawlserv_" + websiteProperties.nameSpace + "_" + liI->second + "_links`");

				// rename parsing tables
				std::vector<std::pair<unsigned long, std::string>> parsedTables = this->getCustomTables("parsed", liI->first);
				for(auto taI = parsedTables.begin(); taI != parsedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_parsed_"
							+ taI->second + "` RENAME TO `" + "crawlserv_" + websiteProperties.nameSpace + "_" + liI->second
							+ "_parsed_" + taI->second + "`");
				}

				// rename extracting tables
				std::vector<std::pair<unsigned long, std::string>> extractedTables = this->getCustomTables("extracted", liI->first);
				for(auto taI = extractedTables.begin(); taI != extractedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_extracted_"
							+ taI->second + "` RENAME TO `" + "crawlserv_" + websiteProperties.nameSpace + "_" + liI->second
							+ "_extracted_" + taI->second + "`");
				}

				// rename analyzing tables
				std::vector<std::pair<unsigned long, std::string>> analyzedTables = this->getCustomTables("analyzed", liI->first);
				for(auto taI = analyzedTables.begin(); taI != analyzedTables.end(); ++taI) {
					renameStatement->execute("ALTER TABLE `crawlserv_" + oldNamespace + "_" + liI->second + "_analyzed_"
							+ taI->second + "` RENAME TO `" + "crawlserv_" + websiteProperties.nameSpace + "_" + liI->second
							+ "_analyzed_" + taI->second + "`");
				}
			}

			// create SQL statement for updating
			std::unique_ptr<sql::PreparedStatement> updateStatement(this->connection->prepareStatement(
					"UPDATE crawlserv_websites SET domain = ?, namespace = ?, name = ? WHERE id = ? LIMIT 1"));

			// execute SQL statement for updating
			updateStatement->setString(1, websiteProperties.domain);
			updateStatement->setString(2, websiteProperties.nameSpace);
			updateStatement->setString(3, websiteProperties.name);
			updateStatement->setUInt64(4, websiteId);
			updateStatement->execute();
		}
		else {
			// create SQL statement for updating
			std::unique_ptr<sql::PreparedStatement> updateStatement(this->connection->prepareStatement(
					"UPDATE crawlserv_websites SET domain = ?, name = ? WHERE id = ? LIMIT 1"));

			// execute SQL statement for updating
			updateStatement->setString(1, websiteProperties.domain);
			updateStatement->setString(2, websiteProperties.name);
			updateStatement->setUInt64(3, websiteId);
			updateStatement->execute();
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateWebsite", e); }
}

// delete website (and all associated data) from the database by its ID
void Database::deleteWebsite(unsigned long websiteId) {
	// check argument
	if(!websiteId) throw Database::Exception("Main::Database::deleteWebsite(): No website ID specified");

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
		this->checkConnection();

		// create SQL statement for deletion of website
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"DELETE FROM crawlserv_websites WHERE id = ? LIMIT 1"));

		// execute SQL statements for deletion of website
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->execute();

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_websites")) this->resetAutoIncrement("crawlserv_websites");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteWebsite", e); }
}

// duplicate website in the database by its ID (no processed data will be duplicated)
unsigned long Database::duplicateWebsite(unsigned long websiteId) {
	unsigned long result = 0;

	// check argument
	if(!websiteId) throw Database::Exception("Main::Database::duplicateWebsite(): No website ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement for geting website info
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT name, namespace, domain FROM crawlserv_websites WHERE id = ? LIMIT 1"));

		// execute SQL statement for geting website info
		sqlStatement->setUInt64(1, websiteId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			std::string websiteNamespace =sqlResultSet->getString("namespace");
			std::string websiteName = sqlResultSet->getString("name");
			std::string websiteDomain =sqlResultSet->getString("domain");

			// create new namespace and new name
			std::string newNamespace = Database::duplicateWebsiteNamespace(websiteNamespace);
			std::string newName = websiteName + " (copy)";

			// add website
			result = this->addWebsite(crawlservpp::Struct::WebsiteProperties(websiteDomain, newNamespace, newName));

			// create SQL statement for geting URL list info
			sqlStatement.reset(this->connection->prepareStatement(
					"SELECT name, namespace FROM crawlserv_urllists WHERE website = ?"));

			// execute SQL statement for geting URL list info
			sqlStatement->setUInt64(1, websiteId);
			sqlResultSet.reset(sqlStatement->executeQuery());

			// get results
			while(sqlResultSet && sqlResultSet->next()) {
				// add URL lists with same name (except for "default", which has already been created)
				std::string urlListName = sqlResultSet->getString("namespace");
				if(urlListName != "default")
					this->addUrlList(result, crawlservpp::Struct::UrlListProperties(sqlResultSet->getString("namespace"), urlListName));
			}

			// create SQL statement for getting queries
			sqlStatement.reset(this->connection->prepareStatement(
					"SELECT name, query, type, resultbool, resultsingle, resultmulti,"
					" textonly FROM crawlserv_queries WHERE website = ?"));

			// execute SQL statement for getting queries
			sqlStatement->setUInt64(1, websiteId);
			sqlResultSet.reset(sqlStatement->executeQuery());

			// get results
			while(sqlResultSet && sqlResultSet->next()) {
				// add query
				this->addQuery(result, crawlservpp::Struct::QueryProperties(sqlResultSet->getString("name"),
						sqlResultSet->getString("query"), sqlResultSet->getString("type"),
						sqlResultSet->getBoolean("resultbool"),	sqlResultSet->getBoolean("resultsingle"),
						sqlResultSet->getBoolean("resultmulti"), sqlResultSet->getBoolean("textonly")));

			}

			// create SQL statement for getting configurations
			sqlStatement.reset(this->connection->prepareStatement(
					"SELECT module, name, config FROM crawlserv_configs WHERE website = ?"));

			// execute SQL statement for getting configurations
			sqlStatement->setUInt64(1, websiteId);
			sqlResultSet.reset(sqlStatement->executeQuery());

			// get results
			while(sqlResultSet && sqlResultSet->next()) {
				// add configuration
				this->addConfiguration(result, crawlservpp::Struct::ConfigProperties(sqlResultSet->getString("module"),
						sqlResultSet->getString("name"), sqlResultSet->getString("config")));
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
unsigned long Database::addUrlList(unsigned long websiteId, const crawlservpp::Struct::UrlListProperties& listProperties) {
	unsigned long result = 0;

	// check arguments
	if(!websiteId)
		throw Database::Exception("Main::Database::addUrlList(): No website ID specified");
	if(listProperties.nameSpace.empty())
		throw Database::Exception("Main::Database::addUrlList(): No URL list namespace specified");
	if(listProperties.name.empty())
		throw Database::Exception("Main::Database::addUrlList(): No URL list name specified");

	// get website namespace
	std::string websiteNamespace = this->getWebsiteNamespace(websiteId);

	// check URL list namespace
	if(this->isUrlListNamespace(websiteId, listProperties.nameSpace))
		throw Database::Exception("Main::Database::addUrlList(): URL list namespace already exists");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement for adding URL list
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"INSERT INTO crawlserv_urllists(website, namespace, name) VALUES (?, ?, ?)"));

		// execute SQL query for adding URL list
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setString(2, listProperties.nameSpace);
		sqlStatement->setString(3, listProperties.name);
		sqlStatement->execute();

		// get ID of newly added URL list
		result = this->getLastInsertedId();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::addUrlList", e); }

	// create table for URL list
	std::vector<TableColumn> columns;
	columns.reserve(11);
	columns.push_back(TableColumn("manual", "BOOLEAN DEFAULT FALSE NOT NULL"));
	columns.push_back(TableColumn("url", "VARCHAR(2000) NOT NULL"));
	columns.push_back(TableColumn("hash", "INT UNSIGNED DEFAULT 0 NOT NULL", true));
	columns.push_back(TableColumn("crawled", "BOOLEAN DEFAULT FALSE NOT NULL"));
	columns.push_back(TableColumn("parsed", "BOOLEAN DEFAULT FALSE NOT NULL"));
	columns.push_back(TableColumn("extracted", "BOOLEAN DEFAULT FALSE NOT NULL"));
	columns.push_back(TableColumn("analyzed", "BOOLEAN DEFAULT FALSE NOT NULL"));
	columns.push_back(TableColumn("crawllock", "DATETIME DEFAULT NULL"));
	columns.push_back(TableColumn("parselock", "DATETIME DEFAULT NULL"));
	columns.push_back(TableColumn("extractlock", "DATETIME DEFAULT NULL"));
	columns.push_back(TableColumn("analyzelock", "DATETIME DEFAULT NULL"));
	this->createTable("crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace, columns, false);
	columns.clear();

	// create table for crawled content
	columns.reserve(6);
	columns.push_back(TableColumn("url", "BIGINT UNSIGNED NOT NULL",
			"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace, "id"));
	columns.push_back(TableColumn("crawltime", "DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP NOT NULL", true));
	columns.push_back(TableColumn("archived", "BOOLEAN DEFAULT FALSE NOT NULL"));
	columns.push_back(TableColumn("response", "SMALLINT UNSIGNED NOT NULL DEFAULT 0"));
	columns.push_back(TableColumn("type", "TINYTEXT NOT NULL"));
	columns.push_back(TableColumn("content", "LONGTEXT NOT NULL"));
	this->createTable("crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace + "_crawled", columns, true);
	columns.clear();

	// create table for linkage information
	columns.reserve(3);
	columns.push_back(TableColumn("url", "BIGINT UNSIGNED NOT NULL",
			"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace, "id"));
	columns.push_back(TableColumn("fromurl", "BIGINT UNSIGNED NOT NULL",
			"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace, "id"));
	columns.push_back(TableColumn("tourl", "BIGINT UNSIGNED NOT NULL",
			"crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace, "id"));
	this->createTable("crawlserv_" + websiteNamespace + "_" + listProperties.nameSpace + "_links", columns, false);
	columns.clear();

	return result;
}

// get URL lists for ID-specified website from the database
std::vector<std::pair<unsigned long, std::string>> Database::getUrlLists(unsigned long websiteId) {
	std::vector<std::pair<unsigned long, std::string>> result;

	// check arguments
	if(!websiteId) throw Database::Exception("Main::Database::getUrlLists(): No website ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT id, namespace FROM crawlserv_urllists WHERE website = ?"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get results
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next())
				result.push_back(std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("namespace")));
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getUrlLists", e); }

	return result;
}

// get namespace of URL list by its ID
std::string Database::getUrlListNamespace(unsigned long listId) {
	std::string result;

	// check argument
	if(!listId) throw Database::Exception("Main::Database::getUrlListNamespace(): No URL list ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT namespace FROM crawlserv_urllists WHERE id = ? LIMIT 1"));

		// execute SQL query
		sqlStatement->setUInt64(1, listId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("namespace");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getUrlListNamespace", e); }

	return result;
}

// get ID and namespace of URL list from the database using a custom table type and ID
std::pair<unsigned long, std::string> Database::getUrlListNamespaceFromCustomTable(const std::string& type, unsigned long tableId) {
	unsigned long urlListId = 0;

	// check argument
	if(type.empty())
		throw Database::Exception("Main::Database::getUrlListNamespaceFromCustomTable(): No table type specified");
	if(!tableId)
		throw Database::Exception("Main::Database::getUrlListNamespaceFromCustomTable(): No table ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT urllist FROM `crawlserv_" + type + "tables` WHERE id = ? LIMIT 1"));

		// execute SQL query
		sqlStatement->setUInt64(1, tableId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) urlListId = sqlResultSet->getUInt64("urllist");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getUrlListNamespaceFromCustomTable", e); }

	return std::pair<unsigned long, std::string>(urlListId, this->getUrlListNamespace(urlListId));
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
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS (SELECT 1 FROM crawlserv_urllists WHERE website = ? AND namespace = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setString(2, nameSpace);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isUrlListNamespace", e); }

	return result;
}

// update URL list (and all associated tables) in the database
void Database::updateUrlList(unsigned long listId, const crawlservpp::Struct::UrlListProperties& listProperties) {
	// check arguments
	if(!listId)
		throw Database::Exception("Main::Database::updateUrlList(): No website ID specified");
	if(listProperties.nameSpace.empty())
		throw Database::Exception("Main::Database::updateUrlList(): No URL list namespace specified");
	if(listProperties.name.empty())
		throw Database::Exception("Main::Database::updateUrlList(): No URL list name specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string oldListNamespace = this->getUrlListNamespace(listId);

	// check website namespace if necessary
	if(listProperties.nameSpace != oldListNamespace)
		if(this->isUrlListNamespace(websiteNamespace.first, listProperties.nameSpace))
			throw Database::Exception("Main::Database::updateUrlList(): URL list namespace already exists");

	// check connection
	this->checkConnection();

	try {
		if(listProperties.nameSpace != oldListNamespace) {
			// create SQL statement for renaming
			std::unique_ptr<sql::Statement> renameStatement(this->connection->createStatement());

			// rename main tables
			renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
					+ "` RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace + "`");
			renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
					+ "_crawled` RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace + "_crawled`");
			renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
					+ "_links` RENAME TO `crawlserv_" + websiteNamespace.second + "_" + listProperties.nameSpace + "_links`");

			// rename parsing tables
			std::vector<std::pair<unsigned long, std::string>> parsedTables = this->getCustomTables("parsed", listId);
			for(auto taI = parsedTables.begin(); taI != parsedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
						+ "_parsed_" + taI->second + "` RENAME TO `crawlserv_" + websiteNamespace.second + "_"
						+ listProperties.nameSpace	+ "_parsed_" + taI->second + "`");
			}

			// rename extracting tables
			std::vector<std::pair<unsigned long, std::string>> extractedTables = this->getCustomTables("extracted", listId);
			for(auto taI = extractedTables.begin(); taI != extractedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
						+ "_extracted_"	+ taI->second + "` RENAME TO `crawlserv_" + websiteNamespace.second + "_"
						+ listProperties.nameSpace + "_extracted_"	+ taI->second + "`");
			}

			// rename analyzing tables
			std::vector<std::pair<unsigned long, std::string>> analyzedTables = this->getCustomTables("analyzed", listId);
			for(auto taI = analyzedTables.begin(); taI != analyzedTables.end(); ++taI) {
				renameStatement->execute("ALTER TABLE `crawlserv_" + websiteNamespace.second + "_" + oldListNamespace
						+ "_analyzed_" + taI->second + "` RENAME TO `crawlserv_" + websiteNamespace.second + "_"
						+ listProperties.nameSpace + "_analyzed_" + taI->second + "`");
			}

			// create SQL statement for updating
			std::unique_ptr<sql::PreparedStatement> updateStatement(this->connection->prepareStatement(
					"UPDATE crawlserv_urllists SET namespace = ?, name = ? WHERE id = ? LIMIT 1"));

			// execute SQL statement for updating
			updateStatement->setString(1, listProperties.nameSpace);
			updateStatement->setString(2, listProperties.name);
			updateStatement->setUInt64(3, listId);
			updateStatement->execute();
		}
		else {
			// create SQL statement for updating
			std::unique_ptr<sql::PreparedStatement> updateStatement(this->connection->prepareStatement(
					"UPDATE crawlserv_urllists SET name = ? WHERE id = ? LIMIT 1"));

			// execute SQL statement for updating
			updateStatement->setString(1, listProperties.name);
			updateStatement->setUInt64(2, listId);
			updateStatement->execute();
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateUrlList", e); }
}

// delete URL list (and all associated data) from the database by its ID
void Database::deleteUrlList(unsigned long listId) {
	// check argument
	if(!listId) throw Database::Exception("Main::Database::deleteUrlList(): No URL list ID specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string listNamespace = this->getUrlListNamespace(listId);

	// delete parsing tables
	std::vector<std::pair<unsigned long, std::string>> parsingTables = this->getCustomTables("parsed", listId);
	for(auto taI = parsingTables.begin(); taI != parsingTables.end(); ++taI)
		this->deleteCustomTable("parsed", taI->first);

	// delete extracting tables
	std::vector<std::pair<unsigned long, std::string>> extractingTables = this->getCustomTables("extracted", listId);
	for(auto taI = extractingTables.begin(); taI != extractingTables.end(); ++taI)
		this->deleteCustomTable("extracted", taI->first);

	// delete analyzing tables
	std::vector<std::pair<unsigned long, std::string>> analyzingTables = this->getCustomTables("analyzed", listId);
	for(auto taI = analyzingTables.begin(); taI != analyzingTables.end(); ++taI)
		this->deleteCustomTable("analyzed", taI->first);

	// check connection
	this->checkConnection();

	try {
		// create SQL statement for deleting URL list
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"DELETE FROM crawlserv_urllists WHERE id = ? LIMIT 1"));

		// execute SQL statement for deleting URL list
		sqlStatement->setUInt64(1, listId);
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteUrlList", e); }

	// reset auto-increment if table is empty
	if(this->isTableEmpty("crawlserv_urllists")) this->resetAutoIncrement("crawlserv_urllists");

	// delete tables
	this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_links");
	this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace + "_crawled");
	this->deleteTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace);
}

// reset parsing status of ID-specified URL list
void Database::resetParsingStatus(unsigned long listId) {
	// check argument
	if(!listId) throw Database::Exception("Main::Database::resetParsingStatus(): No URL list ID specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string listNamespace = this->getUrlListNamespace(listId);

	// lock URL list
	this->lockTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace);

	try {
		try {
			// update URL list
			this->execute("UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace
					+ "` SET parsed = FALSE, parselock = NULL");
		}
		// any exeption: try to release table lock and re-throw
		catch(...) {
			try { this->releaseLocks(); }
			catch(...) {}
			throw;
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetParsingStatus", e); }

	// release lock
	this->releaseLocks();
}

// reset extracting status of ID-specified URL list
void Database::resetExtractingStatus(unsigned long listId) {
	// check argument
	if(!listId) throw Database::Exception("Main::Database::resetExtractingStatus(): No URL list ID specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string listNamespace = this->getUrlListNamespace(listId);

	// lock URL list
	this->lockTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace);

	try {
		try {
			// update URL list
			this->execute("UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace
					+ "` SET extracted = FALSE, extractlock = NULL");
		}
		// any exeption: try to release table lock and re-throw
		catch(...) {
			try { this->releaseLocks(); }
			catch(...) {}
			throw;
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetExtractingStatus", e); }

	// release lock
	this->releaseLocks();
}

// reset analyzing status of ID-specified URL list
void Database::resetAnalyzingStatus(unsigned long listId) {
	// check argument
	if(!listId) throw Database::Exception("Main::Database::resetAnalyzingStatus(): No URL list ID specified");

	// get website namespace and URL list name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromUrlList(listId);
	std::string listNamespace = this->getUrlListNamespace(listId);

	// lock URL list
	this->lockTable("crawlserv_" + websiteNamespace.second + "_" + listNamespace);

	try {
		try {
			// update URL list
			this->execute("UPDATE `crawlserv_" + websiteNamespace.second + "_" + listNamespace
					+ "` SET analyzed = FALSE, analyzelock = NULL");
		}
		// any exeption: try to release table lock and re-throw
		catch(...) {
			try { this->releaseLocks(); }
			catch(...) {}
			throw;
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetAnalyzingStatus", e); }

	// release lock
	this->releaseLocks();
}

/*
 * QUERY FUNCTIONS
 */

// add a query to the database and return its new ID (add global query when websiteId is zero)
unsigned long Database::addQuery(unsigned long websiteId, const crawlservpp::Struct::QueryProperties& queryProperties) {
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
		// create SQL statement for adding query
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"INSERT INTO crawlserv_queries(website, name, query, type, resultbool, resultsingle, resultmulti, textonly)"
				" VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

		// execute SQL query for adding query
		if(websiteId) sqlStatement->setUInt64(1, websiteId);
		else sqlStatement->setNull(1, 0);
		sqlStatement->setString(2, queryProperties.name);
		sqlStatement->setString(3, queryProperties.text);
		sqlStatement->setString(4, queryProperties.type);
		sqlStatement->setBoolean(5, queryProperties.resultBool);
		sqlStatement->setBoolean(6, queryProperties.resultSingle);
		sqlStatement->setBoolean(7, queryProperties.resultMulti);
		sqlStatement->setBoolean(8, queryProperties.textOnly);
		sqlStatement->execute();

		// get id
		result = this->getLastInsertedId();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::addQuery", e); }

	return result;
}

// get the properties of a query from the database by its ID
void Database::getQueryProperties(unsigned long queryId, crawlservpp::Struct::QueryProperties& queryPropertiesTo) {
	// check argument
	if(!queryId) throw Database::Exception("Main::Database::getQueryProperties(): No query ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT name, query, type, resultbool, resultsingle, resultmulti, textonly"
				" FROM crawlserv_queries WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

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
void Database::updateQuery(unsigned long queryId, const crawlservpp::Struct::QueryProperties& queryProperties) {
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
		// create SQL statement for updating
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"UPDATE crawlserv_queries SET name = ?, query = ?, type = ?, resultbool = ?,"
				" resultsingle = ?, resultmulti = ?, textonly = ? WHERE id = ? LIMIT 1"));

		// execute SQL statement for updating
		sqlStatement->setString(1, queryProperties.name);
		sqlStatement->setString(2, queryProperties.text);
		sqlStatement->setString(3, queryProperties.type);
		sqlStatement->setBoolean(4, queryProperties.resultBool);
		sqlStatement->setBoolean(5, queryProperties.resultSingle);
		sqlStatement->setBoolean(6, queryProperties.resultMulti);
		sqlStatement->setBoolean(7, queryProperties.textOnly);
		sqlStatement->setUInt64(8, queryId);
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateQuery", e); }
}

// delete query from the database by its ID
void Database::deleteQuery(unsigned long queryId) {
	// check argument
	if(!queryId) throw Database::Exception("Main::Database::deleteQuery(): No query ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"DELETE FROM crawlserv_queries WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		sqlStatement->execute();

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_queries")) this->resetAutoIncrement("crawlserv_queries");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteQuery", e); }
}

// duplicate query in the database by its ID
unsigned long Database::duplicateQuery(unsigned long queryId) {
	unsigned long result = 0;

	// check argument
	if(!queryId) throw Database::Exception("Main::Database::duplicateQuery(): No query ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement for getting query info
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT website, name, query, type, resultbool, resultsingle, resultmulti,"
				" textonly FROM crawlserv_queries WHERE id = ? LIMIT 1"));

		// execute SQL statement for getting query info
		sqlStatement->setUInt64(1, queryId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			// add query
			result = this->addQuery(sqlResultSet->getUInt64("website"), crawlservpp::Struct::QueryProperties(
					sqlResultSet->getString("name") + " (copy)", sqlResultSet->getString("query"),
					sqlResultSet->getString("type"), sqlResultSet->getBoolean("resultbool"),
					sqlResultSet->getBoolean("resultsingle"), sqlResultSet->getBoolean("resultmulti"),
					sqlResultSet->getBoolean("textonly")));
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::duplicateQuery", e); }

	return result;
}

/*
 * CONFIGURATION FUNCTIONS
 */

// add a configuration to the database and return its new ID (add global configuration when websiteId is zero)
unsigned long Database::addConfiguration(unsigned long websiteId, const crawlservpp::Struct::ConfigProperties& configProperties) {
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
		// create SQL statement for adding configuration
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"INSERT INTO crawlserv_configs(website, module, name, config) VALUES (?, ?, ?, ?)"));

		// execute SQL query for adding website
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setString(2, configProperties.module);
		sqlStatement->setString(3, configProperties.name);
		sqlStatement->setString(4, configProperties.config);
		sqlStatement->execute();

		// get id
		result = this->getLastInsertedId();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::addConfiguration", e); }

	return result;
}

// get a configuration from the database by its ID
const std::string Database::getConfiguration(unsigned long configId) {
	std::string result;

	// check argument
	if(!configId) throw Database::Exception("Main::Database::getConfiguration(): No configuration ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT config FROM crawlserv_configs WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("config");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getConfiguration", e); }

	return result;
}

// update configuration in the database (NOTE: module will not be updated!)
void Database::updateConfiguration(unsigned long configId, const crawlservpp::Struct::ConfigProperties& configProperties) {
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
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"UPDATE crawlserv_configs SET name = ?, config = ? WHERE id = ? LIMIT 1"));

		// execute SQL statement for updating
		sqlStatement->setString(1, configProperties.name);
		sqlStatement->setString(2, configProperties.config);
		sqlStatement->setUInt64(3, configId);
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateConfiguration", e); }
}

// delete configuration from the database by its ID
void Database::deleteConfiguration(unsigned long configId) {
	// check argument
	if(!configId) throw Database::Exception("Main::Database::deleteConfiguration(): No configuration ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"DELETE FROM crawlserv_configs WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		sqlStatement->execute();

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_configs")) this->resetAutoIncrement("crawlserv_configs");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteConfiguration", e); }
}

// duplicate configuration in the database by its ID
unsigned long Database::duplicateConfiguration(unsigned long configId) {
	unsigned long result = 0;

	// check argument
	if(!configId) throw Database::Exception("Main::Database::duplicateConfiguration(): No configuration ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement for getting configuration info
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT website, module, name, config FROM crawlserv_configs WHERE id = ? LIMIT 1"));

		// execute SQL statement for getting configuration info
		sqlStatement->setUInt64(1, configId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			// add configuration
			result = this->addConfiguration(sqlResultSet->getUInt64("website"), crawlservpp::Struct::ConfigProperties(
					sqlResultSet->getString("module"), sqlResultSet->getString("name") + " (copy)",
					sqlResultSet->getString("config")));
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::duplicateConfiguration", e); }

	return result;
}

/*
 * CUSTOM TABLE FUNCTIONS
 */

// lock custom tables of the specified type
void Database::lockCustomTables(const std::string& type, unsigned long websiteId, unsigned long listId, unsigned long timeOut) {
	// check arguments
	if(type.empty()) throw Database::Exception("Main::Database::lockCustomTables(): No table type specified");
	if(!websiteId) throw Database::Exception("Main::Database::lockCustomTables(): No website ID specified");
	if(!listId) throw Database::Exception("Main::Database::lockCustomTables(): No website ID specified");
	if(!timeOut) throw Database::Exception("Main::Database::lockCustomTables(): No lock time-out specified");

	// remove previous lock if necessary
	this->unlockCustomTables(type);

	while(true) { // do not continue until lock is gone or time-out occured
		// check connection
		this->checkConnection();

		// lock table
		this->lockTable("crawlserv_targetlocks");

		try {
			try {
				// create SQL statement for checking target table lock
				std::unique_ptr<sql::PreparedStatement> checkStatement(this->connection->prepareStatement(
						"SELECT EXISTS(SELECT 1 FROM crawlserv_targetlocks"
						" WHERE tabletype = ? AND website = ? AND urllist = ? AND locktime > NOW() LIMIT 1"
						") AS result"));

				// execute SQL statement
				checkStatement->setString(1, type);
				checkStatement->setUInt64(2, websiteId);
				checkStatement->setUInt64(3, listId);
				std::unique_ptr<sql::ResultSet> sqlResultSet(checkStatement->executeQuery());

				// get result
				if(sqlResultSet && sqlResultSet->next() && sqlResultSet->getBoolean("result")) {
					// target table lock is active: unlock table, sleep and try again
					this->unlockTables();
					std::this_thread::sleep_for(std::chrono::seconds(MAIN_DATABASE_SLEEP_ON_LOCK_SECONDS));
					continue;
				}

				// create SQL statement for inserting target table lock
				std::unique_ptr<sql::PreparedStatement> insertStatement(this->connection->prepareStatement(
						"INSERT INTO crawlserv_targetlocks(tabletype, website, urllist, locktime)"
						" VALUES (?, ?, ?, NOW() + INTERVAL ? SECOND)"));

				// execute SQL statement
				insertStatement->setString(1, type);
				insertStatement->setUInt64(2, websiteId);
				insertStatement->setUInt64(3, listId);
				insertStatement->setUInt64(4, timeOut);
				insertStatement->execute();

				// save lock ID
				this->customTableLocks.push_back(std::pair<std::string, unsigned long>(type, this->getLastInsertedId()));
			}
			// any exception: try to unlock table and re-throw
			catch(...) {
				try { this->unlockTables(); }
				catch(...) {}
				throw;
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::lockCustomTables", e); }

		// unlock table and break
		this->unlockTables();
		break;
	}
}

// add a custom table of the specified type to the database if such a table does not exist already, return table ID
unsigned long Database::addCustomTable(const CustomTableProperties& properties) {
	unsigned long result = 0;

	// check arguments
	if(properties.type.empty())
		throw Database::Exception("Main::Database::addCustomTable(): No table type specified");
	if(!properties.website)
		throw Database::Exception("Main::Database::addCustomTable(): No website ID specified");
	if(!properties.urlList)
		throw Database::Exception("Main::Database::addCustomTable(): No URL list ID specified");
	if(properties.name.empty())
		throw Database::Exception("Main::Database::addCustomTable(): No table name specified");
	if(properties.columns.empty())
		throw Database::Exception("Main::Database::addCustomTable(): No columns specified");

	// check connection
	this->checkConnection();

	try {
		// check whether table exists
		if(this->isTableExists(properties.fullName)) {
			// table exists already: add columns that do not exist yet and check columns that do exist
			for(auto i = properties.columns.begin(); i != properties.columns.end(); ++i) {
				if(!(i->name.empty())) {
					std::string columnName = properties.type + "_" + i->name;
					if(this->isColumnExists(properties.fullName, columnName)) {
						// column does exist: check type

					}
					else {
						// column does not exist: add column
						this->addColumn(properties.fullName, TableColumn(columnName, *i));
					}
				}
			}

			// compress table if necessary
			if(properties.compressed) this->compressTable(properties.fullName);
		}
		else {
			// table does not exist: create table
			this->createTable(properties.fullName, properties.columns, properties.compressed);
		}

		// create SQL statement for checking for entry
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT id FROM `crawlserv_" + properties.type + "tables` WHERE website = ? AND urllist = ? AND name = ? LIMIT 1"));

		// execute SQL statement for checking for entry
		sqlStatement->setUInt64(1, properties.website);
		sqlStatement->setUInt64(2, properties.urlList);
		sqlStatement->setString(3, properties.name);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		if(sqlResultSet && sqlResultSet->next())
			// entry exists: return ID
			result = sqlResultSet->getUInt64("id");
		else {
			// entry does not exist already: create SQL statement for adding table
			sqlStatement.reset(this->connection->prepareStatement(
					"INSERT INTO `crawlserv_" + properties.type + "tables`(website, urllist, name) VALUES (?, ?, ?)"));

			// execute SQL statement for adding table
			sqlStatement->setUInt64(1, properties.website);
			sqlStatement->setUInt64(2, properties.urlList);
			sqlStatement->setString(3, properties.name);
			sqlStatement->execute();

			// return ID of newly inserted table
			result = this->getLastInsertedId();
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::addCustomTable", e); }

	return result;
}

// get custom tables of the specified type for an ID-specified URL list from the database
std::vector<std::pair<unsigned long, std::string>> Database::getCustomTables(const std::string& type, unsigned long listId) {
	std::vector<std::pair<unsigned long, std::string>> result;

	// check arguments
	if(type.empty()) throw Database::Exception("Main::Database::getCustomTables(): No table type specified");
	if(!listId) throw Database::Exception("Main::Database::getCustomTables(): No URL list ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT id, name FROM `crawlserv_" + type + "tables` WHERE urllist = ?"));

		// execute SQL statement
		sqlStatement->setUInt64(1, listId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get results
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next())
				result.push_back(std::pair<unsigned long, std::string>(
						sqlResultSet->getUInt64("id"), sqlResultSet->getString("name")));
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomTables", e); }

	return result;
}

// get the ID of a custom table of the specified type from the database by its website ID, URL list ID and table name
unsigned long Database::getCustomTableId(const std::string& type, unsigned long websiteId, unsigned long listId,
		const std::string& tableName) {
	unsigned long result = 0;

	// check arguments
	if(type.empty()) throw Database::Exception("Main::Database::getCustomTableId(): No table type specified");
	if(!websiteId) throw Database::Exception("Main::Database::getCustomTableId(): No website ID specified");
	if(!listId) throw Database::Exception("Main::Database::getCustomTableId(): No URL list ID specified");
	if(tableName.empty()) throw Database::Exception("Main::Database::getCustomTableId(): No table name specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT id FROM `crawlserv_" + type + "tables` WHERE website = ? AND urllist = ? AND name = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, listId);
		sqlStatement->setString(3, tableName);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("id");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomTableId", e); }

	return result;
}

// get the name of a custom table of the specified type from the database by its ID
std::string Database::getCustomTableName(const std::string& type, unsigned long tableId) {
	std::string result;

	// check arguments
	if(type.empty()) throw Database::Exception("Main::Database::getCustomTable(): No table type specified");
	if(!tableId) throw Database::Exception("Main::Database::getCustomTable(): No table ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT name FROM `crawlserv_" + type + "tables` WHERE id = ? LIMIT 1"));

		// execute SQL statement
		sqlStatement->setUInt64(1, tableId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("name");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomTable", e); }

	return result;
}

// delete custom table of the specified type from the database by its ID
void Database::deleteCustomTable(const std::string& type, unsigned long tableId) {
	// check arguments
	if(type.empty()) throw Database::Exception("Main::Database::deleteCustomTable(): No table type specified");
	if(!tableId) throw Database::Exception("Main::Database::deleteCustomTable(): No table ID specified");

	// get namespace, URL list name and table name
	std::pair<unsigned long, std::string> websiteNamespace = this->getWebsiteNamespaceFromCustomTable(type, tableId);
	std::pair<unsigned long, std::string> listNamespace = this->getUrlListNamespaceFromCustomTable(type, tableId);
	std::string tableName = this->getCustomTableName(type, tableId);

	// check connection
	this->checkConnection();

	try {
		// create SQL statement for deletion
		std::unique_ptr<sql::PreparedStatement> deleteStatement(this->connection->prepareStatement(
				"DELETE FROM `crawlserv_" + type + "tables` WHERE id = ? LIMIT 1"));

		// execute SQL statement for deletion
		deleteStatement->setUInt64(1, tableId);
		deleteStatement->execute();

		// create SQL statement for dropping table
		std::unique_ptr<sql::Statement> dropStatement(this->connection->createStatement());

		// execute SQL query for dropping table
		dropStatement->execute("DROP TABLE IF EXISTS `crawlserv_" + websiteNamespace.second + "_" + listNamespace.second
				+ "_" + type + "_" + tableName + "`");

		// reset auto-increment if table is empty
		if(this->isTableEmpty("crawlserv_" + type + "tables")) this->resetAutoIncrement("crawlserv_" + type + "tables");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteCustomTable", e); }
}

// unlock custom tables of the specified type
void Database::unlockCustomTables(const std::string& type) {
	auto lockIt = std::find_if(this->customTableLocks.begin(), this->customTableLocks.end(),
			[&type](const std::pair<std::string, unsigned long>& element) {
		return element.first == type;
	});
	if(lockIt != this->customTableLocks.end()) {
		// check connection
		this->checkConnection();

		try {
			// create SQL statement
			std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
					"DELETE FROM crawlserv_targetlocks WHERE id = ? LIMIT 1"));

			// execute SQL statement
			sqlStatement->setUInt64(1, lockIt->second);
			sqlStatement->execute();
		}
		catch(const sql::SQLException &e) { this->sqlException("Main::Database::unlockParsingTables", e); }

		lockIt->second = 0;
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

// check whether the connection to the database is still valid and try to re-connect if necesssary, throws Database::Exception
//  WARNING: afterwards, old references to prepared SQL statements may be invalid!
void Database::checkConnection() {
	// check driver
	if(!(this->driver))
		throw Database::Exception("Main::Database::checkConnection(): MySQL driver not loaded");

	try {
		// check connection
		if(this->connection) {
#ifdef MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS
			// check whether re-connect should be performed anyway
			auto milliseconds = this->reconnectTimer.tick();
			if(milliseconds < MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS * 1000) {
#endif
				// check whether connection is valid
				if(this->connection->isValid()) return;
				milliseconds = 0;

#ifdef MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS
			}
#endif

			// try to re-connect
			if(!(this->connection->reconnect())) {
				// simple re-connect failed: try to reset connection after sleeping over it for some time
				this->connection.reset();

				try {
					this->connect();
				}
				catch(const Database::Exception& e) {
					if(this->sleepOnError) std::this_thread::sleep_for(std::chrono::seconds(this->sleepOnError));
					this->connect();
				}
			}

#ifdef MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS
			// log re-connect on idle
			if(milliseconds) this->log("re-connected to database after idling for "
					+ crawlservpp::Helper::DateTime::secondsToString(std::round((float) milliseconds / 1000)) + ".");
#endif
		}
		else this->connect();

		// recover prepared SQL statements
		for(auto i = this->preparedStatements.begin(); i != this->preparedStatements.end(); ++i)
			i->refresh(this->connection.get());
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::checkConnection", e); }
}

// check whether a website ID is valid
bool Database::isWebsite(unsigned long websiteId) {
	bool result = false;

	// check argument
	if(!websiteId) throw Database::Exception("Main::Database::isWebsite(): No website ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS(SELECT 1 FROM crawlserv_websites WHERE id = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isWebsite", e); }

	return result;
}

// check whether a URL list ID is valid
bool Database::isUrlList(unsigned long urlListId) {
	bool result = false;

	// check argument
	if(!urlListId) throw Database::Exception("Main::Database::isUrlList(): No URL list ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS(SELECT 1 FROM crawlserv_urllists WHERE id = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setUInt64(1, urlListId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isUrlList", e); }

	return result;
}

// check whether a URL list ID is valid for the ID-specified website
bool Database::isUrlList(unsigned long websiteId, unsigned long urlListId) {
	bool result = false;

	// check arguments
	if(!websiteId) throw Database::Exception("Main::Database::isUrlList(): No website ID specified");
	if(!urlListId) throw Database::Exception("Main::Database::isUrlList(): No URL list ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS(SELECT 1 FROM crawlserv_urllists WHERE website = ? AND id = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, urlListId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isUrlList", e); }

	return result;
}

// check whether a query ID is valid
bool Database::isQuery(unsigned long queryId) {
	bool result = false;

	// check argument
	if(!queryId) throw Database::Exception("Main::Database::isQuery(): No query ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS(SELECT 1 FROM crawlserv_queries WHERE id = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setUInt64(1, queryId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isQuery", e); }

	return result;
}

// check whether a query ID is valid for the ID-specified website (look for global queries if websiteID is zero)
bool Database::isQuery(unsigned long websiteId, unsigned long queryId) {
	bool result = false;

	// check arguments
	if(!queryId) throw Database::Exception("Main::Database::isQuery(): No query ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS(SELECT 1 FROM crawlserv_queries WHERE (website = ? OR website = 0) AND id = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, queryId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isQuery", e); }

	return result;
}

// check whether a configuration ID is valid
bool Database::isConfiguration(unsigned long configId) {
	bool result = false;

	// check argument
	if(!configId) throw Database::Exception("Main::Database::isConfiguration(): No configuration ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS(SELECT 1 FROM crawlserv_configs WHERE id = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setUInt64(1, configId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isConfiguration", e); }

	return result;
}

// check whether a configuration ID is valid for the ID-specified website (look for global configurations if websiteId is zero)
bool Database::isConfiguration(unsigned long websiteId, unsigned long configId) {
	bool result = false;

	// check arguments
	if(!configId) throw Database::Exception("Main::Database::isConfiguration(): No configuration ID specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT EXISTS(SELECT 1 FROM crawlserv_configs WHERE website = ? AND id = ? LIMIT 1) AS result"));

		// execute SQL statement
		sqlStatement->setUInt64(1, websiteId);
		sqlStatement->setUInt64(2, configId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isConfiguration", e); }

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
	if(data.type == crawlservpp::Main::Data::Type::_unknown)
		throw Database::Exception("Main::Database::getCustomData(): No column type specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(
				"SELECT `" + data.column + "` FROM `" + data.table + "` WHERE (" + data.condition + ")"));

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
					throw Database::Exception("Main::Database::getCustomData(): Invalid data type when getting custom data.");
				}
			}
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
}

// get custom values from multiple fields of a row in the database
void Database::getCustomData(Data::GetFields& data) {
	std::string sqlQuery;

	// check argument
	if(data.columns.empty())
		throw Database::Exception("Main::Database::getCustomData(): No column names specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown)
		throw Database::Exception("Main::Database::getCustomData(): No column type specified");
	data.values.clear();
	data.values.reserve(data.columns.size());

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// create SQL query
		sqlQuery = "SELECT ";
		for(auto i = data.columns.begin(); i != data.columns.end(); ++i) {
			sqlQuery += "`" + *i + "`, ";
		}
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " FROM `" + data.table + "` WHERE (" + data.condition + ")";

		// execute SQL statement
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(sqlQuery));

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
				throw Database::Exception("Main::Database::getCustomData(): Invalid data type when getting custom data.");
			}
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getCustomData", e); }
}

// get custom values from multiple fields of a row with different types in the database
void Database::getCustomData(Data::GetFieldsMixed& data) {
	std::string sqlQuery;

	// check argument
	if(data.columns_types.empty()) throw Database::Exception("Main::Database::getCustomData(): No columns specified");
	data.values.clear();
	data.values.reserve(data.columns_types.size());

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// create SQL query
		sqlQuery = "SELECT ";
		for(auto i = data.columns_types.begin(); i != data.columns_types.end(); ++i) {
			sqlQuery += "`" + i->first + "`, ";
		}
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " FROM `" + data.table + "` WHERE (" + data.condition + ")";

		// execute SQL statement
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(sqlQuery));

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
	std::string sqlQuery;

	// check argument
	if(data.column.empty())
		throw Database::Exception("Main::Database::getCustomData(): No columns specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown)
		throw Database::Exception("Main::Database::getCustomData(): No column type specified");
	data.values.clear();

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// create SQL query
		sqlQuery = "SELECT `" + data.column + "` FROM `" + data.table + "`";
		if(!data.condition.empty()) sqlQuery += " WHERE (" + data.condition + ")";
		if(!data.order.empty()) sqlQuery += " ORDER BY (" + data.order + ")";

		// execute SQL statement
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(sqlQuery));

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
	std::string sqlQuery;

	// check argument
	if(data.columns.empty())
		throw Database::Exception("Main::Database::getCustomData(): No column name specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown)
		throw Database::Exception("Main::Database::getCustomData(): No column type specified");
	data.values.clear();
	data.values.reserve(data.columns.size());

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

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
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(sqlQuery));

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
	std::string sqlQuery;

	// check argument
	if(data.columns_types.empty()) throw Database::Exception("Main::Database::getCustomData(): No columns specified");
	data.values.clear();
	data.values.reserve(data.columns_types.size());

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

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
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(sqlQuery));

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
	if(data.type == crawlservpp::Main::Data::Type::_unknown)
		throw Database::Exception("Main::Database::insertCustomData(): No column type specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"INSERT INTO `" + data.table + "` (`" + data.column + "`) VALUES (?)"));

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
						std::ostringstream errStrStr;
						errStrStr.imbue(std::locale(""));
						errStrStr << "Main::Database::insertCustomData(): Size (" << data.value._s.size()
								<< " bytes) of custom value for `" << data.table << "`.`" << data.column << "` exceeds the ";
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
				throw Database::Exception("Main::Database::insertCustomData(): Invalid data type when inserting custom data.");
			}
		}

		// execute SQL statement
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::insertCustomData", e); }
}

// insert custom values into multiple fields of the same type into a row in the database
void Database::insertCustomData(const Data::InsertFields& data) {
	std::string sqlQuery;

	// check argument
	if(data.columns_values.empty())
		throw Database::Exception("Main::Database::insertCustomData(): No columns specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown)
		throw Database::Exception("Main::Database::insertCustomData(): No column type specified");

	// check connection
	this->checkConnection();

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
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(sqlQuery));

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
						std::ostringstream errStrStr;
						errStrStr.imbue(std::locale(""));
						errStrStr << "Main::Database::insertCustomData(): Size (" << i->second._s.size()
								<< " bytes) of custom value for `" << data.table << "`.`" << i->first
								<< "` exceeds the ";
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
			throw Database::Exception("Main::Database::insertCustomData(): Invalid data type when inserting custom data.");
		}

		// execute SQL statement
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::insertCustomData", e); }
}

// insert custom values into multiple fields of different types into a row in the database
void Database::insertCustomData(const Data::InsertFieldsMixed& data) {
	std::string sqlQuery;

	// check argument
	if(data.columns_types_values.empty())
		throw Database::Exception("Main::Database::insertCustomData(): No columns specified");

	// check connection
	this->checkConnection();

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
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(sqlQuery));

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
							std::ostringstream errStrStr;
							errStrStr.imbue(std::locale(""));
							errStrStr << "Main::Database::insertCustomData(): Size (" << std::get<2>(*i)._s.size()
									<< " bytes) of custom value for `" << data.table << "`.`" << std::get<0>(*i)
									<< "` exceeds the ";
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
					throw Database::Exception("Main::Database::insertCustomData(): Invalid data type when inserting custom data.");
				}
			}
			counter++;
		}

		// execute SQL statement
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::insertCustomData", e); }
}

// update one custom value in one field of a row in the database
void Database::updateCustomData(const Data::UpdateValue& data) {
	// check argument
	if(data.column.empty())
		throw Database::Exception("Main::Database::updateCustomData(): No column name specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown)
		throw Database::Exception("Main::Database::updateCustomData(): No column type specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"UPDATE `" + data.table + "` SET `" + data.column + "` = ? WHERE (" + data.condition + ")"));

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
						std::ostringstream errStrStr;
						errStrStr.imbue(std::locale(""));
						errStrStr << "Main::Database::updateCustomData(): Size (" << data.value._s.size()
								<< " bytes) of custom value for `" << data.table << "`.`" << data.column
								<< "` exceeds the ";
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
				throw Database::Exception("Main::Database::updateCustomData(): Invalid data type when updating custom data.");
			}
		}

		// execute SQL statement
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateCustomData", e); }
}

// update custom values in multiple fields of a row with the same type in the database
void Database::updateCustomData(const Data::UpdateFields& data) {
	std::string sqlQuery;

	// check argument
	if(data.columns_values.empty())
		throw Database::Exception("Main::Database::updateCustomData(): No columns specified");
	if(data.type == crawlservpp::Main::Data::Type::_unknown)
		throw Database::Exception("Main::Database::updateCustomData(): No column type specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL query
		sqlQuery = "UPDATE `" + data.table + "` SET ";
		for(auto i = data.columns_values.begin(); i != data.columns_values.end(); ++i) sqlQuery += "`" + i->first + "` = ?, ";
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " WHERE (" + data.condition + ")";

		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(sqlQuery));

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
						std::ostringstream errStrStr;
						errStrStr.imbue(std::locale(""));
						errStrStr << "Main::Database::updateCustomData(): Size (" << i->second._s.size()
								<< " bytes) of custom value for `" << data.table << "`.`" << i->first
								<< "` exceeds the ";
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
			throw Database::Exception("Main::Database::updateCustomData(): Invalid data type when updating custom data.");
		}

		// execute SQL statement
		sqlStatement->execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::updateCustomData", e); }
}

// update custom values in multiple fields of a row with different types in the database
void Database::updateCustomData(const Data::UpdateFieldsMixed& data) {
	std::string sqlQuery;

	// check argument
	if(data.columns_types_values.empty()) throw Database::Exception("Main::Database::updateCustomData(): No columns specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL query
		sqlQuery = "UPDATE `" + data.table + "` SET ";
		for(auto i = data.columns_types_values.begin(); i != data.columns_types_values.end(); ++i)
			sqlQuery += "`" + std::get<0>(*i) + "` = ?, ";
		sqlQuery.pop_back();
		sqlQuery.pop_back();
		sqlQuery += " WHERE (" + data.condition + ")";

		// create SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(sqlQuery));

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
							std::ostringstream errStrStr;
							errStrStr.imbue(std::locale(""));
							errStrStr << "Main::Database::updateCustomData(): Size (" << std::get<2>(*i)._s.size()
									<< " bytes) of custom value for `" << data.table << "`.`" << std::get<0>(*i)
									<< "` exceeds the ";
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
					throw Database::Exception("Main::Database::updateCustomData(): Invalid data type when updating custom data.");
				}
			}
			counter++;
		}

		// execute SQL statement
		sqlStatement->execute();
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
	this->preparedStatements.resize(this->preparedStatements.size() + numberOfAdditionalPreparedStatements);
}

// prepare additional SQL statement, return ID of newly prepared SQL statement
unsigned short Database::addPreparedStatement(const std::string& sqlQuery) {
	// check connection
	this->checkConnection();

	// try to prepare SQL statement
	try {
		this->preparedStatements.emplace_back(
				crawlservpp::Wrapper::PreparedSqlStatement(this->connection.get(), sqlQuery));
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::addPreparedStatement", e); }

	return this->preparedStatements.size();
}

// get reference to prepared SQL statement by its ID
//  WARNING: Do not run Database::checkConnection() while using this reference!
sql::PreparedStatement& Database::getPreparedStatement(unsigned short id) {
	try { return this->preparedStatements.at(id - 1).get();	}
	catch(const sql::SQLException &e) {
		this->sqlException("Main::Database::getPreparedStatement", e);
		throw;//will not be used
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
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.lastId);

	try {
		// execute SQL statement
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("id");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::getLastInsertedId", e); }

	return result;
}

// reset the auto increment of an (empty) table in the database
void Database::resetAutoIncrement(const std::string& tableName) {
	// check argument
	if(tableName.empty()) throw Database::Exception("Main::Database::resetAutoIncrement(): No table name specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		sqlStatement->execute("ALTER TABLE `" + tableName + "` AUTO_INCREMENT = 1");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::resetAutoIncrement", e); }
}

// lock a table in the database for writing
void Database::lockTable(const std::string& tableName) {
	// check argument
	if(tableName.empty()) throw Database::Exception("Main::Database::lockTable(): No table name specified");

	// check for lock and unlock if necessary
	if(this->tablesLocked) this->unlockTables();

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		this->tablesLocked = true;
		sqlStatement->execute("LOCK TABLES `" + tableName + "` WRITE");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::lockTable", e); }
}

// lock two tables in the database for writing (plus the alias 'a' for reading the first and the alias 'b' for reading the second table)
void Database::lockTables(const std::string& tableName1, const std::string& tableName2) {
	// check arguments
	if(tableName1.empty()) throw Database::Exception("Main::Database::lockTables(): Table name #1 missing");
	if(tableName2.empty()) throw Database::Exception("Main::Database::lockTables(): Table name #2 missing");

	// check for lock and unlock if necessary
	if(this->tablesLocked) this->unlockTables();

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		this->tablesLocked = true;
		sqlStatement->execute("LOCK TABLES `" + tableName1 + "` WRITE, `" + tableName1 + "` AS a READ, `" + tableName2 + "` WRITE,"
				"`" + tableName2 + "` AS b READ");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::lockTables", e); }
}

// unlock tables in the database
void Database::unlockTables() {
	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		sqlStatement->execute("UNLOCK TABLES");
		this->tablesLocked = false;
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::unlockTables", e); }
}

// check whether a name-specified table in the database is empty
bool Database::isTableEmpty(const std::string& tableName) {
	bool result = false;

	// check argument
	if(tableName.empty()) throw Database::Exception("Main::Database::isTableEmpty(): No table name specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(
				"SELECT NOT EXISTS (SELECT 1 FROM `" + tableName + "` LIMIT 1) AS result"));

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isTableEmpty", e); }

	return result;
}

// check whether a specific table exists in the database
bool Database::isTableExists(const std::string& tableName) {
	bool result = false;

	// check argument
	if(tableName.empty()) throw Database::Exception("Main::Database::isTableExists(): No table name specified");

	// check connection
	this->checkConnection();

	try {
		// create and execute SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT COUNT(*) AS result FROM INFORMATION_SCHEMA.TABLES"
				" WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ? LIMIT 1"));

		// get result
		sqlStatement->setString(1, this->settings.name);
		sqlStatement->setString(2, tableName);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isTableExists", e); }

	return result;
}

// check whether a specific column of a specific table exists in the database
bool Database::isColumnExists(const std::string& tableName, const std::string& columnName) {
	bool result = false;

	// check arguments
	if(tableName.empty()) throw Database::Exception("Main::Database::isColumnExists(): No table name specified");
	if(columnName.empty()) throw Database::Exception("Main::Database::isColumnExists(): No column name specified");

	// check connection
	this->checkConnection();

	try {
		// create and execute SQL statement
		std::unique_ptr<sql::PreparedStatement> sqlStatement(this->connection->prepareStatement(
				"SELECT COUNT(*) AS result FROM INFORMATION_SCHEMA.COLUMNS"
				" WHERE TABLE_SCHEMA = ? AND TABLE_NAME = ? AND COLUMN_NAME = ? LIMIT 1"));

		// get result
		sqlStatement->setString(1, this->settings.name);
		sqlStatement->setString(2, tableName);
		sqlStatement->setString(3, columnName);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery());
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::isColumnExists", e); }

	return result;
}

// add a table to the database (the primary key 'id' will be created automatically)
void Database::createTable(const std::string& tableName, const std::vector<TableColumn>& columns, bool compressed) {
	// check arguments
	if(tableName.empty())
		throw Database::Exception("Main::Database::createTable(): No table name specified");
	if(columns.empty())
		throw Database::Exception("Main::Database::createTable(): No columns specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL query
		std::string sqlQuery("CREATE TABLE IF NOT EXISTS `" + tableName + "`(id SERIAL");
		std::string properties;
		for(auto i = columns.begin(); i != columns.end(); ++i) {
			// check values
			if(i->name.empty())
				throw Database::Exception("Main::Database::createTable(): A column is missing its name");
			if(i->type.empty())
				throw Database::Exception("Main::Database::createTable(): A column is missing its type");

			// add to SQL query
			sqlQuery += ", `" + i->name + "` " + i->type;

			// add indices and references
			if(i->indexed) properties += ", INDEX(`" + i->name + "`)";
			if(!(i->referenceTable.empty())) {
				if(i->referenceColumn.empty())
					throw Database::Exception("Main::Database::createTable(): A column reference is missing its source column name");
				properties += ", FOREIGN KEY(`" + i->name + "`) REFERENCES `" + i->referenceTable + "`(`" + i->referenceColumn
						+ "`) ON UPDATE RESTRICT ON DELETE CASCADE";
			}
		}
		sqlQuery += ", PRIMARY KEY(id)" + properties + ") CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
		if(compressed) sqlQuery += ", ROW_FORMAT=COMPRESSED";

		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		sqlStatement->execute(sqlQuery);
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::createTable", e); }
}

// add a column to a table in the database
void Database::addColumn(const std::string& tableName, const TableColumn& column) {
	// check arguments
	if(tableName.empty()) throw Database::Exception("Main::Database::addColumn(): No table name specified");
	if(column.name.empty()) throw Database::Exception("Main::Database::addColumn(): No column name specified");
	if(column.type.empty()) throw Database::Exception("Main::Database::addColumn(): No column type specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL query
		std::string sqlQuery = "ALTER TABLE `" + tableName + "` ADD COLUMN `" + column.name + "` " + column.type;
		if(!column.referenceTable.empty()) {
			if(column.referenceColumn.empty())
				throw Database::Exception("Main::Database::addColumn(): A column reference is missing its source column name");
			sqlQuery += ", ADD FOREIGN KEY(`" + column.name + "`) REFERENCES `" + column.referenceTable + "`(`"
					+ column.referenceColumn + "`) ON UPDATE RESTRICT ON DELETE CASCADE";
		}

		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		sqlStatement->execute(sqlQuery);
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::addColumn", e); }
}

// compress a table in the database
void Database::compressTable(const std::string& tableName) {
	bool compressed = false;

	// check argument
	if(tableName.empty()) throw Database::Exception("Main::Database::compressTable(): No table name specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement for checking whether the table is already compressed
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement->executeQuery(
				"SELECT LOWER(row_format) = 'compressed' AS result FROM information_schema.tables"
				" WHERE table_schema = '" + this->settings.name + "' AND table_name = '" + tableName + "' LIMIT 1;"));

		// get result
		if(sqlResultSet && sqlResultSet->next()) compressed = sqlResultSet->getBoolean("result");
		else throw Database::Exception("Main::Database::compressTable(): Could not determine row format of table \'" + tableName + "\'");

		// execute SQL statement for compressing the table if table is not already compressed
		if(!compressed) sqlStatement->execute("ALTER TABLE `" + tableName + "` ROW_FORMAT=COMPRESSED");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::compressTable", e); }
}

// delete a table from the database if it exists
void Database::deleteTable(const std::string& tableName) {
	// check arguments
	if(tableName.empty()) throw Database::Exception("Main::Database::deleteTableIfExists(): No table name specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		sqlStatement->execute("DROP TABLE IF EXISTS `" + tableName + "`");
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::deleteTable", e); }
}

/*
 * EXCEPTION HELPER FUNCTION (protected)
 */

// catch SQL exception and re-throw it as ConnectionException or Exception
void Database::sqlException(const std::string& function, const sql::SQLException& e) {
	// create error string
	std::ostringstream errorStrStr;
	int error = e.getErrorCode();
	errorStrStr << function << "() SQL Error #" << error << " (State " << e.getSQLState() << "): " << e.what();

	// check for connection error
	switch(error) {
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

	default:
		// throw database exception
		throw Database::Exception(errorStrStr.str());
	}
}

/*
 * INTERNAL HELPER FUNCTION (private)
 */

// run file with SQL commands
void Database::run(const std::string& sqlFile) {
	// check argument
	if(sqlFile.empty()) throw Database::Exception("Main::Database::run(): No SQL file specified");

	// open SQL file
	std::ifstream initSQLFile(sqlFile);

	if(initSQLFile.is_open()) {
		// check connection
		this->checkConnection();

		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());
		std::string line;

		if(!sqlStatement) throw Database::Exception("Main::Database::run(): Could not create SQL statement");

		// execute lines in SQL file
		unsigned long lineCounter = 1;
		while(std::getline(initSQLFile, line)) {
			try {
				if(!line.empty()) sqlStatement->execute(line);
				lineCounter++;
			}
			catch(const sql::SQLException &e) { this->sqlException("Main::Database::run", e); }
		}

		// close SQL file
		initSQLFile.close();
	}
	else throw Database::Exception("Main::Database::run(): Could not open \'" + sqlFile + "\' for reading");
}

// execute a SQL query
void Database::execute(const std::string& sqlQuery) {
	// check argument
	if(sqlQuery.empty()) throw Database::Exception("Main::Database::execute(): No SQL query specified");

	// check connection
	this->checkConnection();

	try {
		// create SQL statement
		std::unique_ptr<sql::Statement> sqlStatement(this->connection->createStatement());

		// execute SQL statement
		sqlStatement->execute(sqlQuery);
	}
	catch(const sql::SQLException &e) { this->sqlException("Main::Database::execute", e); }
}

}
