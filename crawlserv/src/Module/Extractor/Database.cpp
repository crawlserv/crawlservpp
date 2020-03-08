/*
 *
 * ---
 *
 *  Copyright (C) 2019-2020 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
 * Database.cpp
 *
 * This class provides database functionality for an extractor thread by implementing the Wrapper::Database interface.
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
							  overwrite(true),
							  rawContentIsSource(false),
							  targetTableId(0),
							  ps(_ps()) {}

	// destructor stub
	Database::~Database() {}

	// set maximum cache size for URLs
	void Database::setCacheSize(size_t setCacheSize) {
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

	// set whether existing datasets with identical ID should be overwrittem
	void Database::setOverwrite(bool isOverwrite) {
		this->overwrite = isOverwrite;
	}

	// create target table if it does not exists or add custom field columns if they do not exist
	void Database::initTargetTable() {
		const auto& options = this->getOptions();

		// create table names
		this->urlListTable = "crawlserv_" + options.websiteNamespace + "_" + options.urlListNamespace;
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

		properties.columns.emplace_back(
				"content",
				"BIGINT UNSIGNED NOT NULL",
				this->urlListTable + "_crawled",
				"id"
		);
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
		const unsigned short verbose = this->getLoggingVerbose();

		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(
				sizeof(ps) / sizeof(unsigned short) + this->sources.size()
		);

		// prepare SQL statements
		if(!(this->ps.fetchUrls)) {
			this->log(verbose, "prepares fetchUrls()...");

			std::string sqlQueryString(
									"SELECT tmp1.id, tmp1.url FROM"
									" ("
										" SELECT `" + this->urlListTable + "`.id,"
										" `" + this->urlListTable + "`.url"
										" FROM `" + this->urlListTable + "`"
										" WHERE `" + this->urlListTable + "`.id > ?"
			);

			if(!(this->extractCustom))
				sqlQueryString +=		" AND `" + this->urlListTable + "`.manual = FALSE";

			sqlQueryString +=			" AND EXISTS"
										" ("
											" SELECT *"
											" FROM `" + this->urlListTable + "_parsing`"
											" WHERE `" + this->urlListTable + "_parsing`.url"
											" = `" + this->urlListTable + "`.id"
											" AND `" + this->urlListTable + "_parsing`.success"
										" )"
										" ORDER BY `" + this->urlListTable + "`.id"
									" ) AS tmp1"
									" LEFT OUTER JOIN "
									" ("
										" SELECT url, MAX(locktime)"
										" AS locktime";

			if(!(this->reextract))
				sqlQueryString += 		", MAX(success)"
										 " AS success";

			sqlQueryString +=			" FROM `" + this->extractingTable + "`"
										" WHERE target = " + std::to_string(this->targetTableId) +
										" AND url > ?"
										" AND"
										" ("
											"locktime >= NOW()";

			if(!(this->reextract))
				sqlQueryString +=			" OR success = TRUE";

			sqlQueryString +=			" )"
										" GROUP BY url"
									" ) AS tmp2"
									" ON tmp1.id = tmp2.url"
									" WHERE tmp2.locktime IS NULL";

			if(!(this->reextract))
				sqlQueryString +=	" AND tmp2.success IS NULL";

			if(this->cacheSize)
				sqlQueryString +=	" LIMIT " + std::to_string(this->cacheSize);

			this->log(verbose, "> " + sqlQueryString);

			this->ps.fetchUrls = this->addPreparedStatement(sqlQueryString);
		}

		if(!(this->ps.lockUrl)) {
			this->log(verbose, "prepares lockUrls() [1/4]...");

			this->ps.lockUrl = this->addPreparedStatement(this->queryLockUrls(1));
		}

		if(!(this->ps.lock10Urls)) {
			this->log(verbose, "prepares lockUrls() [2/4]...");

			this->ps.lock10Urls = this->addPreparedStatement(this->queryLockUrls(10));
		}

		if(!(this->ps.lock100Urls)) {
			this->log(verbose, "prepares lockUrls() [3/4]...");

			this->ps.lock100Urls = this->addPreparedStatement(this->queryLockUrls(100));
		}

		if(!(this->ps.lock1000Urls)) {
			this->log(verbose, "prepares lockUrls() [4/4]...");

			this->ps.lock1000Urls = this->addPreparedStatement(this->queryLockUrls(1000));
		}

		if(!(this->ps.getUrlPosition)) {
			this->log(verbose, "prepares getUrlPosition()...");

			this->ps.getUrlPosition = this->addPreparedStatement(
					"SELECT COUNT(id)"
					" AS result"
					" FROM `" + this->urlListTable + "`"
					" WHERE id < ?"
			);
		}

		if(!(this->ps.getNumberOfUrls)) {
			this->log(verbose, "prepares getNumberOfUrls()...");

			this->ps.getNumberOfUrls = this->addPreparedStatement(
					"SELECT COUNT(id)"
					" AS result"
					" FROM `" + this->urlListTable + "`"
			);
		}

		if(!(this->ps.getLockTime)) {
			this->log(verbose, "prepares getLockTime()...");

			this->ps.getLockTime = this->addPreparedStatement(
					"SELECT NOW() + INTERVAL ? SECOND"
					" AS locktime"
			);
		}

		if(!(this->ps.getUrlLockTime)) {
			this->log(verbose, "prepares getUrlLockTime()...");

			this->ps.getUrlLockTime = this->addPreparedStatement(
					"SELECT MAX(locktime)"
					" AS locktime"
					" FROM `" + this->extractingTable + "`"
					" WHERE target = " + std::to_string(this->targetTableId) +
					" AND url = ?"
					" GROUP BY url"
					" LIMIT 1"
			);
		}

		if(!(this->ps.renewUrlLockIfOk)) {
			this->log(verbose, "prepares renewUrlLockIfOk()...");

			this->ps.renewUrlLockIfOk = this->addPreparedStatement(
					"UPDATE `" + this->extractingTable + "`"
					" SET locktime = GREATEST"
					"("
						"?,"
						"? + INTERVAL 1 SECOND"
					")"
					" WHERE target = " + std::to_string(this->targetTableId) +
					" AND url = ?"
					" AND"
					" ("
						" locktime <= ?"
						" OR locktime IS NULL"
						" OR locktime < NOW()"
					" )"
			);
		}

		if(!(this->ps.unLockUrlIfOk)) {
			this->log(verbose, "prepares unLockUrlIfOk()...");

			this->ps.unLockUrlIfOk = this->addPreparedStatement(
					"UPDATE `" + this->extractingTable + "`"
					" SET locktime = NULL"
					" WHERE target = " + std::to_string(this->targetTableId) +
					" AND url = ?"
					" AND"
					" ("
						" locktime <= ?"
						" OR locktime <= NOW()"
					" )"
			);
		}

		if(!(this->ps.checkExtractingTable)) {
			this->log(verbose, "prepares checkExtractingTable()...");

			this->ps.checkExtractingTable = this->addPreparedStatement(
					"DELETE t1 FROM `" + this->extractingTable + "` t1"
					" INNER JOIN `" + this->extractingTable + "` t2"
					" WHERE t1.id < t2.id"
					" AND t1.url = t2.url"
					" AND t1.target = t2.target"
					" AND t1.target = " + std::to_string(this->targetTableId)
			);
		}

		if(!(this->ps.getContent)) {
			this->log(verbose, "prepares getContent()...");

			if(this->rawContentIsSource)
				this->ps.getContent = this->addPreparedStatement(
						"SELECT id, content FROM `" + this->urlListTable + "_crawled`"
						" WHERE url = ?"
						" ORDER BY crawltime DESC"
						" LIMIT 1"
				);
			else
				this->ps.getContent = this->addPreparedStatement(
						"SELECT id FROM `" + this->urlListTable + "_crawled`"
						" WHERE url = ?"
						" ORDER BY crawltime DESC"
						" LIMIT 1"
				);
		}

		if(!(this->ps.setUrlFinishedIfLockOk)) {
			this->log(verbose, "prepares setUrlFinished() [1/4]...");

			this->ps.setUrlFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(1));
		}

		if(!(this->ps.set10UrlsFinishedIfLockOk)) {
			this->log(verbose, "prepares setUrlFinished() [2/4]...");

			this->ps.set10UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(10));
		}

		if(!(this->ps.set100UrlsFinishedIfLockOk)) {
			this->log(verbose, "prepares setUrlFinished() [3/4]...");

			this->ps.set100UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(100));
		}

		if(!(this->ps.set1000UrlsFinishedIfLockOk)) {
			this->log(verbose, "prepares setUrlFinished() [4/4]...");

			this->ps.set1000UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(1000));
		}

		if(!(this->ps.updateOrAddEntry)) {
			this->log(verbose, "prepares updateOrAddEntries() [1/4]...");

			this->ps.updateOrAddEntry = this->addPreparedStatement(this->queryUpdateOrAddEntries(1));
		}

		if(!(this->ps.updateOrAdd10Entries)) {
			this->log(verbose, "prepares updateOrAddEntries() [2/4]...");

			this->ps.updateOrAdd10Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(10));
		}

		if(!(this->ps.updateOrAdd100Entries)) {
			this->log(verbose, "prepares updateOrAddEntries() [3/4]...");

			this->ps.updateOrAdd100Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(100));
		}

		if(!(this->ps.updateOrAdd1000Entries)) {
			this->log(verbose, "prepares updateOrAddEntries() [4/4]...");

			this->ps.updateOrAdd1000Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(1000));
		}

		if(!(this->ps.updateTargetTable)) {
			this->log(verbose, "prepares updateTargetTable()...");

			this->ps.updateTargetTable = this->addPreparedStatement(
					"UPDATE crawlserv_extractedtables SET updated = CURRENT_TIMESTAMP"
					" WHERE id = " + std::to_string(this->targetTableId) +
					" LIMIT 1"
			);
		}

		if(this->psGetLatestParsedData.empty()) {
			this->log(verbose, "prepares getLatestParsedData()...");

			while(!(this->sources.empty())) {
				this->psGetLatestParsedData.push_back(
						this->addPreparedStatement(
								"SELECT `" + this->sources.front().second + "`"
								" AS result"
								" FROM `" + this->urlListTable + "_parsed_" + this->sources.front().first + "`"
								" WHERE content = ("
								"	SELECT id"
								"    FROM `" + this->urlListTable + "_crawled`"
								"    WHERE url = ?"
								"    ORDER BY id DESC"
								"    LIMIT 1"
								" )"
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
	std::string Database::fetchUrls(size_t lastId, std::queue<IdString>& cache, size_t lockTimeout) {
		// queue for locking URLs
		std::queue<size_t> lockingQueue;

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
			for(size_t n = 0; n < 1000; ++n) {
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
			for(size_t n = 0; n < 100; ++n) {
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
			for(size_t n = 0; n < 10; ++n) {
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
	size_t Database::getUrlPosition(size_t urlId) {
		size_t result = 0;

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
	size_t Database::getNumberOfUrls() {
		size_t result = 0;

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
	std::string Database::getLockTime(size_t lockTimeout) {
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
	std::string Database::getUrlLockTime(size_t urlId) {
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
	std::string Database::renewUrlLockIfOk(size_t urlId, const std::string& lockTime, size_t lockTimeout) {
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
	bool Database::unLockUrlIfOk(size_t urlId, const std::string& lockTime) {
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
			size_t counter = 1;

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
	bool Database::getContent(size_t urlId, IdString& contentTo) {
		// check argument
		if(!urlId)
			throw Exception("Extractor:Database::getContent(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getContent))
			throw Exception("Missing prepared SQL statement for Extractor:Database::getContent(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getContent));

		// get URL latest content from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				contentTo.first = sqlResultSet->getUInt64("id");

				if(this->rawContentIsSource)
					contentTo.second = sqlResultSet->getString("content");

				return true;
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::getContent", e); }

		return false;
	}

	// get the latest parsed data for the ID-specified URL from the index-specified source, throws Database::Exception
	//  NOTE:	The source index is determined by the order of adding the sources (starting with 0).
	//			Returns an empty string when no data has been found.
	void Database::getLatestParsedData(size_t urlId, size_t sourceIndex, std::string& resultTo) {
		// check argument
		if(!urlId)
			throw Exception("No URL specified for Database::getLatestParsedData(...)");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(sourceIndex >= this->psGetLatestParsedData.size() || !(this->psGetLatestParsedData.at(sourceIndex)))
			throw Exception("Missing prepared SQL statement for Database::getLatestParsedData(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(
				this->psGetLatestParsedData.at(sourceIndex)
		);

		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				resultTo = sqlResultSet->getString("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Extractor:Database::getLatestParsedData", e); }
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

		// count fields
		size_t fields = 4 + std::count_if(
				this->targetFieldNames.begin(),
				this->targetFieldNames.end(),
				[](const auto& fieldName) {
					return !fieldName.empty();
				}
		);

		if(this->overwrite)
			fields += 3;

		try {
			// add 1,000 entries at once
			while(entries.size() >= 1000) {
				for(unsigned short n = 0; n < 1000; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					unsigned int counter = 0;

					// set standard values
					if(this->overwrite) {
						sqlStatement1000.setUInt64(n * fields + 1, entries.front().contentId);
						sqlStatement1000.setString(n * fields + 2, entries.front().dataId);
						sqlStatement1000.setString(n * fields + 3, entries.front().dataId);

						counter += 3;
					}

					sqlStatement1000.setUInt64(n * fields + counter + 1, entries.front().contentId);
					sqlStatement1000.setString(n * fields + counter + 2, entries.front().dataId);
					sqlStatement1000.setString(n * fields + counter + 3, entries.front().dataId);

					if(entries.front().dateTime.empty())
						sqlStatement1000.setNull(n * fields + counter + 4, 0);
					else
						sqlStatement1000.setString(n * fields + counter + 4, entries.front().dateTime);

					// set custom values
					counter += 5;

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

					unsigned int counter = 0;

					// set standard values
					if(this->overwrite) {
						sqlStatement100.setUInt64(n * fields + 1, entries.front().contentId);
						sqlStatement100.setString(n * fields + 2, entries.front().dataId);
						sqlStatement100.setString(n * fields + 3, entries.front().dataId);

						counter += 3;
					}

					sqlStatement100.setUInt64(n * fields + counter + 1, entries.front().contentId);
					sqlStatement100.setString(n * fields + counter + 2, entries.front().dataId);
					sqlStatement100.setString(n * fields + counter + 3, entries.front().dataId);

					if(entries.front().dateTime.empty())
						sqlStatement100.setNull(n * fields + counter + 4, 0);
					else
						sqlStatement100.setString(n * fields + counter + 4, entries.front().dateTime);

					// set custom values
					counter += 5;

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

					unsigned int counter = 0;

					// set standard values
					if(this->overwrite) {
						sqlStatement10.setUInt64(n * fields + 1, entries.front().contentId);
						sqlStatement10.setString(n * fields + 2, entries.front().dataId);
						sqlStatement10.setString(n * fields + 3, entries.front().dataId);

						counter += 3;
					}

					sqlStatement10.setUInt64(n * fields + counter + 1, entries.front().contentId);
					sqlStatement10.setString(n * fields + counter + 2, entries.front().dataId);
					sqlStatement10.setString(n * fields + counter + 3, entries.front().dataId);

					if(entries.front().dateTime.empty())
						sqlStatement10.setNull(n * fields + counter + 4, 0);
					else
						sqlStatement10.setString(n * fields + counter + 4, entries.front().dateTime);

					// set custom values
					counter += 5;

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

				unsigned int counter = 0;

				// set standard values
				if(this->overwrite) {
					sqlStatement1.setUInt64(1, entries.front().contentId);
					sqlStatement1.setString(2, entries.front().dataId);
					sqlStatement1.setString(3, entries.front().dataId);

					counter += 3;
				}

				sqlStatement1.setUInt64(counter + 1, entries.front().contentId);
				sqlStatement1.setString(counter + 2, entries.front().dataId);
				sqlStatement1.setString(counter + 3, entries.front().dataId);

				if(entries.front().dateTime.empty())
					sqlStatement1.setNull(counter + 4, 0);
				else
					sqlStatement1.setString(counter + 4, entries.front().dateTime);

				// set custom values
				counter += 5;

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
				for(size_t n = 0; n < 1000; ++n) {
					sqlStatement1000.setUInt64(n * 2 + 1, finished.front().first);
					sqlStatement1000.setString(n * 2 + 2, finished.front().second);

					finished.pop();
				}

				Database::sqlExecute(sqlStatement1000);
			}

			// set 100 URLs at once
			while(finished.size() > 100) {
				for(size_t n = 0; n < 100; ++n) {
					sqlStatement100.setUInt64(n * 2 + 1, finished.front().first);
					sqlStatement100.setString(n * 2 + 2, finished.front().second);

					finished.pop();
				}

				Database::sqlExecute(sqlStatement100);
			}

			// set 10 URLs at once
			while(finished.size() > 10) {
				for(size_t n = 0; n < 10; ++n) {
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
		size_t tooLarge = 0;

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

			this->log(this->getLoggingMin(), logStrStr.str());

			if(adjustServerSettings)
				this->log(
						this->getLoggingMin(),
						"Adjust the server's \'max_allowed_packet\' setting accordingly."
				);

			return false;
		}

		return true;
	}

	// generate a SQL query for locking a specific number of URLs, throws Database::Exception
	std::string Database::queryLockUrls(unsigned int numberOfUrls) {
		// check arguments
		if(!numberOfUrls)
			throw Exception("Database::queryLockUrls(): No number of URLs specified");

		// create INSERT INTO clause
		std::string sqlQueryString(
				"INSERT INTO `" + this->extractingTable + "`(id, target, url, locktime)"
				" VALUES"
		);

		// create VALUES clauses
		for(unsigned int n = 1; n <= numberOfUrls; ++n) {
			sqlQueryString +=	" ("
									" ("
										"SELECT id FROM `" + this->extractingTable + "`"
										" AS `" + this->extractingTableAlias + std::to_string(n) + "`"
										" WHERE target = " + std::to_string(this->targetTableId) +
										" AND url = ?"
										" ORDER BY id DESC"
										" LIMIT 1"
									" ),"
									" " + std::to_string(this->targetTableId) + ","
									" ?,"
									" ?"
								" )";

			if(n < numberOfUrls)
				sqlQueryString += ", ";
		}

		// create ON DUPLICATE KEY UPDATE clause
		sqlQueryString +=		" ON DUPLICATE KEY"
								" UPDATE locktime = VALUES(locktime)";

		return sqlQueryString;
	}

	// generate SQL query for updating or adding a specific number of extracted entries, throws Database::Exception
	std::string Database::queryUpdateOrAddEntries(unsigned int numberOfEntries) {
		// check arguments
		if(!numberOfEntries)
			throw Exception("Database::queryUpdateOrAddEntries(): No number of entries specified");

		// create INSERT INTO clause
		std::string sqlQueryStr("INSERT INTO `" + this->targetTableFull + "`"
								" (");
		if(this->overwrite)
			sqlQueryStr +=			" id,";

		sqlQueryStr += 				" content,"
									" extracted_id,"
									" hash,"
									" extracted_datetime";

		size_t counter = 0;

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
			sqlQueryStr +=		"( ";

			if(this->overwrite)
				sqlQueryStr +=		"("
										"SELECT id"
										" FROM "
										" ("
											" SELECT id, extracted_id"
											" FROM `" + this->targetTableFull + "`"
											" AS `" + this->targetTableAlias + "`"
											" WHERE content = ?"
											" AND hash = CRC32( ? )"
										" ) AS tmp"
										" WHERE extracted_id LIKE ?"
										" LIMIT 1"
									"),";


			sqlQueryStr +=			"?, ?, CRC32( ? ), ?";

			for(size_t c = 0; c < counter; ++c)
				sqlQueryStr +=	 		", ?";

			sqlQueryStr +=		")";

			if(n < numberOfEntries)
				sqlQueryStr +=		", ";
		}

		// create ON DUPLICATE KEY UPDATE clause
		if(this->overwrite) {
			sqlQueryStr +=		" ON DUPLICATE KEY UPDATE"
								" hash = VALUES(hash),"
								" extracted_datetime = VALUES(extracted_datetime)";

			for(const auto& targetFieldName : this->targetFieldNames)
				if(!targetFieldName.empty())
					sqlQueryStr +=	", `extracted__" + targetFieldName + "`"
									" = VALUES(`extracted__" + targetFieldName + "`)";
		}

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
		std::string sqlQueryString(
				"UPDATE `" + this->extractingTable + "`"
				" SET locktime = NULL, success = TRUE"
				" WHERE "
		);

		// create WHERE clause
		for(unsigned int n = 0; n < numberOfUrls; ++n) {
			if(n > 0)
				sqlQueryString += " OR ";

			sqlQueryString +=	" ( "
									" target = " + std::to_string(this->targetTableId) +
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
		return sqlQueryString;
	}

	// generate SQL query for unlocking multiple URls if they haven't been locked since fetching
	std::string Database::queryUnlockUrlsIfOk(unsigned int numberOfUrls) {
		std::string sqlQueryString(
							"UPDATE `" + this->extractingTable + "`"
							" SET locktime = NULL"
							" WHERE target = " + std::to_string(this->targetTableId) +
							" AND"
							" ("
		);

		for(size_t n = 1; n <= numberOfUrls; ++n) {
			sqlQueryString += " url = ?";

			if(n < numberOfUrls)
				sqlQueryString += " OR";
		}

		sqlQueryString +=	" )"
							" AND"
							" ("
								" locktime <= ?"
								" OR locktime <= NOW()"
							" )";

		return sqlQueryString;
	}

} /* crawlservpp::Module::Extractor */
