/*
 * Database.cpp
 *
 * This class provides database functionality for a parser thread by implementing the Module::DBWrapper interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

namespace crawlservpp::Module::Parser {

// constructor: initialize values
Database::Database(crawlservpp::Module::DBThread& dbThread) : crawlservpp::Module::DBWrapper(dbThread) {
	this->website = 0;
	this->urlList = 0;
	this->reparse = false;
	this->parseCustom = false;
	this->logging = false;
	this->verbose = false;

	this->psIsUrlParsed = 0;
	this->psGetNextUrl = 0;
	this->psGetUrlPosition = 0;
	this->psGetNumberOfUrls = 0;
	this->psIsUrlLockable = 0;
	this->psCheckUrlLock = 0;
	this->psGetUrlLock = 0;
	this->psLockUrl = 0;
	this->psUnLockUrl = 0;
	this->psGetContentIdFromParsedId = 0;
	this->psGetLatestContent = 0;
	this->psGetAllContents = 0;
	this->psSetUrlFinished = 0;
	this->psGetEntryId = 0;
	this->psUpdateEntry = 0;
	this->psAddEntry = 0;
	this->psUpdateParsedTable = 0;
}

// destructor stub
Database::~Database() {}

// convert thread ID to string for logging
void Database::setId(unsigned long analyzerId) {
	std::ostringstream idStrStr;
	idStrStr << analyzerId;
	this->idString = idStrStr.str();
}

// set website ID (and convert it to string for SQL requests)
void Database::setWebsite(unsigned long websiteId) {
	std::ostringstream idStrStr;
	idStrStr << websiteId;
	this->website = websiteId;
	this->websiteIdString = idStrStr.str();
}

// set website namespace
void Database::setWebsiteNamespace(const std::string& websiteNamespace) {
	this->websiteName = websiteNamespace;
}

// set URL list ID (and convert it to string for SQL requests)
void Database::setUrlList(unsigned long listId) {
	std::ostringstream idStrStr;
	idStrStr << listId;
	this->urlList = listId;
	this->listIdString = idStrStr.str();
}

// set URL list namespace
void Database::setUrlListNamespace(const std::string& urlListNamespace) {
	this->urlListName = urlListNamespace;
}

// enable or disable reparsing
void Database::setReparse(bool isReparse) {
	this->reparse = isReparse;
}

// enable or disable parsing custom URLs
void Database::setParseCustom(bool isParseCustom) {
	this->parseCustom = isParseCustom;
}

// enable or disable logging
void Database::setLogging(bool isLogging) {
	this->logging = isLogging;
}

// enable or disable verbose logging
void Database::setVerbose(bool isVerbose) {
	this->verbose = isVerbose;
}

// set target table name
void Database::setTargetTable(const std::string& table) {
	this->targetTableName = table;
}

// set target table fields
void Database::setTargetFields(const std::vector<std::string>& fields) {
	this->targetFieldNames = fields;
}

// create target table if it does not exists or add custom field columns if they do not exist
void Database::initTargetTable() {
	// create table names
	this->urlListTable = "crawlserv_" + this->websiteName + "_" + this->urlListName;
	this->targetTableFull = this->urlListTable + "_parsed_" + this->targetTableName;

	// check for existence of target table
	if(this->isTableExists(this->targetTableFull)) {
		// target table exists: add columns that do not exist
		for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i)
			if(!(i->empty()) && !(this->isColumnExists(this->targetTableFull, "parsed__" + *i)))
				this->addColumn(this->targetTableFull, TableColumn("parsed__" + *i, "LONGTEXT"));
		this->compressTable(this->targetTableFull);
	}
	else {
		// create target table
		std::vector<TableColumn> columns;
		columns.reserve(3 + this->targetFieldNames.size());
		columns.push_back(TableColumn("content", "BIGINT UNSIGNED NOT NULL", this->urlListTable + "_crawled", "id"));
		columns.push_back(TableColumn("parsed_id", "TEXT NOT NULL"));
		columns.push_back(TableColumn("parsed_datetime", "DATETIME DEFAULT NULL"));
		for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i)
			columns.push_back(TableColumn("parsed__" + *i, "LONGTEXT"));

		this->createTable(this->targetTableFull, columns, true);

		// add target table to index
		this->addParsedTable(this->website, this->urlList, this->targetTableName);
	}
}

// prepare SQL statements for parser
bool Database::prepare() {
	// check connection to database
	if(!(this->checkConnection())) {
		this->errorMessage = this->getDatabaseErrorMessage();
		return false;
	}

	// reserve memory
	this->reservePreparedStatements(16);

	try {
		// prepare SQL statements for parser
		if(!(this->psIsUrlParsed)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares isUrlParsed()...");
			this->psIsUrlParsed = this->addPreparedStatement("SELECT EXISTS (SELECT 1 FROM `" + this->urlListTable + "` WHERE id = ?"
					" AND parsed = TRUE) AS result");
		}
		if(!(this->psGetNextUrl)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares getNextUrl()...");
			std::string sqlQuery = "SELECT a.id, a.url FROM `" + this->urlListTable + "` AS a, `" + this->urlListTable + "_crawled` AS b"
					" WHERE a.id = b.url AND a.id > ? AND b.response < 400 AND (a.parselock IS NULL OR a.parselock < NOW())";
			if(!reparse) sqlQuery += " AND a.parsed = FALSE";
			if(!parseCustom) sqlQuery += " AND a.manual = FALSE";
			sqlQuery += " ORDER BY a.id LIMIT 1";
			this->psGetNextUrl = this->addPreparedStatement(sqlQuery);
		}
		if(!(this->psGetUrlPosition)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares getUrlPosition()...");
			this->psGetUrlPosition = this->addPreparedStatement("SELECT COUNT(id) AS result FROM `" + this->urlListTable + "` WHERE id < ?");
		}
		if(!(this->psGetNumberOfUrls)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares getNumberOfUrls()...");
			this->psGetNumberOfUrls = this->addPreparedStatement("SELECT COUNT(id) AS result FROM `" + this->urlListTable + "`");
		}

		if(!(this->psIsUrlLockable)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares isUrlLockable()...");
			this->psIsUrlLockable = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM `" + this->urlListTable
					+ "` WHERE id = ? AND (parselock IS NULL OR parselock < NOW())) AS result");
		}
		if(!(this->psGetUrlLock)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares getUrlLock()...");
			this->psGetUrlLock = this->addPreparedStatement("SELECT parselock FROM `" + this->urlListTable + "` WHERE id = ? LIMIT 1");
		}
		if(!(this->psCheckUrlLock)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares checkUrlLock()...");
			this->psCheckUrlLock = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM `" + this->urlListTable
					+ "` WHERE id = ? AND (parselock < NOW() OR parselock <= ? OR parselock IS NULL)) AS result");
		}
		if(!(this->psLockUrl)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares lockUrl()...");
			this->psLockUrl = this->addPreparedStatement("UPDATE `" + this->urlListTable + "` SET parselock = NOW() + INTERVAL ? SECOND"
					" WHERE id = ? LIMIT 1");
		}
		if(!(this->psUnLockUrl)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares unLockUrl()...");
			this->psUnLockUrl = this->addPreparedStatement("UPDATE `" + this->urlListTable + "` SET parselock = NULL WHERE id = ?"
					" LIMIT 1");
		}
		if(!(this->psGetContentIdFromParsedId)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares getContentIdFromParsedId()...");
			this->psGetContentIdFromParsedId = this->addPreparedStatement("SELECT content FROM `" + this->targetTableFull
					+ "` WHERE parsed_id = ? LIMIT 1");
		}
		if(!(this->psGetLatestContent)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares getLatestContent()...");
			this->psGetLatestContent = this->addPreparedStatement("SELECT id, content FROM `" + this->urlListTable + "_crawled`"
					" WHERE url = ? ORDER BY crawltime DESC LIMIT ?, 1");
		}
		if(!(this->psGetAllContents)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares getAllContents()...");
			this->psGetAllContents = this->addPreparedStatement("SELECT id, content FROM `" + this->urlListTable + "_crawled`"
					" WHERE url = ?");
		}
		if(!(this->psSetUrlFinished)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares setUrlFinished()...");
			this->psSetUrlFinished = this->addPreparedStatement("UPDATE `" + this->urlListTable + "` SET parsed = TRUE, analyzed = FALSE,"
					"parselock = NULL WHERE id = ? LIMIT 1");
		}
		if(!(this->psGetEntryId)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares getEntryId()...");
			this->psGetEntryId = this->addPreparedStatement("SELECT id FROM `" + this->targetTableFull + "` WHERE content = ? LIMIT 1");
		}
		if(!(this->psUpdateEntry)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares updateEntry()...");
			std::string sqlQuery = "UPDATE `" + this->targetTableFull + "` SET parsed_id = ?, parsed_datetime = ?";
			for(auto i = this->targetFieldNames.begin(); i!= this->targetFieldNames.end(); ++i)
				if(!(i->empty())) sqlQuery += ", `parsed__" + *i + "` = ?";
			sqlQuery += " WHERE id = ? LIMIT 1";
			if(this->verbose) this->log("parser", "[#" + this->idString + "] > " + sqlQuery);
			this->psUpdateEntry = this->addPreparedStatement(sqlQuery);
		}
		if(!(this->psAddEntry)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares addEntry()...");
			std::string sqlQuery = "INSERT INTO `" + this->targetTableFull + "`(content, parsed_id, parsed_datetime";
			unsigned long counter = 0;
			for(auto i = this->targetFieldNames.begin(); i!= this->targetFieldNames.end(); ++i) {
				if(!(i->empty())) {
					sqlQuery += ", `parsed__" + *i + "`";
					counter++;
				}
			}
			sqlQuery += ") VALUES (?, ?, ?";
			for(unsigned long n = 0; n < counter; n++) sqlQuery += ", ?";
			sqlQuery += ")";
			if(this->verbose) this->log("parser", "[#" + this->idString + "] > " + sqlQuery);
			this->psAddEntry = this->addPreparedStatement(sqlQuery);
		}
		if(!(this->psUpdateParsedTable)) {
			if(this->verbose) this->log("parser", "[#" + this->idString + "] prepares parsingTableUpdated()...");
			this->psUpdateParsedTable = this->addPreparedStatement("UPDATE crawlserv_parsedtables SET updated = CURRENT_TIMESTAMP"
					" WHERE website = ? AND urllist = ? AND name = ? LIMIT 1");
			sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psUpdateParsedTable);
			sqlStatement->setUInt64(1, this->website);
			sqlStatement->setUInt64(2, this->urlList);
			sqlStatement->setString(3, this->targetTableName);

		}
	}
	catch(sql::SQLException &e) {
		// set error message
		std::ostringstream errorStrStr;
		errorStrStr << "prepare() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();
		return false;
	}

	return true;
}

// lock URL list
void Database::lockUrlList() {
	this->lockTable(this->urlListTable);
}

// lock URL list and table with crawled content (for URL selection)
void Database::lockUrlListAndCrawledTable() {
	this->lockTables(this->urlListTable, this->urlListTable + "_crawled");
}

// check whether an URL has been parsed
bool Database::isUrlParsed(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsUrlParsed)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::isUrlParsed(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psIsUrlParsed);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::isUrlParsed(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlParsed() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// get the next URL to parse from database or empty IdString if all URLs have been parsed
std::pair<unsigned long, std::string> Database::getNextUrl(unsigned long currentUrlId) {
	sql::ResultSet * sqlResultSet = NULL;
	std::pair<unsigned long, std::string> result;

	// check prepared SQL statement
	if(!(this->psGetNextUrl)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getNextUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetNextUrl);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::getNextUrl(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, currentUrlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			result.first = sqlResultSet->getUInt64("id");
			result.second = sqlResultSet->getString("url");
		}

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getNextUrl() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// get the position of the URL in the URL list
unsigned long Database::getUrlPosition(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetUrlPosition)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getUrlPosition()");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetUrlPosition);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::getUrlPosition() is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get URL position of URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("result");

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlPosition() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// get the number of URLs in the URL list
unsigned long Database::getNumberOfUrls() {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetNumberOfUrls)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getNumberOfUrls()");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetNumberOfUrls);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::getNumberOfUrls() is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get number of URLs from database
	try {
		// execute SQL query
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("result");

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getNumberOfUrls() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// check whether an URL is already locked in database
bool Database::isUrlLockable(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsUrlLockable)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::isUrlLockable(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psIsUrlLockable);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::isUrlLockable(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlLockable() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// get the URL lock end time of a specific URL from database
std::string Database::getUrlLock(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check prepared SQL statement
	if(!(this->psGetUrlLock)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getUrlLock(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetUrlLock);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::getUrlLock(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("parselock");

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlLock() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// check whether the URL has not been locked again after a specific lock time (or is not locked anymore)
bool Database::checkUrlLock(unsigned long urlId, const std::string& lockTime) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psCheckUrlLock)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::checkUrlLock(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psCheckUrlLock);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::checkUrlLock(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->setString(2, lockTime);
		sqlResultSet = sqlStatement->executeQuery();

		// get (and parse) result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "checkUrlLock() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// lock a URL in the database
std::string Database::lockUrl(unsigned long urlId, unsigned long lockTimeout) {
	// check prepared SQL statement
	if(!(this->psLockUrl)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::lockUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psLockUrl);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::lockUrl(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// lock URL in database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, lockTimeout);
		sqlStatement->setUInt64(2, urlId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "lockUrl() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return this->getUrlLock(urlId);
}

// unlock a URL in the database
void Database::unLockUrl(unsigned long urlId) {
	// check prepared SQL statement
	if(!(this->psUnLockUrl)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::unLockUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psUnLockUrl);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::unLockUrl(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// unlock URL in database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "unLockUrl() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// get content ID from parsed ID
unsigned long Database::getContentIdFromParsedId(const std::string& parsedId) {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetContentIdFromParsedId))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getContentIdFromParsedId(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetContentIdFromParsedId);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::getContentIdFromParsedId(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement->setString(1, parsedId);
		sqlResultSet = sqlStatement->executeQuery();

		// get (and parse) result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("content");

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getContentIdFromParsedId() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// get latest content for the ID-specified URL, return false if there is no content
bool Database::getLatestContent(unsigned long urlId, unsigned long index,
		std::pair<unsigned long, std::string>& contentTo) {
	sql::ResultSet * sqlResultSet = NULL;
	std::pair<unsigned long, std::string> result;
	bool success = false;

	// check prepared SQL statement
	if(!(this->psGetLatestContent))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getLatestContent(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetLatestContent);
	if(!sqlStatement)
		throw DatabaseException("Prepared SQL statement for Module::Parser::Database::getLatestContent(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->setUInt64(2, index);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			result = std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content"));
			success = true;
		}

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getLatestContent() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	if(success) {
		contentTo = result;
		return true;
	}

	return false;
}

// get all contents for the ID-specified URL
std::vector<std::pair<unsigned long, std::string>> Database::getAllContents(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<std::pair<unsigned long, std::string>> result;

	// check prepared SQL statement
	if(!(this->psGetAllContents))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getAllContents(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetAllContents);
	if(!sqlStatement)
		throw DatabaseException("Prepared SQL statement for Module::Parser::Database::getAllContents(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next())
				result.push_back(std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content")));
		}

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getAllContents() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// add parsed data to database (update if row for ID-specified content already exists
void Database::updateOrAddEntry(unsigned long contentId, const std::string& parsedId,
		const std::string& parsedDateTime, const std::vector<std::string>& parsedFields) {
	// check data sizes
	unsigned long tooLarge = 0;
	if(parsedId.size() > this->getMaxAllowedPacketSize()) tooLarge = parsedId.size();
	if(parsedDateTime.size() > this->getMaxAllowedPacketSize() && parsedDateTime.size() > tooLarge) tooLarge = parsedDateTime.size();
	for(auto i = parsedFields.begin(); i != parsedFields.end(); ++i)
		if(i->size() > this->getMaxAllowedPacketSize() && i->size() > tooLarge) tooLarge = i->size();
	if(tooLarge) {
		if(this->logging) {
			// show warning about data size
			bool adjustServerSettings = false;
			std::ostringstream logStrStr;
			logStrStr.imbue(std::locale(""));
			logStrStr << "[#" << this->idString << "] WARNING: An entry could not be saved to the database,"
					" because the size of a parsed value (" << tooLarge << " bytes) exceeds the ";
			if(tooLarge > 1073741824) logStrStr << "mySQL maximum of 1 GiB.";
			else {
				logStrStr << "current mySQL server maximum of " << this->getMaxAllowedPacketSize() << " bytes.";
				adjustServerSettings = true;
			}
			this->log("parser", logStrStr.str());
			if(adjustServerSettings)
				this->log("parser", "[#" + this->idString + "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
		}
		return;
	}

	// lock target table and parsing table index
	this->lockTables(this->targetTableFull, "crawlserv_parsedtables");

	try {
		// update existing or add new entry
		unsigned long entryId = this->getEntryId(contentId);
		if(entryId) this->updateEntry(entryId, parsedId, parsedDateTime, parsedFields);
		else this->addEntry(contentId, parsedId, parsedDateTime, parsedFields);
	}
	catch(...) {
		try { this->unlockTables(); }
		catch(...) {}
		throw;
	}

	// unlock target table
	this->unlockTables();
}

// set URL as parsed in the database
void Database::setUrlFinished(unsigned long urlId) {
	// check prepared SQL statement
	if(!(this->psSetUrlFinished))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::setUrlFinished(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psSetUrlFinished);
	if(!sqlStatement)
		throw DatabaseException("Prepared SQL statement for Module::Parser::Database::setUrlFinished(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setUrlFinished() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// helper function: get ID of parsing entry for ID-specified content, return 0 if no entry exists
unsigned long Database::getEntryId(unsigned long contentId) {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetEntryId)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getEntryId(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetEntryId);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::getEntryId(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, contentId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("id");

		// delete result
		MAIN_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getEntryId() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
	catch(...) {
		// any other exception: free memory and re-throw
		MAIN_DATABASE_DELETE(sqlResultSet);
		throw;
	}

	return result;
}

// helper function: update ID-specified parsing entry
void Database::updateEntry(unsigned long entryId, const std::string& parsedId,
		const std::string& parsedDateTime, const std::vector<std::string>& parsedFields) {
	// check prepared SQL statement
	if(!(this->psUpdateEntry)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::updateEntry(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psUpdateEntry);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::updateEntry(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement->setString(1, parsedId);
		if(!parsedDateTime.empty()) sqlStatement->setString(2, parsedDateTime);
		else sqlStatement->setNull(2, 0);

		unsigned int counter = 3;
		for(auto i = parsedFields.begin(); i != parsedFields.end(); ++i) {
			if(!(this->targetFieldNames.at(i - parsedFields.begin()).empty())) {
				sqlStatement->setString(counter, *i);
				counter++;
			}
		}
		sqlStatement->setUInt64(counter, entryId);
		sqlStatement->execute();

		// update parsing table
		this->updateParsedTable();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "updateEntry() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// helper function: add parsing entry for ID-specified content
void Database::addEntry(unsigned long contentId, const std::string& parsedId, const std::string& parsedDateTime,
		const std::vector<std::string>& parsedFields) {
	// check prepared SQL statement
	if(!(this->psAddEntry)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::addEntry(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psAddEntry);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::addEntry(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, contentId);
		sqlStatement->setString(2, parsedId);
		if(!parsedDateTime.empty()) sqlStatement->setString(3, parsedDateTime);
		else sqlStatement->setNull(3, 0);

		unsigned int counter = 4;
		for(auto i = parsedFields.begin(); i != parsedFields.end(); ++i) {
			if(!(this->targetFieldNames.at(i - parsedFields.begin()).empty())) {
				sqlStatement->setString(counter, *i);
				counter++;
			}
		}
		sqlStatement->execute();

		// update parsing table
		this->updateParsedTable();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "addEntry() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// helper function: update parsing table index
void Database::updateParsedTable() {
	// check prepared SQL statement
	if(!(this->psUpdateParsedTable))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::updateParsedTable(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psUpdateParsedTable);
	if(!sqlStatement) throw DatabaseException("Prepared SQL statement for Module::Parser::Database::updateParsedTable(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw DatabaseException(this->errorMessage);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "updateParsedTable() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

}
