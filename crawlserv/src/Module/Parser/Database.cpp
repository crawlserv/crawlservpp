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
Database::Database(Module::Database& dbThread) : Wrapper::Database(dbThread),
		website(0), urlList(0), reparse(false), parseCustom(true), logging(true), verbose(false),
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
void Database::initTargetTable(CallbackIsRunning isRunning) {
	// create table names
	this->urlListTable = "crawlserv_" + this->websiteName + "_" + this->urlListName;
	this->parsingTable = this->urlListTable + "_parsing";
	this->targetTableFull = this->urlListTable + "_parsed_" + this->targetTableName;

	// create table properties
	TargetTableProperties properties("parsed", this->website, this->urlList, this->targetTableName, this->targetTableFull, true);
	properties.columns.reserve(3 + this->targetFieldNames.size());
	properties.columns.emplace_back("content", "BIGINT UNSIGNED NOT NULL");
	properties.columns.emplace_back("parsed_id", "TEXT NOT NULL");
	properties.columns.emplace_back("parsed_datetime", "DATETIME DEFAULT NULL");
	for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i)
		if(!(i->empty())) properties.columns.emplace_back("parsed__" + *i, "LONGTEXT");

	{ // lock parsing tables
		TargetTablesLock(*this, "parsed", this->website, this->urlList, this->timeoutTargetLock, isRunning);

		// add target table if it does not exist already
		this->targetTableId = this->addTargetTable(properties);
	} // parsing tables unlocked
}

// prepare SQL statements for parser
void Database::prepare() {
	// check connection to database
	this->checkConnection();

	// reserve memory
	this->reserveForPreparedStatements(19);

	// prepare SQL statements for parser
	if(!(this->ps.isUrlParsed)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isUrlParsed()...");
		this->ps.isUrlParsed = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + this->parsingTable + "` WHERE target = ? AND url = ? AND success = TRUE) AS result");
	}

	if(!(this->ps.getNextUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getNextUrl()...");
		std::ostringstream sqlQueryStr;
		sqlQueryStr << "SELECT a.id AS url_id, a.url AS url, c.id AS lock_id FROM `" << this->urlListTable << "` AS a INNER JOIN `"
				<< this->urlListTable << "_crawled` AS b ON a.id = b.url LEFT OUTER JOIN `" << this->parsingTable
				<< "` AS c ON a.id = c.url AND c.target = " << this->targetTableId
				<< " WHERE a.id > ? AND b.response < 400 AND (c.locktime IS NULL OR c.locktime < NOW())";
		if(!(this->reparse)) sqlQueryStr << " AND (c.success IS NULL OR c.success = FALSE)";
		if(!(this->parseCustom)) sqlQueryStr << " AND a.manual = FALSE";
		sqlQueryStr << " ORDER BY a.id LIMIT 1";
		if(this->verbose) this->log("[#" + this->idString + "] > " + sqlQueryStr.str());
		this->ps.getNextUrl = this->addPreparedStatement(sqlQueryStr.str());
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
		std::ostringstream sqlQueryStr;
		this->ps.isUrlLockable = this->addPreparedStatement("SELECT (locktime IS NULL OR locktime < NOW()) AS result FROM `"
				+ this->parsingTable + "` WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.getUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlLock()...");
		this->ps.getUrlLock = this->addPreparedStatement(
				"SELECT locktime FROM `" + this->parsingTable + "` WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.getUrlLockId)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlLockId()...");
		std::ostringstream sqlQueryStr;
		sqlQueryStr << "SELECT id FROM `" << this->parsingTable << "` WHERE target = " << this->targetTableId
				<< " AND url = ? LIMIT 1";
		this->ps.getUrlLockId = this->addPreparedStatement(sqlQueryStr.str());
	}

	if(!(this->ps.checkUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares checkUrlLock()...");
		this->ps.checkUrlLock = this->addPreparedStatement(
				"SELECT (locktime < NOW() OR locktime <= ? OR locktime IS NULL) AS result FROM " + this->parsingTable
				+ " WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.lockUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares lockUrl() [1/2]...");
		this->ps.lockUrl = this->addPreparedStatement("UPDATE `" + this->parsingTable
				+ "` SET locktime = NOW() + INTERVAL ? SECOND WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.addUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares lockUrl() [2/2]...");
		std::ostringstream sqlQueryStr;
		sqlQueryStr << "INSERT INTO `" << this->parsingTable << "` (target, url, locktime, success) VALUES (" << this->targetTableId
				<< ", ?, NOW() + INTERVAL ? SECOND, FALSE)";
		this->ps.addUrlLock = this->addPreparedStatement(sqlQueryStr.str());
	}

	if(!(this->ps.unLockUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares unLockUrl()...");
		this->ps.unLockUrl = this->addPreparedStatement("UPDATE `" + this->parsingTable
				+ "` SET locktime = NULL WHERE id = ? LIMIT 1");
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
				"UPDATE `" + this->parsingTable + "` SET locktime = NULL, success = TRUE WHERE id = ? LIMIT 1");
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
				" WHERE id = ? LIMIT 1");
	}
}

// get the next URL to parse from database, return ID, URL and lock ID of next URL or empty tuple if all URLs have been parsed
Database::UrlProperties Database::getNextUrl(unsigned long currentUrlId) {
	UrlProperties result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getNextUrl))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getNextUrl(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getNextUrl);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, currentUrlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			result = UrlProperties(
					sqlResultSet->getUInt64("url_id"),
					sqlResultSet->getString("url"),
					sqlResultSet->getUInt64("lock_id")
			);
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
	if(!(this->ps.getUrlPosition))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getUrlPosition()");

	// get prepared SQL statement
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
	if(!(this->ps.getNumberOfUrls))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getNumberOfUrls()");

	// get prepared SQL statement
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
bool Database::isUrlLockable(unsigned long lockId) {
	bool result = false;

	// check lock ID
	if(!lockId) return true; // no URL lock exists yet, URL is therefore lockable

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isUrlLockable))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::isUrlLockable(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlLockable);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, lockId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::isUrlLockable", e); }

	return result;
}

