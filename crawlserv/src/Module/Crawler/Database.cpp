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
Database::Database(Module::Database& dbThread) : Wrapper::Database(dbThread), recrawl(false),
		logging(true), verbose(false), urlCaseSensitive(true), urlDebug(false), urlStartupCheck(true), ps({0}) {}

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

// enable or disable case-sensitive URLs
void Database::setUrlCaseSensitive(bool isUrlCaseSensitive) {
	this->urlCaseSensitive = isUrlCaseSensitive;
}

// enable or disable URL debugging
void Database::setUrlDebug(bool isUrlDebug) {
	this->urlDebug = isUrlDebug;
}

// enable or disable URL check on startup
void Database::setUrlStartupCheck(bool isUrlStartupCheck) {
	this->urlStartupCheck = isUrlStartupCheck;
}

// prepare SQL statements for crawler, throws Main::Database::Exception
void Database::prepare() {
	// create table names
	this->urlListTable = "crawlserv_" + this->websiteName + "_" + this->urlListName;
	this->crawlingTable = this->urlListTable + "_crawling";
	std::string crawledTable = this->urlListTable + "_crawled";

	// create SQL string for URL hashing
	std::string hash("CRC32( ");
	if(this->urlCaseSensitive) hash += "?";
	else hash += "LOWER( ? )";
	hash.push_back(')');

	// check connection to database
	this->checkConnection();

	// reserve memory
	this->reserveForPreparedStatements(23);

	// prepare SQL statements for crawler
	if(!(this->ps.isUrlExists)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isUrlExists()...");

		std::string sqlQuery(
				"SELECT EXISTS ("
					" SELECT * FROM ("
						" SELECT url FROM `" + this->urlListTable + "` WHERE hash = " + hash +
					" ) AS HashResult WHERE ");
		if(this->urlCaseSensitive) sqlQuery += "url LIKE ?";
		else sqlQuery += "LOWER( url ) LIKE LOWER( ? )";
		sqlQuery +=
				" ) AS result";

		this->ps.isUrlExists = this->addPreparedStatement(sqlQuery);
	}

	if(!(this->ps.getUrlIdLockId)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlIdLockId()...");
		this->ps.getUrlIdLockId = this->addPreparedStatement(
				"SELECT a.id AS url_id, b.id AS lock_id FROM `" + this->urlListTable + "` AS a LEFT OUTER JOIN `"
				+ this->crawlingTable + "` AS b ON a.id = b.url WHERE a.url = ? LIMIT 1");
	}

	if(!(this->ps.isUrlCrawled)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isUrlCrawled()...");
		this->ps.isUrlCrawled = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + this->crawlingTable + "` WHERE id = ? AND success = TRUE) AS result");
	}

	if(!(this->ps.getNextUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getNextUrl()...");

		std::ostringstream sqlQueryStrStr;
		sqlQueryStrStr << "SELECT a.id AS url_id, a.url AS url, b.id AS lock_id FROM `" + this->urlListTable
				+ "` AS a LEFT OUTER JOIN `" + this->crawlingTable
				+ "` AS b ON a.id = b.url WHERE a.id > ? AND manual = FALSE";
		if(!(this->recrawl)) sqlQueryStrStr << " AND (b.success IS NULL OR b.success = FALSE)";
		sqlQueryStrStr << " AND (b.locktime IS NULL OR b.locktime < NOW()) ORDER BY a.id LIMIT 1";

		this->ps.getNextUrl = this->addPreparedStatement(sqlQueryStrStr.str());
	}

	if(!(this->ps.addUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares addUrl()...");
		this->ps.addUrl = this->addPreparedStatement(
				"INSERT INTO `" + this->urlListTable + "`(url, hash, manual) VALUES (?, " + hash + ", ?)");
	}

	if(!(this->ps.add10Urls)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares addUrls() [1/3]...");
		this->ps.add10Urls = this->addPreparedStatement(
				"INSERT INTO `" + this->urlListTable + "`(url, hash) VALUES"
				 " (?, " + hash + "), (?, " + hash + "), (?, " + hash + "), (?, " + hash + "), "
				 " (?, " + hash + "), (?, " + hash + "), (?, " + hash + "), (?, " + hash + "), "
				 " (?, " + hash + "), (?, " + hash + ")");
	}

	if(!(this->ps.add100Urls)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares addUrls() [2/3]...");

		std::string sqlQuery = "INSERT INTO `" + this->urlListTable + "`(url, hash) VALUES (?, " + hash + ")";
		for(unsigned short n = 0; n < 99; n++) sqlQuery += ", (?, " + hash + ")";

		this->ps.add100Urls = this->addPreparedStatement(sqlQuery);
	}

	if(!(this->ps.add1000Urls)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares addUrls() [3/3]...");

		std::string sqlQuery = "INSERT INTO `" + this->urlListTable + "`(url, hash) VALUES (?, " + hash + ")";
		for(unsigned short n = 0; n < 999; n++) sqlQuery += ", (?, " + hash + ")";

		this->ps.add1000Urls = this->addPreparedStatement(sqlQuery);
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
				"SELECT (locktime IS NULL OR locktime < NOW()) AS result FROM `"
				+ this->crawlingTable + "` WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.getUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlLock()...");
		this->ps.getUrlLock = this->addPreparedStatement(
				"SELECT locktime FROM `" + this->crawlingTable + "` WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.getUrlLockId)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares getUrlLockId()...");

		std::ostringstream sqlQueryStr;
		sqlQueryStr << "SELECT id FROM `" << this->crawlingTable << "` WHERE url = ? LIMIT 1";

		this->ps.getUrlLockId = this->addPreparedStatement(sqlQueryStr.str());
	}

	if(!(this->ps.checkUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares checkUrlLock()...");
		this->ps.checkUrlLock = this->addPreparedStatement(
				"SELECT (locktime < NOW() OR locktime <= ? OR locktime IS NULL) AS result FROM "
				+ this->crawlingTable + " WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.lockUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares lockUrl() [1/2]...");
		this->ps.lockUrl = this->addPreparedStatement("UPDATE `" + this->crawlingTable
				+ "` SET locktime = NOW() + INTERVAL ? SECOND WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.addUrlLock)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares lockUrl() [2/2]...");
		std::ostringstream sqlQueryStr;
		sqlQueryStr << "INSERT INTO `" << this->crawlingTable
				<< "` (url, locktime, success) VALUES (?, NOW() + INTERVAL ? SECOND, FALSE)";
		this->ps.addUrlLock = this->addPreparedStatement(sqlQueryStr.str());
	}

	if(!(this->ps.unLockUrl)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares unLockUrl()...");
		this->ps.unLockUrl = this->addPreparedStatement("UPDATE `" + this->crawlingTable
				+ "` SET locktime = NULL WHERE id = ? LIMIT 1");
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
				"UPDATE `" + this->crawlingTable + "` SET success = TRUE, locktime = NULL WHERE id = ? LIMIT 1");
	}

	if(!(this->ps.isArchivedContentExists)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares isArchivedContentExists()...");
		this->ps.isArchivedContentExists = this->addPreparedStatement(
				"SELECT EXISTS (SELECT * FROM `" + crawledTable + "` WHERE url = ? AND crawltime = ?) AS result");
	}

	if(!(this->ps.urlDuplicationCheck) && (this->urlStartupCheck || this->urlDebug)) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares urlDuplicationCheck()...");

		std::string groupBy;
		if(this->urlCaseSensitive) groupBy = "url";
		else groupBy = "LOWER(url)";

		this->ps.urlDuplicationCheck = this->addPreparedStatement(
				" SELECT CAST( " + groupBy + " AS BINARY ) AS url, COUNT( " + groupBy + " ) FROM `" + this->urlListTable
					+ "` GROUP BY CAST( " + groupBy + " AS BINARY ) HAVING COUNT( " + groupBy + " ) > 1"
		);
	}

	if(!(this->ps.urlHashCheck) && this->urlStartupCheck) {
		if(this->verbose) this->log("[#" + this->idString + "] prepares urlHashCheck()...");

		std::string urlHash("CRC32( ");
		if(this->urlCaseSensitive) urlHash += "url";
		else urlHash += "LOWER( url )";
		urlHash.push_back(')');

		this->ps.urlHashCheck = this->addPreparedStatement(
				"SELECT url FROM `" + this->urlListTable + "` WHERE hash <> " + urlHash + " LIMIT 1"
		);
	}
}

