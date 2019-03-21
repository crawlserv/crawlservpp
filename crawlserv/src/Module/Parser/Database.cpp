/*
 * Database.cpp
 *
 * This class provides database functionality for a parser thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module::Parser {
	// constructor: initialize values
	Database::Database(Module::Database& dbThread)
							: Wrapper::Database(dbThread),
							  parsingTableAlias("a"),
							  targetTableAlias("b"),
							  website(0),
							  urlList(0),
							  cacheSize(2500),
							  reparse(false),
							  parseCustom(true),
							  logging(true),
							  verbose(false),
							  timeoutTargetLock(0),
							  targetTableId(0),
							  ps({0}) {}

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

	// set URL list ID (and convert it to string for SQL requests)
	void Database::setUrlList(unsigned long listId) {
		std::ostringstream idStrStr;
		idStrStr << listId;
		this->urlList = listId;
		this->listIdString = idStrStr.str();
	}

	// set namespaces for website and URL list
	void Database::setNamespaces(const std::string& website, const std::string& urlList) {
		this->websiteName = website;
		this->urlListName = urlList;
	}

	// set maximum cache size for URLs
	void Database::setCacheSize(unsigned long setCacheSize) {
		this->cacheSize = setCacheSize;
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
		this->parsingTable = this->urlListTable + "_parsing";
		this->targetTableFull = this->urlListTable + "_parsed_" + this->targetTableName;

		// create table properties
		TargetTableProperties properties(
				"parsed",
				this->website,
				this->urlList,
				this->targetTableName,
				this->targetTableFull,
				true
		);
		properties.columns.reserve(4 + this->targetFieldNames.size());

		properties.columns.emplace_back("content", "BIGINT UNSIGNED NOT NULL UNIQUE");
		properties.columns.emplace_back("parsed_id", "TEXT NOT NULL");
		properties.columns.emplace_back("hash", "INT UNSIGNED DEFAULT 0 NOT NULL", true);
		properties.columns.emplace_back("parsed_datetime", "DATETIME DEFAULT NULL");

		for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i)
			if(!(i->empty()))
				properties.columns.emplace_back("parsed__" + *i, "LONGTEXT");

		// add target table if it does not exist already
		this->targetTableId = this->addTargetTable(properties);
	}

	// prepare SQL statements for parser
	void Database::prepare() {
		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(ps) / sizeof(unsigned short));

		// prepare SQL statements for parser
		if(!(this->ps.fetchUrls)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares fetchUrls()...");

			std::ostringstream sqlQueryStrStr;

			sqlQueryStrStr <<		"SELECT tmp1.id, tmp1.url FROM"
									" ("
										" SELECT `" << this->urlListTable << "`.id,"
										" `" << this->urlListTable << "`.url"
										" FROM `" << this->urlListTable << "`"
										" WHERE `" << this->urlListTable << "`.id > ?";
			if(!(this->parseCustom))
				sqlQueryStrStr <<		" AND `" << this->urlListTable << "`.manual = FALSE";

			sqlQueryStrStr <<			" AND EXISTS"
										" ("
											" SELECT *"
											" FROM `" << this->urlListTable << "_crawled`"
											" WHERE `" << this->urlListTable << "_crawled`.url"
											" = `" << this->urlListTable << "`.id"
											" AND `" <<  this->urlListTable << "_crawled`.response < 400"
										" )"
										" ORDER BY `" << this->urlListTable << "`.id"
									" ) AS tmp1"
									" LEFT OUTER JOIN "
									" ("
										" SELECT url, MAX(locktime) AS locktime";

			if(!(this->reparse))
				sqlQueryStrStr <<		", MAX(success) AS success";

			sqlQueryStrStr <<			" FROM `" << this->parsingTable << "`"
										" WHERE target = " << this->targetTableId <<
										" AND url > ?"
										" AND"
										" ("
											"locktime >= NOW()";

			if(!(this->reparse))
				sqlQueryStrStr <<			" OR success = TRUE";

			sqlQueryStrStr <<			" )"
										" GROUP BY url"
									" ) AS tmp2"
									" ON tmp1.id = tmp2.url"
									" WHERE tmp2.locktime IS NULL";

			if(!(this->reparse))
				sqlQueryStrStr <<	" AND tmp2.success IS NULL";

			if(this->cacheSize)
				sqlQueryStrStr <<	" LIMIT " << this->cacheSize;

			if(this->verbose)
				this->log("[#" + this->idString + "] > " + sqlQueryStrStr.str());

			this->ps.fetchUrls = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.lockUrl)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares lockUrls() [1/4]...");

			this->ps.lockUrl = this->addPreparedStatement(this->queryLockUrls(1));
		}

		if(!(this->ps.lock10Urls)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares lockUrls() [2/4]...");

			this->ps.lock10Urls = this->addPreparedStatement(this->queryLockUrls(10));
		}

		if(!(this->ps.lock100Urls)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares lockUrls() [3/4]...");

			this->ps.lock100Urls = this->addPreparedStatement(this->queryLockUrls(100));
		}

		if(!(this->ps.lock1000Urls)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares lockUrls() [4/4]...");

			this->ps.lock1000Urls = this->addPreparedStatement(this->queryLockUrls(1000));
		}

		if(!(this->ps.getUrlPosition)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getUrlPosition()...");

			this->ps.getUrlPosition = this->addPreparedStatement(
					"SELECT COUNT(id) AS result"
					" FROM `" + this->urlListTable + "` WHERE id < ?"
			);
		}

		if(!(this->ps.getNumberOfUrls)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getNumberOfUrls()...");

			this->ps.getNumberOfUrls = this->addPreparedStatement(
					"SELECT COUNT(id) AS result"
					" FROM `" + this->urlListTable + "`"
			);
		}

		if(!(this->ps.getLockTime)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getLockTime()...");

			this->ps.getLockTime = this->addPreparedStatement(
					"SELECT NOW() + INTERVAL ? SECOND AS locktime"
			);
		}

		if(!(this->ps.getUrlLockTime)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getUrlLockTime()...");

			std::ostringstream sqlQueryStrStr;
			sqlQueryStrStr <<	"SELECT MAX(locktime) AS locktime"
								" FROM `" << this->parsingTable << "`"
								" WHERE target = " << this->targetTableId <<
								" AND url = ?"
								" GROUP BY url"
								" LIMIT 1";

			this->ps.getUrlLockTime = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.renewUrlLockIfOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares renewUrlLockIfOk()...");

			std::ostringstream sqlQueryStrStr;

			sqlQueryStrStr <<	"UPDATE `" << this->parsingTable << "`"
								" SET locktime = GREATEST"
								"("
									"?,"
									"? + INTERVAL 1 SECOND"
								")"
								" WHERE target = " << this->targetTableId <<
								" AND url = ?"
								" AND"
								" ("
									" locktime <= ?"
									" OR locktime IS NULL"
									" OR locktime < NOW()"
								" )";

			this->ps.renewUrlLockIfOk = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.unLockUrlIfOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares unLockUrlIfOk()...");

			std::ostringstream sqlQueryStrStr;
			sqlQueryStrStr <<	"UPDATE `" << this->parsingTable << "`"
								" SET locktime = NULL"
								" WHERE target = " << this->targetTableId <<
								" AND url = ?"
								" AND"
								" ("
									" locktime <= ?"
									" OR locktime <= NOW()"
								" )";

			this->ps.unLockUrlIfOk = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.checkParsingTable)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares checkParsingTable()...");

			std::ostringstream sqlQueryStrStr;
			sqlQueryStrStr <<	"DELETE t1 FROM `" << this->parsingTable + "` t1"
								" INNER JOIN `" << this->parsingTable + "` t2"
								" WHERE t1.id < t2.id"
								" AND t1.url = t2.url"
								" AND t1.target = t2.target"
								" AND t1.target = " << this->targetTableId;

			this->ps.checkParsingTable = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.getContentIdFromParsedId)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getContentIdFromParsedId()...");

			this->ps.getContentIdFromParsedId = this->addPreparedStatement(
					"SELECT content FROM"
					" ("
						" SELECT id, parsed_id, content FROM "
						" `" + this->targetTableFull + "`"
						" WHERE hash = CRC32( ? )"
					" ) AS tmp"
					" WHERE parsed_id LIKE ?"
					" ORDER BY id DESC"
					" LIMIT 1"
			);
		}

		if(!(this->ps.getLatestContent)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getLatestContent()...");

			this->ps.getLatestContent = this->addPreparedStatement(
					"SELECT id, content FROM `" + this->urlListTable + "_crawled`"
					" WHERE url = ?"
					" ORDER BY crawltime DESC"
					" LIMIT ?, 1"
			);
		}

		if(!(this->ps.getAllContents)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getAllContents()...");

			this->ps.getAllContents = this->addPreparedStatement(
					"SELECT id, content FROM `" + this->urlListTable + "_crawled`"
					" WHERE url = ?"
			);
		}

		if(!(this->ps.setUrlFinishedIfLockOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares setUrlFinished() [1/4]...");

			this->ps.setUrlFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(1));
		}

		if(!(this->ps.set10UrlsFinishedIfLockOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares setUrlFinished() [2/4]...");

			this->ps.set10UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(10));
		}

		if(!(this->ps.set100UrlsFinishedIfLockOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares setUrlFinished() [3/4]...");

			this->ps.set100UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(100));
		}

		if(!(this->ps.set1000UrlsFinishedIfLockOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares setUrlFinished() [4/4]...");

			this->ps.set1000UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(1000));
		}

		if(!(this->ps.updateOrAddEntry)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares updateOrAddEntries() [1/4]...");

			this->ps.updateOrAddEntry = this->addPreparedStatement(this->queryUpdateOrAddEntries(1));
		}

		if(!(this->ps.updateOrAdd10Entries)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares updateOrAddEntries() [2/4]...");

			this->ps.updateOrAdd10Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(10));
		}

		if(!(this->ps.updateOrAdd100Entries)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares updateOrAddEntries() [3/4]...");

			this->ps.updateOrAdd100Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(100));
		}

		if(!(this->ps.updateOrAdd1000Entries)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares updateOrAddEntries() [4/4]...");

			this->ps.updateOrAdd1000Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(1000));
		}

		if(!(this->ps.updateTargetTable)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares updateTargetTable()...");

			std::ostringstream sqlQueryStrStr;
			sqlQueryStrStr <<	"UPDATE crawlserv_parsedtables SET updated = CURRENT_TIMESTAMP"
								" WHERE id = " << this->targetTableId << " LIMIT 1";

			this->ps.updateTargetTable = this->addPreparedStatement(sqlQueryStrStr.str());
		}
	}

	// fetch and lock next URLs to parse from database, add them to the cache (i. e. queue), return the lock expiration time
	std::string Database::fetchUrls(unsigned long lastId, std::queue<IdString>& cache, unsigned long lockTimeout) {
		// queue for locking URLs
		std::queue<unsigned long> lockingQueue;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.fetchUrls)
				|| !(this->ps.lockUrl)
				|| !(this->ps.lock10Urls)
				|| !(this->ps.lock100Urls)
				|| !(this->ps.lock1000Urls))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::fetchUrls(...)");

		// get prepared SQL statements
		sql::PreparedStatement& sqlStatementFetch = this->getPreparedStatement(this->ps.fetchUrls);
		sql::PreparedStatement& sqlStatementLock1 = this->getPreparedStatement(this->ps.lockUrl);
		sql::PreparedStatement& sqlStatementLock10 = this->getPreparedStatement(this->ps.lock10Urls);
		sql::PreparedStatement& sqlStatementLock100 = this->getPreparedStatement(this->ps.lock100Urls);
		sql::PreparedStatement& sqlStatementLock1000 = this->getPreparedStatement(this->ps.lock1000Urls);

		// get lock expiration time
		std::string lockTime = this->getLockTime(lockTimeout);

		try {
			// execute SQL query for fetching URLs
			sqlStatementFetch.setUInt64(1, lastId);
			sqlStatementFetch.setUInt64(2, lastId);

			SqlResultSetPtr sqlResultSetFetch(Database::sqlExecuteQuery(sqlStatementFetch));

			// get results from fetching URLs
			if(sqlResultSetFetch) {
				while(sqlResultSetFetch->next()) {
					cache.emplace(
						sqlResultSetFetch->getUInt64("id"),
						sqlResultSetFetch->getString("url")
					);

					lockingQueue.push(cache.back().first);
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::fetchUrls", e); }

		// set 1,000 locks at once
		while(lockingQueue.size() >= 1000) {
			for(unsigned long n = 0; n < 1000; ++n) {
				sqlStatementLock1000.setUInt64(n * 3 + 1, lockingQueue.front());
				sqlStatementLock1000.setUInt64(n * 3 + 2, lockingQueue.front());
				sqlStatementLock1000.setString(n * 3 + 3, lockTime);
				lockingQueue.pop();
			}

			// execute SQL query
			Database::sqlExecute(sqlStatementLock1000);
		}

		// set 100 locks at once
		while(lockingQueue.size() >= 100) {
			for(unsigned long n = 0; n < 100; ++n) {
				sqlStatementLock100.setUInt64(n * 3 + 1, lockingQueue.front());
				sqlStatementLock100.setUInt64(n * 3 + 2, lockingQueue.front());
				sqlStatementLock100.setString(n * 3 + 3, lockTime);
				lockingQueue.pop();
			}

			// execute SQL query
			Database::sqlExecute(sqlStatementLock100);
		}

		// set 10 locks at once
		while(lockingQueue.size() >= 10) {
			for(unsigned long n = 0; n < 10; ++n) {
				sqlStatementLock10.setUInt64(n * 3 + 1, lockingQueue.front());
				sqlStatementLock10.setUInt64(n * 3 + 2, lockingQueue.front());
				sqlStatementLock10.setString(n * 3 + 3, lockTime);
				lockingQueue.pop();
			}

			// execute SQL query
			Database::sqlExecute(sqlStatementLock10);
		}

		// set remaining locks
		while(!lockingQueue.empty()) {
			sqlStatementLock1.setUInt64(1, lockingQueue.front());
			sqlStatementLock1.setUInt64(2, lockingQueue.front());
			sqlStatementLock1.setString(3, lockTime);
			lockingQueue.pop();

			// execute SQL query
			Database::sqlExecute(sqlStatementLock1);
		}

		// return the expiration time of all locks
		return lockTime;

		return std::string();
	}

	// get the position of the URL in the URL list
	unsigned long Database::getUrlPosition(unsigned long urlId) {
		unsigned long result = 0;

		// check argument
		if(!urlId)
			throw DatabaseException("Parser::Database::getUrlPosition(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlPosition))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::getUrlPosition()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlPosition);

		// get URL position of URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("result");
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
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::getNumberOfUrls()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getNumberOfUrls);

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getNumberOfUrls", e); }

		return result;
	}

	// let the database calculate the current URL lock expiration time
	std::string Database::getLockTime(unsigned long lockTimeout) {
		std::string result;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getLockTime))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::getLockTime(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getLockTime);

		// get URL lock end time from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, lockTimeout);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("locktime");
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getLockTime", e); }

		return result;
	}

	// get the URL lock expiration time for a specific URL from the database
	std::string Database::getUrlLockTime(unsigned long urlId) {
		std::string result;

		// check argument
		if(!urlId)
			return "";

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlLockTime))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::getUrlLockTime(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlLockTime);

		// get URL lock end time from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("locktime");
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getUrlLockTime", e); }

		return result;
	}

	// lock a URL in the database if it is lockable (or is still locked) or return an empty string if locking was unsuccessful
	std::string Database::renewUrlLockIfOk(unsigned long urlId, const std::string& lockTime, unsigned long lockTimeout) {
		std::string dbg;

		// check argument
		if(!urlId)
			throw DatabaseException("Parser::Database::renewUrlLockIfOk(): No URL ID specified");

		// get lock time
		std::string newLockTime = this->getLockTime(lockTimeout);

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.renewUrlLockIfOk))
			throw DatabaseException("Missing prepared SQL statement for Module::Parser::Database::renewUrlLockIfOk(...)");

		// get prepared SQL statement for renewing the URL lock
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.renewUrlLockIfOk);

		// lock URL in database
		dbg = "renewUrlLockIfOk";
		try {
			// execute SQL query
			sqlStatement.setString(1, newLockTime);
			sqlStatement.setString(2, lockTime);
			sqlStatement.setUInt64(3, urlId);
			sqlStatement.setString(4, lockTime);

			if(Database::sqlExecuteUpdate(sqlStatement) < 1)
				return std::string(); // locking failed when no entries have been updated
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::lockUrlIfOk > " + dbg, e); }

		// get new expiration time of URL lock
		return newLockTime;
	}

	// unlock a URL in the database, return whether unlocking was successful
	bool Database::unLockUrlIfOk(unsigned long urlId, const std::string& lockTime) {
		// check argument
		if(!urlId)
			return true; // no URL lock to unlock

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.unLockUrlIfOk))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::unLockUrlIfOk(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.unLockUrlIfOk);

		// unlock URL in database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setString(2, lockTime);
			return Database::sqlExecuteUpdate(sqlStatement) > 0;
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::unLockUrlIfOk", e); }

		return false;
	}

	// unlock multiple URLs in the database, reset lock time
	//  NOTE: creates the prepared statement shortly before query execution as it is only used once (on shutdown)
	void Database::unLockUrlsIfOk(std::queue<IdString>& urls, std::string& lockTime) {
		// check argument
		if(urls.empty())
			return; // no URLs to unlock

		// check connection
		this->checkConnection();

		// create and get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(
				this->addPreparedStatement(
						this->queryUnlockUrlsIfOk(urls.size())
				)
		);

		// unlock URLs in database
		try {
			// set placeholders
			unsigned long counter = 1;

			while(!urls.empty()) {
				sqlStatement.setUInt64(counter, urls.front().first);

				urls.pop();

				counter++;
			}

			sqlStatement.setString(counter, lockTime);

			// execute SQL query
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::unLockUrlsIfOk", e); }

		lockTime.clear();
	}

	// get content ID from parsed ID
	unsigned long Database::getContentIdFromParsedId(const std::string& parsedId) {
		unsigned long result = 0;

		// check argument
		if(parsedId.empty())
			throw DatabaseException("Parser::Database::getContentIdFromParsedId(): No parsed ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getContentIdFromParsedId))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::getContentIdFromParsedId(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getContentIdFromParsedId);

		// get content ID from database
		try {
			// execute SQL query
			sqlStatement.setString(1, parsedId);
			sqlStatement.setString(2, parsedId);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get (and parse) result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("content");
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getContentIdFromParsedId", e); }

		return result;
	}

	// check the parsing table, delete duplicate URL locks and return the number of deleted URL locks
	unsigned int Database::checkParsingTable() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement for locking the URL
		if(!(this->ps.checkParsingTable))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::checkParsingTable(...)");

		// get prepared SQL statement for locking the URL
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.checkParsingTable);

		// lock URL in database if not locked (and not parsed yet when re-parsing is deactivated)
		try {
			// execute SQL query
			int result = Database::sqlExecuteUpdate(sqlStatement);

			if(result > 0)
				return result;
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::checkParsingTable", e); }

		return 0;
	}

	// get latest content for the ID-specified URL, return false if there is no content
	bool Database::getLatestContent(unsigned long urlId, unsigned long index, IdString& contentTo) {
		IdString result;
		bool success = false;

		// check argument
		if(!urlId)
			throw DatabaseException("Parser::Database::getLatestContent(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getLatestContent))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::getLatestContent(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getLatestContent);

		// get URL latest content from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setUInt64(2, index);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = IdString(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content"));
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

		// check argument
		if(!urlId)
			throw DatabaseException("Parser::Database::getAllContents(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getAllContents))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::getAllContents(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getAllContents);

		// get all contents from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

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
	void Database::updateOrAddEntries(std::queue<ParsingEntry>& entries, std::queue<std::string>& logEntriesTo) {
		// check argument
		if(entries.empty())
			return;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(		!(this->ps.updateOrAddEntry)
				|| !(this->ps.updateOrAdd10Entries)
				|| !(this->ps.updateOrAdd100Entries)
				|| !(this->ps.updateOrAdd1000Entries)
		)
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::updateOrAddEntries(...)");

		// get prepared SQL statements
		sql::PreparedStatement& sqlStatement1 = this->getPreparedStatement(this->ps.updateOrAddEntry);
		sql::PreparedStatement& sqlStatement10 = this->getPreparedStatement(this->ps.updateOrAdd10Entries);
		sql::PreparedStatement& sqlStatement100 = this->getPreparedStatement(this->ps.updateOrAdd100Entries);
		sql::PreparedStatement& sqlStatement1000 = this->getPreparedStatement(this->ps.updateOrAdd1000Entries);

		// count fields
		unsigned long fields = 4;

		for(auto i = this->targetFieldNames.begin(); i!= this->targetFieldNames.end(); ++i)
			if(!(i->empty())) fields++;

		try {

			// add 1,000 entries at once
			while(entries.size() >= 1000) {
				for(unsigned short n = 0; n < 1000; ++n) {
					// check entry
					this->checkEntrySize(entries.front(), logEntriesTo);

					// set default values
					sqlStatement1000.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement1000.setString(n * fields + 2, entries.front().parsedId);
					sqlStatement1000.setString(n * fields + 3, entries.front().parsedId);

					if(entries.front().dateTime.empty())
						sqlStatement1000.setNull(n * fields + 4, 0);
					else
						sqlStatement1000.setString(n * fields + 4, entries.front().dateTime);

					// set custom values
					unsigned int counter = 5;

					for(auto i = entries.front().fields.begin(); i != entries.front().fields.end(); ++i) {
						if(!(this->targetFieldNames.at(i - entries.front().fields.begin()).empty())) {
							sqlStatement1000.setString(n * fields + counter, *i);

							counter++;
						}
					}

					// remove entry
					entries.pop();
				}

				// execute SQL query
				Database::sqlExecute(sqlStatement1000);
			}

			// add 100 entries at once
			while(entries.size() >= 100) {
				for(unsigned char n = 0; n < 100; ++n) {
					// check entry
					this->checkEntrySize(entries.front(), logEntriesTo);

					// set default values
					sqlStatement100.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement100.setString(n * fields + 2, entries.front().parsedId);
					sqlStatement100.setString(n * fields + 3, entries.front().parsedId);

					if(entries.front().dateTime.empty())
						sqlStatement100.setNull(n * fields + 4, 0);
					else
						sqlStatement100.setString(n * fields + 4, entries.front().dateTime);

					// set custom values
					unsigned int counter = 5;

					for(auto i = entries.front().fields.begin(); i != entries.front().fields.end(); ++i) {
						if(!(this->targetFieldNames.at(i - entries.front().fields.begin()).empty())) {
							sqlStatement100.setString(n * fields + counter, *i);

							counter++;
						}
					}

					// remove entry
					entries.pop();
				}

				// execute SQL query
				Database::sqlExecute(sqlStatement100);
			}

			// add 10 entries at once
			while(entries.size() >= 10) {
				for(unsigned char n = 0; n < 10; ++n) {
					// check entry
					this->checkEntrySize(entries.front(), logEntriesTo);

					// set default values
					sqlStatement10.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement10.setString(n * fields + 2, entries.front().parsedId);
					sqlStatement10.setString(n * fields + 3, entries.front().parsedId);

					if(entries.front().dateTime.empty())
						sqlStatement10.setNull(n * fields + 4, 0);
					else
						sqlStatement10.setString(n * fields + 4, entries.front().dateTime);

					// set custom values
					unsigned int counter = 5;

					for(auto i = entries.front().fields.begin(); i != entries.front().fields.end(); ++i) {
						if(!(this->targetFieldNames.at(i - entries.front().fields.begin()).empty())) {
							sqlStatement10.setString(n * fields + counter, *i);

							counter++;
						}
					}

					// remove entry
					entries.pop();
				}

				// execute SQL query
				Database::sqlExecute(sqlStatement10);
			}

			// add remaining entries
			while(!entries.empty()) {
				// check entry
				this->checkEntrySize(entries.front(), logEntriesTo);

				// set default values
				sqlStatement1.setUInt64(1, entries.front().contentId);
				sqlStatement1.setString(2, entries.front().parsedId);
				sqlStatement1.setString(3, entries.front().parsedId);

				if(entries.front().dateTime.empty())
					sqlStatement1.setNull(4, 0);
				else
					sqlStatement1.setString(4, entries.front().dateTime);

				// set custom values
				unsigned int counter = 5;

				for(auto i = entries.front().fields.begin(); i != entries.front().fields.end(); ++i) {
					if(!(this->targetFieldNames.at(i - entries.front().fields.begin()).empty())) {
						sqlStatement1.setString(counter, *i);

						counter++;
					}
				}

				// remove entry
				entries.pop();

				// execute SQL query
				Database::sqlExecute(sqlStatement1);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::updateOrAddEntries", e); }
	}

	// set URL as parsed in the database
	void Database::setUrlsFinishedIfLockOk(std::queue<IdString>& finished) {
		// check argument
		if(finished.empty())
			return;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.setUrlFinishedIfLockOk) || !(this->ps.set10UrlsFinishedIfLockOk)
				|| !(this->ps.set100UrlsFinishedIfLockOk) || !(this->ps.set1000UrlsFinishedIfLockOk))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::setUrlsFinishedIfLockOk(...)");

		// get prepared SQL statements
		sql::PreparedStatement& sqlStatement1 = this->getPreparedStatement(this->ps.setUrlFinishedIfLockOk);
		sql::PreparedStatement& sqlStatement10 = this->getPreparedStatement(this->ps.set10UrlsFinishedIfLockOk);
		sql::PreparedStatement& sqlStatement100 = this->getPreparedStatement(this->ps.set100UrlsFinishedIfLockOk);
		sql::PreparedStatement& sqlStatement1000 = this->getPreparedStatement(this->ps.set1000UrlsFinishedIfLockOk);

		// set URLs to finished in database
		try {

			// set 1,000 URLs at once
			while(finished.size() > 1000) {
				for(unsigned long n = 0; n < 1000; ++n) {
					sqlStatement1000.setUInt64(n * 2 + 1, finished.front().first);
					sqlStatement1000.setString(n * 2 + 2, finished.front().second);
					finished.pop();
				}
				Database::sqlExecute(sqlStatement1000);
			}

			// set 100 URLs at once
			while(finished.size() > 100) {
				for(unsigned long n = 0; n < 100; ++n) {
					sqlStatement100.setUInt64(n * 2 + 1, finished.front().first);
					sqlStatement100.setString(n * 2 + 2, finished.front().second);
					finished.pop();
				}
				Database::sqlExecute(sqlStatement100);
			}

			// set 10 URLs at once
			while(finished.size() > 10) {
				for(unsigned long n = 0; n < 10; ++n) {
					sqlStatement10.setUInt64(n * 2 + 1, finished.front().first);
					sqlStatement10.setString(n * 2 + 2, finished.front().second);
					finished.pop();
				}
				Database::sqlExecute(sqlStatement10);
			}

			// set remaining URLs
			while(!finished.empty()) {
				sqlStatement1.setUInt64(1, finished.front().first);
				sqlStatement1.setString(2, finished.front().second);
				finished.pop();

				Database::sqlExecute(sqlStatement1);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::setUrlsFinishedIfLockOk", e); }
	}

	// update target table
	void Database::updateTargetTable() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.updateTargetTable))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::updateTargetTable(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.updateTargetTable);

		try {
			// execute SQL query
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::updateTargetTable", e); }
	}

	// check the value sizes in a parsing entry and remove values that are too large for the database
	bool Database::checkEntrySize(ParsingEntry& entry, std::queue<std::string>& logEntriesTo) {
		// check data sizes
		unsigned long tooLarge = 0;

		if(entry.parsedId.size() > this->getMaxAllowedPacketSize()) {
			tooLarge = entry.parsedId.size();

			entry.parsedId.clear();
		}

		if(entry.dateTime.size() > this->getMaxAllowedPacketSize() && entry.dateTime.size() > tooLarge) {
			tooLarge = entry.dateTime.size();

			entry.dateTime.clear();
		}

		for(auto i = entry.fields.begin(); i != entry.fields.end(); ++i) {
			if(i->size() > this->getMaxAllowedPacketSize() && i->size() > tooLarge) {
				tooLarge = i->size();

				i->clear();
			}
		}

		if(tooLarge) {
			if(this->logging) {
				// show warning about data size
				bool adjustServerSettings = false;
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr <<	"[#" << this->idString << "]"
								" WARNING: An entry could not be saved to the database,"
								" because the size of a parsed value (" << tooLarge << " bytes) exceeds the ";

				if(tooLarge > 1073741824)
					logStrStr << "MySQL maximum of 1 GiB.";
				else {
					logStrStr << "current MySQL server maximum of " << this->getMaxAllowedPacketSize() << " bytes.";

					adjustServerSettings = true;
				}

				logEntriesTo.emplace(logStrStr.str());

				if(adjustServerSettings)
					logEntriesTo.emplace(
							"[#" + this->idString + "]"
							" Adjust the server's \'max_allowed_packet\' setting accordingly."
					);
			}

			return false;
		}

		return true;
	}

	// generate a SQL query for locking a specific number of URLs
	std::string Database::queryLockUrls(unsigned int numberOfUrls) {
		// check arguments
		if(!numberOfUrls)
			throw DatabaseException("Database::queryLockUrls(): No number of URLs specified");

		std::ostringstream sqlQueryStrStr;

		// create INSERT INTO clause
		sqlQueryStrStr <<		"INSERT INTO `" << this->parsingTable << "`(id, target, url, locktime)"
								" VALUES";

		// create VALUES clauses
		for(unsigned int n = 1; n <= numberOfUrls; ++n) {
			sqlQueryStrStr <<	" ("
									" ("
										"SELECT id FROM `" << this->parsingTable << "`"
										" AS `" << this->parsingTableAlias << n << "`"
										" WHERE target = " << this->targetTableId <<
										" AND url = ?"
										" ORDER BY id DESC"
										" LIMIT 1"
									" ),"
									" " << this->targetTableId << ","
									" ?,"
									" ?"
								" )";

			if(n < numberOfUrls)
				sqlQueryStrStr << ", ";
		}

		// crate ON DUPLICATE KEY UPDATE clause
		sqlQueryStrStr <<		" ON DUPLICATE KEY"
								" UPDATE locktime = VALUES(locktime)";

		return sqlQueryStrStr.str();
	}

	// generate SQL query for updating or adding a specific number of parsed entries
	std::string Database::queryUpdateOrAddEntries(unsigned int numberOfEntries) {
		// check arguments
		if(!numberOfEntries)
			throw DatabaseException("Database::queryUpdateOrAddEntries(): No number of entries specified");

		// create INSERT INTO clause
		std::string sqlQueryStr("INSERT INTO `" + this->targetTableFull + "`"
								" ("
									" content,"
									" parsed_id,"
									" hash,"
									" parsed_datetime");

		unsigned long counter = 0;

		for(auto i = this->targetFieldNames.begin(); i!= this->targetFieldNames.end(); ++i) {
			if(!(i->empty())) {
				sqlQueryStr += 		", `parsed__" + *i + "`";
				counter++;
			}
		}

		sqlQueryStr += 			")"
								" VALUES ";

		// create placeholder list (including existence check)
		for(unsigned int n = 1; n <= numberOfEntries; ++n) {
			sqlQueryStr +=		"( "
										"?, ?, CRC32( ? ), ?";

			for(unsigned long c = 0; c < counter; c++)
				sqlQueryStr +=	 		", ?";

			sqlQueryStr +=			")";

			if(n < numberOfEntries)
				sqlQueryStr +=		", ";
		}

		// create ON DUPLICATE KEY UPDATE clause
		sqlQueryStr +=				" ON DUPLICATE KEY UPDATE"
									" parsed_id = VALUES(parsed_id),"
									" hash = VALUES(hash),"
									" parsed_datetime = VALUES(parsed_datetime)";

		for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i)
			if(!(i->empty()))
				sqlQueryStr 	+=	", `parsed__" + *i + "`"
									" = VALUES(`parsed__" + *i + "`)";

		// return query
		return sqlQueryStr;
	}

	// generate SQL query for setting a specific number of URLs to finished if they haven't been locked since parsing
	std::string Database::querySetUrlsFinishedIfLockOk(unsigned int numberOfUrls) {
		// check arguments
		if(!numberOfUrls)
			throw DatabaseException("Database::querySetUrlsFinishedIfLockOk(): No number of URLs specified");

		// create UPDATE SET clause
		std::ostringstream sqlQueryStrStr;

		sqlQueryStrStr << "UPDATE `" << this->parsingTable << "` SET locktime = NULL, success = TRUE";

		// create WHERE clause
		sqlQueryStrStr << " WHERE ";

		for(unsigned int n = 0; n < numberOfUrls; ++n) {
			if(n > 0)
				sqlQueryStrStr << " OR ";

			sqlQueryStrStr <<	" ( "
									" target = " << this->targetTableId <<
									" AND url = ?"
									" AND"
									" ("
										" locktime <= ?"
										" OR locktime < NOW()"
										" OR locktime IS NULL"
									" )"
								" )";
		}

		// return query
		return sqlQueryStrStr.str();
	}

	// generate SQL query for unlocking multiple URls if they haven't been locked since fetching
	std::string Database::queryUnlockUrlsIfOk(unsigned int numberOfUrls) {
		std::ostringstream sqlQueryStrStr;

		sqlQueryStrStr <<	"UPDATE `" << this->parsingTable << "`"
							" SET locktime = NULL"
							" WHERE target = " << this->targetTableId <<
							" AND"
							" (";

		for(unsigned long n = 1; n <= numberOfUrls; ++n) {
			sqlQueryStrStr <<	" url = ?";

			if(n < numberOfUrls)
				sqlQueryStrStr << " OR";
		}

		sqlQueryStrStr <<	" )"
							" AND"
							" ("
								" locktime <= ?"
								" OR locktime <= NOW()"
							" )";

		return sqlQueryStrStr.str();
	}

} /* crawlservpp::Module::Crawler */
