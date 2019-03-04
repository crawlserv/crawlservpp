/*
 * Database.cpp
 *
 * This class provides database functionality for a parser thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

namespace crawlservpp::Module::Parser {

// constructor: initialize values
Database::Database(crawlservpp::Module::Database& dbThread) : crawlservpp::Wrapper::Database(dbThread),
		website(0), urlList(0), reparse(false), parseCustom(false), logging(false), verbose(false),
		timeoutTargetLock(0), targetTableId(0), ps({0}) {}

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

// set time-out for target table lock (in seconds)
void Database::setTimeoutTargetLock(unsigned long timeOut) {
	this->timeoutTargetLock = timeOut;
}

// create target table if it does not exists or add custom field columns if they do not exist
void Database::initTargetTable() {
	// create table names
	this->urlListTable = "crawlserv_" + this->websiteName + "_" + this->urlListName;
	this->targetTableFull = this->urlListTable + "_parsed_" + this->targetTableName;

	// create table properties
	CustomTableProperties properties("parsed", this->website, this->urlList, this->targetTableName, this->targetTableFull, true);
	properties.columns.reserve(this->targetFieldNames.size());
	for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i)
		if(!(i->empty())) properties.columns.push_back(TableColumn("parsed__" + *i, "LONGTEXT"));

	// lock parsing tables
	this->lockCustomTables("parsed", this->website, this->urlList, this->timeoutTargetLock);

	try {
		// add parsing table if it does not exist already
		this->targetTableId = this->addCustomTable(properties);
	}
	// any exception: try to unlock parsing tables and re-throw
	catch(...) {
		this->unlockCustomTables("parsed");
		throw;
	}

	// unlock parsing tables
	this->unlockCustomTables("parsed");
}

// prepare SQL statements for parser
void Database::prepare() {
	// check connection to database
	this->checkConnection();

	// reserve memory
	this->reserveForPreparedStatements(16);

	// prepare SQL statements for parser
	if(!(this->ps.isUrlParsed)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isUrlParsed()...");
		this->ps.isUrlParsed = this->addPreparedStatement(
				"SELECT EXISTS (SELECT 1 FROM `" + this->urlListTable + "` WHERE id = ? AND parsed = TRUE) AS result");
	}
	if(!(this->ps.getNextUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getNextUrl()...");
		std::string sqlQuery = "SELECT a.id, a.url FROM `" + this->urlListTable + "` AS a, `" + this->urlListTable + "_crawled` AS b"
				" WHERE a.id = b.url AND a.id > ? AND b.response < 400 AND (a.parselock IS NULL OR a.parselock < NOW())";
		if(!reparse) sqlQuery += " AND a.parsed = FALSE";
		if(!parseCustom) sqlQuery += " AND a.manual = FALSE";
		sqlQuery += " ORDER BY a.id LIMIT 1";
		this->ps.getNextUrl = this->addPreparedStatement(sqlQuery);
	}
	if(!(this->ps.getUrlPosition)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlPosition()...");
		this->ps.getUrlPosition = this->addPreparedStatement(
				"SELECT COUNT(id) AS result FROM `" + this->urlListTable + "` WHERE id < ?");
	}
	if(!(this->ps.getNumberOfUrls)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getNumberOfUrls()...");
		this->ps.getNumberOfUrls = this->addPreparedStatement(
				"SELECT COUNT(id) AS result FROM `" + this->urlListTable + "`");
	}
	if(!(this->ps.isUrlLockable)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isUrlLockable()...");
		this->ps.isUrlLockable = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + this->urlListTable
				+ "` WHERE id = ? AND (parselock IS NULL OR parselock < NOW())) AS result");
	}
	if(!(this->ps.getUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlLock()...");
		this->ps.getUrlLock = this->addPreparedStatement(
				"SELECT parselock FROM `" + this->urlListTable + "` WHERE id = ? LIMIT 1");
	}
	if(!(this->ps.checkUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares checkUrlLock()...");
		this->ps.checkUrlLock = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + this->urlListTable
				+ "` WHERE id = ? AND (parselock < NOW() OR parselock <= ? OR parselock IS NULL)) AS result");
	}
	if(!(this->ps.lockUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares lockUrl()...");
		this->ps.lockUrl = this->addPreparedStatement(
				"UPDATE `"
				+ this->urlListTable + "` SET parselock = NOW() + INTERVAL ? SECOND WHERE id = ? LIMIT 1");
	}
	if(!(this->ps.unLockUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares unLockUrl()...");
		this->ps.unLockUrl = this->addPreparedStatement(
				"UPDATE `" + this->urlListTable + "` SET parselock = NULL WHERE id = ? LIMIT 1");
	}
	if(!(this->ps.getContentIdFromParsedId)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getContentIdFromParsedId()...");
		this->ps.getContentIdFromParsedId = this->addPreparedStatement(
				"SELECT content FROM `" + this->targetTableFull	+ "` WHERE parsed_id = ? LIMIT 1");
	}
	if(!(this->ps.getLatestContent)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getLatestContent()...");
		this->ps.getLatestContent = this->addPreparedStatement(
				"SELECT id, content FROM `"
				+ this->urlListTable + "_crawled` WHERE url = ? ORDER BY crawltime DESC LIMIT ?, 1");
	}
	if(!(this->ps.getAllContents)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getAllContents()...");
		this->ps.getAllContents = this->addPreparedStatement(
				"SELECT id, content FROM `" + this->urlListTable + "_crawled` WHERE url = ?");
	}
	if(!(this->ps.setUrlFinished)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares setUrlFinished()...");
		this->ps.setUrlFinished = this->addPreparedStatement(
				"UPDATE `" + this->urlListTable
				+ "` SET parsed = TRUE, analyzed = FALSE, parselock = NULL WHERE id = ? LIMIT 1");
	}
	if(!(this->ps.getEntryId)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getEntryId()...");
		this->ps.getEntryId = this->addPreparedStatement(
				"SELECT id FROM `" + this->targetTableFull + "` WHERE content = ? LIMIT 1");
	}
	if(!(this->ps.updateEntry)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares updateEntry()...");
		std::string sqlQuery = "UPDATE `" + this->targetTableFull + "` SET parsed_id = ?, parsed_datetime = ?";
		for(auto i = this->targetFieldNames.begin(); i!= this->targetFieldNames.end(); ++i)
			if(!(i->empty())) sqlQuery += ", `parsed__" + *i + "` = ?";
		sqlQuery += " WHERE id = ? LIMIT 1";
		if(this->verbose) this->log("[#" + this->idString + "] > " + sqlQuery);
		this->ps.updateEntry = this->addPreparedStatement(sqlQuery);
	}
	if(!(this->ps.addEntry)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares addEntry()...");
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
		if(this->verbose) this->log("[#" + this->idString + "] > " + sqlQuery);
		this->ps.addEntry = this->addPreparedStatement(sqlQuery);
	}
	if(!(this->ps.updateParsedTable)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares parsingTableUpdated()...");
		this->ps.updateParsedTable = this->addPreparedStatement("UPDATE crawlserv_parsedtables SET updated = CURRENT_TIMESTAMP"
				" WHERE website = ? AND urllist = ? AND name = ? LIMIT 1");
	}
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
	bool result = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isUrlParsed)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::isUrlParsed(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlParsed);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::isUrlParsed", e); }

	return result;
}

// get the next URL to parse from database or empty IdString if all URLs have been parsed
std::pair<unsigned long, std::string> Database::getNextUrl(unsigned long currentUrlId) {
	std::pair<unsigned long, std::string> result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getNextUrl)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getNextUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getNextUrl);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, currentUrlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			result.first = sqlResultSet->getUInt64("id");
			result.second = sqlResultSet->getString("url");
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getNextUrl", e); }

	return result;
}

// get the position of the URL in the URL list
unsigned long Database::getUrlPosition(unsigned long urlId) {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlPosition)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getUrlPosition()");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlPosition);

	// get URL position of URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getUrlPosition", e); }

	return result;
}

// get the number of URLs in the URL list
unsigned long Database::getNumberOfUrls() {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getNumberOfUrls)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getNumberOfUrls()");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getNumberOfUrls);

	// get number of URLs from database
	try {
		// execute SQL query
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getNumberOfUrls", e); }

	return result;
}

// check whether an URL is already locked in database
bool Database::isUrlLockable(unsigned long urlId) {
	bool result = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isUrlLockable)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::isUrlLockable(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlLockable);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::isUrlLockable", e); }

	return result;
}

// get the URL lock end time of a specific URL from database
std::string Database::getUrlLock(unsigned long urlId) {
	std::string result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlLock)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getUrlLock(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlLock);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("parselock");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getUrlLock", e); }

	return result;
}

// check whether the URL has not been locked again after a specific lock time (or is not locked anymore)
bool Database::checkUrlLock(unsigned long urlId, const std::string& lockTime) {
	bool result = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.checkUrlLock)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::checkUrlLock(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.checkUrlLock);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.setString(2, lockTime);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get (and parse) result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::checkUrlLock", e); }

	return result;
}

// lock a URL in the database
std::string Database::lockUrl(unsigned long urlId, unsigned long lockTimeout) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.lockUrl)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::lockUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.lockUrl);

	// lock URL in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, lockTimeout);
		sqlStatement.setUInt64(2, urlId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::lockUrl", e); }

	return this->getUrlLock(urlId);
}

// unlock a URL in the database
void Database::unLockUrl(unsigned long urlId) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.unLockUrl)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::unLockUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.unLockUrl);

	// unlock URL in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::unLockUrl", e); }
}

// get content ID from parsed ID
unsigned long Database::getContentIdFromParsedId(const std::string& parsedId) {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getContentIdFromParsedId))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getContentIdFromParsedId(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getContentIdFromParsedId);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setString(1, parsedId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get (and parse) result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("content");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getContentIdFromParsedId", e); }

	return result;
}

// get latest content for the ID-specified URL, return false if there is no content
bool Database::getLatestContent(unsigned long urlId, unsigned long index,
		std::pair<unsigned long, std::string>& contentTo) {
	std::pair<unsigned long, std::string> result;
	bool success = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getLatestContent))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getLatestContent(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getLatestContent);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.setUInt64(2, index);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			result = std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content"));
			success = true;
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getLatestContent", e); }

	if(success) {
		contentTo = result;
		return true;
	}

	return false;
}

// get all contents for the ID-specified URL
std::vector<std::pair<unsigned long, std::string>> Database::getAllContents(unsigned long urlId) {
	std::vector<std::pair<unsigned long, std::string>> result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getAllContents))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getAllContents(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getAllContents);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet) {
			result.reserve(sqlResultSet->rowsCount());
			while(sqlResultSet->next())
				result.push_back(std::pair<unsigned long, std::string>(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content")));
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getAllContents", e); }

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
			this->log(logStrStr.str());
			if(adjustServerSettings)
				this->log("[#" + this->idString + "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
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
	// any exception: try to unlock table and re-throw
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
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.setUrlFinished))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::setUrlFinished(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setUrlFinished);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::setUrlFinished", e); }
}

// helper function: get ID of parsing entry for ID-specified content, return 0 if no entry exists
unsigned long Database::getEntryId(unsigned long contentId) {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getEntryId)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getEntryId(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getEntryId);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, contentId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("id");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getEntryId", e); }

	return result;
}

// helper function: update ID-specified parsing entry
void Database::updateEntry(unsigned long entryId, const std::string& parsedId,
		const std::string& parsedDateTime, const std::vector<std::string>& parsedFields) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.updateEntry)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::updateEntry(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.updateEntry);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement.setString(1, parsedId);
		if(!parsedDateTime.empty()) sqlStatement.setString(2, parsedDateTime);
		else sqlStatement.setNull(2, 0);

		unsigned int counter = 3;
		for(auto i = parsedFields.begin(); i != parsedFields.end(); ++i) {
			if(!(this->targetFieldNames.at(i - parsedFields.begin()).empty())) {
				sqlStatement.setString(counter, *i);
				counter++;
			}
		}
		sqlStatement.setUInt64(counter, entryId);
		sqlStatement.execute();

		// update parsing table
		this->updateParsedTable();
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::updateEntry", e); }
}

// helper function: add parsing entry for ID-specified content
void Database::addEntry(unsigned long contentId, const std::string& parsedId, const std::string& parsedDateTime,
		const std::vector<std::string>& parsedFields) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.addEntry)) throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::addEntry(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.addEntry);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, contentId);
		sqlStatement.setString(2, parsedId);
		if(!parsedDateTime.empty()) sqlStatement.setString(3, parsedDateTime);
		else sqlStatement.setNull(3, 0);

		unsigned int counter = 4;
		for(auto i = parsedFields.begin(); i != parsedFields.end(); ++i) {
			if(!(this->targetFieldNames.at(i - parsedFields.begin()).empty())) {
				sqlStatement.setString(counter, *i);
				counter++;
			}
		}
		sqlStatement.execute();

		// update parsing table
		this->updateParsedTable();
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::addEntry", e); }
}

// helper function: update parsing table index
void Database::updateParsedTable() {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.updateParsedTable))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::updateParsedTable(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.updateParsedTable);

	// get ID of URL from database
	sqlStatement.setUInt64(1, this->website);
	sqlStatement.setUInt64(2, this->urlList);
	sqlStatement.setString(3, this->targetTableName);
	try {
		// execute SQL query
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::updateParsedTable", e); }
}

}