// check whether URL exists in database (uses hash check to first check probably existence of URL)
bool Database::isUrlExists(const std::string& urlString) {
	bool result = false;

	// check argument
	if(urlString.empty())
		throw DatabaseException("Crawler::Database::isUrlExists(): No URL specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isUrlExists))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::isUrlExists(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlExists);

	// check URL existence in database
	try {
		// execute SQL query
		sqlStatement.setString(1, urlString);
		sqlStatement.setString(2, urlString);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isUrlExists", e); }

	return result;
}

// get the ID and lock ID of an URL (uses hash check for first checking the probable existence of the URL)
//  NOTE: The second value of the tuple (the URL name) needs already to be set!
void Database::getUrlIdLockId(UrlProperties& urlProperties) {
	// check argument
	if(urlProperties.url.empty())
		throw DatabaseException("Crawler::Database::getUrlIdLockId(): No URL specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlIdLockId))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlId(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlIdLockId);

	// get ID of URL from database
	try {
		// execute SQL query for getting URL
		sqlStatement.setString(1, urlProperties.url);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next())  {
			urlProperties.id = sqlResultSet->getUInt64("url_id");
			urlProperties.lockId = sqlResultSet->getUInt64("lock_id");
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlIdLockId", e); }
}

// check whether an URL, specified by its crawling i.e. lock ID, has been crawled
bool Database::isUrlCrawled(unsigned long crawlingId) {
	bool result = false;

	// check argument
	if(!crawlingId) return false;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isUrlCrawled))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::isUrlCrawled(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlCrawled);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, crawlingId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isUrlCrawled", e); }

	return result;
}

// get the next URL to crawl from database, return ID, URL and lock ID of next URL or empty tuple if all URLs have been parsed
Database::UrlProperties Database::getNextUrl(unsigned long currentUrlId) {
	UrlProperties result;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getNextUrl))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getNextUrl(...)");

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
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getNextUrl", e); }

	return result;
}