// get the URL lock expiration time for a specific URL from database
std::string Database::getUrlLock(unsigned long lockId) {
	std::string result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlLock))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getUrlLock(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlLock);

	// get URL lock end time from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, lockId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("locktime");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getUrlLock", e); }

	return result;
}

// get the URL lock ID for a specific URL from database if none has been received yet
//  NOTE: The first value of the tuple (the URL ID) needs already to be set!
void Database::getUrlLockId(UrlProperties& urlProperties) {
	// check arguments
	if(!urlProperties.id) throw DatabaseException("Parser::Database::getUrlLockId(): No URL ID specified");
	if(urlProperties.lockId) return; // already got lock ID

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlLockId))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getUrlLockId(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlLockId);

	// get lock ID from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlProperties.id);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next())
			urlProperties.lockId = sqlResultSet->getUInt64("id");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getUrlLockId", e); }
}

// check whether the URL has not been locked again after a specific lock time (or is not locked anymore)
//  NOTE: if no lock time is specified, the function will return true only if the URL is not locked already
bool Database::checkUrlLock(unsigned long lockId, const std::string& lockTime) {
	bool result = false;

	// check arguments
	if(!lockId) throw DatabaseException("Parser::Database::checkUrlLock(): No URL lock ID specified");
	if(lockTime.empty()) return this->isUrlLockable(lockId);

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.checkUrlLock))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::checkUrlLock(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.checkUrlLock);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setString(1, lockTime);
		sqlStatement.setUInt64(2, lockId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::checkUrlLock", e); }

	return result;
}

// lock a URL in the database
std::string Database::lockUrl(UrlProperties& urlProperties, unsigned long lockTimeout) {
	// check connection
	this->checkConnection();

	if(urlProperties.lockId) {
		// check prepared SQL statement for locking the URL
		if(!(this->ps.lockUrl))
			throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::lockUrl(...)");

		// get prepared SQL statement for locking the URL
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.lockUrl);

		// lock URL in database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, lockTimeout);
			sqlStatement.setUInt64(2, urlProperties.lockId);
			sqlStatement.execute();
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::lockUrl", e); }
	}
	else {
		// check prepared SQL statement for adding an URL lock
		if(!(this->ps.addUrlLock))
			throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::lockUrl(...)");

		// get prepared SQL statement for adding an URL lock
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.addUrlLock);

		// add URL lock to database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlProperties.id);
			sqlStatement.setUInt64(2, lockTimeout);
			sqlStatement.execute();

			// get result
			urlProperties.lockId = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::lockUrl", e); }
	}

	return this->getUrlLock(urlProperties.lockId);
}

// unlock a URL in the database
void Database::unLockUrl(unsigned long lockId) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.unLockUrl))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::unLockUrl(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.unLockUrl);

	// unlock URL in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, lockId);
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

	// get prepared SQL statement
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

	// get prepared SQL statement
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
std::queue<Database::IdString> Database::getAllContents(unsigned long urlId) {
	std::queue<IdString> result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getAllContents))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getAllContents(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getAllContents);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet) {
			while(sqlResultSet->next())
				result.emplace(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content"));
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getAllContents", e); }

	return result;
}

// add parsed data to database (update if row for ID-specified content already exists
void Database::updateOrAddEntry(const ParsingEntry& data) {
	// check data sizes
	unsigned long tooLarge = 0;
	if(data.parsedId.size() > this->getMaxAllowedPacketSize()) tooLarge = data.parsedId.size();
	if(data.dateTime.size() > this->getMaxAllowedPacketSize() && data.dateTime.size() > tooLarge)
		tooLarge = data.dateTime.size();
	for(auto i = data.fields.begin(); i != data.fields.end(); ++i)
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

	{ // lock target table and parsing index table
		TableLock targetAndParsingIndexTableLock(*this, this->targetTableFull, "crawlserv_parsedtables");

		// update existing or add new entry
		unsigned long entryId = this->getEntryId(data.contentId);
		if(entryId) this->updateEntry(entryId, data.parsedId, data.dateTime, data.fields);
		else this->addEntry(data.contentId, data.parsedId, data.dateTime, data.fields);
	} // target table and parsing index table unlocked
}

// set URL as parsed in the database
void Database::setUrlFinished(unsigned long parsingId) {
	// check connection
	this->checkConnection();

	// check prepared SQL statements
	if(!(this->ps.setUrlFinished))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::setUrlFinished(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setUrlFinished);

	// get ID of URL from database
	try {
		// execute SQL query for setting URL to finished
		sqlStatement.setUInt64(1, parsingId);
		sqlStatement.execute();

		// TODO: execute SQL query for resetting analyzing status
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::setUrlFinished", e); }
}

// helper function: get ID of parsing entry for ID-specified content, return 0 if no entry exists
unsigned long Database::getEntryId(unsigned long contentId) {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getEntryId))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::getEntryId(...)");

	// get prepared SQL statement
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
	if(!(this->ps.updateEntry))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::updateEntry(...)");

	// get prepared SQL statement
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
	if(!(this->ps.addEntry))
		throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::addEntry(...)");

	// get prepared SQL statement
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

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.updateParsedTable);

	// get ID of URL from database
	sqlStatement.setUInt64(1, this->targetTableId);
	try {
		// execute SQL query
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Parser::Database::updateParsedTable", e); }
}

}
