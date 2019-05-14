/*
 * Database.cpp
 *
 * This class provides database functionality for a extractor thread by implementing the Wrapper::Database interface.
 *
 *  Created on: May 9, 2019
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module::Extractor {
	// constructor: initialize values
	Database::Database(Module::Database& dbThread)
							: Wrapper::Database(dbThread),
							  extractingTableAlias("a"),
							  targetTableAlias("b"),
							  cacheSize(2500),
							  reextract(false),
							  extractCustom(false),
							  rawContentIsSource(false),
							  targetTableId(0),
							  ps(_ps()) {}

	// destructor stub
	Database::~Database() {}

	// set maximum cache size for URLs
	void Database::setCacheSize(unsigned long setCacheSize) {
		this->cacheSize = setCacheSize;
	}

	// enable or disable reextracting
	void Database::setReextract(bool isReextract) {
		this->reextract = isReextract;
	}

	// enable or disable extracting from custom URLs
	void Database::setExtractCustom(bool isExtractCustom) {
		this->extractCustom = isExtractCustom;
	}

	// set whether raw crawled data is used as source
	void Database::setRawContentIsSource(bool isRawContentIsSource) {
		this->rawContentIsSource = isRawContentIsSource;
	}

	// set tables and columns of parsed data sources
	// NOTE: Uses std::queue::swap() - do not use argument afterwards!
	void Database::setSources(std::queue<StringString>& tablesAndColumns) {
		this->sources.swap(tablesAndColumns);
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
		this->urlListTable = "crawlserv_" + this->getOptions().websiteNamespace + "_" + this->getOptions().urlListNamespace;
		this->extractingTable = this->urlListTable + "_extracting";
		this->targetTableFull = this->urlListTable + "_extracted_" + this->targetTableName;

		// create table properties
		TargetTableProperties properties(
				"extracted",
				this->getOptions().websiteId,
				this->getOptions().urlListId,
				this->targetTableName,
				this->targetTableFull,
				true
		);

		properties.columns.reserve(4 + this->targetFieldNames.size());

		properties.columns.emplace_back("content", "BIGINT UNSIGNED NOT NULL UNIQUE");
		properties.columns.emplace_back("extracted_id", "TEXT NOT NULL");
		properties.columns.emplace_back("hash", "INT UNSIGNED DEFAULT 0 NOT NULL", true);
		properties.columns.emplace_back("extracted_datetime", "DATETIME DEFAULT NULL");

		for(const auto& targetFieldName : this->targetFieldNames)
			if(!targetFieldName.empty())
				properties.columns.emplace_back("extracted__" + targetFieldName, "LONGTEXT");

		// add target table if it does not exist already
		this->targetTableId = this->addTargetTable(properties);
	}

	// prepare SQL statements for extractor
	void Database::prepare() {
		// check connection to database
		this->checkConnection();

		// reserve memory
		if(this->rawContentIsSource)
			this->reserveForPreparedStatements(sizeof(ps) / sizeof(unsigned short) + this->sources.size());
		else
			this->reserveForPreparedStatements(sizeof(ps) / sizeof(unsigned short) + this->sources.size() - 1);

		// prepare SQL statements
		if(!(this->ps.fetchUrls)) {
			if(this->isVerbose())
				this->log("prepares fetchUrls()...");

			std::ostringstream sqlQueryStrStr;

			sqlQueryStrStr <<		"SELECT tmp1.id, tmp1.url FROM"
									" ("
										" SELECT `" << this->urlListTable << "`.id,"
										" `" << this->urlListTable << "`.url"
										" FROM `" << this->urlListTable << "`"
										" WHERE `" << this->urlListTable << "`.id > ?";
			if(!(this->extractCustom))
				sqlQueryStrStr <<		" AND `" << this->urlListTable << "`.manual = FALSE";

			sqlQueryStrStr <<			" AND EXISTS"
										" ("
											" SELECT *"
											" FROM `" << this->urlListTable << "_parsing`"
											" WHERE `" << this->urlListTable << "_parsing`.url"
											" = `" << this->urlListTable << "`.id"
											" AND `" <<  this->urlListTable << "_parsing`.success"
										" )"
										" ORDER BY `" << this->urlListTable << "`.id"
									" ) AS tmp1"
									" LEFT OUTER JOIN "
									" ("
										" SELECT url, MAX(locktime) AS locktime";

			if(!(this->reextract))
				sqlQueryStrStr <<		", MAX(success) AS success";

			sqlQueryStrStr <<			" FROM `" << this->extractingTable << "`"
										" WHERE target = " << this->targetTableId <<
										" AND url > ?"
										" AND"
										" ("
											"locktime >= NOW()";

			if(!(this->reextract))
				sqlQueryStrStr <<			" OR success = TRUE";

			sqlQueryStrStr <<			" )"
										" GROUP BY url"
									" ) AS tmp2"
									" ON tmp1.id = tmp2.url"
									" WHERE tmp2.locktime IS NULL";

			if(!(this->reextract))
				sqlQueryStrStr <<	" AND tmp2.success IS NULL";

			if(this->cacheSize)
				sqlQueryStrStr <<	" LIMIT " << this->cacheSize;

			if(this->isVerbose())
				this->log("> " + sqlQueryStrStr.str());

			this->ps.fetchUrls = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.lockUrl)) {
			if(this->isVerbose())
				this->log("prepares lockUrls() [1/4]...");

			this->ps.lockUrl = this->addPreparedStatement(this->queryLockUrls(1));
		}

		if(!(this->ps.lock10Urls)) {
			if(this->isVerbose())
				this->log("prepares lockUrls() [2/4]...");

			this->ps.lock10Urls = this->addPreparedStatement(this->queryLockUrls(10));
		}

		if(!(this->ps.lock100Urls)) {
			if(this->isVerbose())
				this->log("prepares lockUrls() [3/4]...");

			this->ps.lock100Urls = this->addPreparedStatement(this->queryLockUrls(100));
		}

		if(!(this->ps.lock1000Urls)) {
			if(this->isVerbose())
				this->log("prepares lockUrls() [4/4]...");

			this->ps.lock1000Urls = this->addPreparedStatement(this->queryLockUrls(1000));
		}

		if(!(this->ps.getUrlPosition)) {
			if(this->isVerbose())
				this->log("prepares getUrlPosition()...");

			this->ps.getUrlPosition = this->addPreparedStatement(
					"SELECT COUNT(id) AS result"
					" FROM `" + this->urlListTable + "` WHERE id < ?"
			);
		}

		if(!(this->ps.getNumberOfUrls)) {
			if(this->isVerbose())
				this->log("prepares getNumberOfUrls()...");

			this->ps.getNumberOfUrls = this->addPreparedStatement(
					"SELECT COUNT(id) AS result"
					" FROM `" + this->urlListTable + "`"
			);
		}

		if(!(this->ps.getLockTime)) {
			if(this->isVerbose())
				this->log("prepares getLockTime()...");

			this->ps.getLockTime = this->addPreparedStatement(
					"SELECT NOW() + INTERVAL ? SECOND AS locktime"
			);
		}

		if(!(this->ps.getUrlLockTime)) {
			if(this->isVerbose())
				this->log("prepares getUrlLockTime()...");

			std::ostringstream sqlQueryStrStr;

			sqlQueryStrStr <<	"SELECT MAX(locktime) AS locktime"
								" FROM `" << this->extractingTable << "`"
								" WHERE target = " << this->targetTableId <<
								" AND url = ?"
								" GROUP BY url"
								" LIMIT 1";

			this->ps.getUrlLockTime = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.renewUrlLockIfOk)) {
			if(this->isVerbose())
				this->log("prepares renewUrlLockIfOk()...");

			std::ostringstream sqlQueryStrStr;

			sqlQueryStrStr <<	"UPDATE `" << this->extractingTable << "`"
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
			if(this->isVerbose())
				this->log("prepares unLockUrlIfOk()...");

			std::ostringstream sqlQueryStrStr;

			sqlQueryStrStr <<	"UPDATE `" << this->extractingTable << "`"
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

		if(!(this->ps.checkExtractingTable)) {
			if(this->isVerbose())
				this->log("prepares checkExtractingTable()...");

			std::ostringstream sqlQueryStrStr;

			sqlQueryStrStr <<	"DELETE t1 FROM `" << this->extractingTable + "` t1"
								" INNER JOIN `" << this->extractingTable + "` t2"
								" WHERE t1.id < t2.id"
								" AND t1.url = t2.url"
								" AND t1.target = t2.target"
								" AND t1.target = " << this->targetTableId;

			this->ps.checkExtractingTable = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(this->rawContentIsSource && !(this->ps.getLatestContent)) {
			if(this->isVerbose())
				this->log("prepares getLatestContent()...");

			this->ps.getLatestContent = this->addPreparedStatement(
					"SELECT id, content FROM `" + this->urlListTable + "_crawled`"
					" WHERE url = ?"
					" ORDER BY crawltime DESC"
					" LIMIT ?, 1"
			);
		}

		if(!(this->ps.setUrlFinishedIfLockOk)) {
			if(this->isVerbose())
				this->log("prepares setUrlFinished() [1/4]...");

			this->ps.setUrlFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(1));
		}

		if(!(this->ps.set10UrlsFinishedIfLockOk)) {
			if(this->isVerbose())
				this->log("prepares setUrlFinished() [2/4]...");

			this->ps.set10UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(10));
		}

		if(!(this->ps.set100UrlsFinishedIfLockOk)) {
			if(this->isVerbose())
				this->log("prepares setUrlFinished() [3/4]...");

			this->ps.set100UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(100));
		}

		if(!(this->ps.set1000UrlsFinishedIfLockOk)) {
			if(this->isVerbose())
				this->log("prepares setUrlFinished() [4/4]...");

			this->ps.set1000UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(1000));
		}

		if(!(this->ps.updateOrAddEntry)) {
			if(this->isVerbose())
				this->log("prepares updateOrAddEntries() [1/4]...");

			this->ps.updateOrAddEntry = this->addPreparedStatement(this->queryUpdateOrAddEntries(1));
		}

		if(!(this->ps.updateOrAdd10Entries)) {
			if(this->isVerbose())
				this->log("prepares updateOrAddEntries() [2/4]...");

			this->ps.updateOrAdd10Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(10));
		}

		if(!(this->ps.updateOrAdd100Entries)) {
			if(this->isVerbose())
				this->log("prepares updateOrAddEntries() [3/4]...");

			this->ps.updateOrAdd100Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(100));
		}

		if(!(this->ps.updateOrAdd1000Entries)) {
			if(this->isVerbose())
				this->log("prepares updateOrAddEntries() [4/4]...");

			this->ps.updateOrAdd1000Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(1000));
		}

		if(!(this->ps.updateTargetTable)) {
			if(this->isVerbose())
				this->log("prepares updateTargetTable()...");

			std::ostringstream sqlQueryStrStr;

			sqlQueryStrStr <<	"UPDATE crawlserv_extractedtables SET updated = CURRENT_TIMESTAMP"
								" WHERE id = " << this->targetTableId << " LIMIT 1";

			this->ps.updateTargetTable = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(this->psGetParsedData.empty()) {
			if(this->isVerbose())
				this->log("prepares getParsedData()...");

			while(!(this->sources.empty())) {
				this->psGetParsedData.push_back(
						this->addPreparedStatement(
								"SELECT `" + this->sources.front().second + "` AS result"
								" FROM `" + this->urlListTable + "_parsed_" + this->sources.front().first + "`"
								" WHERE url = ?"
								" ORDER BY id DESC"
								" LIMIT 1"
						)
				);

				sources.pop();
			}
		}
	}

	// fetch and lock next URLs to extract from database, add them to the cache (i. e. queue), return the lock expiration time
	//  throws Database::Exception
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
			throw Exception("Missing prepared SQL statement for Extractor:Database::fetchUrls(...)");

		// get prepared SQL statements
		sql::PreparedStatement& sqlStatementFetch = this->getPreparedStatement(this->ps.fetchUrls);
		sql::PreparedStatement& sqlStatementLock1 = this->getPreparedStatement(this->ps.lockUrl);
		sql::PreparedStatement& sqlStatementLock10 = this->getPreparedStatement(this->ps.lock10Urls);
		sql::PreparedStatement& sqlStatementLock100 = this->getPreparedStatement(this->ps.lock100Urls);
		sql::PreparedStatement& sqlStatementLock1000 = this->getPreparedStatement(this->ps.lock1000Urls);

		// get lock expiration time
		const std::string lockTime(this->getLockTime(lockTimeout));

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
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::fetchUrls", e); }

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
	}

	// get the position of the URL in the URL list, throws Database::Exception
	unsigned long Database::getUrlPosition(unsigned long urlId) {
		unsigned long result = 0;

		// check argument
		if(!urlId)
			throw Exception("Extractor:Database::getUrlPosition(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlPosition))
			throw Exception("Missing prepared SQL statement for Extractor:Database::getUrlPosition()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getUrlPosition));

		// get URL position of URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::getUrlPosition", e); }

		return result;
	}

	// get the number of URLs in the URL list, throws Database::Exception
	unsigned long Database::getNumberOfUrls() {
		unsigned long result = 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getNumberOfUrls))
			throw Exception("Missing prepared SQL statement for Extractor:Database::getNumberOfUrls()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getNumberOfUrls));

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::getNumberOfUrls", e); }

		return result;
	}

	// let the database calculate the current URL lock expiration time, throws Database::Exception
	std::string Database::getLockTime(unsigned long lockTimeout) {
		std::string result;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getLockTime))
			throw Exception("Missing prepared SQL statement for Extractor:Database::getLockTime(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getLockTime));

		// get URL lock end time from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, lockTimeout);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("locktime");
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::getLockTime", e); }

		return result;
	}

	// get the URL lock expiration time for a specific URL from the database, throws Database::Exception
	std::string Database::getUrlLockTime(unsigned long urlId) {
		std::string result;

		// check argument
		if(!urlId)
			return "";

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlLockTime))
			throw Exception("Missing prepared SQL statement for Extractor:Database::getUrlLockTime(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getUrlLockTime));

		// get URL lock end time from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("locktime");
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::getUrlLockTime", e); }

		return result;
	}

	// lock a URL in the database if it is lockable (or is still locked) or return an empty string if locking was unsuccessful,
	//  throws Database::Exception
	std::string Database::renewUrlLockIfOk(unsigned long urlId, const std::string& lockTime, unsigned long lockTimeout) {
		// check argument
		if(!urlId)
			throw Exception("Extractor:Database::renewUrlLockIfOk(): No URL ID specified");

		// get lock time
		const std::string newLockTime(this->getLockTime(lockTimeout));

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.renewUrlLockIfOk))
			throw Exception("Missing prepared SQL statement for Module::Extractor:Database::renewUrlLockIfOk(...)");

		// get prepared SQL statement for renewing the URL lock
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.renewUrlLockIfOk));

		// lock URL in database
		try {
			// execute SQL query
			sqlStatement.setString(1, newLockTime);
			sqlStatement.setString(2, lockTime);
			sqlStatement.setUInt64(3, urlId);
			sqlStatement.setString(4, lockTime);

			if(Database::sqlExecuteUpdate(sqlStatement) < 1)
				return std::string(); // locking failed when no entries have been updated
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::renewUrlLockIfOk", e); }

		// get new expiration time of URL lock
		return newLockTime;
	}

	// unlock a URL in the database, return whether unlocking was successful, throws Database::Exception
	bool Database::unLockUrlIfOk(unsigned long urlId, const std::string& lockTime) {
		// check argument
		if(!urlId)
			return true; // no URL lock to unlock

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.unLockUrlIfOk))
			throw Exception("Missing prepared SQL statement for Extractor:Database::unLockUrlIfOk(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.unLockUrlIfOk));

		// unlock URL in database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setString(2, lockTime);

			return Database::sqlExecuteUpdate(sqlStatement) > 0;
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::unLockUrlIfOk", e); }

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
		sql::PreparedStatement& sqlStatement(
				this->getPreparedStatement(
						this->addPreparedStatement(
								this->queryUnlockUrlsIfOk(urls.size())
						)
				)
		);

		// unlock URLs in database
		try {
			// set placeholders
			unsigned long counter = 1;

			while(!urls.empty()) {
				sqlStatement.setUInt64(counter, urls.front().first);

				urls.pop();

				++counter;
			}

			sqlStatement.setString(counter, lockTime);

			// execute SQL query
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::unLockUrlsIfOk", e); }

		lockTime.clear();
	}

	// check the extracting table, delete duplicate URL locks and return the number of deleted URL locks,
	//  throws Database::Exception
	unsigned int Database::checkExtractingTable() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement for locking the URL
		if(!(this->ps.checkExtractingTable))
			throw Exception("Missing prepared SQL statement for Extractor:Database::checkExtractingTable(...)");

		// get prepared SQL statement for locking the URL
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.checkExtractingTable));

		// lock URL in database if not locked (and not extracted yet when re-extracting is deactivated)
		try {
			// execute SQL query
			int result = Database::sqlExecuteUpdate(sqlStatement);

			if(result > 0)
				return result;
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::checkExtractingTable", e); }

		return 0;
	}

	// get latest content for the ID-specified URL, return false if there is no content,
	//  throws Database::Exception
	bool Database::getLatestContent(unsigned long urlId, unsigned long index, IdString& contentTo) {
		IdString result;
		bool success = false;

		// check argument
		if(!urlId)
			throw Exception("Extractor:Database::getLatestContent(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getLatestContent))
			throw Exception("Missing prepared SQL statement for Extractor:Database::getLatestContent(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getLatestContent));

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
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::getLatestContent", e); }

		if(success) {
			contentTo = result;

			return true;
		}

		return false;
	}

	// get parsed data for the ID-specified URL from the index-specified source, throws Database::Exception
	//  NOTE:	The source index is determined by the order of adding the sources (starting with 0).
	//			Returns an empty string when no data has been found.
	std::string Database::getParsedData(unsigned long urlId, unsigned long sourceIndex) {
		std::string result;

		// check argument
		if(!urlId)
			throw Exception("No URL specified for Database::getParsedData(...)");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(sourceIndex >= this->psGetParsedData.size() || !(this->psGetParsedData.at(sourceIndex)))
			throw Exception("Missing prepared SQL statement for Database::getParsedData(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(
				this->psGetParsedData.at(sourceIndex)
		);

		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getString("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::getParsedData", e); }

		return result;
	}

	// add extracted data to database (update if row for ID-specified content already exists, throws Database::Exception
	void Database::updateOrAddEntries(std::queue<DataEntry>& entries) {
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
			throw Exception("Missing prepared SQL statement for Extractor:Database::updateOrAddEntries(...)");

		// get prepared SQL statements
		sql::PreparedStatement& sqlStatement1 = this->getPreparedStatement(this->ps.updateOrAddEntry);
		sql::PreparedStatement& sqlStatement10 = this->getPreparedStatement(this->ps.updateOrAdd10Entries);
		sql::PreparedStatement& sqlStatement100 = this->getPreparedStatement(this->ps.updateOrAdd100Entries);
		sql::PreparedStatement& sqlStatement1000 = this->getPreparedStatement(this->ps.updateOrAdd1000Entries);

		// TODO: update if exists (when exactly?)

		// count fields
		const unsigned long fields = 4 + std::count_if(
				this->targetFieldNames.begin(),
				this->targetFieldNames.end(),
				[](const auto& fieldName) {
					return !fieldName.empty();
				}
		);

		try {
			// add 1,000 entries at once
			while(entries.size() >= 1000) {
				for(unsigned short n = 0; n < 1000; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					// set default values
					sqlStatement1000.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement1000.setString(n * fields + 2, entries.front().dataId);
					sqlStatement1000.setString(n * fields + 3, entries.front().dataId);

					if(entries.front().dateTime.empty())
						sqlStatement1000.setNull(n * fields + 4, 0);
					else
						sqlStatement1000.setString(n * fields + 4, entries.front().dateTime);

					// set custom values
					unsigned int counter = 5;

					for(auto i = entries.front().fields.begin(); i != entries.front().fields.end(); ++i) {
						if(!(this->targetFieldNames.at(i - entries.front().fields.begin()).empty())) {
							sqlStatement1000.setString(n * fields + counter, *i);

							++counter;
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
					this->checkEntrySize(entries.front());

					// set default values
					sqlStatement100.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement100.setString(n * fields + 2, entries.front().dataId);
					sqlStatement100.setString(n * fields + 3, entries.front().dataId);

					if(entries.front().dateTime.empty())
						sqlStatement100.setNull(n * fields + 4, 0);
					else
						sqlStatement100.setString(n * fields + 4, entries.front().dateTime);

					// set custom values
					unsigned int counter = 5;

					for(auto i = entries.front().fields.begin(); i != entries.front().fields.end(); ++i) {
						if(!(this->targetFieldNames.at(i - entries.front().fields.begin()).empty())) {
							sqlStatement100.setString(n * fields + counter, *i);

							++counter;
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
					this->checkEntrySize(entries.front());

					// set default values
					sqlStatement10.setUInt64(n * fields + 1, entries.front().contentId);
					sqlStatement10.setString(n * fields + 2, entries.front().dataId);
					sqlStatement10.setString(n * fields + 3, entries.front().dataId);

					if(entries.front().dateTime.empty())
						sqlStatement10.setNull(n * fields + 4, 0);
					else
						sqlStatement10.setString(n * fields + 4, entries.front().dateTime);

					// set custom values
					unsigned int counter = 5;

					for(auto i = entries.front().fields.begin(); i != entries.front().fields.end(); ++i) {
						if(!(this->targetFieldNames.at(i - entries.front().fields.begin()).empty())) {
							sqlStatement10.setString(n * fields + counter, *i);

							++counter;
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
				this->checkEntrySize(entries.front());

				// set default values
				sqlStatement1.setUInt64(1, entries.front().contentId);
				sqlStatement1.setString(2, entries.front().dataId);
				sqlStatement1.setString(3, entries.front().dataId);

				if(entries.front().dateTime.empty())
					sqlStatement1.setNull(4, 0);
				else
					sqlStatement1.setString(4, entries.front().dateTime);

				// set custom values
				unsigned int counter = 5;

				for(auto i = entries.front().fields.begin(); i != entries.front().fields.end(); ++i) {
					if(!(this->targetFieldNames.at(i - entries.front().fields.begin()).empty())) {
						sqlStatement1.setString(counter, *i);

						++counter;
					}
				}

				// remove entry
				entries.pop();

				// execute SQL query
				Database::sqlExecute(sqlStatement1);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::updateOrAddEntries", e); }
	}

	// set URL as extracted in the database, throws Database::Exception
	void Database::setUrlsFinishedIfLockOk(std::queue<IdString>& finished) {
		// check argument
		if(finished.empty())
			return;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.setUrlFinishedIfLockOk) || !(this->ps.set10UrlsFinishedIfLockOk)
				|| !(this->ps.set100UrlsFinishedIfLockOk) || !(this->ps.set1000UrlsFinishedIfLockOk))
			throw Exception("Missing prepared SQL statement for Extractor:Database::setUrlsFinishedIfLockOk(...)");

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
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::setUrlsFinishedIfLockOk", e); }
	}

	// update target table, throws Database::Exception
	void Database::updateTargetTable() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.updateTargetTable))
			throw Exception("Missing prepared SQL statement for Extractor:Database::updateTargetTable(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.updateTargetTable));

		try {
			// execute SQL query
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::updateTargetTable", e); }
	}

	// check the value sizes in a extracting entry and remove values that are too large for the database
	bool Database::checkEntrySize(DataEntry& entry) {
		// check data sizes
		unsigned long tooLarge = 0;

		if(entry.dataId.size() > this->getMaxAllowedPacketSize()) {
			tooLarge = entry.dataId.size();

			entry.dataId.clear();
		}

		if(entry.dateTime.size() > this->getMaxAllowedPacketSize() && entry.dateTime.size() > tooLarge) {
			tooLarge = entry.dateTime.size();

			entry.dateTime.clear();
		}

		for(auto& field : entry.fields) {
			if(field.size() > this->getMaxAllowedPacketSize()) {
				if(field.size() > tooLarge)
					tooLarge = field.size();

				field.clear();
			}
		}

		if(tooLarge) {
			if(this->isLogging()) {
				// show warning about data size
				bool adjustServerSettings = false;

				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr	<<	"WARNING: An entry could not be saved to the database,"
								" because the size of an extracted value ("
							<<	tooLarge
							<< " bytes) exceeds the ";

				if(tooLarge > 1073741824)
					logStrStr << "MySQL maximum of 1 GiB.";
				else {
					logStrStr	<< "current MySQL server maximum of "
								<< this->getMaxAllowedPacketSize()
								<< " bytes.";

					adjustServerSettings = true;
				}

				this->log(logStrStr.str());

				if(adjustServerSettings)
					this->log(
							"Adjust the server's \'max_allowed_packet\' setting accordingly."
					);
			}

			return false;
		}

		return true;
	}

	// generate a SQL query for locking a specific number of URLs, throws Database::Exception
	std::string Database::queryLockUrls(unsigned int numberOfUrls) {
		// check arguments
		if(!numberOfUrls)
			throw Exception("Database::queryLockUrls(): No number of URLs specified");

		std::ostringstream sqlQueryStrStr;

		// create INSERT INTO clause
		sqlQueryStrStr <<		"INSERT INTO `" << this->extractingTable << "`(id, target, url, locktime)"
								" VALUES";

		// create VALUES clauses
		for(unsigned int n = 1; n <= numberOfUrls; ++n) {
			sqlQueryStrStr <<	" ("
									" ("
										"SELECT id FROM `" << this->extractingTable << "`"
										" AS `" << this->extractingTableAlias << n << "`"
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

	// generate SQL query for updating or adding a specific number of extracted entries, throws Database::Exception
	std::string Database::queryUpdateOrAddEntries(unsigned int numberOfEntries) {
		// TODO: update when exists (when exactly?)

		// check arguments
		if(!numberOfEntries)
			throw Exception("Database::queryUpdateOrAddEntries(): No number of entries specified");

		// create INSERT INTO clause
		std::string sqlQueryStr("INSERT INTO `" + this->targetTableFull + "`"
								" ("
									" content,"
									" extracted_id,"
									" hash,"
									" extracted_datetime");

		unsigned long counter = 0;

		for(const auto& targetFieldName : this->targetFieldNames) {
			if(!targetFieldName.empty()) {
				sqlQueryStr += 		", `extracted__" + targetFieldName + "`";

				++counter;
			}
		}

		sqlQueryStr += 			")"
								" VALUES ";

		// create placeholder list (including existence check)
		for(unsigned int n = 1; n <= numberOfEntries; ++n) {
			sqlQueryStr +=		"( "
										"?, ?, CRC32( ? ), ?";

			for(unsigned long c = 0; c < counter; ++c)
				sqlQueryStr +=	 		", ?";

			sqlQueryStr +=			")";

			if(n < numberOfEntries)
				sqlQueryStr +=		", ";
		}

		// create ON DUPLICATE KEY UPDATE clause
		sqlQueryStr +=				" ON DUPLICATE KEY UPDATE"
									" extracted_id = VALUES(extracted_id),"
									" hash = VALUES(hash),"
									" extracted_datetime = VALUES(extracted_datetime)";

		for(const auto& targetFieldName : this->targetFieldNames)
			if(!targetFieldName.empty())
				sqlQueryStr 	+=	", `extracted__" + targetFieldName + "`"
									" = VALUES(`extracted__" + targetFieldName + "`)";

		// return query
		return sqlQueryStr;
	}

	// generate SQL query for setting a specific number of URLs to finished if they haven't been locked since extracting,
	//  throws Database::Exception
	std::string Database::querySetUrlsFinishedIfLockOk(unsigned int numberOfUrls) {
		// check arguments
		if(!numberOfUrls)
			throw Exception("Database::querySetUrlsFinishedIfLockOk(): No number of URLs specified");

		// create UPDATE SET clause
		std::ostringstream sqlQueryStrStr;

		sqlQueryStrStr << "UPDATE `" << this->extractingTable << "` SET locktime = NULL, success = TRUE";

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

		sqlQueryStrStr <<	"UPDATE `" << this->extractingTable << "`"
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

} /* crawlservpp::Module::Extractor */