// add URL to database
void Database::addUrl(const std::string& urlString, bool manual) {
	// check argument
	if(urlString.empty())
		throw DatabaseException("Crawler::Database::addUrl(): No URL specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.addUrl))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::addUrl(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.addUrl);

	// add URL to database and get resulting ID
	try {
		// execute SQL query
		sqlStatement.setString(1, urlString);
		sqlStatement.setString(2, urlString);
		sqlStatement.setBoolean(3, manual);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::addUrl", e); }
}

// add URLs to the database
void Database::addUrls(const std::vector<std::string>& urls) {
	unsigned long pos = 0;

	// check argument
	if(urls.empty()) return;

	// check connection
	this->checkConnection();

	// check prepared SQL statements
	if(!(this->ps.add10Urls))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::addUrls(...)");
	if(!(this->ps.add100Urls))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::addUrls(...)");
	if(!(this->ps.add1000Urls))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::addUrls(...)");

	// get prepared SQL statements
	sql::PreparedStatement& sqlStatement10 = this->getPreparedStatement(this->ps.add10Urls);
	sql::PreparedStatement& sqlStatement100 = this->getPreparedStatement(this->ps.add100Urls);
	sql::PreparedStatement& sqlStatement1000 = this->getPreparedStatement(this->ps.add1000Urls);

	try {
		// add 1.000 URLs at once
		while(urls.size() - pos >= 1000) {
			for(unsigned long n = 0; n < 1000; n++) {
				sqlStatement1000.setString((n * 2) + 1, urls.at(pos + n));
				sqlStatement1000.setString((n * 2) + 2, urls.at(pos + n));
			}
			sqlStatement1000.execute();
			pos += 1000;
		}

		// add 100 URLs at once
		while(urls.size() - pos >= 100) {
			for(unsigned long n = 0; n < 100; n++) {
				sqlStatement100.setString((n * 2) + 1, urls.at(pos + n));
				sqlStatement100.setString((n * 2) + 2, urls.at(pos + n));
			}
			sqlStatement100.execute();
			pos += 100;
		}

		// add 10 URLs at once
		while(urls.size() - pos >= 10) {
			for(unsigned long n = 0; n < 10; n++) {
				sqlStatement10.setString((n * 2) + 1, urls.at(pos + n));
				sqlStatement10.setString((n * 2) + 2, urls.at(pos + n));
			}
			sqlStatement10.execute();
			pos += 10;
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::addUrls", e); }

	// add remaining URLs
	while(urls.size() - pos) {
		this->addUrl(urls.at(pos), false);
		pos++;
	}
}

// add URL to database and return ID of newly added URL
unsigned long Database::addUrlGetId(const std::string& urlString, bool manual) {
	unsigned long result = 0;

	// add URL to database
	this->addUrl(urlString, manual);

	// get resulting ID
	try {
		// get result
		result = this->getLastInsertedId();
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::addUrlGetId", e); }
	return result;
}

// get the position of the URL in the URL list
unsigned long Database::getUrlPosition(unsigned long urlId) {
	unsigned long result = 0;

	// check argument
	if(!urlId) return 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlPosition))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlPosition()");

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
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlPosition", e); }

	return result;
}

// get the number of URLs in the URL list
unsigned long Database::getNumberOfUrls() {
	unsigned long result = 0;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getNumberOfUrls))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getNumberOfUrls()");

	// get prepared SQL statement
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

// check the URL list for duplicates, throw DatabaseException if duplicates are found
void Database::urlDuplicationCheck() {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.urlDuplicationCheck))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::urlDuplicationCheck()");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.urlDuplicationCheck);

	// get number of URLs from database
	try {
		// execute SQL query
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			throw DatabaseException("Crawler::Database::urlDuplicationCheck(): Duplicate URL \'"
					+ sqlResultSet->getString("url") + "\" in `" + this->urlListTable + "`");
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::urlDuplicationCheck", e); }
}

// check the hash values in the URL list, throw DatabaseException if at least one mismatch is found
void Database::urlHashCheck() {
	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.urlDuplicationCheck))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::urlHashCheck()");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.urlHashCheck);

	// get number of URLs from database
	try {
		// execute SQL query
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			throw DatabaseException("Crawler::Database::urlHashCheck(): Corrupted hash for URL \'"
					+ sqlResultSet->getString("url") + "\" in `" + this->urlListTable + "`");
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::urlHashCheck", e); }
}

