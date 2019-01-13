/*
 * Database.cpp
 *
 * This class provides database functionality for a parser thread by implementing the Module::DBWrapper interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

// constructor: initialize values
crawlservpp::Module::Parser::Database::Database(crawlservpp::Module::DBThread& dbThread) : crawlservpp::Module::DBWrapper(dbThread) {
	this->fieldNames = NULL;
	this->psIsUrlParsed = 0;
	this->psGetNextUrl = 0;
	this->psGetUrlPosition = 0;
	this->psGetNumberOfUrls = 0;
	this->psIsUrlLockable = 0;
	this->psCheckUrlLock = 0;
	this->psGetUrlLock = 0;
	this->psLockUrl = 0;
	this->psUnLockUrl = 0;
	this->psGetLatestContent = 0;
	this->psGetAllContents = 0;
	this->psSetUrlFinished = 0;
	this->psGetEntryId = 0;
	this->psUpdateEntry = 0;
	this->psAddEntry = 0;
	this->psUpdateParsedTable = 0;
}

// destructor stub
crawlservpp::Module::Parser::Database::~Database() {}

// create target table if it does not exists or add custom field columns if they do not exist
void crawlservpp::Module::Parser::Database::initTargetTable(unsigned long websiteId, unsigned long listId, const std::string& websiteNameSpace,
		const std::string& urlListNameSpace, const  std::string& tableName, const std::vector<std::string> * fields) {
	// create table names
	this->urlListTable = "crawlserv_" + websiteNameSpace + "_" + urlListNameSpace;
	this->targetTable = this->urlListTable + "_parsed_" + tableName;

	// save field names
	this->fieldNames = fields;

	// create table if not exists
	if(this->isTableExists(this->targetTable)) {
		// add columns that do not exist
		for(auto i = this->fieldNames->begin(); i != this->fieldNames->end(); ++i) {
			if(!(this->isColumnExists(this->targetTable, "parsed__" + *i))) {
				this->execute("ALTER TABLE " + this->targetTable + " ADD parsed__" + *i + " LONGTEXT");
			}
		}
	}
	else {
		// create table
		std::string sqlQuery = "CREATE TABLE " + this->targetTable + "(id SERIAL, content BIGINT UNSIGNED NOT NULL,"
				" parsed_id TEXT NOT NULL, parsed_datetime DATETIME DEFAULT NULL";
		for(auto i = this->fieldNames->begin(); i != this->fieldNames->end(); ++i) sqlQuery += ", parsed__" + *i + " LONGTEXT";
		sqlQuery += ", PRIMARY KEY(id), FOREIGN KEY(content) REFERENCES " + this->urlListTable + "_crawled(id)"
				" ON UPDATE RESTRICT ON DELETE CASCADE) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci, ROW_FORMAT=COMPRESSED";
		this->execute(sqlQuery);

		// add target table to index
		this->addParsedTable(websiteId, listId, tableName);
	}
}

// prepare SQL statements for parser
bool crawlservpp::Module::Parser::Database::prepare(unsigned long parserId, unsigned long websiteId, unsigned long listId, const std::string& tableName,
		bool reparse, bool verbose) {
	// convert id to string
	std::ostringstream idStrStr;
	idStrStr << parserId;
	std::string idString = idStrStr.str();

	// check connection to database
	if(!(this->checkConnection())) {
		this->errorMessage = this->getDatabaseErrorMessage();
		return false;
	}

	try {
		// prepare SQL statements for parser
		if(!(this->psIsUrlParsed)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares isUrlParsed()...");
			this->psIsUrlParsed = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM " + this->urlListTable + " WHERE id = ?"
					" AND parsed = TRUE LIMIT 1) AS result");
		}
		if(!(this->psGetNextUrl)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares getNextUrl()...");
			if(reparse) this->psGetNextUrl = this->addPreparedStatement("SELECT id, url FROM " + this->urlListTable
							+ " WHERE id > ? AND (parselock IS NULL OR parselock < NOW()) ORDER BY id LIMIT 1");
			else this->psGetNextUrl = this->addPreparedStatement("SELECT id, url FROM " + this->urlListTable
				+ " WHERE id > ? AND parsed = 0 AND (parselock IS NULL OR parselock < NOW()) ORDER BY id LIMIT 1");
		}
		if(!(this->psGetUrlPosition)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares getUrlPosition()...");
			this->psGetUrlPosition = this->addPreparedStatement("SELECT COUNT(id) AS result FROM " + this->urlListTable + " WHERE id < ?");
		}
		if(!(this->psGetNumberOfUrls)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares getNumberOfUrls()...");
			this->psGetNumberOfUrls = this->addPreparedStatement("SELECT COUNT(id) AS result FROM " + this->urlListTable);
		}

		if(!(this->psIsUrlLockable)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares isUrlLockable()...");
			this->psIsUrlLockable = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM " + this->urlListTable
					+ " WHERE id = ? AND (parselock IS NULL OR parselock < NOW())) AS result");
		}
		if(!(this->psGetUrlLock)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares getUrlLock()...");
			this->psGetUrlLock = this->addPreparedStatement("SELECT parselock FROM " + this->urlListTable + " WHERE id = ? LIMIT 1");
		}
		if(!(this->psCheckUrlLock)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares checkUrlLock()...");
			this->psCheckUrlLock = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM " + this->urlListTable
					+ " WHERE id = ? AND (parselock < NOW() OR parselock <= ? OR parselock IS NULL)) AS result");
		}
		if(!(this->psLockUrl)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares lockUrl()...");
			this->psLockUrl = this->addPreparedStatement("UPDATE " + this->urlListTable + " SET parselock = NOW() + INTERVAL ? SECOND"
					" WHERE id = ? LIMIT 1");
		}
		if(!(this->psUnLockUrl)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares unLockUrl()...");
			this->psUnLockUrl = this->addPreparedStatement("UPDATE " + this->urlListTable + " SET parselock = NULL WHERE id = ?"
					" LIMIT 1");
		}
		if(!(this->psGetLatestContent)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares getLatestContent()...");
			this->psGetLatestContent = this->addPreparedStatement("SELECT id, content FROM " + this->urlListTable + "_crawled"
					" WHERE url = ? ORDER BY crawltime DESC LIMIT ?, 1");
		}
		if(!(this->psGetAllContents)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares getAllContents()...");
			this->psGetAllContents = this->addPreparedStatement("SELECT id, content FROM " + this->urlListTable + "_crawled"
					" WHERE url = ?");
		}
		if(!(this->psSetUrlFinished)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares setUrlFinished()...");
			this->psSetUrlFinished = this->addPreparedStatement("UPDATE " + this->urlListTable + " SET parsed = TRUE, analyzed = FALSE,"
					"parselock = NULL WHERE id = ? LIMIT 1");
		}
		if(!(this->psGetEntryId)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares getEntryId()...");
			this->psGetEntryId = this->addPreparedStatement("SELECT id FROM " + this->targetTable + " WHERE content = ? LIMIT 1");
		}
		if(!(this->psUpdateEntry)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares updateEntry()...");
			std::string sqlQuery = "UPDATE " + this->targetTable + " SET parsed_id = ?, parsed_datetime = ?";
			for(std::vector<std::string>::const_iterator i = this->fieldNames->begin(); i!= this->fieldNames->end(); ++i)
				sqlQuery += ", parsed__" + *i + " = ?";
			sqlQuery += " WHERE id = ? LIMIT 1";
			if(verbose) this->log("parser", "[#" + idString + "] > " + sqlQuery);
			this->psUpdateEntry = this->addPreparedStatement(sqlQuery);
		}
		if(!(this->psAddEntry)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares addEntry()...");
			std::string sqlQuery = "INSERT INTO " + this->targetTable + "(content, parsed_id, parsed_datetime";
			for(std::vector<std::string>::const_iterator i = this->fieldNames->begin(); i!= this->fieldNames->end(); ++i)
				sqlQuery += ", parsed__" + *i;
			sqlQuery += ") VALUES (?, ?, ?";
			for(unsigned long n = 0; n < this->fieldNames->size(); n++) sqlQuery += ", ?";
			sqlQuery += ")";
			if(verbose) this->log("parser", "[#" + idString + "] > " + sqlQuery);
			this->psAddEntry = this->addPreparedStatement(sqlQuery);
		}
		if(!(this->psUpdateParsedTable)) {
			if(verbose) this->log("parser", "[#" + idString + "] prepares parsingTableUpdated()...");
			std::stringstream sqlQueryStrStr;
			sqlQueryStrStr << "UPDATE crawlserv_parsedtables SET updated = CURRENT_TIMESTAMP WHERE website = " << websiteId
					<< ", urllist = " << listId << ", name = " << tableName << " LIMIT 1";
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
void crawlservpp::Module::Parser::Database::lockUrlList() {
	this->lockTable(this->urlListTable);
}

// check whether an URL has been parsed
bool crawlservpp::Module::Parser::Database::isUrlParsed(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsUrlParsed)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::isUrlParsed(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psIsUrlParsed);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::isUrlParsed(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlParsed() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get the next URL to parse from database or empty IdString if all URLs have been parsed
crawlservpp::Struct::IdString crawlservpp::Module::Parser::Database::getNextUrl(unsigned long currentUrlId) {
	sql::ResultSet * sqlResultSet = NULL;
	crawlservpp::Struct::IdString result;

	// check prepared SQL statement
	if(!(this->psGetNextUrl)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::getNextUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetNextUrl);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::getNextUrl(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, currentUrlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) {
			result.id = sqlResultSet->getUInt64("id");
			result.string = sqlResultSet->getString("url");
		}

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getNextUrl() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get the position of the URL in the URL list
unsigned long crawlservpp::Module::Parser::Database::getUrlPosition(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetUrlPosition)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::getUrlPosition()");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetUrlPosition);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::getUrlPosition() is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get URL position of URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getUInt64("result");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlPosition() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get the number of URLs in the URL list
unsigned long crawlservpp::Module::Parser::Database::getNumberOfUrls() {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetNumberOfUrls)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::getNumberOfUrls()");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetNumberOfUrls);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::getNumberOfUrls() is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get number of URLs from database
	try {
		// execute SQL query
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getUInt64("result");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getNumberOfUrls() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether an URL is already locked in database
bool crawlservpp::Module::Parser::Database::isUrlLockable(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsUrlLockable)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::isUrlLockable(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psIsUrlLockable);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::isUrlLockable(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlLockable() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get the URL lock end time of a specific URL from database
std::string crawlservpp::Module::Parser::Database::getUrlLock(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check prepared SQL statement
	if(!(this->psGetUrlLock)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::getUrlLock(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetUrlLock);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::getUrlLock(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getString("parselock");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlLock() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether the URL has not been locked again after a specific lock time (or is not locked anymore)
bool crawlservpp::Module::Parser::Database::checkUrlLock(unsigned long urlId, const std::string& lockTime) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psCheckUrlLock)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::checkUrlLock(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psCheckUrlLock);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::checkUrlLock(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->setString(2, lockTime);
		sqlResultSet = sqlStatement->executeQuery();

		// get (and parse) result
		sqlResultSet->next();
		result = sqlResultSet->getBoolean("result");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "checkUrlLock() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// lock a URL in the database
std::string crawlservpp::Module::Parser::Database::lockUrl(unsigned long urlId, unsigned long lockTimeout) {
	// check prepared SQL statement
	if(!(this->psLockUrl)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::lockUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psLockUrl);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::lockUrl(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

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
		throw std::runtime_error(errorStrStr.str());
	}

	return this->getUrlLock(urlId);
}

// unlock a URL in the database
void crawlservpp::Module::Parser::Database::unLockUrl(unsigned long urlId) {
	// check prepared SQL statement
	if(!(this->psUnLockUrl)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::unLockUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psUnLockUrl);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::unLockUrl(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

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
		throw std::runtime_error(errorStrStr.str());
	}
}

// get latest content for the id-specified URL, return false if there is no content
bool crawlservpp::Module::Parser::Database::getLatestContent(unsigned long urlId, unsigned long index, crawlservpp::Struct::IdString& contentTo) {
	sql::ResultSet * sqlResultSet = NULL;
	crawlservpp::Struct::IdString result;
	bool success = false;

	// check prepared SQL statement
	if(!(this->psGetLatestContent)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::getLatestContent(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetLatestContent);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::getLatestContent(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->setUInt64(2, index);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) {
			result = crawlservpp::Struct::IdString(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content"));
			success = true;
		}

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getLatestContent() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	if(success) {
		contentTo = result;
		return true;
	}

	return false;
}

// get all contents for the id-specified URL
std::vector<crawlservpp::Struct::IdString> crawlservpp::Module::Parser::Database::getAllContents(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	std::vector<crawlservpp::Struct::IdString> result;

	// check prepared SQL statement
	if(!(this->psGetAllContents)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::getAllContents(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetAllContents);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::getAllContents(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		while(sqlResultSet->next()) result.push_back(crawlservpp::Struct::IdString(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content")));

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getAllContents() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// add parsed data to database (update if row for id-specified content already exists
void crawlservpp::Module::Parser::Database::updateOrAddEntry(unsigned long contentId, const std::string& parsedId, const std::string& parsedDateTime,
			const std::vector<std::string>& parsedFields) {
	// lock target table
	this->lockTable(this->targetTable);

	// update existing or add new entry
	unsigned long entryId = this->getEntryId(contentId);
	if(entryId) this->updateEntry(entryId, parsedId, parsedDateTime, parsedFields);
	else this->addEntry(contentId, parsedId, parsedDateTime, parsedFields);

	// unlock target table
	this->unlockTables();
}

// set URL as parsed in the database
void crawlservpp::Module::Parser::Database::setUrlFinished(unsigned long urlId) {
	// check prepared SQL statement
	if(!(this->psSetUrlFinished)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::setUrlFinished(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psSetUrlFinished);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::setUrlFinished(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get id of URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setUrlFinished() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// helper function: get id of parsing entry for id-specified content, return 0 if no entry exists
unsigned long crawlservpp::Module::Parser::Database::getEntryId(unsigned long contentId) {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetEntryId)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::getEntryId(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetEntryId);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::getEntryId(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, contentId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getUInt64("id");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "getEntryId() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// helper function: update id-specified parsing entry
void crawlservpp::Module::Parser::Database::updateEntry(unsigned long entryId, const std::string& parsedId, const std::string& parsedDateTime,
		const std::vector<std::string>& parsedFields) {
	// check prepared SQL statement
	if(!(this->psUpdateEntry)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::updateEntry(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psUpdateEntry);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::updateEntry(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get id of URL from database
	try {
		// execute SQL query
		sqlStatement->setString(1, parsedId);
		if(parsedDateTime.length())	sqlStatement->setString(2, parsedDateTime);
		else sqlStatement->setNull(2, sql::DataType::VARCHAR);

		unsigned int counter = 3;
		for(auto i = parsedFields.begin(); i != parsedFields.end(); ++i) {
			sqlStatement->setString(counter, *i);
			counter++;
		}
		sqlStatement->setUInt64(counter, entryId);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "updateEntry() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// helper function: add parsing entry for id-specified content
void crawlservpp::Module::Parser::Database::addEntry(unsigned long contentId, const std::string& parsedId, const std::string& parsedDateTime,
		const std::vector<std::string>& parsedFields) {
	// check prepared SQL statement
	if(!(this->psAddEntry)) throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::addEntry(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psAddEntry);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::addEntry(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get id of URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, contentId);
		sqlStatement->setString(2, parsedId);
		if(parsedDateTime.length())	sqlStatement->setString(3, parsedDateTime);
		else sqlStatement->setNull(3, sql::DataType::VARCHAR);

		unsigned int counter = 4;
		for(auto i = parsedFields.begin(); i != parsedFields.end(); ++i) {
			sqlStatement->setString(counter, *i);
			counter++;
		}
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "addEntry() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// helper function: update parsing table index
void crawlservpp::Module::Parser::Database::updateParsedTable() {
	// check prepared SQL statement
	if(!(this->psUpdateParsedTable))
		throw std::runtime_error("Missing prepared SQL statement for Module::Parser::Database::updateParsedTable(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psUpdateParsedTable);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for Module::Parser::Database::updateParsedTable(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get id of URL from database
	try {
		// execute SQL query
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "updateParsedTable() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}
