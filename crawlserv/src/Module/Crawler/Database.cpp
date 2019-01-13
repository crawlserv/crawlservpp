/*
 * Database.cpp
 *
 * This class provides database functionality for a crawler thread by implementing the Module::DBWrapper interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

// constructor: initialize values
crawlservpp::Module::Crawler::Database::Database(crawlservpp::Module::DBThread& dbThread) : crawlservpp::Module::DBWrapper(dbThread) {
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
crawlservpp::Module::Crawler::Database::~Database() {}

// prepare SQL statements for crawler
bool crawlservpp::Module::Crawler::Database::prepare(unsigned long crawlerId, const std::string& websiteNameSpace,
		const std::string& urlListNameSpace, bool recrawl, bool verbose) {
	// convert ID to string
	std::ostringstream idStrStr;
	idStrStr << crawlerId;
	std::string idString = idStrStr.str();

	// create table names
	this->urlListTable = "crawlserv_" + websiteNameSpace + "_" + urlListNameSpace;
	this->linkTable = this->urlListTable + "_links";
	std::string crawledTable = this->urlListTable + "_crawled";

	// check connection to database
	if(!(this->checkConnection())) {
		this->errorMessage = this->getDatabaseErrorMessage();
		return false;
	}

	try {
		// prepare SQL statements for crawler
		if(!(this->psIsUrlExists)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares isUrlExists()...");
			this->psIsUrlExists = this->addPreparedStatement("SELECT EXISTS (SELECT id FROM " + this->urlListTable
					+ " WHERE url = ?) AS result");
		}
		if(!(this->psIsUrlHashExists)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares hash check for URLs...");
			this->psIsUrlHashExists = this->addPreparedStatement("SELECT EXISTS (SELECT id FROM " + this->urlListTable
					+ " WHERE hash = CRC32( ? )) AS result");
		}
		if(!(this->psGetUrlId)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares getUrlId()...");
			this->psGetUrlId = this->addPreparedStatement("SELECT id FROM " + this->urlListTable + " WHERE url = ? LIMIT 1");
		}
		if(!(this->psIsUrlCrawled)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares isUrlCrawled()...");
			this->psIsUrlCrawled = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM " + this->urlListTable + " WHERE id = ?"
					" AND crawled = TRUE LIMIT 1) AS result");
		}
		if(!(this->psGetNextUrl)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares getNextUrl()...");
			if(recrawl) this->psGetNextUrl = this->addPreparedStatement("SELECT id, url FROM " + this->urlListTable
							+ " WHERE id > ? AND manual = FALSE AND (crawllock IS NULL OR crawllock < NOW()) ORDER BY id LIMIT 1");
			else this->psGetNextUrl = this->addPreparedStatement("SELECT id, url FROM " + this->urlListTable
				+ " WHERE id > ? AND crawled = 0 AND manual = FALSE AND (crawllock IS NULL OR crawllock < NOW()) ORDER BY id LIMIT 1");
		}
		if(!(this->psAddUrl)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares addUrl()...");
			this->psAddUrl =
					this->addPreparedStatement("INSERT INTO " + this->urlListTable + "(url, hash, manual) VALUES(?, CRC32(?), ?)");
		}
		if(!(this->psGetUrlPosition)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares getUrlPosition()...");
			this->psGetUrlPosition = this->addPreparedStatement("SELECT COUNT(id) AS result FROM " + this->urlListTable
					+ " WHERE id < ?");
		}
		if(!(this->psGetNumberOfUrls)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares getNumberOfUrls()...");
			this->psGetNumberOfUrls = this->addPreparedStatement("SELECT COUNT(id) AS result FROM " + this->urlListTable);
		}

		if(!(this->psIsUrlLockable)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares isUrlLockable()...");
			this->psIsUrlLockable = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM " + this->urlListTable
					+ " WHERE id = ? AND (crawllock IS NULL OR crawllock < NOW())) AS result");
		}
		if(!(this->psGetUrlLock)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares getUrlLock()...");
			this->psGetUrlLock = this->addPreparedStatement("SELECT crawllock FROM " + this->urlListTable + " WHERE id = ? LIMIT 1");
		}
		if(!(this->psCheckUrlLock)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares checkUrlLock()...");
			this->psCheckUrlLock = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM " + this->urlListTable
					+ " WHERE id = ? AND (crawllock < NOW() OR crawllock <= ? OR crawllock IS NULL)) AS result");
		}
		if(!(this->psLockUrl)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares lockUrl()...");
			this->psLockUrl = this->addPreparedStatement("UPDATE " + this->urlListTable + " SET crawllock = NOW() + INTERVAL ? SECOND"
					" WHERE id = ? LIMIT 1");
		}
		if(!(this->psUnLockUrl)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares unLockUrl()...");
			this->psUnLockUrl = this->addPreparedStatement("UPDATE " + this->urlListTable + " SET crawllock = NULL WHERE id = ? LIMIT 1");
		}

		if(!(this->psSaveContent)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares saveContent()...");
			this->psSaveContent = this->addPreparedStatement("INSERT INTO " + crawledTable + "(url, response, type, content)"
					" VALUES (?, ?, ?, ?)");
		}
		if(!(this->psSaveArchivedContent)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares saveArchivedContent()...");
			this->psSaveArchivedContent = this->addPreparedStatement("INSERT INTO " + crawledTable + "(url, crawltime, archived,"
					" response, type, content) VALUES (?, ?, TRUE, ?, ?, ?)");
		}
		if(!(this->psSetUrlFinished)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares setUrlFinished()...");
			this->psSetUrlFinished = this->addPreparedStatement("UPDATE " + this->urlListTable + " SET crawled = TRUE, parsed = FALSE,"
					" extracted = FALSE, analyzed = FALSE, crawllock = NULL WHERE id = ? LIMIT 1");
		}
		if(!(this->psIsArchivedContentExists)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares isArchivedContentExists()...");
			this->psIsArchivedContentExists = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM " + crawledTable
					+ " WHERE url = ? AND crawltime = ?) AS result");
		}
		if(!(this->psIsLinkExists)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares addLinkIfNotExists() [1/3]...");
			this->psIsLinkExists = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM " + linkTable + " WHERE fromurl = ?"
					" AND tourl = ?) AS result");
		}
		if(!(this->psAddLink)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares addLinkIfNotExists() [2/3]...");
			this->psAddLink = this->addPreparedStatement("INSERT INTO " + linkTable + "(fromurl, tourl, archived) VALUES(?, ?, FALSE)");
		}
		if(!(this->psAddLinkArchived)) {
			if(verbose) this->log("crawler", "[#" + idString + "] prepares addLinkIfNotExists() [3/3]...");
			this->psAddLinkArchived = this->addPreparedStatement("INSERT INTO " + linkTable + "(fromurl, tourl, archived)"
					" VALUES(?, ?, TRUE)");
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

// check whether URL exists in database (uses hash check to first check probably existence of URL)
bool crawlservpp::Module::Crawler::Database::isUrlExists(const std::string& urlString) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statements
	if(!(this->psIsUrlHashExists))
		throw std::runtime_error("Missing prepared SQL statement for URL hash checks");
	sql::PreparedStatement * hashStatement = this->getPreparedStatement(this->psIsUrlHashExists);
	if(!hashStatement)
		throw std::runtime_error("Prepared SQL statement for URL hash checks is NULL");
	if(!(this->psIsUrlExists)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::isUrlExists(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psIsUrlExists);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::isUrlExists(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check URL existence in database
	try {
		// execute SQL query for hash check
		hashStatement->setString(1, urlString);
		sqlResultSet = hashStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);

		if(result) { // hash found -> perform real comparison
			// execute SQL query for checking URL
			sqlStatement->setString(1, urlString);
			sqlResultSet = sqlStatement->executeQuery();

			// get result
			if(sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

			// delete result
			GLOBAL_DATABASE_DELETE(sqlResultSet);
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlExists() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// lock URL list
void crawlservpp::Module::Crawler::Database::lockUrlList() {
	this->lockTables(this->urlListTable, this->linkTable);
}

// get the ID of an URL (uses hash check for first checking probable existence of URL)
unsigned long crawlservpp::Module::Crawler::Database::getUrlId(const std::string& urlString) {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetUrlId)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::getUrlId(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetUrlId);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::getUrlId(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get ID of URL from database
	try {
		// execute SQL query for getting URL
		sqlStatement->setString(1, urlString);
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
		errorStrStr << "getUrlId() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") - " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// check whether an URL has been crawled
bool crawlservpp::Module::Crawler::Database::isUrlCrawled(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsUrlCrawled)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::isUrlCrawled(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psIsUrlCrawled);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::isUrlCrawled(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "isUrlCrawled() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get the next URL to crawl from database or empty IdString if all URLs have been crawled
crawlservpp::Struct::IdString crawlservpp::Module::Crawler::Database::getNextUrl(unsigned long currentUrlId) {
	sql::ResultSet * sqlResultSet = NULL;
	crawlservpp::Struct::IdString result;

	// check prepared SQL statement
	if(!(this->psGetNextUrl)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::getNextUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetNextUrl);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::getNextUrl(...) is NULL");

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

// add URL to database and return ID of newly added URL
unsigned long crawlservpp::Module::Crawler::Database::addUrl(const std::string& urlString, bool manual) {
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psAddUrl)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::addUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psAddUrl);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::addUrl(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// add URL to database and get resulting ID
	try {
		// execute SQL query
		sqlStatement->setString(1, urlString);
		sqlStatement->setString(2, urlString);
		sqlStatement->setBoolean(3, manual);
		sqlStatement->execute();

		// get result
		result = this->getLastInsertedId();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "addUrl(\'" + urlString + "\', " + (manual ? "true" : "false") + ") SQL Error #" << e.getErrorCode()
				<< " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// get the position of the URL in the URL list
unsigned long crawlservpp::Module::Crawler::Database::getUrlPosition(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetUrlPosition)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::getUrlPosition()");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetUrlPosition);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::getUrlPosition() is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get URL position of URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getUInt64("result");

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
unsigned long crawlservpp::Module::Crawler::Database::getNumberOfUrls() {
	sql::ResultSet * sqlResultSet = NULL;
	unsigned long result = 0;

	// check prepared SQL statement
	if(!(this->psGetNumberOfUrls)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::getNumberOfUrls()");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetNumberOfUrls);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::getNumberOfUrls() is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get number of URLs from database
	try {
		// execute SQL query
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getUInt64("result");

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

// check whether a link between two websites already exists and add it to the database if not
void crawlservpp::Module::Crawler::Database::addLinkIfNotExists(unsigned long from, unsigned long to, bool archived) {
	sql::PreparedStatement * addStatement = NULL;
	sql::ResultSet * sqlResultSet = NULL;

	// check prepared SQL statements
	if(!(this->psIsLinkExists)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::addLinkIfNotExists(...)");
	sql::PreparedStatement * checkStatement = this->getPreparedStatement(this->psIsLinkExists);
	if(!checkStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::addLinkIfNotExists(...) is NULL");
	if(archived) {
		if(!(this->psAddLinkArchived)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::addLinkIfNotExists(...)");
		addStatement = this->getPreparedStatement(this->psAddLinkArchived);
	}
	else {
		if(!(this->psAddLink)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::addLinkIfNotExists(...)");
		addStatement = this->getPreparedStatement(this->psAddLink);
	}
	if(!addStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::addLinkIfNotExists(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check existence of link and add it to database if it does not exist there already
	try {
		// execute SQL query for checking existence of link
		checkStatement->setUInt64(1, from);
		checkStatement->setUInt64(2, to);
		sqlResultSet = checkStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) if(!(sqlResultSet->getBoolean("result"))) {

			// link does not exist: execute SQL query for adding link
			addStatement->setUInt64(1, from);
			addStatement->setUInt64(2, to);
			addStatement->execute();
		}

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "addLinkIfNotExists() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// check whether an URL is already locked in database
bool crawlservpp::Module::Crawler::Database::isUrlLockable(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsUrlLockable)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::isUrlLockable(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psIsUrlLockable);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::isUrlLockable(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

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
std::string crawlservpp::Module::Crawler::Database::getUrlLock(unsigned long urlId) {
	sql::ResultSet * sqlResultSet = NULL;
	std::string result;

	// check prepared SQL statement
	if(!(this->psGetUrlLock)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::getUrlLock(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetUrlLock);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::getUrlLock(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get URL lock from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getString("crawllock");

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
bool crawlservpp::Module::Crawler::Database::checkUrlLock(unsigned long urlId, const std::string& lockTime) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psCheckUrlLock)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::checkUrlLock(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psCheckUrlLock);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::checkUrlLock(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// check URL lock in database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->setString(2, lockTime);
		sqlResultSet = sqlStatement->executeQuery();

		// get (and parse) result
		if(sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

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
std::string crawlservpp::Module::Crawler::Database::lockUrl(unsigned long urlId, unsigned long lockTimeout) {
	// check prepared SQL statement
	if(!(this->psLockUrl)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::lockUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psLockUrl);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::lockUrl(...) is NULL");

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
void crawlservpp::Module::Crawler::Database::unLockUrl(unsigned long urlId) {
	// check prepared SQL statement
	if(!(this->psUnLockUrl)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::unLockUrl(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psUnLockUrl);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::unLockUrl(...) is NULL");

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

// save content to database
void crawlservpp::Module::Crawler::Database::saveContent(unsigned long urlId, unsigned int response, const std::string& type,
		const std::string& content) {
	// check prepared SQL statement
	if(!(this->psSaveContent)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::saveContent(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psSaveContent);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::saveContent(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// save content to database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->setUInt(2, response);
		sqlStatement->setString(3, type);
		sqlStatement->setString(4, content);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error: save content to 'debug'
		std::ofstream out("debug");
		if(out.is_open()) {
			out << content;
			out.close();
		}

		std::ostringstream errorStrStr;
		errorStrStr << "saveContent() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// save archived content to database
void crawlservpp::Module::Crawler::Database::saveArchivedContent(unsigned long urlId, const std::string& timeStamp, unsigned int response,
		const std::string& type, const std::string& content) {
	// check prepared SQL statement
	if(!(this->psSaveArchivedContent))
		throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::saveArchivedContent(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psSaveArchivedContent);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::saveArchivedContent(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// save archived content to database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->setString(2, timeStamp);
		sqlStatement->setUInt(3, response);
		sqlStatement->setString(4, type);
		sqlStatement->setString(5, content);
		sqlStatement->execute();
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "saveArchivedContent(" << urlId << ", " << timeStamp << ", " << response << ") SQL Error #" << e.getErrorCode()
				<< " (SQLState " << e.getSQLState() << ") " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}
}

// set URL as crawled in the database
void crawlservpp::Module::Crawler::Database::setUrlFinished(unsigned long urlId) {
	// check prepared SQL statement
	if(!(this->psSetUrlFinished)) throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::setUrlFinished(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psSetUrlFinished);
	if(!sqlStatement) throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::setUrlFinished(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->getDatabaseErrorMessage());

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
		throw std::runtime_error(errorStrStr.str());
	}
}

bool crawlservpp::Module::Crawler::Database::isArchivedContentExists(unsigned long urlId, const std::string& timeStamp) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = false;

	// check prepared SQL statement
	if(!(this->psIsArchivedContentExists))
		throw std::runtime_error("Missing prepared SQL statement for DatabaseCrawler::isArchivedContentExists(...)");
	sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psIsArchivedContentExists);
	if(!sqlStatement)
		throw std::runtime_error("Prepared SQL statement for DatabaseCrawler::isArchivedContentExists(...) is NULL");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// get next URL from database
	try {
		// execute SQL query
		sqlStatement->setUInt64(1, urlId);
		sqlStatement->setString(2, timeStamp);
		sqlResultSet = sqlStatement->executeQuery();

		// get result
		if(sqlResultSet->next()) result = sqlResultSet->getBoolean("result");

		// delete result
		GLOBAL_DATABASE_DELETE(sqlResultSet);
	}
	catch(sql::SQLException &e) {
		// SQL error
		GLOBAL_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "isArchivedContentExists() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") "
				<< e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// helper function: check the current URL lock and re-lock the URL if possible, return whether the re-locking was successful
bool crawlservpp::Module::Crawler::Database::renewUrlLock(unsigned long lockTimeout, unsigned long urlId, std::string& lockTime) {
	if(this->checkUrlLock(urlId, lockTime)) {
		lockTime = this->lockUrl(urlId, lockTimeout);
		return true;
	}
	return false;
}