// check whether an URL is already locked in database
bool Database::isUrlLockable(unsigned long lockId) {
	bool result = false;

	// check argument
	if(!lockId) return true;

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isUrlLockable))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::isUrlLockable(...)");

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
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isUrlLockable", e); }

	return result;
}

// check whether the URL has not been locked again after a specific lock time (or is not locked anymore)
//  NOTE: if no lock time is specified, the function will return true only if the URL is not locked already
bool Database::checkUrlLock(unsigned long lockId, const std::string& lockTime) {
	bool result = false;

	// check arguments
	if(!lockId)
		throw DatabaseException("Crawler::Database::checkUrlLock(): No URL lock ID specified");
	if(lockTime.empty()) return this->isUrlLockable(lockId);

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.checkUrlLock))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::checkUrlLock(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.checkUrlLock);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setString(1, lockTime);
		sqlStatement.setUInt64(2, lockId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get (and parse) result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::checkUrlLock", e); }

	return result;
}

// get the URL lock end time of a specific URL from database
std::string Database::getUrlLock(unsigned long lockId) {
	std::string result;

	// check argument
	if(!lockId)
		throw DatabaseException("Crawler::Database::getUrlLock(): No URL lock ID specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.getUrlLock))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlLock(...)");

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
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlLock", e); }

	if(result.empty()) {
		std::ostringstream errStrStr;
		errStrStr << "Crawler::Database::getUrlLock(): Locking of URL lock #" << lockId << " failed";
		throw DatabaseException(errStrStr.str());
	}

	return result;
}

