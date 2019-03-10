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
	Database::Database(Module::Database& dbThread, const std::string& setTargetTableAlias)
			: Wrapper::Database(dbThread), website(0), urlList(0), cacheSize(2500), reparse(false),
			  parseCustom(true), logging(true), verbose(false), timeoutTargetLock(0), targetTableId(0),
			  ps({0}), targetTableAlias(setTargetTableAlias) {}

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
	//  NOTE: Waiting for other locks to be released requires a callback function to get the running status of the thread.
	void Database::initTargetTable(CallbackIsRunning isRunning) {
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
		this->reserveForPreparedStatements(sizeof(ps) / sizeof(unsigned short));

		// prepare SQL statements for parser
		if(!(this->ps.fetchUrls)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares fetchUrls()...");

			std::ostringstream sqlQueryStr;
			sqlQueryStr << "SELECT DISTINCT a.id AS url_id, a.url AS url, c.id AS lock_id FROM `"
						<< this->urlListTable << "` AS a INNER JOIN `" << this->urlListTable
						<< "_crawled` AS b ON a.id = b.url LEFT OUTER JOIN `" << this->parsingTable
						<< "` AS c ON a.id = c.url AND c.target = " << this->targetTableId
						<< " WHERE a.id > ? AND b.response < 400 AND (c.locktime IS NULL OR c.locktime < NOW())";

			if(!(this->reparse))
				sqlQueryStr << " AND (c.success IS NULL OR c.success = FALSE)";

			if(!(this->parseCustom))
				sqlQueryStr << " AND a.manual = FALSE";

			sqlQueryStr << " ORDER BY a.id";

			if(this->cacheSize)
				sqlQueryStr << " LIMIT " << this->cacheSize;

			if(this->verbose)
				this->log("[#" + this->idString + "] > " + sqlQueryStr.str());

			this->ps.fetchUrls = this->addPreparedStatement(sqlQueryStr.str());
		}

		if(!(this->ps.getUrlPosition)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getUrlPosition()...");

			this->ps.getUrlPosition = this->addPreparedStatement(
					"SELECT COUNT(id) AS result FROM `" + this->urlListTable + "` WHERE id < ?"
			);
		}

		if(!(this->ps.getNumberOfUrls)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getNumberOfUrls()...");

			this->ps.getNumberOfUrls = this->addPreparedStatement(
					"SELECT COUNT(id) result FROM `" + this->urlListTable + "`"
			);
		}

		if(!(this->ps.getLockTime)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getLockTime()...");

			this->ps.getLockTime = this->addPreparedStatement(
					"SELECT locktime FROM `" + this->parsingTable + "` WHERE id = ? LIMIT 1"
			);
		}

		if(!(this->ps.getLockId)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getLockId()...");

			std::ostringstream sqlQueryStr;
			sqlQueryStr <<	"SELECT id FROM `" << this->parsingTable << "`"
							" WHERE target = " << this->targetTableId << " AND url = ? LIMIT 1";

			this->ps.getLockId = this->addPreparedStatement(sqlQueryStr.str());
		}

		if(!(this->ps.lockUrlIfOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares lockUrlIfOk() [1/2]...");

			std::string sqlQuery("UPDATE `");
			sqlQuery += this->parsingTable;
			sqlQuery += "` SET locktime = NOW() + INTERVAL ? SECOND WHERE id = ?"
							" AND (locktime IS NULL OR locktime < NOW())";

			if(!(this->reparse))
				sqlQuery += " AND NOT success";

			sqlQuery += " LIMIT 1";

			this->ps.lockUrlIfOk = this->addPreparedStatement(sqlQuery);
		}

		if(!(this->ps.addUrlLock)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares lockUrlIfOk() [1/2]...");

			std::ostringstream sqlQueryStr;
			sqlQueryStr <<	"INSERT INTO `" << this->parsingTable << "` (target, url, locktime, success)"
							" VALUES (" << this->targetTableId << ", ?, NOW() + INTERVAL ? SECOND, FALSE)";

			this->ps.addUrlLock = this->addPreparedStatement(sqlQueryStr.str());
		}

		if(!(this->ps.unLockUrlIfOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares unLockUrlIfOk()...");

			this->ps.unLockUrlIfOk = this->addPreparedStatement(
					"UPDATE `" + this->parsingTable + "` SET locktime = NULL"
					" WHERE id = ? AND (locktime <= ? OR locktime <= NOW()) LIMIT 1"
			);
		}

		if(!(this->ps.getContentIdFromParsedId)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getContentIdFromParsedId()...");

			this->ps.getContentIdFromParsedId = this->addPreparedStatement(
					"SELECT content FROM `" + this->targetTableFull + "` WHERE parsed_id = ? LIMIT 1"
			);
		}

		if(!(this->ps.getLatestContent)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getLatestContent()...");

			this->ps.getLatestContent = this->addPreparedStatement(
					"SELECT id, content FROM `" + this->urlListTable + "_crawled`"
					" WHERE url = ? ORDER BY crawltime DESC LIMIT ?, 1"
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

	// fetch next URLs to parse from database and add them to cache (i. e. queue)
	void Database::fetchUrls(unsigned long lastId, std::queue<UrlProperties>& cache) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.fetchUrls))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::fetchUrls(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.fetchUrls);

		try {
			// execute SQL query
			sqlStatement.setUInt64(1, lastId);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get results
			if(sqlResultSet) {
				while(sqlResultSet->next()) {
					cache.emplace(
						sqlResultSet->getUInt64("url_id"),
						sqlResultSet->getString("url"),
						sqlResultSet->getUInt64("lock_id")
					);
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::fetchUrls", e); }
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

	// get the URL lock expiration time for a specific URL from database
	std::string Database::getLockTime(unsigned long lockId) {
		std::string result;

		// check argument
		if(!lockId)
			return "";

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
			sqlStatement.setUInt64(1, lockId);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("locktime");
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getLockTime", e); }

		return result;
	}

	// get the URL lock ID for a specific URL from database if none has been received yet
	//  NOTE: The first value of the tuple (the URL ID) needs already to be set!
	void Database::getLockId(UrlProperties& urlProperties) {
		// check arguments
		if(!urlProperties.id)
			throw DatabaseException("Parser::Database::getLockId(): No URL ID specified");
		if(urlProperties.lockId)
			return; // already got the lock ID

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getLockId))
			throw DatabaseException("Missing prepared SQL statement for Parser::Database::getLockId(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getLockId);

		// get lock ID from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlProperties.id);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				urlProperties.lockId = sqlResultSet->getUInt64("id");
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getLockId", e); }
	}

	// lock a URL in the database if it is lockable (and in the case of no re-parse: not parsed yet)
	//  NOTE: Returns an empty string if the URL is not lockable or, when re-parsing is deactived, already parsed.
	std::string Database::lockUrlIfOk(UrlProperties& urlProperties, unsigned long lockTimeout) {
		// check argument
		if(!urlProperties.id)
			throw DatabaseException("Parser::Database::lockUrl(): No URL ID specified");

		// check connection
		this->checkConnection();

		if(urlProperties.lockId) {
			// check prepared SQL statement for locking the URL
			if(!(this->ps.lockUrlIfOk))
				throw DatabaseException("Missing prepared SQL statement for Parser::Database::lockUrlIfOk(...)");

			// get prepared SQL statement for locking the URL
			sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.lockUrlIfOk);

			// lock URL in database if not locked (and not parsed yet when re-parsing is deactivated)
			try {
				// execute SQL query
				sqlStatement.setUInt64(1, lockTimeout);
				sqlStatement.setUInt64(2, urlProperties.lockId);
				if(Database::sqlExecuteUpdate(sqlStatement) < 1)
					return std::string(); // locking query not successful
			}
			catch(const sql::SQLException &e) { this->sqlException("Parser::Database::lockUrlIfOk", e); }
		}
		else {
			// check prepared SQL statement for adding an URL lock
			if(!(this->ps.addUrlLock))
				throw DatabaseException("Missing prepared SQL statement for Parser::Database::lockUrlIfOk(...)");

			// get prepared SQL statement for adding an URL lock
			sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.addUrlLock);

			// add URL lock to database
			try {
				// execute SQL query
				sqlStatement.setUInt64(1, urlProperties.id);
				sqlStatement.setUInt64(2, lockTimeout);
				Database::sqlExecute(sqlStatement);

				// get result
				urlProperties.lockId = this->getLastInsertedId();
			}
			catch(const sql::SQLException &e) { this->sqlException("Parser::Database::lockUrlIfOk", e); }
		}

		// return new URL lock expiration time
		return this->getLockTime(urlProperties.lockId);
	}

	// unlock a URL in the database
	void Database::unLockUrlIfOk(unsigned long lockId, const std::string& lockTime) {
		// check argument
		if(!lockId)
			return; // no URL lock to unlock

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
			sqlStatement.setUInt64(1, lockId);
			sqlStatement.setString(2, lockTime);
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::unLockUrlIfOk", e); }
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
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get (and parse) result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("content");
		}
		catch(const sql::SQLException &e) { this->sqlException("Parser::Database::getContentIdFromParsedId", e); }

		return result;
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
	//  NOTE: If tables are locked, both the parsing table (reading access as 'b') AND the target table need to be locked!
	void Database::updateOrAddEntries(std::queue<ParsingEntry>& entries, std::queue<std::string>& logEntriesTo) {
		// check argument
		if(entries.empty())
			return;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.updateOrAddEntry) || !(this->ps.updateOrAdd10Entries) || !(this->ps.updateOrAdd100Entries)
				|| !(this->ps.updateOrAdd1000Entries))
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
				for(unsigned short n = 0; n < 1000; n++) {
					// check entry
					this->checkEntry(entries.front(), logEntriesTo);

					// set default values
					sqlStatement1000.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement1000.setUInt64(n * fields + 2, entries.front().contentId);
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
				for(unsigned char n = 0; n < 100; n++) {
					// check entry
					this->checkEntry(entries.front(), logEntriesTo);

					// set default values
					sqlStatement100.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement100.setUInt64(n * fields + 2, entries.front().contentId);
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
				for(unsigned char n = 0; n < 10; n++) {
					// check entry
					this->checkEntry(entries.front(), logEntriesTo);

					// set default values
					sqlStatement10.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement10.setUInt64(n * fields + 2, entries.front().contentId);
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
				this->checkEntry(entries.front(), logEntriesTo);

				// set default values
				sqlStatement1.setUInt64(1, entries.front().contentId);
				sqlStatement1.setUInt64(2, entries.front().contentId);
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
				for(unsigned long n = 0; n < 1000; n++) {
					sqlStatement1000.setUInt64(n * 2 + 1, finished.front().first);
					sqlStatement1000.setString(n * 2 + 2, finished.front().second);
					finished.pop();
				}
				Database::sqlExecute(sqlStatement1000);
			}

			// set 100 URLs at once
			while(finished.size() > 100) {
				for(unsigned long n = 0; n < 100; n++) {
					sqlStatement100.setUInt64(n * 2 + 1, finished.front().first);
					sqlStatement100.setString(n * 2 + 2, finished.front().second);
					finished.pop();
				}
				Database::sqlExecute(sqlStatement100);
			}

			// set 10 URLs at once
			while(finished.size() > 10) {
				for(unsigned long n = 0; n < 10; n++) {
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
	bool Database::checkEntry(ParsingEntry& entry, std::queue<std::string>& logEntriesTo) {
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
				logStrStr << "[#" << this->idString << "] WARNING: An entry could not be saved to the database,"
						" because the size of a parsed value (" << tooLarge << " bytes) exceeds the ";
				if(tooLarge > 1073741824) logStrStr << "MySQL maximum of 1 GiB.";
				else {
					logStrStr << "current MySQL server maximum of " << this->getMaxAllowedPacketSize() << " bytes.";
					adjustServerSettings = true;
				}
				logEntriesTo.emplace(logStrStr.str());
				if(adjustServerSettings)
					logEntriesTo.emplace("[#" + this->idString
							+ "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
			}
			return false;
		}

		return true;
	}

	// generate a SQL query for the updating or adding a specific number of parsed entries
	std::string Database::queryUpdateOrAddEntries(unsigned int numberOfEntries) {
		// check arguments
		if(!numberOfEntries)
			throw DatabaseException("Database::queryUpdateOrAddEntries(): No number of entries specified");

		// create INSERT INTO clause
		std::ostringstream sqlQueryStrStr;
		sqlQueryStrStr << "INSERT INTO `" << this->targetTableFull << "`(id, content, parsed_id, parsed_datetime";

		unsigned long counter = 0;
		for(auto i = this->targetFieldNames.begin(); i!= this->targetFieldNames.end(); ++i) {
			if(!(i->empty())) {
				sqlQueryStrStr << ", `parsed__" + *i + "`";
				counter++;
			}
		}

		sqlQueryStrStr << ") VALUES ";

		// create placeholder list (including existence check)
		for(unsigned int n = 0; n < numberOfEntries; n++) {
			sqlQueryStrStr
				<<	"( (SELECT id FROM `" << this->targetTableFull
				<<	"` AS `" << this->targetTableAlias << (n + 1) << "`"
					" WHERE content = ? LIMIT 1), ?, ?, ?";

			for(unsigned long c = 0; c < counter; c++)
				sqlQueryStrStr << ", ?";

			sqlQueryStrStr << ")";

			if((n + 1) < numberOfEntries)
				sqlQueryStrStr << ", ";
		}

		// create ON DUPLICATE KEY UPDATE clause
		sqlQueryStrStr << " ON DUPLICATE KEY UPDATE"
						" content = VALUES(content),"
						" parsed_id = VALUES(parsed_id),"
						" parsed_datetime = VALUES(parsed_datetime)";

		for(auto i = this->targetFieldNames.begin(); i!= this->targetFieldNames.end(); ++i)
			if(!(i->empty()))
				sqlQueryStrStr << ", `parsed__" << *i << "` = VALUES(`parsed__" << *i << "`)";

		// return query
		return sqlQueryStrStr.str();
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
		for(unsigned int n = 0; n < numberOfUrls; n++) {
			if(n > 0) sqlQueryStrStr << " OR ";
			sqlQueryStrStr << "( id = ? AND ( locktime <= ? OR locktime < NOW() OR locktime IS NULL ) )";
		}

		// add LIMIT
		sqlQueryStrStr << " LIMIT " << numberOfUrls;

		// return query
		return sqlQueryStrStr.str();
	}

} /* crawlservpp::Module::Crawler */
