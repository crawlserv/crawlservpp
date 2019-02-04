/*
 * Database.cpp
 *
 * This class provides database functionality for a crawler thread by implementing the Module::DBWrapper interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

namespace crawlservpp::Module::Crawler {

// constructor: initialize values
Database::Database(crawlservpp::Module::DBThread& dbThread) : crawlservpp::Module::DBWrapper(dbThread) {
	this->recrawl = false;
	this->logging = false;
	this->verbose = false;

	this->psIsUrlExists = 0;
	this->psIsUrlHashExists = 0;
	this->psGetUrlId = 0;
	this->psIsUrlCrawled = 0;
	this->psGetNextUrl = 0;
	this->psAddUrl = 0;
	this->psGetUrlPosition = 0;
	this->psGetNumberOfUrls = 0;
	this->psIsUrlLockable = 0;
	this->psCheckUrlLock = 0;
	this->psGetUrlLock = 0;
	this->psLockUrl = 0;
	this->psUnLockUrl = 0;
	this->psSaveContent = 0;
	this->psSaveArchivedContent = 0;
	this->psSetUrlFinished = 0;
	this->psIsArchivedContentExists = 0;
	this->psIsLinkExists = 0;
	this->psAddLink = 0;
	this->psAddLinkArchived = 0;
}

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
	this->reservePreparedStatements(20);

	try {
		// prepare SQL statements for crawler
		if(!(this->psIsUrlExists)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares isUrlExists()...");
			this->psIsUrlExists = this->addPreparedStatement("SELECT EXISTS (SELECT 1 FROM `" + this->urlListTable
					+ "` WHERE url = ?) AS result");
		}
		if(!(this->psIsUrlHashExists)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares hash check for URLs...");
			this->psIsUrlHashExists = this->addPreparedStatement("SELECT EXISTS (SELECT 1 FROM `" + this->urlListTable
					+ "` WHERE hash = CRC32( ? )) AS result");
		}
		if(!(this->psGetUrlId)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares getUrlId()...");
			this->psGetUrlId = this->addPreparedStatement("SELECT id FROM `" + this->urlListTable + "` WHERE url = ? LIMIT 1");
		}
		if(!(this->psIsUrlCrawled)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares isUrlCrawled()...");
			this->psIsUrlCrawled = this->addPreparedStatement("SELECT EXISTS (SELECT 1 FROM `" + this->urlListTable + "` WHERE id = ?"
					" AND crawled = TRUE) AS result");
		}
		if(!(this->psGetNextUrl)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares getNextUrl()...");
			if(this->recrawl) this->psGetNextUrl = this->addPreparedStatement("SELECT id, url FROM `" + this->urlListTable
							+ "` WHERE id > ? AND manual = FALSE AND (crawllock IS NULL OR crawllock < NOW()) ORDER BY id LIMIT 1");
			else this->psGetNextUrl = this->addPreparedStatement("SELECT id, url FROM `" + this->urlListTable
				+ "` WHERE id > ? AND crawled = 0 AND manual = FALSE AND (crawllock IS NULL OR crawllock < NOW()) ORDER BY id LIMIT 1");
		}
		if(!(this->psAddUrl)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares addUrl()...");
			this->psAddUrl =
					this->addPreparedStatement("INSERT INTO `" + this->urlListTable + "`(url, hash, manual) VALUES(?, CRC32(?), ?)");
		}
		if(!(this->psGetUrlPosition)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares getUrlPosition()...");
			this->psGetUrlPosition = this->addPreparedStatement("SELECT COUNT(id) AS result FROM `" + this->urlListTable
					+ "` WHERE id < ?");
		}
		if(!(this->psGetNumberOfUrls)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares getNumberOfUrls()...");
			this->psGetNumberOfUrls = this->addPreparedStatement("SELECT COUNT(id) AS result FROM `" + this->urlListTable + "`");
		}

		if(!(this->psIsUrlLockable)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares isUrlLockable()...");
			this->psIsUrlLockable = this->addPreparedStatement("SELECT EXISTS (SELECT 1 FROM `" + this->urlListTable
					+ "` WHERE id = ? AND (crawllock IS NULL OR crawllock < NOW())) AS result");
		}
		if(!(this->psGetUrlLock)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares getUrlLock()...");
			this->psGetUrlLock = this->addPreparedStatement("SELECT crawllock FROM `" + this->urlListTable + "` WHERE id = ? LIMIT 1");
		}
		if(!(this->psCheckUrlLock)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares checkUrlLock()...");
			this->psCheckUrlLock = this->addPreparedStatement("SELECT EXISTS (SELECT 1 FROM `" + this->urlListTable
					+ "` WHERE id = ? AND (crawllock < NOW() OR crawllock <= ? OR crawllock IS NULL)) AS result");
		}
		if(!(this->psLockUrl)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares lockUrl()...");
			this->psLockUrl = this->addPreparedStatement("UPDATE `" + this->urlListTable + "` SET crawllock = NOW() + INTERVAL ? SECOND"
					" WHERE id = ? LIMIT 1");
		}
		if(!(this->psUnLockUrl)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares unLockUrl()...");
			this->psUnLockUrl = this->addPreparedStatement("UPDATE `" + this->urlListTable + "` SET crawllock = NULL WHERE id = ? LIMIT 1");
		}

		if(!(this->psSaveContent)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares saveContent()...");
			this->psSaveContent = this->addPreparedStatement("INSERT INTO `" + crawledTable + "`(url, response, type, content)"
					" VALUES (?, ?, ?, ?)");
		}
		if(!(this->psSaveArchivedContent)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares saveArchivedContent()...");
			this->psSaveArchivedContent = this->addPreparedStatement("INSERT INTO `" + crawledTable + "`(url, crawltime, archived,"
					" response, type, content) VALUES (?, ?, TRUE, ?, ?, ?)");
		}
		if(!(this->psSetUrlFinished)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares setUrlFinished()...");
			this->psSetUrlFinished = this->addPreparedStatement("UPDATE `" + this->urlListTable + "` SET crawled = TRUE, parsed = FALSE,"
					" extracted = FALSE, analyzed = FALSE, crawllock = NULL WHERE id = ? LIMIT 1");
		}
		if(!(this->psIsArchivedContentExists)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares isArchivedContentExists()...");
			this->psIsArchivedContentExists = this->addPreparedStatement("SELECT EXISTS (SELECT 1 FROM `" + crawledTable
					+ "` WHERE url = ? AND crawltime = ?) AS result");
		}
		if(!(this->psIsLinkExists)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares addLinkIfNotExists() [1/3]...");
			this->psIsLinkExists = this->addPreparedStatement("SELECT EXISTS (SELECT 1 FROM `" + linkTable + "` WHERE fromurl = ?"
					" AND tourl = ?) AS result");
		}
		if(!(this->psAddLink)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares addLinkIfNotExists() [2/3]...");
			this->psAddLink = this->addPreparedStatement("INSERT INTO `" + linkTable + "`(fromurl, tourl, archived) VALUES(?, ?, FALSE)");
		}
		if(!(this->psAddLinkArchived)) {
			if(this->verbose) this->log("crawler", "[#" + this->idString + "] prepares addLinkIfNotExists() [3/3]...");
			this->psAddLinkArchived = this->addPreparedStatement("INSERT INTO `" + linkTable + "`(fromurl, tourl, archived)"
					" VALUES(?, ?, TRUE)");
		}
	}
	catch(sql::SQLException &e) {
		// set error message
		std::ostringstream errorStrStr;
		errorStrStr << "prepare() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// check whether URL exists in database (uses hash check to first check probably existence of URL)
bool Database::isUrlExists(const std::string& urlString) {
	bool result = false;

	// check prepared SQL statements
	if(!(this->psIsUrlHashExists))
		throw DatabaseException("Missing prepared SQL statement for URL hash checks");
	sql::PreparedStatement& hashStatement = this->getPreparedStatement(this->psIsUrlHashExists);
	if(!(this->psIsUrlExists)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::isUrlExists(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psIsUrlExists);

	// check connection
	this->checkConnection();

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
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlExists() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// lock URL list
void Database::lockUrlList() {
	this->lockTables(this->urlListTable, this->linkTable);
}

// get the ID of an URL (uses hash check for first checking probable existence of URL)
unsigned long Database::getUrlId(const std::string& urlString) {
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetUrlId)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::getUrlId(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psGetUrlId);

	// check connection
	this->checkConnection();

	// get ID of URL from database
	try {
		// execute SQL query for getting URL
		sqlStatement.setString(1, urlString);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("id");
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlId() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") - " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// check whether an URL has been crawled
bool Database::isUrlCrawled(unsigned long urlId) {
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsUrlCrawled)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::isUrlCrawled(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psIsUrlCrawled);

	// check connection
	this->checkConnection();

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlCrawled() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// get the next URL to crawl from database or empty IdString if all URLs have been crawled
std::pair<unsigned long, std::string> Database::getNextUrl(unsigned long currentUrlId) {
	std::pair<unsigned long, std::string> result;

	// check prepared SQL statement
	if(!(this->psGetNextUrl)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::getNextUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psGetNextUrl);

	// check connection
	this->checkConnection();

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
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "getNextUrl() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// add URL to database and return ID of newly added URL
unsigned long Database::addUrl(const std::string& urlString, bool manual) {
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psAddUrl)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::addUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psAddUrl);

	// check connection
	this->checkConnection();

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
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "addUrl(\'" + urlString + "\', " + (manual ? "true" : "false") + ") SQL Error #" << e.getErrorCode()
				<< " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// get the position of the URL in the URL list
unsigned long Database::getUrlPosition(unsigned long urlId) {
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetUrlPosition)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::getUrlPosition()");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psGetUrlPosition);

	// check connection
	this->checkConnection();

	// get URL position of URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("result");
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlPosition() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// get the number of URLs in the URL list
unsigned long Database::getNumberOfUrls() {
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetNumberOfUrls)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::getNumberOfUrls()");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psGetNumberOfUrls);

	// check connection
	this->checkConnection();

	// get number of URLs from database
	try {
		// execute SQL query
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getUInt64("result");
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "getNumberOfUrls() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// check whether a link between two websites already exists and add it to the database if not
void Database::addLinkIfNotExists(unsigned long from, unsigned long to, bool archived) {
	// check prepared SQL statements
	if(!(this->psIsLinkExists)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::addLinkIfNotExists(...)");
	if(!(this->psAddLinkArchived)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::addLinkIfNotExists(...)");
	if(!(this->psAddLink)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::addLinkIfNotExists(...)");
	sql::PreparedStatement& checkStatement = this->getPreparedStatement(this->psIsLinkExists);
	sql::PreparedStatement& addStatement = archived ?
			this->getPreparedStatement(this->psAddLinkArchived) : this->getPreparedStatement(this->psAddLink);

	// check connection
	this->checkConnection();

	// check existence of link and add it to database if it does not exist there already
	try {
		// execute SQL query for checking existence of link
		checkStatement.setUInt64(1, from);
		checkStatement.setUInt64(2, to);
		std::unique_ptr<sql::ResultSet> sqlResultSet(checkStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) if(!(sqlResultSet->getBoolean("result"))) {

			// link does not exist: execute SQL query for adding link
			addStatement.setUInt64(1, from);
			addStatement.setUInt64(2, to);
			addStatement.execute();
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "addLinkIfNotExists() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// check whether an URL is already locked in database
bool Database::isUrlLockable(unsigned long urlId) {
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsUrlLockable)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::isUrlLockable(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psIsUrlLockable);

	// check connection
	this->checkConnection();

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlLockable() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// get the URL lock end time of a specific URL from database
std::string Database::getUrlLock(unsigned long urlId) {
	std::string result;

	// check prepared SQL statement
	if(!(this->psGetUrlLock)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::getUrlLock(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psGetUrlLock);

	// check connection
	this->checkConnection();

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getString("crawllock");
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "getUrlLock() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// check whether the URL has not been locked again after a specific lock time (or is not locked anymore)
bool Database::checkUrlLock(unsigned long urlId, const std::string& lockTime) {
	bool result = false;

	// check prepared SQL statement
	if(!(this->psCheckUrlLock)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::checkUrlLock(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psCheckUrlLock);

	// check connection
	this->checkConnection();

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.setString(2, lockTime);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get (and parse) result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "checkUrlLock() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}

	return result;
}

// lock a URL in the database
std::string Database::lockUrl(unsigned long urlId, unsigned long lockTimeout) {
	// check prepared SQL statement
	if(!(this->psLockUrl)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::lockUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psLockUrl);

	// check connection
	this->checkConnection();

	// lock URL in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, lockTimeout);
		sqlStatement.setUInt64(2, urlId);
		sqlStatement.execute();
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
	if(!(this->psUnLockUrl)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::unLockUrl(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psUnLockUrl);

	// check connection
	this->checkConnection();

	// unlock URL in database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "unLockUrl() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// save content to database
void Database::saveContent(unsigned long urlId, unsigned int response, const std::string& type,
		const std::string& content) {
	// check prepared SQL statement
	if(!(this->psSaveContent)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::saveContent(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psSaveContent);

	// check connection
	this->checkConnection();

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
			this->log("crawler", logStrStr.str());
			if(adjustServerSettings)
				this->log("crawler", "[#" + this->idString + "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "saveContent() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// save archived content to database
void Database::saveArchivedContent(unsigned long urlId, const std::string& timeStamp, unsigned int response,
		const std::string& type, const std::string& content) {
	// check prepared SQL statement
	if(!(this->psSaveArchivedContent))
		throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::saveArchivedContent(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psSaveArchivedContent);

	// check connection
	this->checkConnection();

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
			this->log("crawler", logStrStr.str());
			if(adjustServerSettings)
				this->log("crawler", "[#" + this->idString + "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "saveArchivedContent(" << urlId << ", " << timeStamp << ", " << response << ") SQL Error #" << e.getErrorCode()
				<< " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

// set URL as crawled in the database
void Database::setUrlFinished(unsigned long urlId) {
	// check prepared SQL statement
	if(!(this->psSetUrlFinished)) throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::setUrlFinished(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psSetUrlFinished);

	// check connection
	this->checkConnection();

	// get ID of URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "setUrlFinished() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw DatabaseException(errorStrStr.str());
	}
}

bool Database::isArchivedContentExists(unsigned long urlId, const std::string& timeStamp) {
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsArchivedContentExists))
		throw DatabaseException("Missing prepared SQL statement for DatabaseCrawler::isArchivedContentExists(...)");
	sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->psIsArchivedContentExists);

	// check connection
	this->checkConnection();

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement.setUInt64(1, urlId);
		sqlStatement.setString(2, timeStamp);
		std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) result = sqlResultSet->getBoolean("result");
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "isArchivedContentExists() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") "
				<< e.what();
		throw DatabaseException(errorStrStr.str());
	}

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