// get the URL lock ID for a specific URL from the database if none has been received yet
//  NOTE: The first value of the tuple (the URL ID) needs already to be set!
void Database::getUrlLockId(UrlProperties& urlProperties) {
	// check arguments
	if(!urlProperties.id)
		throw DatabaseException("Parser::Database::getUrlLockId(): No URL ID specified");
	if(urlProperties.lockId) return; // already got lock ID

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.checkUrlLock))
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

// lock a URL in the database
std::string Database::lockUrl(UrlProperties& urlProperties, unsigned long lockTimeout) {
	// check argument
	if(!urlProperties.id)
		throw DatabaseException("Crawler::Database::lockUrl(): No URL ID specified");

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

			// get lock ID
			urlProperties.lockId = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::lockUrl", e); }
	}

	return this->getUrlLock(urlProperties.lockId);
}

// unlock a URL in the database
void Database::unLockUrl(unsigned long lockId) {
	// check argument
	if(!lockId)
		throw DatabaseException("Crawler::Database::unLockUrl(): No URL lock ID specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.unLockUrl))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::unLockUrl(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.unLockUrl);

	// unlock URL in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, lockId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::unLockUrl", e); }
}

// save content to database
void Database::saveContent(unsigned long urlId, unsigned int response, const std::string& type, const std::string& content) {
	// check argument
	if(!urlId)
		throw DatabaseException("Crawler::Database::saveContent(): No URL ID specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.saveContent))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::saveContent(...)");

	// get prepared SQL statement
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
			logStrStr << "[#" << this->idString
					<< "] WARNING: Some content could not be saved to the database, because its size ("
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
	// check arguments
	if(!urlId)
		throw DatabaseException("Crawler::Database::saveArchivedContent(): No URL ID specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.saveArchivedContent))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::saveArchivedContent(...)");

	// get prepared SQL statement
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
void Database::setUrlFinished(unsigned long crawlingId) {
	// check argument
	if(!crawlingId)
		throw DatabaseException("Crawler::Database::setUrlFinished(): No crawling ID specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.setUrlFinished))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::setUrlFinished(...)");

	// get prepared SQL statement
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setUrlFinished);

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, crawlingId);
		sqlStatement.execute();
	}
	catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::setUrlFinished", e); }
}

bool Database::isArchivedContentExists(unsigned long urlId, const std::string& timeStamp) {
	bool result = false;

	// check argument
	if(!urlId)
		throw DatabaseException("Crawler::Database::isArchivedContentExists(): No URL ID specified");

	// check connection
	this->checkConnection();

	// check prepared SQL statement
	if(!(this->ps.isArchivedContentExists))
		throw DatabaseException("Missing prepared SQL statement for Crawler::Database::isArchivedContentExists(...)");

	// get prepared SQL statement
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
//  NOTE: This function locks and unlocks the crawling table by itself and writes the lock expiration time to lockTime
bool Database::renewUrlLock(unsigned long lockTimeout, UrlProperties& urlProperties, std::string& lockTime) {
	bool result = false;

	// check argument
	if(!urlProperties.id)
		throw DatabaseException("Crawler::Database::renewUrlLock(): No URL ID specified");

	{ // lock crawling table
		TableLock crawlingTableLock(*this, this->crawlingTable);

		// get lock ID if not already received
		this->getUrlLockId(urlProperties);

		// check whether URL lock exists
		if(urlProperties.lockId) {
			// check URL lock
			if(this->checkUrlLock(urlProperties.lockId, lockTime)) {
				// lock URL
				lockTime = this->lockUrl(urlProperties, lockTimeout);
				this->releaseLocks();
				result = true;
			}
		}
		else {
			lockTime = this->lockUrl(urlProperties, lockTimeout); // add URL lock if none exists
			result = true;
		}
	} // crawling table unlocked

	return result;
}

}
