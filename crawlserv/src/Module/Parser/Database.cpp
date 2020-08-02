/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * This class provides database functionality for a parser thread
 *  by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module::Parser {

	/*
	 * CONSTRUCTION
	 */

	//! Constructor setting the database connection for the thread.
	/*!
	 * \param dbThread Reference to the database
	 * 	 connection used by the parser thread.
	 */
	Database::Database(Module::Database& dbThread)
							: Wrapper::Database(dbThread) {}

	/*
	 * SETTERS
	 */

	//! Sets the maximum cache size for URLs.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the parser.
	 *
	 * \param setCacheSize The maximum number
	 *   of URLs that can be cached.
	 *
	 * \sa prepare
	 */
	void Database::setCacheSize(std::uint64_t setCacheSize) {
		this->cacheSize = setCacheSize;
	}

	//! Sets whether to re-parse data from already processed URLs.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the parser.
	 *
	 * \param isReParse Set to true, and data
	 *   from already processed URLs will be
	 *   re-parsed.
	 *
	 * \sa prepare
	 */
	void Database::setReparse(bool isReParse) {
		this->reParse = isReParse;
	}

	//! Sets whether to parse data from custom URLs.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the parser.
	 *
	 * \param isParseCustom Set to true, and
	 *   data will be parsed from custom URLs
	 *   as well.
	 *
	 * \sa prepare
	 */
	void Database::setParseCustom(bool isParseCustom) {
		this->parseCustom = isParseCustom;
	}

	//! Sets the name of the target table.
	/*!
	 * \note Needs to be set before
	 *   initializing the target table.
	 *
	 * \param table Constant reference to a
	 *   string containing the name of the
	 *   table to which the parsed data
	 *   will be written.
	 *
	 * \sa setTargetFields, initTargetTable
	 */
	void Database::setTargetTable(const std::string& table) {
		this->targetTableName = table;
	}

	//! Sets the columns of the target table.
	/*!
	 * \note Needs to be set before
	 *   initializing the target table.
	 *
	 * \param fields Constant reference to a
	 *   vector containing the names of the
	 *   columns to which the parsed data
	 *   will be written.
	 *
	 * \sa setTargetTable, initTargetTable
	 */
	void Database::setTargetFields(const std::vector<std::string>& fields) {
		this->targetFieldNames = fields;
	}

	/*
	 * TARGET TABLE INITIALIZATION
	 */

	//! Creates the target table, if it does not exist, or adds target columns needed by the parser.
	/*!
	 * If the target table does not exist,
	 *  it will be created. If the target
	 *  table exists, those target columns,
	 *  that it does not contain already,
	 *  will be added to the existing table.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the new
	 *   target table, or updating the existing
	 *   target table in the database.
	 *
	 *  \sa setTargetTable, setTargetFields,
	 *    Main::Database::addTargetTable
	 */
	void Database::initTargetTable() {
		// get namespaces
		const auto& options{this->getOptions()};

		// create table names
		this->urlListTable = "crawlserv_";

		this->urlListTable += options.websiteNamespace;
		this->urlListTable += "_";
		this->urlListTable += options.urlListNamespace;

		this->parsingTable = this->urlListTable + "_parsing";

		this->targetTableFull = this->urlListTable;

		this->targetTableFull += "_parsed_";
		this->targetTableFull += this->targetTableName;

		// create table properties
		constexpr auto minTargetColumns{4};

		TargetTableProperties properties(
				"parsed",
				this->getOptions().websiteId,
				this->getOptions().urlListId,
				this->targetTableName,
				this->targetTableFull,
				true
		);

		properties.columns.reserve(minTargetColumns + this->targetFieldNames.size());

		properties.columns.emplace_back(
				"content",
				"BIGINT UNSIGNED NOT NULL UNIQUE",
				this->urlListTable + "_crawled",
				"id"
		);
		properties.columns.emplace_back("parsed_id", "TEXT NOT NULL");
		properties.columns.emplace_back("hash", "INT UNSIGNED DEFAULT 0 NOT NULL", true);
		properties.columns.emplace_back("parsed_datetime", "DATETIME DEFAULT NULL");

		for(const auto& targetFieldName : this->targetFieldNames) {
			if(!targetFieldName.empty()) {
				properties.columns.emplace_back("parsed__" + targetFieldName, "LONGTEXT");
			}
		}

		// add target table if it does not exist already
		this->targetTableId = this->addTargetTable(properties);
	}

	/*
	 * PREPARED SQL STATEMENTS
	 */

	//! Prepares the SQL statements needed by the parser.
	/*!
	 * \note The target table needs to be prepared
	 *   first.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while preparing and
	 *   adding the SQL statements.
	 *
	 * \sa Main::Database::addPreparedStatement
	 */
	void Database::prepare() {
		const auto verbose{this->getLoggingVerbose()};

		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(ps) / sizeof(std::uint16_t));

		// prepare SQL statements
		if(this->ps.fetchUrls == 0) {
			this->log(verbose, "prepares fetchUrls()...");

			std::string sqlQueryString{
									"SELECT tmp1.id, tmp1.url"
									" FROM"
									" ("
										" SELECT `"
			};

			sqlQueryString +=				this->urlListTable;
			sqlQueryString +=				"`.id,"
										" `";
			sqlQueryString +=				this->urlListTable;
			sqlQueryString +=				"`.url"
										" FROM `";
			sqlQueryString +=				this->urlListTable;
			sqlQueryString +=				"`"
										" WHERE `";
			sqlQueryString +=				this->urlListTable;
			sqlQueryString +=				"`.id > ?";

			if(!(this->parseCustom)) {
				sqlQueryString +=		" AND `";
				sqlQueryString +=			this->urlListTable;
				sqlQueryString +=			"`.manual = FALSE";
			}

			sqlQueryString +=			" AND EXISTS"
										" ("
											" SELECT *"
											" FROM `";
			sqlQueryString +=					this->urlListTable;
			sqlQueryString +=					"_crawled`"
											" WHERE `";
			sqlQueryString +=					this->urlListTable;
			sqlQueryString +=					"_crawled`.url"
											" = `";
			sqlQueryString +=					this->urlListTable;
			sqlQueryString +=					"`.id"
											" AND `";
			sqlQueryString +=					this->urlListTable;
			sqlQueryString +=					"_crawled`.response < 400"
										" )"
										" ORDER BY `";
			sqlQueryString +=				this->urlListTable;
			sqlQueryString +=				"`.id"
									" ) AS tmp1"
									" LEFT OUTER JOIN "
									" ("
										" SELECT url, MAX(locktime)"
										" AS locktime";

			if(!(this->reParse)) {
				sqlQueryString +=		", MAX(success)"
										" AS success";
			}

			sqlQueryString +=			" FROM `";
			sqlQueryString +=				this->parsingTable;
			sqlQueryString +=				"`"
										" WHERE target = ";
			sqlQueryString +=				std::to_string(this->targetTableId);
			sqlQueryString +=			" AND url > ?"
										" AND"
										" ("
											"locktime >= NOW()";

			if(!(this->reParse)) {
				sqlQueryString +=			" OR success = TRUE";
			}

			sqlQueryString +=			" )"
										" GROUP BY url"
									" ) AS tmp2"
									" ON tmp1.id = tmp2.url"
									" WHERE tmp2.locktime IS NULL";

			if(!(this->reParse)) {
				sqlQueryString +=	" AND tmp2.success IS NULL";
			}

			if(this->cacheSize > 0) {
				sqlQueryString +=	" LIMIT ";
				sqlQueryString +=		std::to_string(this->cacheSize);
			}

			this->log(verbose, "> " + sqlQueryString);

			this->ps.fetchUrls = this->addPreparedStatement(sqlQueryString);
		}

		if(this->ps.lockUrl == 0) {
			this->log(verbose, "prepares lockUrls() [1/4]...");

			this->ps.lockUrl = this->addPreparedStatement(this->queryLockUrls(1));
		}

		if(this->ps.lock10Urls == 0) {
			this->log(verbose, "prepares lockUrls() [2/4]...");

			this->ps.lock10Urls = this->addPreparedStatement(this->queryLockUrls(nAtOnce10));
		}

		if(this->ps.lock100Urls == 0) {
			this->log(verbose, "prepares lockUrls() [3/4]...");

			this->ps.lock100Urls = this->addPreparedStatement(this->queryLockUrls(nAtOnce100));
		}

		if(this->ps.lock1000Urls == 0) {
			this->log(verbose, "prepares lockUrls() [4/4]...");

			this->ps.lock1000Urls = this->addPreparedStatement(this->queryLockUrls(nAtOnce1000));
		}

		if(this->ps.getUrlPosition == 0) {
			this->log(verbose, "prepares getUrlPosition()...");

			this->ps.getUrlPosition = this->addPreparedStatement(
					"SELECT COUNT(id)"
					" AS result"
					" FROM `" + this->urlListTable + "`"
					" WHERE id < ?"
			);
		}

		if(this->ps.getNumberOfUrls == 0) {
			this->log(verbose, "prepares getNumberOfUrls()...");

			this->ps.getNumberOfUrls = this->addPreparedStatement(
					"SELECT COUNT(id)"
					" AS result"
					" FROM `" + this->urlListTable + "`"
			);
		}

		if(this->ps.getLockTime == 0) {
			this->log(verbose, "prepares getLockTime()...");

			this->ps.getLockTime = this->addPreparedStatement(
					"SELECT NOW() + INTERVAL ? SECOND"
					" AS locktime"
			);
		}

		if(this->ps.getUrlLockTime == 0) {
			this->log(verbose, "prepares getUrlLockTime()...");

			this->ps.getUrlLockTime = this->addPreparedStatement(
					"SELECT MAX(locktime)"
					" AS locktime"
					" FROM `" + this->parsingTable + "`"
					" WHERE target = " + std::to_string(this->targetTableId) +
					" AND url = ?"
					" GROUP BY url"
					" LIMIT 1"
			);
		}

		if(this->ps.renewUrlLockIfOk == 0) {
			this->log(verbose, "prepares renewUrlLockIfOk()...");

			this->ps.renewUrlLockIfOk = this->addPreparedStatement(
					"UPDATE `" + this->parsingTable + "`"
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

		if(this->ps.unLockUrlIfOk == 0) {
			this->log(verbose, "prepares unLockUrlIfOk()...");

			this->ps.unLockUrlIfOk = this->addPreparedStatement(
					"UPDATE `" + this->parsingTable + "`"
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

		if(this->ps.checkParsingTable == 0) {
			this->log(verbose, "prepares checkParsingTable()...");

			this->ps.checkParsingTable = this->addPreparedStatement(
					"DELETE t1"
					" FROM `" + this->parsingTable + "` t1"
					" INNER JOIN `" + this->parsingTable + "` t2"
					" WHERE t1.id < t2.id"
					" AND t1.url = t2.url"
					" AND t1.target = t2.target"
					" AND t1.target = " + std::to_string(this->targetTableId)
			);
		}

		if(this->ps.getContentIdFromParsedId == 0) {
			this->log(verbose, "prepares getContentIdFromParsedId()...");

			this->ps.getContentIdFromParsedId = this->addPreparedStatement(
					"SELECT content"
					" FROM"
					" ("
						" SELECT id, parsed_id, content"
						" FROM `" + this->targetTableFull + "`"
						" WHERE hash = CRC32( ? )"
					" ) AS tmp"
					" WHERE parsed_id LIKE ?"
					" ORDER BY id DESC"
					" LIMIT 1"
			);
		}

		if(this->ps.getNumberOfContents == 0) {
			this->log(verbose, "prepares getNumberOfContents()...");

			this->ps.getNumberOfContents = this->addPreparedStatement(
					"SELECT COUNT(*)"
					" AS result"
					" FROM `" + this->urlListTable + "_crawled`"
					" WHERE url = ?"
			);
		}

		if(this->ps.getLatestContent == 0) {
			this->log(verbose, "prepares getLatestContent()...");

			this->ps.getLatestContent = this->addPreparedStatement(
					"SELECT id, content"
					" FROM `" + this->urlListTable + "_crawled`"
					" FORCE INDEX(url)"
					" WHERE url = ?"
					" ORDER BY crawltime DESC"
					" LIMIT ?, 1"
			);
		}

		if(this->ps.getAllContents == 0) {
			this->log(verbose, "prepares getAllContents()...");

			this->ps.getAllContents = this->addPreparedStatement(
					"SELECT id, content"
					" FROM `" + this->urlListTable + "_crawled`"
					" WHERE url = ?"
			);
		}

		if(this->ps.setUrlFinishedIfLockOk == 0) {
			this->log(verbose, "prepares setUrlFinished() [1/4]...");

			this->ps.setUrlFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(1));
		}

		if(this->ps.set10UrlsFinishedIfLockOk == 0) {
			this->log(verbose, "prepares setUrlFinished() [2/4]...");

			this->ps.set10UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(nAtOnce10));
		}

		if(this->ps.set100UrlsFinishedIfLockOk == 0) {
			this->log(verbose, "prepares setUrlFinished() [3/4]...");

			this->ps.set100UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(nAtOnce100));
		}

		if(this->ps.set1000UrlsFinishedIfLockOk == 0) {
			this->log(verbose, "prepares setUrlFinished() [4/4]...");

			this->ps.set1000UrlsFinishedIfLockOk = this->addPreparedStatement(this->querySetUrlsFinishedIfLockOk(nAtOnce1000));
		}

		if(this->ps.updateOrAddEntry == 0) {
			this->log(verbose, "prepares updateOrAddEntries() [1/4]...");

			this->ps.updateOrAddEntry = this->addPreparedStatement(this->queryUpdateOrAddEntries(1));
		}

		if(this->ps.updateOrAdd10Entries == 0) {
			this->log(verbose, "prepares updateOrAddEntries() [2/4]...");

			this->ps.updateOrAdd10Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(nAtOnce10));
		}

		if(this->ps.updateOrAdd100Entries == 0) {
			this->log(verbose, "prepares updateOrAddEntries() [3/4]...");

			this->ps.updateOrAdd100Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(nAtOnce100));
		}

		if(this->ps.updateOrAdd1000Entries == 0) {
			this->log(verbose, "prepares updateOrAddEntries() [4/4]...");

			this->ps.updateOrAdd1000Entries = this->addPreparedStatement(this->queryUpdateOrAddEntries(nAtOnce1000));
		}

		if(this->ps.updateTargetTable == 0) {
			this->log(verbose, "prepares updateTargetTable()...");

			this->ps.updateTargetTable = this->addPreparedStatement(
					"UPDATE crawlserv_parsedtables"
					" SET updated = CURRENT_TIMESTAMP"
					" WHERE id = " + std::to_string(this->targetTableId) +
					" LIMIT 1"
			);
		}
	}

	/*
	 * URLS
	 */

	//! Fetches, locks, and adds the next URLs to the cache, i.e. to the caching queue to be processed.
	/*!
	 * \param lastId The last ID that has been
	 *   processed, or zero if non has been
	 *   processed yet.
	 * \param cache Reference to the caching queue,
	 *   i.e. the queue storing the IDs and URIs of
	 *   the URLs still in the cache.
	 * \param lockTimeout The maximum locking time
	 *   for the URLs that are being processed, in
	 *   seconds.
	 *
	 * \returns The expiration time of the new lock
	 *   for the URLs in the cache, as string in the
	 *   format @c YYYY-MM-DD HH:MM:SS.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if one of the prepared SQL statements for
	 *   fetching and locking URLs is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while fetching and locking
	 *   the URLs.
	 */
	std::string Database::fetchUrls(std::uint64_t lastId, std::queue<IdString>& cache, std::uint32_t lockTimeout) {
		// queue for locking URLs
		std::queue<std::uint64_t> lockingQueue;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(
				this->ps.fetchUrls == 0
				|| this->ps.lockUrl == 0
				|| this->ps.lock10Urls == 0
				|| this->ps.lock100Urls == 0
				|| this->ps.lock1000Urls == 0
		) {
			throw Exception(
					"Parser::Database::fetchUrls():"
					" Missing prepared SQL statement(s)"
			);
		}

		// get prepared SQL statements
		auto& sqlStatementFetch{this->getPreparedStatement(this->ps.fetchUrls)};
		auto& sqlStatementLock1{this->getPreparedStatement(this->ps.lockUrl)};
		auto& sqlStatementLock10{this->getPreparedStatement(this->ps.lock10Urls)};
		auto& sqlStatementLock100{this->getPreparedStatement(this->ps.lock100Urls)};
		auto& sqlStatementLock1000{this->getPreparedStatement(this->ps.lock1000Urls)};

		// get lock expiration time
		auto lockTime{this->getLockTime(lockTimeout)};

		try {
			// execute SQL query for fetching URLs
			sqlStatementFetch.setUInt64(sqlArg1, lastId);
			sqlStatementFetch.setUInt64(sqlArg2, lastId);

			SqlResultSetPtr sqlResultSetFetch{Database::sqlExecuteQuery(sqlStatementFetch)};

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
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::fetchUrls", e);
		}

		// set 1,000 locks at once
		constexpr auto numArgsLock{3};

		while(lockingQueue.size() >= nAtOnce1000) {
			for(std::uint16_t n{0}; n < nAtOnce1000; ++n) {
				sqlStatementLock1000.setUInt64(n * numArgsLock + sqlArg1, lockingQueue.front());
				sqlStatementLock1000.setUInt64(n * numArgsLock + sqlArg2, lockingQueue.front());
				sqlStatementLock1000.setString(n * numArgsLock + sqlArg3, lockTime);

				lockingQueue.pop();
			}

			// execute SQL query
			Database::sqlExecute(sqlStatementLock1000);
		}

		// set 100 locks at once
		while(lockingQueue.size() >= nAtOnce100) {
			for(std::uint8_t n{0}; n < nAtOnce100; ++n) {
				sqlStatementLock100.setUInt64(n * numArgsLock + sqlArg1, lockingQueue.front());
				sqlStatementLock100.setUInt64(n * numArgsLock + sqlArg2, lockingQueue.front());
				sqlStatementLock100.setString(n * numArgsLock + sqlArg3, lockTime);

				lockingQueue.pop();
			}

			// execute SQL query
			Database::sqlExecute(sqlStatementLock100);
		}

		// set 10 locks at once
		while(lockingQueue.size() >= nAtOnce10) {
			for(std::uint8_t n{0}; n < nAtOnce10; ++n) {
				sqlStatementLock10.setUInt64(n * numArgsLock + sqlArg1, lockingQueue.front());
				sqlStatementLock10.setUInt64(n * numArgsLock + sqlArg2, lockingQueue.front());
				sqlStatementLock10.setString(n * numArgsLock + sqlArg3, lockTime);

				lockingQueue.pop();
			}

			// execute SQL query
			Database::sqlExecute(sqlStatementLock10);
		}

		// set remaining locks
		while(!lockingQueue.empty()) {
			sqlStatementLock1.setUInt64(sqlArg1, lockingQueue.front());
			sqlStatementLock1.setUInt64(sqlArg2, lockingQueue.front());
			sqlStatementLock1.setString(sqlArg3, lockTime);

			lockingQueue.pop();

			// execute SQL query
			Database::sqlExecute(sqlStatementLock1);
		}

		// return the expiration time of all locks
		return lockTime;
	}

	//! Gets the position of a URL in the URL list.
	/*!
	 * \param urlId The ID of the URL whose position
	 *   will be retrieved from the database.
	 *
	 * \returns The position of the URL in the URL
	 *   list, starting with zero for the beginning
	 *   of the list.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for retrieving the position
	 *   of a URL in the URL list is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the position
	 *   of the URL in the URL list.
	 */
	std::uint64_t Database::getUrlPosition(std::uint64_t urlId) {
		std::uint64_t result{0};

		// check argument
		if(urlId == 0) {
			throw Exception(
					"Parser::Database::getUrlPosition():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getUrlPosition == 0) {
			throw Exception(
					"Parser::Database::getUrlPosition():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getUrlPosition)};

		// get URL position of URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::getUrlPosition", e);
		}

		return result;
	}

	//! Gets the number of URLs in the URL list.
	/*!
	 * \returns The number of URLs in the current
	 *   URL list, or zero if the URL list is
	 *   empty.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if the prepared SQL statement for retrieving
	 *   the number of URLs in the URL list is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the number
	 *   of URLs in the URL list.
	 */
	std::uint64_t Database::getNumberOfUrls() {
		std::uint64_t result{0};

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(ps.getNumberOfUrls == 0) {
			throw Exception(
					"Parser::Database::getNumberOfUrls():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getNumberOfUrls)};

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::getNumberOfUrls", e);
		}

		return result;
	}

	/*
	 * URL LOCKING
	 */

	//! Gets the current URL lock expiration time from the database.
	/*!
	 * The database calculates the lock expiration
	 *  time based on the given local maximum
	 *  locking time.
	 *
	 * \param lockTimeout The maximum URL locking
	 *   time, in seconds.
	 *
	 * \returns The current URL lock expiration
	 *   time, as string in the format @c
	 *   YYYY-MM-DD HH:MM:SS.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if the prepared SQL statement for
	 *   calculating the URL lock expiration time is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while calculating the current
	 *   URL lock expiration time.
	 */
	std::string Database::getLockTime(std::uint32_t lockTimeout) {
		std::string result;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getLockTime == 0) {
			throw Exception(
					"Parser::Database::getLockTime():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getLockTime)};

		// get URL lock end time from database
		try {
			// execute SQL query
			sqlStatement.setUInt(sqlArg1, lockTimeout);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getString("locktime");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::getLockTime", e);
		}

		return result;
	}

	//! Gets the current lock expiration time for a URL from the database.
	/*!
	 * \param urlId ID of the URL whose current lock
	 *   expiration time will be retrieved from the
	 *   database.
	 *
	 * \returns The current lock expiration time of
	 *   the given URL, as string in the format @c
	 *   YYYY-MM-DD HH:MM:SS, or an empty string, if
	 *   no URL is given, or the URL has not been
	 *   locked.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if the prepared SQL statement for
	 *   retrieving the current lock expiration time
	 *   of a URL is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the current
	 *   lock expiration time for the given URL.
	 */
	std::string Database::getUrlLockTime(std::uint64_t urlId) {
		std::string result;

		// check argument
		if(urlId == 0) {
			return "";
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getUrlLockTime == 0) {
			throw Exception(
					"Parser::Database::getUrlLockTime():"
					"Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getUrlLockTime)};

		// get URL lock end time from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getString("locktime");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::getUrlLockTime", e);
		}

		return result;
	}

	//! Locks a URL in the database, if it is lockable, or extends its locking time, if it is still locked by the parser.
	/*!
	 * \param urlId ID of the URL that will be
	 *   locked, or whose locking time will be
	 *   extended.
	 * \param lockTime The expiration time of the
	 *   previous lock held over the given URL
	 *   by the current thread.
	 * \param lockTimeout The maximum URL locking
	 *   time, in seconds.
	 *
	 * \returns The new expiration time of the lock,
	 *   as string in the format @c
	 *   YYYY-MM-DD HH:MM:SS, or an empty string, if
	 *   the URL could not be locked, because it is
	 *   currently locked by another thread.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for locking a URL is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while locking the URL, or
	 *   renewing its URL lock.
	 */
	std::string Database::renewUrlLockIfOk(std::uint64_t urlId, const std::string& lockTime, std::uint32_t lockTimeout) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Parser::Database::renewUrlLockIfOk():"
					" No URL has been specified"
			);
		}

		// get lock time
		auto newLockTime{this->getLockTime(lockTimeout)};

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(this->ps.renewUrlLockIfOk == 0) {
			throw Exception(
					"Parser::Database::renewUrlLockIfOk():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement for renewing the URL lock
		auto& sqlStatement{this->getPreparedStatement(this->ps.renewUrlLockIfOk)};

		// lock URL in database
		try {
			// execute SQL query
			sqlStatement.setString(sqlArg1, newLockTime);
			sqlStatement.setString(sqlArg2, lockTime);
			sqlStatement.setUInt64(sqlArg3, urlId);
			sqlStatement.setString(sqlArg4, lockTime);

			if(Database::sqlExecuteUpdate(sqlStatement) < 1) {
				return std::string(); // locking failed when no entries have been updated
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::renewUrlLockIfOk", e);
		}

		// get new expiration time of URL lock
		return newLockTime;
	}

	//! Unlocks a URL in the database.
	/*!
	 * \param urlId ID of the URL that will be
	 *   unlocked, if its lock is still active
	 *   and held by the current thread.
	 * \param lockTime The expiration time of the
	 *   lock held over the given URL by the
	 *   current thread.
	 *
	 * \returns True, if the unlocking was
	 *   successful, or no URL has been given.
	 *   False, if the URL could not be unlocked,
	 *   because its lock has expired and it has
	 *   already been locked by another thread.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if the prepared SQL statement for unlocking
	 *   a URL is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while trying to unlock the
	 *   URL.
	 */
	bool Database::unLockUrlIfOk(std::uint64_t urlId, const std::string& lockTime) {
		// check argument
		if(urlId == 0) {
			return true; // no URL lock to unlock
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.unLockUrlIfOk == 0) {
			throw Exception(
					"Parser::Database::unLockUrlIfOk():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.unLockUrlIfOk)};

		// unlock URL in database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);
			sqlStatement.setString(sqlArg2, lockTime);

			return Database::sqlExecuteUpdate(sqlStatement) > 0;
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::unLockUrlIfOk", e);
		}

		return false;
	}

	//! Unlocks multiple URLs in the database at once.
	/*!
	 * \note The SQL statements needed for
	 *   unlocking the URLs will only be created
	 *   shortly before query execution, as it
	 *   should only be used once (on shutdown
	 *   on the parser). During normal
	 *   operations, URLs are unlocked as they
	 *   are processed — one by one.
	 *
	 * \param urls Reference to a queue containing
	 *   IDs and URIs of the URLs to unlock. It
	 *   will be cleared while trying to unlock
	 *   the URLs, even if some or all of the URLs
	 *   could not be unlocked, because their lock
	 *   has expired and they have already been
	 *   locked by another thread. If empty,
	 *   nothing will be done.
	 * \param lockTime The expiration time of the
	 *   lock held over the given URLs by the
	 *   current thread.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while trying to unlock the
	 *   URLs.
	 */
	void Database::unLockUrlsIfOk(std::queue<IdString>& urls, std::string& lockTime) {
		// check argument
		if(urls.empty()) {
			return; // no URLs to unlock
		}

		// check connection
		this->checkConnection();

		// create and get prepared SQL statement
		auto& sqlStatement{
			this->getPreparedStatement(
					this->addPreparedStatement(
							this->queryUnlockUrlsIfOk(urls.size())
					)
			)
		};

		// unlock URLs in database
		try {
			// set placeholders
			std::size_t counter{sqlArg1};

			while(!urls.empty()) {
				sqlStatement.setUInt64(counter, urls.front().first);

				urls.pop();

				++counter;
			}

			sqlStatement.setString(counter, lockTime);

			// execute SQL query
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::unLockUrlsIfOk", e);
		}

		lockTime.clear();
	}

	/*
	 * PARSING
	 */

	//! Checks the parsing table.
	/*!
	 * Deletes duplicate URL locks.
	 *
	 * \returns The number of duplicate URL locks
	 *   that have been deleted. Zero, if no
	 *   duplicate locks have been found.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if the prepared SQL statement for checking
	 *   the table is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the table.
	 */
	std::uint32_t Database::checkParsingTable() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement for locking the URL
		if(this->ps.checkParsingTable == 0) {
			throw Exception(
					"Parser::Database::checkParsingTable():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement for locking the URL
		auto& sqlStatement{this->getPreparedStatement(this->ps.checkParsingTable)};

		// lock URL in database if not locked (and not parsed yet when re-parsing is deactivated)
		try {
			// execute SQL query
			const auto result{Database::sqlExecuteUpdate(sqlStatement)};

			if(result > 0) {
				return result;
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::checkParsingTable", e);
		}

		return 0;
	}

	//! Gets the number of crawled contents stored for a specific URL from the database.
	/*!
	 * \param urlId The ID of the URL whose number
	 *   of contents will be retrieved from the
	 *   database.
	 *
	 * \returns The number of contents stored for
	 *   the given URL in the database.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for retrieving the number
	 *   of contents stored for a URL in the
	 *   database is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the number
	 *   of contents stored for the given URL.
	 */
	std::uint64_t Database::getNumberOfContents(std::uint64_t urlId) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Parser::Database::getNumberOfContents():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getNumberOfContents == 0) {
			throw Exception(
					"Parser::Database::getNumberOfContents():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getNumberOfContents)};

		// get number of contents for URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				return sqlResultSet->getUInt64("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::getNumberOfContents", e);
		}

		return 0;
	}

	//! Gets crawled content stored in the database for a specific URL.
	/*!
	 * \param urlId The ID of the URL whose crawled
	 *   content will be retrieved from the database.
	 * \param index Index of the content to be
	 *   retrieved for the given URL, starting at
	 *   zero for the latest content.
	 * \param contentTo Reference to which the
	 *   retrieved content for the given URL and its
	 *   ID will be stored. Will not be changed, if
	 *   no content could be retrieved from the
	 *   database.
	 *
	 * \returns True, if content has succesfully
	 *   been retrieved from the database. False,
	 *   if the requested content does not exist.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for retrieving the content
	 *   stored in the database for a URL is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   contents stored for the given URL from the
	 *   database.
	 */
	bool Database::getLatestContent(std::uint64_t urlId, std::uint64_t index, IdString& contentTo) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Parser::Database::getLatestContent():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getLatestContent == 0) {
			throw Exception(
					"Parser::Database::getLatestContent():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getLatestContent)};

		// get latest content for URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);
			sqlStatement.setUInt64(sqlArg2, index);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				contentTo = IdString(
						sqlResultSet->getUInt64("id"),
						sqlResultSet->getString("content")
				);

				return true;
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::getLatestContent", e);
		}

		return false;
	}

	//! Gets all crawled contents stored in the database for a specific URL.
	/*!
	 * \param urlId The ID of the URL whose crawled
	 *   contents will be retrieved from the database.
	 *
	 * \returns A queue containing all contents
	 *   stored in the database for the specified URL,
	 *   and their IDs. The queue is empty, when no
	 *   content has been crawled for the given URL.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for retrieving all contents
	 *   stored in the database for a URL is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the
	 *   contents stored for the given URL from the
	 *   database.
	 */
	std::queue<Database::IdString> Database::getAllContents(std::uint64_t urlId) {
		std::queue<IdString> result;

		// check argument
		if(urlId == 0) {
			throw Exception(
					"Parser::Database::getAllContents():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getAllContents == 0) {
			throw Exception(
					"Parser::Database::getAllContents():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getAllContents)};

		// get all contents from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet) {
				while(sqlResultSet->next()) {
					result.emplace(sqlResultSet->getUInt64("id"), sqlResultSet->getString("content"));
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::getAllContents", e);
		}

		return result;
	}

	//! Gets the latest content ID from a parsed ID.
	/*!
	 * \param parsedId ID that has been parsed.
	 *
	 * \returns ID of the latest content for which
	 *   the same ID has been parsed. Zero, if
	 *   no content with the specified ID has been
	 *   parsed yet.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if no parsed ID has been specified, i.e.
	 *   it is zero, or if the prepared SQL
	 *   statement for retrieving the content ID
	 *   from a parsed ID is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the content
	 *   for the specified parsed ID from the
	 *   database.
	 */
	std::uint64_t Database::getContentIdFromParsedId(const std::string& parsedId) {
		std::uint64_t result{0};

		// check argument
		if(parsedId.empty()) {
			throw Exception(
					"Parser::Database::getContentIdFromParsedId():"
					" No parsed ID has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getContentIdFromParsedId == 0) {
			throw Exception(
					"Parser::Database::getContentIdFromParsedId():"
					"Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getContentIdFromParsedId)};

		// get content ID from database
		try {
			// execute SQL query
			sqlStatement.setString(sqlArg1, parsedId);
			sqlStatement.setString(sqlArg2, parsedId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get (and parse) result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("content");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::getContentIdFromParsedId", e);
		}

		return result;
	}

	//! Adds parsed data to the database, or updates data that already exists.
	/*!
	 * \param entries Reference to a queue
	 *   containing the data to add. If empty,
	 *   nothing will be done. The queue will be
	 *   emptied as the data will be processed.
	 * \param statusSetter Data needed to keep
	 *   the status of the thread updated.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if any of the prepared SQL statements for
	 *   adding and updating parsed data is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding or updating
	 *   parsed data in the database.
	 */
	void Database::updateOrAddEntries(
			std::queue<DataEntry>& entries,
			StatusSetter& statusSetter
	) {
		// check argument
		if(entries.empty()) {
			return;
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(
				this->ps.updateOrAddEntry == 0
				|| this->ps.updateOrAdd10Entries == 0
				|| this->ps.updateOrAdd100Entries == 0
				|| this->ps.updateOrAdd1000Entries == 0
		) {
			throw Exception(
					"Parser::Database::updateOrAddEntries():"
					" Missing prepared SQL statement(s)"
			);
		}

		// get prepared SQL statements
		auto& sqlStatement1{this->getPreparedStatement(this->ps.updateOrAddEntry)};
		auto& sqlStatement10{this->getPreparedStatement(this->ps.updateOrAdd10Entries)};
		auto& sqlStatement100{this->getPreparedStatement(this->ps.updateOrAdd100Entries)};
		auto& sqlStatement1000{this->getPreparedStatement(this->ps.updateOrAdd1000Entries)};

		// count fields
		constexpr auto minFields{5};

		const auto fields{
			std::count_if(
					this->targetFieldNames.cbegin(),
					this->targetFieldNames.cend(),
					[](const auto& fieldName) {
						return !fieldName.empty();
					}
			) + minFields
		};

		try {
			const auto total{entries.size()};
			std::size_t done{0};

			// add 1,000 entries at once
			while(entries.size() >= nAtOnce1000) {
				for(std::uint16_t n{0}; n < nAtOnce1000; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					// set standard values
					sqlStatement1000.setUInt64(n * fields + sqlArg1, entries.front().contentId);
					sqlStatement1000.setUInt64(n * fields + sqlArg2, entries.front().contentId);
					sqlStatement1000.setString(n * fields + sqlArg3, entries.front().dataId);
					sqlStatement1000.setString(n * fields + sqlArg4, entries.front().dataId);

					if(entries.front().dateTime.empty()) {
						sqlStatement1000.setNull(n * fields + sqlArg5, 0);
					}
					else {
						sqlStatement1000.setString(n * fields + sqlArg5, entries.front().dateTime);
					}

					// set custom values
					std::size_t counter{sqlArg6};

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->targetFieldNames.at(index).empty())) {
							sqlStatement1000.setString(n * fields + counter, *it);

							++counter;
						}
					}

					// remove entry
					entries.pop();
				}

				// execute SQL query
				Database::sqlExecute(sqlStatement1000);

				// update status
				done += nAtOnce1000;

				statusSetter.update(done, total);
			}

			// add 100 entries at once
			while(entries.size() >= nAtOnce100) {
				for(std::uint8_t n{0}; n < nAtOnce100; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					// set standard values
					sqlStatement100.setUInt64(n * fields + sqlArg1, entries.front().contentId);
					sqlStatement100.setUInt64(n * fields + sqlArg2, entries.front().contentId);
					sqlStatement100.setString(n * fields + sqlArg3, entries.front().dataId);
					sqlStatement100.setString(n * fields + sqlArg4, entries.front().dataId);

					if(entries.front().dateTime.empty()) {
						sqlStatement100.setNull(n * fields + sqlArg5, 0);
					}
					else {
						sqlStatement100.setString(n * fields + sqlArg5, entries.front().dateTime);
					}

					// set custom values
					std::size_t counter{sqlArg6};

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->targetFieldNames.at(index).empty())) {
							sqlStatement100.setString(n * fields + counter, *it);

							++counter;
						}
					}

					// remove entry
					entries.pop();
				}

				// execute SQL query
				Database::sqlExecute(sqlStatement100);

				// update status
				done += nAtOnce100;

				statusSetter.update(done, total);
			}

			// add 10 entries at once
			while(entries.size() >= nAtOnce10) {
				for(std::uint8_t n{0}; n < nAtOnce10; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					// set standard values
					sqlStatement10.setUInt64(n * fields + sqlArg1, entries.front().contentId);
					sqlStatement10.setUInt64(n * fields + sqlArg2, entries.front().contentId);
					sqlStatement10.setString(n * fields + sqlArg3, entries.front().dataId);
					sqlStatement10.setString(n * fields + sqlArg4, entries.front().dataId);

					if(entries.front().dateTime.empty()) {
						sqlStatement10.setNull(n * fields + sqlArg5, 0);
					}
					else {
						sqlStatement10.setString(n * fields + sqlArg5, entries.front().dateTime);
					}

					// set custom values
					std::size_t counter{sqlArg6};

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->targetFieldNames.at(index).empty())) {
							sqlStatement10.setString(n * fields + counter, *it);

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

				// set standard values
				sqlStatement1.setUInt64(sqlArg1, entries.front().contentId);
				sqlStatement1.setUInt64(sqlArg2, entries.front().contentId);
				sqlStatement1.setString(sqlArg3, entries.front().dataId);
				sqlStatement1.setString(sqlArg4, entries.front().dataId);

				if(entries.front().dateTime.empty()) {
					sqlStatement1.setNull(sqlArg5, 0);
				}
				else {
					sqlStatement1.setString(sqlArg5, entries.front().dateTime);
				}

				// set custom values
				std::size_t counter{sqlArg6};

				for(
						auto it{entries.front().fields.cbegin()};
						it != entries.front().fields.cend();
						++it
				) {
					const auto index{it - entries.front().fields.cbegin()};

					if(!(this->targetFieldNames.at(index).empty())) {
						sqlStatement1.setString(counter, *it);

						++counter;
					}
				}

				// remove entry
				entries.pop();

				// execute SQL query
				Database::sqlExecute(sqlStatement1);
			}

			statusSetter.finish();
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::updateOrAddEntries", e);
		}
	}

	//! Sets URLs to finished in the database, except those locked by another thread.
	/*!
	 * Skips URLs that have been locked by another
	 *  thread, and whose lock is still active.
	 *
	 * \param finished Reference to a queue of
	 *   pairs, containing the IDs and URIs of the
	 *   URLs to be set to finished. If empty,
	 *   nothing will be done.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if any of the prepared SQL statements for
	 *   setting URLs to finished is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while setting URLs to
	 *   finished in the database.
	 */
	void Database::setUrlsFinishedIfLockOk(std::queue<IdString>& finished) {
		// check argument
		if(finished.empty()) {
			return;
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(
				this->ps.setUrlFinishedIfLockOk == 0
				|| this->ps.set10UrlsFinishedIfLockOk == 0
				|| this->ps.set100UrlsFinishedIfLockOk == 0
				|| this->ps.set1000UrlsFinishedIfLockOk == 0
		) {
			throw Exception(
					"Parser::Database::setUrlsFinishedIfLockOk():"
					" Missing prepared SQL statement(s)"
			);
		}

		// get prepared SQL statements
		auto& sqlStatement1{this->getPreparedStatement(this->ps.setUrlFinishedIfLockOk)};
		auto& sqlStatement10{this->getPreparedStatement(this->ps.set10UrlsFinishedIfLockOk)};
		auto& sqlStatement100{this->getPreparedStatement(this->ps.set100UrlsFinishedIfLockOk)};
		auto& sqlStatement1000{this->getPreparedStatement(this->ps.set1000UrlsFinishedIfLockOk)};

		// set URLs to finished in database
		constexpr auto numFields{2};

		try {
			// set 1,000 URLs at once
			while(finished.size() > nAtOnce1000) {
				for(std::uint16_t n{0}; n < nAtOnce1000; ++n) {
					sqlStatement1000.setUInt64(n * numFields + sqlArg1, finished.front().first);
					sqlStatement1000.setString(n * numFields + sqlArg2, finished.front().second);

					finished.pop();
				}

				Database::sqlExecute(sqlStatement1000);
			}

			// set 100 URLs at once
			while(finished.size() > nAtOnce100) {
				for(std::uint8_t n{0}; n < nAtOnce100; ++n) {
					sqlStatement100.setUInt64(n * numFields + sqlArg1, finished.front().first);
					sqlStatement100.setString(n * numFields + sqlArg2, finished.front().second);

					finished.pop();
				}

				Database::sqlExecute(sqlStatement100);
			}

			// set 10 URLs at once
			while(finished.size() > nAtOnce10) {
				for(std::uint8_t n{0}; n < nAtOnce10; ++n) {
					sqlStatement10.setUInt64(n * numFields + sqlArg1, finished.front().first);
					sqlStatement10.setString(n * numFields + sqlArg2, finished.front().second);

					finished.pop();
				}

				Database::sqlExecute(sqlStatement10);
			}

			// set remaining URLs
			while(!finished.empty()) {
				sqlStatement1.setUInt64(sqlArg1, finished.front().first);
				sqlStatement1.setString(sqlArg2, finished.front().second);

				finished.pop();

				Database::sqlExecute(sqlStatement1);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::setUrlsFinishedIfLockOk", e);
		}
	}

	//! Updates the target table.
	/*!
	 * Sets the time that specifies, when the target
	 *  table has last been updated, to now – i.e.
	 *  the current database time.
	 *
	 * \throws Module::Parser::Database::Exception
	 *   if the prepared SQL statements for setting
	 *   the update time of the target table to now
	 *   is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while setting the update time
	 *   of the target table in the database.
	 */
	void Database::updateTargetTable() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.updateTargetTable == 0) {
			throw Exception(
					"Parser::Database::updateTargetTable():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.updateTargetTable)};

		try {
			// execute SQL query
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Parser::Database::updateTargetTable", e);
		}
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// check the value sizes in a parsing entry and remove values that are too large for the database
	bool Database::checkEntrySize(DataEntry& entry) {
		// check data sizes
		const auto max{this->getMaxAllowedPacketSize()};
		std::size_t tooLarge{0};

		if(entry.dataId.size() > max) {
			tooLarge = entry.dataId.size();

			entry.dataId.clear();
		}

		if(entry.dateTime.size() > max) {
			if(entry.dateTime.size() > tooLarge) {
					tooLarge = entry.dateTime.size();
			}

			entry.dateTime.clear();
		}

		for(auto& field : entry.fields) {
			if(field.size() > max) {
				if(field.size() > tooLarge) {
					tooLarge = field.size();
				}

				field.clear();
			}
		}

		if(tooLarge > 0) {
			// show warning about data size
			bool adjustServerSettings{false};

			std::ostringstream logStrStr;

			logStrStr.imbue(std::locale(""));

			logStrStr	<<	"WARNING: An entry could not be saved to the database,"
							" because the size of a parsed value ("
						<<	tooLarge
						<< " bytes) exceeds the ";

			if(tooLarge > maxContentSize) {
				logStrStr << "MySQL maximum of " << maxContentSizeString << ".";
			}
			else {
				logStrStr	<< "current MySQL server maximum of "
							<< max
							<< " bytes.";

				adjustServerSettings = true;
			}

			this->log(this->getLoggingMin(), logStrStr.str());

			if(adjustServerSettings) {
				this->log(
						this->getLoggingMin(),
						"Adjust the server's \'max_allowed_packet\'"
						" setting accordingly."
				);
			}

			return false;
		}

		return true;
	}

	// generate a SQL query for locking a specific number of URLs, throws Database::Exception
	std::string Database::queryLockUrls(std::size_t numberOfUrls) {
		// check arguments
		if(numberOfUrls == 0) {
			throw Exception(
					"Parser::Database::queryLockUrls():"
					" No URLs have been specified"
			);
		}

		// create INSERT INTO clause
		std::string sqlQueryString{"INSERT INTO `"};

		sqlQueryString +=			this->parsingTable;
		sqlQueryString +=				"`(id, target, url, locktime)"
								" VALUES";

		// create VALUES clauses
		for(std::size_t n{1}; n <= numberOfUrls; ++n) {
			sqlQueryString +=	" ("
									" ("
										"SELECT id"
										" FROM `";
			sqlQueryString +=				this->parsingTable;
			sqlQueryString +=				"` AS `";
			sqlQueryString +=				parsingTableAlias;
			sqlQueryString +=				std::to_string(n);
			sqlQueryString +=				"`"
										" WHERE target = ";
			sqlQueryString +=				std::to_string(this->targetTableId);
			sqlQueryString +=			" AND url = ?"
										" ORDER BY id DESC"
										" LIMIT 1"
									" ), ";
			sqlQueryString +=		std::to_string(this->targetTableId);
			sqlQueryString +=		","
									" ?,"
									" ?"
								" )";

			if(n < numberOfUrls) {
				sqlQueryString += ", ";
			}
		}

		// crate ON DUPLICATE KEY UPDATE clause
		sqlQueryString +=		" ON DUPLICATE KEY"
								" UPDATE locktime = VALUES(locktime)";

		return sqlQueryString;
	}

	// generate SQL query for updating or adding a specific number of parsed entries, throws Database::Exception
	std::string Database::queryUpdateOrAddEntries(std::size_t numberOfEntries) {
		// check arguments
		if(numberOfEntries == 0) {
			throw Exception(
					"Parser::Database::queryUpdateOrAddEntries():"
					" No entries have been specified"
			);
		}

		// create INSERT INTO clause
		std::string sqlQueryStr{"INSERT INTO `"};

		sqlQueryStr += 			this->targetTableFull;
		sqlQueryStr += 			"`"
								" ("
									" id,"
									" content,"
									" parsed_id,"
									" hash,"
									" parsed_datetime";

		std::size_t counter{0};

		for(const auto& targetFieldName : this->targetFieldNames) {
			if(!targetFieldName.empty()) {
				sqlQueryStr += 		", `parsed__";
				sqlQueryStr +=		targetFieldName;
				sqlQueryStr +=		"`";

				++counter;
			}
		}

		sqlQueryStr += 			")"
								" VALUES ";

		// create placeholder list (including existence check)
		for(std::size_t n{1}; n <= numberOfEntries; ++n) {
			sqlQueryStr +=		"( "
										"("
											"SELECT id"
											" FROM `";
			sqlQueryStr +=						this->targetTableFull;
			sqlQueryStr +=						"` AS `";
			sqlQueryStr +=						targetTableAlias;
			sqlQueryStr +=						"`"
											" WHERE content = ?"
											" LIMIT 1"
										"),"
										"?, ?, CRC32( ? ), ?";

			for(std::size_t c{0}; c < counter; ++c) {
				sqlQueryStr +=	 		", ?";
			}

			sqlQueryStr +=			")";

			if(n < numberOfEntries) {
				sqlQueryStr +=		", ";
			}
		}

		// create ON DUPLICATE KEY UPDATE clause
		sqlQueryStr +=				" ON DUPLICATE KEY UPDATE"
									" parsed_id = VALUES(parsed_id),"
									" hash = VALUES(hash),"
									" parsed_datetime = VALUES(parsed_datetime)";

		for(const auto& targetFieldName : this->targetFieldNames) {
			if(!targetFieldName.empty()) {
				sqlQueryStr +=		", `parsed__";
				sqlQueryStr +=		targetFieldName;
				sqlQueryStr +=		"` = VALUES(`parsed__";
				sqlQueryStr +=		targetFieldName;
				sqlQueryStr +=		"`)";
			}
		}

		// return query
		return sqlQueryStr;
	}

	// generate SQL query for setting a specific number of URLs to finished if they haven't been locked since parsing,
	//  throws Database::Exception
	std::string Database::querySetUrlsFinishedIfLockOk(std::size_t numberOfUrls) {
		// check arguments
		if(numberOfUrls == 0) {
			throw Exception(
					"Parser::Database::querySetUrlsFinishedIfLockOk():"
					" No URLs have been specified"
			);
		}

		// create UPDATE SET clause
		std::string sqlQueryString{"UPDATE `"};

		sqlQueryString +=			this->parsingTable;
		sqlQueryString +=			"`"
									" SET locktime = NULL, success = TRUE"
									" WHERE ";

		// create WHERE clause
		for(std::size_t n{0}; n < numberOfUrls; ++n) {
			if(n > 0) {
				sqlQueryString += " OR ";
			}

			sqlQueryString +=	" ( "
									" target = ";
			sqlQueryString +=			std::to_string(this->targetTableId);
			sqlQueryString +=		" AND url = ?"
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
	std::string Database::queryUnlockUrlsIfOk(std::size_t numberOfUrls) {
		std::string sqlQueryString{"UPDATE `"};

		sqlQueryString += 			this->parsingTable;
		sqlQueryString +=			"`"
									" SET locktime = NULL"
									" WHERE target = ";
		sqlQueryString +=				std::to_string(this->targetTableId);
		sqlQueryString +=			" AND"
									" (";

		for(std::size_t n{1}; n <= numberOfUrls; ++n) {
			sqlQueryString +=	" url = ?";

			if(n < numberOfUrls) {
				sqlQueryString += " OR";
			}
		}

		sqlQueryString +=	" )"
							" AND"
							" ("
								" locktime <= ?"
								" OR locktime <= NOW()"
							" )";

		return sqlQueryString;
	}

} /* namespace crawlservpp::Module::Parser */
