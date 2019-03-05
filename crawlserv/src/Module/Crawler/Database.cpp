/*
 * Database.cpp
 *
 * This class provides database functionality for a crawler thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

namespace crawlservpp::Module::Crawler {

// constructor: initialize values
Database::Database(crawlservpp::Module::Database& dbThread) : crawlservpp::Wrapper::Database(dbThread), recrawl(false),
		logging(false), verbose(false), ps({0}) {}

// destructor stub
Database::~Database() {}

// convert thread ID to string for logging
void Database::setId(unsigned long analyzerId) {
	std::ostringstream idStrStr;
	idStrStr << analyzerId;
	this->idString = idStrStr.str();
}

// set website namespace
void Database::setWebsiteNamespace(const std::string& websiteNamespace) {
	this->websiteName = websiteNamespace;
}

// set URL list namespace
void Database::setUrlListNamespace(const std::string& urlListNamespace) {
	this->urlListName = urlListNamespace;
}

// enable or disable recrawling
void Database::setRecrawl(bool isRecrawl) {
	this->recrawl = isRecrawl;
}

// enable or disable logging
void Database::setLogging(bool isLogging) {
	this->logging = isLogging;
}

// enable or disable verbose logging
void Database::setVerbose(bool isVerbose) {
	this->verbose = isVerbose;
}

// prepare SQL statements for crawler, throws crawlservpp::Main::Database::Exception
void Database::prepare() {
	// create table names
	this->urlListTable = "crawlserv_" + this->websiteName + "_" + this->urlListName;
	this->linkTable = this->urlListTable + "_links";
	std::string crawledTable = this->urlListTable + "_crawled";

	// check connection to database
	this->checkConnection();

	// reserve memory
	this->reserveForPreparedStatements(17);

	// prepare SQL statements for crawler
	if(!(this->ps.isUrlExists)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isUrlExists()...");
		this->ps.isUrlExists = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + this->urlListTable + "` WHERE url = ?) AS result");
	}

	if(!(this->ps.isUrlHashExists)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares hash check for URLs...");
		this->ps.isUrlHashExists = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + this->urlListTable + "` WHERE hash = CRC32( ? )) AS result");
	}

	if(!(this->ps.getUrlId)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlId()...");
		this->ps.getUrlId = this->addPreparedStatement(
				"SELECT id FROM `" + this->urlListTable + "` WHERE url = ? LIMIT 1");
	}

	if(!(this->ps.isUrlCrawled)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isUrlCrawled()...");
		this->ps.isUrlCrawled = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + this->urlListTable + "` WHERE id = ? AND crawled = TRUE) AS result");
	}

	if(!(this->ps.getNextUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getNextUrl()...");
		if(this->recrawl) this->ps.getNextUrl = this->addPreparedStatement(
				"SELECT id, url FROM `" + this->urlListTable
				+ "` WHERE id > ? AND manual = FALSE AND (crawllock IS NULL OR crawllock < NOW()) ORDER BY id LIMIT 1");
		else this->ps.getNextUrl = this->addPreparedStatement(
				"SELECT id, url FROM `" + this->urlListTable
				+ "` WHERE id > ? AND crawled = 0 AND manual = FALSE AND (crawllock IS NULL OR crawllock < NOW()) ORDER BY id LIMIT 1");
	}

	if(!(this->ps.addUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares addUrl()...");
		this->ps.addUrl = this->addPreparedStatement(
				"INSERT INTO `" + this->urlListTable + "`(url, hash, manual) VALUES(?, CRC32(?), ?)");
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
				+ "` WHERE id = ? AND (crawllock IS NULL OR crawllock < NOW())) AS result");
	}

	if(!(this->ps.getUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlLock()...");
		this->ps.getUrlLock = this->addPreparedStatement(
				"SELECT crawllock FROM `" + this->urlListTable + "` WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.checkUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares checkUrlLock()...");
		this->ps.checkUrlLock = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + this->urlListTable
				+ "` WHERE id = ? AND (crawllock < NOW() OR crawllock <= ? OR crawllock IS NULL)) AS result");
	}

	if(!(this->ps.lockUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares lockUrl()...");
		this->ps.lockUrl = this->addPreparedStatement(
				"UPDATE `" + this->urlListTable + "` SET crawllock = NOW() + INTERVAL ? SECOND WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.unLockUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares unLockUrl()...");
		this->ps.unLockUrl = this->addPreparedStatement(
				"UPDATE `" + this->urlListTable + "` SET crawllock = NULL WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.saveContent)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares saveContent()...");
		this->ps.saveContent = this->addPreparedStatement(
				"INSERT INTO `" + crawledTable + "`(url, response, type, content) VALUES (?, ?, ?, ?)");
	}

	if(!(this->ps.saveArchivedContent)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares saveArchivedContent()...");
		this->ps.saveArchivedContent = this->addPreparedStatement(
				"INSERT INTO `" + crawledTable
				+ "`(url, crawltime, archived, response, type, content) VALUES (?, ?, TRUE, ?, ?, ?)");
	}

	if(!(this->ps.setUrlFinished)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares setUrlFinished()...");
		this->ps.setUrlFinished = this->addPreparedStatement(
				"UPDATE `" + this->urlListTable
				+ "` SET crawled = TRUE, crawllock = NULL WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.isArchivedContentExists)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isArchivedContentExists()...");
		this->ps.isArchivedContentExists = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + crawledTable + "` WHERE url = ? AND crawltime = ?) AS result");
	}
}

// check whether URL exists in database (uses hash check to first check probably existence of URL)
bool Database::isUrlExists(const std::string& urlString) {
	bool result = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statements
	if(!(this->ps.isUrlHashExists))
		throw DatabaseException("Missing prepared SQL statement for URL hash checks");
	sql::PreparedStatement& hashStatement = this->getPreparedStatement(this->ps.isUrlHashExists);
	if(!(this->ps.isUrlExists)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::isUrlExists(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlExists);

	// check URL existence in database
	try {
		// execute SQL query for hash check
		hashStatement.setString(1, urlString);
		std::unique_ptr<sql::ResultSet> sqlResultSet(hashStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		if(result) { // hash found -> perform real comparison
			// execute SQL query for checking URL
			sqlStatement.setString(1, urlString);
			sqlResultSet = std::unique_ptr<sql::ResultSet>(sqlStatement.executeQuery());

			// get result
			if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isUrlExists", e); }

	return result;
}

// lock URL list
void Database::lockUrlList() {
	this->lockTables(this->urlListTable, this->linkTable);
}

// get the ID of an URL (uses hash check for first checking probable existence of URL)
unsigned long Database::getUrlId(const std::string& urlString) {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlId)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlId(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlId);

	// get ID of URL from database
	try {
		// execute SQL query for getting URL
		sqlStatement.setString(1, urlString);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("id");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlId", e); }

	return result;
}

// check whether an URL has been crawled
bool Database::isUrlCrawled(unsigned long urlId) {
	bool result = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isUrlCrawled)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::isUrlCrawled(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlCrawled);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isUrlCrawled", e); }

	return result;
}

// get the next URL to crawl from database or empty IdString if all URLs have been crawled
std::pair<unsigned long, std::string> Database::getNextUrl(unsigned long currentUrlId) {
	std::pair<unsigned long, std::string> result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getNextUrl)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getNextUrl(...)");
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
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getNextUrl", e); }

	return result;
}

// add URL to database and return ID of newly added URL
unsigned long Database::addUrl(const std::string& urlString, bool manual) {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.addUrl)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::addUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.addUrl);

	// add URL to database and get resulting ID
	try {
		// execute SQL query
		sqlStatement.setString(1, urlString);
		sqlStatement.setString(2, urlString);
		sqlStatement.setBoolean(3, manual);
		sqlStatement.execute();

		// get result
		result = this->getLastInsertedId();
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::addUrl", e); }

	return result;
}

// get the position of the URL in the URL list
unsigned long Database::getUrlPosition(unsigned long urlId) {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlPosition)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlPosition()");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlPosition);

	// get URL position of URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlPosition", e); }

	return result;
}

// get the number of URLs in the URL list
unsigned long Database::getNumberOfUrls() {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getNumberOfUrls)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getNumberOfUrls()");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getNumberOfUrls);

	// get number of URLs from database
	try {
		// execute SQL query
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getNumberOfUrls", e); }

	return result;
}

// check whether an URL is already locked in database
bool Database::isUrlLockable(unsigned long urlId) {
	bool result = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isUrlLockable)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::isUrlLockable(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlLockable);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isUrlLockable", e); }

	return result;
}

// get the URL lock end time of a specific URL from database
std::string Database::getUrlLock(unsigned long urlId) {
	std::string result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlLock)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlLock(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlLock);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("crawllock");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlLock", e); }

	return result;
}

// check whether the URL has not been locked again after a specific lock time (or is not locked anymore)
bool Database::checkUrlLock(unsigned long urlId, const std::string& lockTime) {
	bool result = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.checkUrlLock)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::checkUrlLock(...)");
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
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::checkUrlLock", e); }

	return result;
}

// lock a URL in the database
std::string Database::lockUrl(unsigned long urlId, unsigned long lockTimeout) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.lockUrl)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::lockUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.lockUrl);

	// lock URL in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, lockTimeout);
		sqlStatement.setUInt64(2, urlId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::lockUrl", e); }

	return this->getUrlLock(urlId);
}

// unlock a URL in the database
void Database::unLockUrl(unsigned long urlId) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.unLockUrl)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::unLockUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.unLockUrl);

	// unlock URL in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::unLockUrl", e); }
}

// save content to database
void Database::saveContent(unsigned long urlId, unsigned int response, const std::string& type,
		const std::string& content) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.saveContent)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::saveContent(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.saveContent);

	// save content to database
	try {
		// execute SQL query if possible
		if(content.size() <= this->getMaxAllowedPacketSize()) {
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setUInt(2, response);
			sqlStatement.setString(3, type);
			sqlStatement.setString(4, content);
			sqlStatement.execute();
		}
		else if(this->logging) {
			// show warning about content size
			bool adjustServerSettings = false;
			std::ostringstream logStrStr;
			logStrStr.imbue(std::locale(""));
			logStrStr << "[#" << this->idString << "] WARNING: Some content could not be saved to the database, because its size ("
					<< content.size() << " bytes) exceeds the ";
			if(content.size() > 1073741824) logStrStr << "mySQL maximum of 1 GiB.";
			else {
				logStrStr << "current mySQL server maximum of " << this->getMaxAllowedPacketSize() << " bytes.";
				adjustServerSettings = true;
			}
			this->log(logStrStr.str());
			if(adjustServerSettings)
				this->log("[#" + this->idString + "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::saveContent", e); }
}

// save archived content to database
void Database::saveArchivedContent(unsigned long urlId, const std::string& timeStamp, unsigned int response,
		const std::string& type, const std::string& content) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.saveArchivedContent))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::saveArchivedContent(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.saveArchivedContent);

	try {
		// save archived content to database if possible
		if(content.size() <= this->getMaxAllowedPacketSize()) {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setString(2, timeStamp);
			sqlStatement.setUInt(3, response);
			sqlStatement.setString(4, type);
			sqlStatement.setString(5, content);
			sqlStatement.execute();
		}
		else if(this->logging) {
			// show warning about content size
			bool adjustServerSettings = false;
			std::ostringstream logStrStr;
			logStrStr.imbue(std::locale(""));
			logStrStr << "[#" << this->idString << "] WARNING: Some content could not be saved to the database, because its size ("
					<< content.size() << " bytes) exceeds the ";
			if(content.size() > 1073741824) logStrStr << "mySQL maximum of 1 GiB.";
			else {
				logStrStr << "current mySQL server maximum of " << this->getMaxAllowedPacketSize() << " bytes.";
				adjustServerSettings = true;
			}
			this->log(logStrStr.str());
			if(adjustServerSettings)
				this->log("[#" + this->idString + "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::saveArchivedContent", e); }
}

// set URL as crawled in the database
void Database::setUrlFinished(unsigned long urlId) {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.setUrlFinished)) throw DatabaseException("Missing prepared SQL statement for Crawler::Database::setUrlFinished(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setUrlFinished);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::setUrlFinished", e); }
}

bool Database::isArchivedContentExists(unsigned long urlId, const std::string& timeStamp) {
	bool result = false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isArchivedContentExists))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::isArchivedContentExists(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isArchivedContentExists);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.setString(2, timeStamp);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isArchivedContentExists", e); }

	return result;
}

// helper function: check the current URL lock and re-lock the URL if possible, return whether the re-locking was successful
bool Database::renewUrlLock(unsigned long lockTimeout, unsigned long urlId, std::string& lockTime) {
	if(this->checkUrlLock(urlId, lockTime)) {
		lockTime = this->lockUrl(urlId, lockTimeout);
		return true;
	}
	return false;
}

}
