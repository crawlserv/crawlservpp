/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
 * This class provides database functionality for a crawler thread
 *  by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module::Crawler {

	/*
	 * CONSTRUCTION
	 */

	//! Constructor setting the database connection for the thread.
	/*!
	 * \param dbThread Reference to the database
	 * 	 connection used by the crawler thread.
	 */
	Database::Database(Module::Database& dbThread)
						: Wrapper::Database(dbThread) {}

	/*
	 * SETTERS
	 */

	//! Sets the maximum number of URLs to be processed at once.
	/*!
	 * \param setMaxBatchSize The maximum
	 *   number of URLs that will be processed
	 *   in one MySQL query.
	 */
	void Database::setMaxBatchSize(std::uint16_t setMaxBatchSize) {
		this->maxBatchSize = setMaxBatchSize;
	}

	//! Sets whether all URLs will be recrawled.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the crawler.
	 *
	 * \param isRecrawl Set to true, to force
	 *   the re-crawling of all URLs.
	 */
	void Database::setRecrawl(bool isRecrawl) {
		this->recrawl = isRecrawl;
	}

	//! Sets whether the current URL list is case-sensitive.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the crawler.
	 *
	 * \param isUrlCaseSensitive Specifies
	 *   whether the URLs in the current
	 *   URL list are case-sensitive.
	 *
	 * \warning Changing this property of
	 *   the URL list will invalidate all
	 *   hashs previously created!
	 */
	void Database::setUrlCaseSensitive(bool isUrlCaseSensitive) {
		this->urlCaseSensitive = isUrlCaseSensitive;

		// update case sensitivity in database
		this->setUrlListCaseSensitive(this->getOptions().urlListId, this->urlCaseSensitive);
	}

	//! Sets whether to enable URL debugging.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the crawler.
	 *
	 * \param isUrlDebug Specifies whether
	 *   URL debugging is enabled.
	 */
	void Database::setUrlDebug(bool isUrlDebug) {
		this->urlDebug = isUrlDebug;
	}

	//! Sets whether to check URLs on startup.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the crawler.
	 *
	 * \param isUrlStartupCheck Specifies
	 *   whether to perform a check of the URL
	 *   list on startup.
	 */
	// enable or disable URL check on startup
	void Database::setUrlStartupCheck(bool isUrlStartupCheck) {
		this->urlStartupCheck = isUrlStartupCheck;
	}

	/*
	 * PREPARED SQL STATEMENTS
	 */

	//! Prepares the SQL statements for the crawler.
	/*!
	 * \note The target table needs to be
	 *   prepared first.
	 *
	 * \throws Main::Database::Exception if
	 *   a MySQL error occurs during the
	 *   preparation of the SQL statements.
	 */
	void Database::prepare() {
		const auto verbose{this->getLoggingVerbose()};

		// create table names
		this->urlListTable = "crawlserv_"
				+ this->getOptions().websiteNamespace
				+ "_"
				+ this->getOptions().urlListNamespace;
		this->crawlingTable = this->urlListTable
				+ "_crawling";

		const auto crawledTable{this->urlListTable + "_crawled"};

		// create SQL strings for URL hashing
		std::string hashQuery{"CRC32( "};

		if(this->urlCaseSensitive) {
			hashQuery += "?";
		}
		else {
			hashQuery += "LOWER( ? )";
		}

		hashQuery += " )";

		std::string urlHash{"CRC32( "};

		if(this->urlCaseSensitive) {
			urlHash += "url";
		}
		else {
			urlHash += "LOWER( url )";
		}

		urlHash.push_back(')');

		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(std::size_t));

		this->log(verbose, "prepares getUrlId()...");

		this->addPreparedStatement(
				"SELECT id"
				" FROM "
				" ("
					"SELECT id, url"
					" FROM `" + this->urlListTable + "`"
					" WHERE hash = " + hashQuery +
					" ORDER BY id"
				" ) AS tmp"
				" WHERE url = ?"
				" LIMIT 1",
				this->ps.getUrlId
		);

		this->log(verbose, "prepares getNextUrl()...");

		std::string sqlQueryString{
							"SELECT `"
		};

		sqlQueryString += 		urlListTableAlias;
		sqlQueryString += 		"1`.id"
							" AS id,"
							" `";
		sqlQueryString +=		urlListTableAlias;
		sqlQueryString +=		"1`.url"
							" AS url"
							" FROM `";
		sqlQueryString += 		this->urlListTable;
		sqlQueryString += 		"`"
							" AS `";
		sqlQueryString += 		urlListTableAlias;
		sqlQueryString += 		"1`"
							" LEFT OUTER JOIN `";
		sqlQueryString += 		this->crawlingTable;
		sqlQueryString += 		"`"
							" AS `";
		sqlQueryString += 		crawlingTableAlias;
		sqlQueryString += 		"1`"
							" ON `";
		sqlQueryString += 		urlListTableAlias;
		sqlQueryString += 		"1`.id"
							" = `";
		sqlQueryString += 		crawlingTableAlias;
		sqlQueryString += 		"1`.url"
							" WHERE `";
		sqlQueryString += 		urlListTableAlias;
		sqlQueryString += 		"1`.id > ?"
							" AND manual = FALSE";

		if(!(this->recrawl)) {
			sqlQueryString +=
							" AND"
							" ("
								" `";
			sqlQueryString +=	crawlingTableAlias;
			sqlQueryString +=	"1`.success IS NULL"
							" OR `";
			sqlQueryString +=	crawlingTableAlias;
			sqlQueryString +=	"1`.success = FALSE"
							")";
		}

		sqlQueryString +=	" AND"
							" ("
								" `";
		sqlQueryString +=			crawlingTableAlias;
		sqlQueryString +=			"1`.locktime IS NULL"
								" OR `";
		sqlQueryString +=			crawlingTableAlias;
		sqlQueryString +=			"1`.locktime < NOW()"
							" )"
							" ORDER BY `";
		sqlQueryString +=			urlListTableAlias;
		sqlQueryString +=			"1`.id"
							" LIMIT 1";

		this->addPreparedStatement(sqlQueryString, this->ps.getNextUrl);

		this->log(verbose, "prepares addUrlIfNotExists()...");

		sqlQueryString = 	"INSERT IGNORE INTO `";

		sqlQueryString += 		this->urlListTable;
		sqlQueryString += 		"`(id, url, manual, hash)"
							"VALUES ("
								" ("
									"SELECT id"
									" FROM"
									" ("
										"SELECT id, url"
										" FROM `";
		sqlQueryString +=					this->urlListTable;
		sqlQueryString +=					"`"
										" AS `";
		sqlQueryString +=					urlListTableAlias;
		sqlQueryString +=					"1`"
										" WHERE hash = ";
		sqlQueryString +=					hashQuery;
		sqlQueryString +=				" ) AS tmp2"
									" WHERE url = ?"
									" LIMIT 1"
								" ),"
								"?, "
								"?, ";
		sqlQueryString +=		hashQuery;
		sqlQueryString +=	")";

		this->addPreparedStatement(sqlQueryString, this->ps.addUrlIfNotExists);

		this->log(verbose, "prepares addUrlsIfNotExist() [1/3]...");

		this->addPreparedStatement(
				this->queryAddUrlsIfNotExist(
						nAtOnce10,
						hashQuery
				),
				this->ps.add10UrlsIfNotExist
		);

		this->log(verbose, "prepares addUrlsIfNotExist() [2/3]...");

		this->addPreparedStatement(
			this->queryAddUrlsIfNotExist(
					nAtOnce100,
					hashQuery
			),
			this->ps.add100UrlsIfNotExist
		);

		this->log(verbose, "prepares addUrlsIfNotExist() [3/3]...");

		this->addPreparedStatement(
				this->queryAddUrlsIfNotExist(
						this->maxBatchSize,
						hashQuery
				),
				this->ps.addMaxUrlsIfNotExist
		);

		this->log(verbose, "prepares getUrlPosition()...");

		this->addPreparedStatement(
				"SELECT COUNT(*)"
				" AS result"
				" FROM `" + this->urlListTable + "`"
				" WHERE id < ?",
				this->ps.getUrlPosition
		);

		this->log(verbose, "prepares getNumberOfUrls()...");

		this->addPreparedStatement(
				"SELECT COUNT(*)"
				" AS result"
				" FROM `" + this->urlListTable + "`",
				this->ps.getNumberOfUrls
		);

		this->log(verbose, "prepares getUrlLock()...");

		this->addPreparedStatement(
				"SELECT locktime"
				" FROM `" + this->crawlingTable + "`"
				" WHERE url = ?"
				" LIMIT 1",
				this->ps.getUrlLockTime
		);

		this->log(verbose, "prepares isUrlCrawled()...");

		this->addPreparedStatement(
				"SELECT success"
				" FROM `" + this->crawlingTable + "`"
				" WHERE url = ?"
				" LIMIT 1",
				this->ps.isUrlCrawled
		);

		this->log(verbose, "prepares lockUrlIfOk() [1/2]...");

		this->addPreparedStatement(
				"UPDATE `" + this->crawlingTable + "`"
				" SET locktime = GREATEST"
				"("
					"NOW() + INTERVAL ? SECOND,"
					"? + INTERVAL 1 SECOND"
				")"
				" WHERE url = ?"
				" AND"
				" ("
					" locktime <= ?"
					" OR locktime IS NULL"
					" OR locktime < NOW()"
				" )"
				" LIMIT 1",
				this->ps.renewUrlLockIfOk
		);

		this->log(verbose, "prepares lockUrlIfOk() [2/2]...");

		sqlQueryString =	"INSERT INTO `";

		sqlQueryString +=		this->crawlingTable;
		sqlQueryString +=		"`(id, url, locktime)"
							" VALUES"
							" ("
								" ("
									"SELECT id"
									" FROM `";
		sqlQueryString +=				this->crawlingTable;
		sqlQueryString +=				"`"
									" AS ";
		sqlQueryString +=				crawlingTableAlias;
		sqlQueryString +=				"1"
									" WHERE url = ?"
									" LIMIT 1"
								" ),"
								" ?,"
								" NOW() + INTERVAL ? SECOND"
							" )"
							" ON DUPLICATE KEY UPDATE locktime = "
								"IF("
									" ("
										" locktime IS NULL"
										" OR locktime < NOW()"
									" ),"
									" VALUES(locktime),"
									" locktime"
								")";

		this->addPreparedStatement(sqlQueryString, this->ps.addUrlLockIfOk);

		this->log(verbose, "prepares unLockUrlIfOk()...");

		this->addPreparedStatement(
				"UPDATE `" + this->crawlingTable + "`"
				" SET locktime = NULL"
				" WHERE url = ?"
				" AND"
				" ("
					" locktime IS NULL"
					" OR locktime <= ?"
					" OR locktime < NOW()"
				")"
				" LIMIT 1",
				this->ps.unLockUrlIfOk
		);

		this->log(verbose, "prepares setUrlFinishedIfOk()...");

		this->addPreparedStatement(
				"UPDATE `" + this->crawlingTable + "`"
				" SET success = TRUE, locktime = NULL"
				" WHERE url = ?"
				" AND "
				" ("
					" locktime <= ?"
					" OR locktime IS NULL"
					" OR locktime < NOW()"
				")"
				" LIMIT 1",
				this->ps.setUrlFinishedIfOk
		);

		this->log(verbose, "prepares saveContent()...");

		this->addPreparedStatement(
				"INSERT INTO `" + crawledTable
				+ "`(url, response, type, content)"
				" VALUES (?, ?, ?, ?)",
				this->ps.saveContent
		);

		this->log(verbose, "prepares saveArchivedContent()...");

		this->addPreparedStatement(
				"INSERT INTO `" + crawledTable
				+ "`(url, crawltime, archived, response, type, content)"
				" VALUES (?, ?, TRUE, ?, ?, ?)",
				this->ps.saveArchivedContent
		);

		this->log(verbose, "prepares isArchivedContentExists()...");

		this->addPreparedStatement(
				"SELECT EXISTS"
				" ("
					"SELECT *"
					" FROM `" + crawledTable + "`"
					" WHERE url = ?"
					" AND crawltime = ?"
				" )"
				" AS result",
				this->ps.isArchivedContentExists
		);

		if(this->urlStartupCheck || this->urlDebug) {
			this->log(verbose, "prepares urlDuplicationCheck()...");

			std::string groupBy;

			if(this->urlCaseSensitive) {
				groupBy = "url";
			}
			else {
				groupBy = "LOWER(url)";
			}

			this->addPreparedStatement(
					"SELECT "
					" CAST("
						+ groupBy +
						" AS BINARY"
					" )"
					" AS url,"
					" COUNT( " + groupBy + " )"
					" FROM `" + this->urlListTable + "`"
					" GROUP BY CAST( "
						+ groupBy +
						" AS BINARY "
					")"
					" HAVING COUNT("
						+ groupBy + " "
					") > 1",
					this->ps.urlDuplicationCheck
			);
		}
		else if(this->ps.urlDuplicationCheck > 0) {
			this->clearPreparedStatement(this->ps.urlDuplicationCheck);
		}

		this->log(verbose, "prepares urlHashCheck() [1/2]...");

		this->addPreparedStatement(
				"SELECT EXISTS"
				" ("
					" SELECT *"
					" FROM `" + this->urlListTable + "`"
					" WHERE hash <> " + urlHash + ""
				" )"
				" AS result",
				this->ps.urlHashCheck
		);

		this->log(verbose, "prepares urlHashCheck() [2/2]...");

		this->addPreparedStatement(
				"UPDATE `" + this->urlListTable + "`"
				" SET hash = " + urlHash,
				this->ps.urlHashCorrect
		);

		if(this->urlStartupCheck) {
			this->log(verbose, "prepares urlEmptyCheck()...");

			this->addPreparedStatement(
					"SELECT id"
					" FROM `" + this->urlListTable + "`"
					" WHERE url = ''"
					" LIMIT 1",
					this->ps.urlEmptyCheck
			);
		}
		else if(this->ps.urlEmptyCheck > 0) {
			this->clearPreparedStatement(this->ps.urlEmptyCheck);
		}

		if(this->urlStartupCheck) {
			this->log(verbose, "prepares getUrls()...");

			this->addPreparedStatement(
					"SELECT url"
					" FROM `" + this->urlListTable + "`",
					this->ps.getUrls
			);
		}
		else if(this->ps.getUrls > 0) {
			this->clearPreparedStatement(this->ps.getUrls);
		}

		if(this->urlStartupCheck || this->urlDebug) {
			this->log(verbose, "prepares removeDuplicates()...");

			std::string urlComparison;

			if(this->urlCaseSensitive) {
				urlComparison = "url LIKE ?";
			}
			else {
				urlComparison = "LOWER(url) LIKE LOWER(?)";
			}

			this->addPreparedStatement(
					" DELETE"
					"  FROM `" + this->urlListTable + "`"
					"  WHERE id IN "
						" ("
							" SELECT id FROM ("
								" SELECT id FROM `" + this->urlListTable + "`"
								"  WHERE id > ?" // ignore the first occurence
								"  AND hash = " + hashQuery +
							" ) AS tmp"
							" WHERE " + urlComparison +
						" )",
						this->ps.removeDuplicates
			);
		}
		else if(this->ps.removeDuplicates > 0) {
			this->clearPreparedStatement(this->ps.removeDuplicates);
		}
	}

	/*
	 * URLS
	 */

	//! Gets the ID of a URL from the database.
	/*!
	 * Uses a hash check for first checking the
	 *  probable existence of the URL.
	 *
	 * \param url Constant reference to a string
	 *   containing the URL to be checked.
	 *
	 * \returns The ID of the given URL, or zero
	 *   if the URL does not exist in the current
	 *   URL list.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, or the
	 *   prepared SQL statement for retrieving the
	 *   ID of a URL is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the ID of
	 *   the URL from the database.
	 */
	std::uint64_t Database::getUrlId(const std::string& url) {
		// check argument
		if(url.empty()) {
			throw Exception(
					"Crawler::Database::getUrlId():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getUrlId == 0) {
			throw Exception(
					"Crawler::Database::getUrlId():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getUrlId)};

		// get ID of URL from database
		try {
			// execute SQL query for getting URL
			sqlStatement.setString(sqlArg1, url);
			sqlStatement.setString(sqlArg2, url);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next())  {
				return sqlResultSet->getUInt64("id");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::getUrlId", e);
		}

		return 0;
	}

	//! Gets the ID of the next URL to crawl from the database.
	/*!
	 * \param currentUrlId The ID of the URL that
	 *   has been crawled last.
	 *
	 * \returns A pair of the ID and a string
	 *   containing the next URL to crawl,
	 *   or an empty pair if there are no
	 *   more URLs to crawl.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if the prepared SQL statement for
	 *   retrieving the next URL to crawl is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the next URL
	 *   to crawl from the database.
	 */
	Database::IdString Database::getNextUrl(std::uint64_t currentUrlId) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getNextUrl == 0) {
			throw Exception(
					"Crawler::Database::getNextUrl():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getNextUrl)};

		// get next URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, currentUrlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				return IdString(
						sqlResultSet->getUInt64("id"),
						sqlResultSet->getString("url")
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::getNextUrl", e);
		}

		return IdString();
	}

	//! Adds a URL to the database, if it doesnt exist already.
	/*!
	 * \param urlString Constant reference to a
	 *   string containing the URL to be added
	 *   to the current URL list in the database.
	 * \param manual Specifies whether the URL
	 *   is a custom URL, i.e. has been manually
	 *   added.
	 *
	 * \returns True if the URL has been added.
	 *   False, if the URL had already existed.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, or the
	 *   prepared SQL statement for adding a URL to
	 *   the database is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the URL to
	 *   the database.
	 */
	bool Database::addUrlIfNotExists(const std::string& urlString, bool manual) {
		// check argument
		if(urlString.empty()) {
			throw Exception(
					"Crawler::Database::addUrlIfNotExists():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.addUrlIfNotExists == 0) {
			throw Exception(
					"Crawler::Database::addUrlIfNotExists():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.addUrlIfNotExists)};

		// add URL to database and get resulting ID
		try {
			// execute SQL query
			sqlStatement.setString(sqlArg1, urlString);
			sqlStatement.setString(sqlArg2, urlString);
			sqlStatement.setString(sqlArg3, urlString);
			sqlStatement.setBoolean(sqlArg4, manual);
			sqlStatement.setString(sqlArg5, urlString);

			return Database::sqlExecuteUpdate(sqlStatement) > 0;
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::addUrlIfNotExists", e);
		}

		return false; // not really needed
	}

	//! Adds URLs to the database, if they do not exist already.
	/*!
	 * Adds the given URLs in batches of the
	 *  maximum batch size, 100 and 10 to the
	 *  database, if possible, to considerably
	 *  speed up the process.
	 *
	 * \param urls Reference to a queue
	 *   containing the URLs to be added to the
	 *   current URL list in the database. The
	 *   queue will be cleared after a
	 *   succesfull call to the function, even
	 *   if some or all of the given URL have
	 *   not been added, because they already
	 *   existed in the database.
	 * \param manual Specifies whether the URLs
	 *   are custom URL, i.e. have been manually
	 *   added.
	 *
	 * \returns The number of given URLs that
	 *   did not yet exist and have been added
	 *   to the database.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if one of the prepared SQL statements for
	 *   adding URLs to the database is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the URLs to
	 *   the database.
	 */
	std::size_t Database::addUrlsIfNotExist(std::queue<std::string>& urls, bool manual) {
		std::size_t result{};

		// check argument
		if(urls.empty()) {
			return 0;
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(
				this->ps.addUrlIfNotExists == 0
				|| this->ps.add10UrlsIfNotExist == 0
				|| this->ps.add100UrlsIfNotExist == 0
				|| this->ps.addMaxUrlsIfNotExist == 0
		) {
			throw Exception(
					"Crawler::Database::addUrlsIfNotExist():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statements
		auto& sqlStatement1{this->getPreparedStatement(this->ps.addUrlIfNotExists)};
		auto& sqlStatement10{this->getPreparedStatement(this->ps.add10UrlsIfNotExist)};
		auto& sqlStatement100{this->getPreparedStatement(this->ps.add100UrlsIfNotExist)};
		auto& sqlStatementMax{this->getPreparedStatement(this->ps.addMaxUrlsIfNotExist)};

		try {
			// add maximum number of URLs at once
			while(urls.size() >= this->maxBatchSize) {
				for(std::uint16_t n{}; n < this->maxBatchSize; ++n) {
					sqlStatementMax.setString(n * numArgsAddUrl + sqlArg1, urls.front());
					sqlStatementMax.setString(n * numArgsAddUrl + sqlArg2, urls.front());
					sqlStatementMax.setString(n * numArgsAddUrl + sqlArg3, urls.front());
					sqlStatementMax.setBoolean(n * numArgsAddUrl + sqlArg4, manual);
					sqlStatementMax.setString(n * numArgsAddUrl + sqlArg5, urls.front());

					urls.pop();
				}

				const auto added{Database::sqlExecuteUpdate(sqlStatementMax)};

				if(added > 0) {
					result += added;
				}
			}

			// add 100 URLs at once
			while(urls.size() >= nAtOnce100) {
				for(std::uint8_t n{}; n < nAtOnce100; ++n) {
					sqlStatement100.setString(n * numArgsAddUrl + sqlArg1, urls.front());
					sqlStatement100.setString(n * numArgsAddUrl + sqlArg2, urls.front());
					sqlStatement100.setString(n * numArgsAddUrl + sqlArg3, urls.front());
					sqlStatement100.setBoolean(n * numArgsAddUrl + sqlArg4, manual);
					sqlStatement100.setString(n * numArgsAddUrl + sqlArg5, urls.front());

					urls.pop();
				}

				const auto added{Database::sqlExecuteUpdate(sqlStatement100)};

				if(added > 0) {
					result += added;
				}
			}

			// add 10 URLs at once
			while(urls.size() >= nAtOnce10) {
				for(std::uint8_t n{}; n < nAtOnce10; ++n) {
					sqlStatement10.setString(n * numArgsAddUrl + sqlArg1, urls.front());
					sqlStatement10.setString(n * numArgsAddUrl + sqlArg2, urls.front());
					sqlStatement10.setString(n * numArgsAddUrl + sqlArg3, urls.front());
					sqlStatement10.setBoolean(n * numArgsAddUrl + sqlArg4, manual);
					sqlStatement10.setString(n * numArgsAddUrl + sqlArg5, urls.front());

					urls.pop();
				}

				const auto added{Database::sqlExecuteUpdate(sqlStatement10)};

				if(added > 0) {
					result += added;
				}
			}

			// add remaining URLs
			while(!urls.empty()) {
				sqlStatement1.setString(sqlArg1, urls.front());
				sqlStatement1.setString(sqlArg2, urls.front());
				sqlStatement1.setString(sqlArg3, urls.front());
				sqlStatement1.setBoolean(sqlArg4, manual);
				sqlStatement1.setString(sqlArg5, urls.front());

				urls.pop();

				if(Database::sqlExecuteUpdate(sqlStatement1) > 0) {
					++result;
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::addUrlsIfNotExist", e);
		}

		return result;
	}

	//! Gets the position of a URL in the current URL list.
	/*!
	 * \param urlId ID of the URL whose position
	 *   in the current URL list will be retrieved
	 *   from the database.
	 *
	 * \returns The position of the given URL in
	 *   the current URL list.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if the prepared SQL statement for
	 *   retrieving the position of a URL is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the position
	 *   of the URL from the database.
	 */
	std::uint64_t Database::getUrlPosition(std::uint64_t urlId) {
		std::uint64_t result{};

		// check argument
		if(urlId == 0) {
			return 0;
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getUrlPosition == 0) {
			throw Exception(
					"Crawler::Database::getUrlPosition():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getUrlPosition)};

		// get URL position of URL from database
		try {
			// disable locking as data consistency is not needed for calculating the approx. progress
			this->beginNoLock();

			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// re-enable locking
			this->endNoLock();

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::getUrlPosition", e);
		}

		return result;
	}

	//! Gets the number of URL in the current URL list.
	/*!
	 * \returns The total number of URLs in the
	 *   current URL list.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if the prepared SQL statement for
	 *   retrieving the number of URLs is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the number
	 *   of URLs from the database.
	 */
	std::uint64_t Database::getNumberOfUrls() {
		std::uint64_t result{};

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getNumberOfUrls == 0) {
			throw Exception(
					"Crawler::Database::getNumberOfUrls():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getNumberOfUrls)};

		// get number of URLs from database
		try {
			// disable locking as data consistency is not needed for calculating the approx. progress
			this->beginNoLock();

			// execute SQL query
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// re-enable locking
			this->endNoLock();

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::getNumberOfUrls", e);
		}

		return result;
	}

	/*
	 * URL CHECKING
	 */

	//! Checks the current URL list for duplicates.
	/*!
	 * Always throws an exception, unless no
	 *  duplicates are found.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if the prepared SQL statements for
	 *   checking the current URL list for
	 *   duplicates is missing, or if duplicates
	 *   have been found and removed.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the URL list
	 *   for duplicates.
	 */
	void Database::urlDuplicationCheck() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.urlDuplicationCheck == 0) {
			throw Exception(
					"Crawler::Database::urlDuplicationCheck():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.urlDuplicationCheck)};

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result and remove possible duplicates
			if(sqlResultSet) {
				if(sqlResultSet->next()) {
					std::queue<std::string> duplicates;

					do {
						duplicates.push(sqlResultSet->getString("url"));
					} while(sqlResultSet->next());

					std::size_t numDuplicates{};

					while(!duplicates.empty()) {
						numDuplicates += this->removeDuplicates(duplicates.front());

						duplicates.pop();
					}

					// throw exception after duplicates have been removed
					throw Exception(
							"Crawler::Database::urlDuplicationCheck():"
							" removed "
							+ std::to_string(numDuplicates)
							+ " duplicate URL(s) from `"
							+ this->urlListTable + "`"
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::urlDuplicationCheck", e);
		}
	}

	//! Checks the hash values in the current URL list.
	/*!
	 * Always throws an exception, unless all hash
	 *  values are correct.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if the prepared SQL statements for
	 *   checking the hash values in the current URL
	 *   list is missing, or if invalid has values
	 *   have been found and corrected.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the hash values
	 *   in the URL list.
	 */
	void Database::urlHashCheck() {
		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(this->ps.urlHashCheck == 0 || this->ps.urlHashCorrect == 0) {
			throw Exception(
					"Crawler::Database::urlHashCheck():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& checkStatement{this->getPreparedStatement(this->ps.urlHashCheck)};

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(checkStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next() && sqlResultSet->getBoolean("result")) {
				// correct hash values
				auto& correctStatement{this->getPreparedStatement(this->ps.urlHashCorrect)};
				auto updated{Database::sqlExecuteUpdate(correctStatement)};

				if(updated > 0) {
					std::ostringstream logStrStr;

					logStrStr << "corrected hash ";

					if(updated == 1) {
						logStrStr << "value for one URL.";
					}
					else {
						logStrStr.imbue(Helper::CommaLocale::locale());

						logStrStr << "values for " << updated << " URLs.";
					}

					this->log(this->getLoggingMin(), logStrStr.str());
				}
			}

		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::urlHashCheck", e);
		}
	}

	//! Checks for empty URLs in the current URL list.
	/*!
	 * Always throws an exception, unless no empty
	 *  URLs are found.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if the prepared SQL statements for
	 *   checking the current URL list for empty URLs
	 *   is missing, or if empty URLs have been found.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking for empty URLs
	 *   in the current URL list.
	 */
	void Database::urlEmptyCheck() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.urlEmptyCheck == 0) {
			throw Exception(
					"Crawler::Database::urlEmptyCheck():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.urlEmptyCheck)};

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				throw Exception(
						"Crawler::Database::urlEmptyCheck():"
						" Empty URL(s) in `"
						+ this->urlListTable
						+ "`"
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::urlEmptyCheck", e);
		}
	}

	//! Checks for URLs containing invalid UTF-8 characters in the current URL list.
	/*!
	 * Always throws an exception, unless all URLs
	 *  in the current URL list contain only valid
	 *  UTF-8-encoded characters.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if the prepared SQL statement for
	 *   retrieving all URLs from the current URL
	 *   list is missing, if a URL in the current
	 *   URL list contains invalid UTF-8 characters,
	 *   or if a UTF-8 error while checking the URLs
	 *   in the current URL list.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving all URLs
	 *   from the current URL list.
	 *
	 * \sa getUrls
	 */
	void Database::urlUtf8Check() {
		// get all URLs from URL list
		auto urls{this->getUrls()};

		// check URLs for invalid UTF-8
		while(!urls.empty()) {
			std::string err;

			if(!Helper::Utf8::isValidUtf8(urls.front(), err)) {
				if(err.empty()) {
					throw Exception(
							"Crawler::Database::urlUtf8Check():"
							" URL(s) containing invalid UTF-8 in `"
							+ this->urlListTable
							+ "` ["
							+ urls.front()
							+ "]"
					);
				}

				throw Exception(
						"Crawler::Database::urlUtf8Check(): "
						+ err
						+ " in `"
						+ this->urlListTable
						+ "` ["
						+ urls.front()
						+ "]"
				);
			}

			urls.pop();
		}
	}

	/*
	 * URL LOCKING
	 */

	//! Gets the time, until which a URL has been locked.
	/*!
	 * \param urlId The ID of the URL whose lock time
	 *   will be retrieved.
	 *
	 * \returns The time, until which the URL has been
	 *   locked, in the format @c YYYY-MM-DD HH:MM:SS.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if the prepared SQL statement for retrieving
	 *   the lock time is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while retrieving the lock time
	 *   of the URL.
	 */
	std::string Database::getUrlLockTime(std::uint64_t urlId) {
		// check argument
		if(urlId == 0) {
			return std::string();
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getUrlLockTime == 0) {
			throw Exception(
					"Crawler::Database::getUrlLockTime():"
					" Missing prepared SQL statement"
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
				return sqlResultSet->getString("locktime");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::getUrlLockTime", e);
		}

		return std::string();
	}

	//! Gets whether a URL has been crawled.
	/*!
	 * \param urlId The ID of the URL for which to
	 *   check whether it has been crawled.
	 *
	 * \returns True, if the URL has been crawled.
	 *   False, if the URL does not exist, or has
	 *   not yet been crawled.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for checking whether a URL
	 *   has been crawled is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking whether the URL
	 *   has been crawled.
	 */
	bool Database::isUrlCrawled(std::uint64_t urlId) {
		// check arguments
		if(urlId == 0) {
			throw Exception(
					"Crawler::Database::isUrlCrawled():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.isUrlCrawled == 0) {
			throw Exception(
					"Module::Crawler::Database::isUrlCrawled():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.isUrlCrawled)};

		// get lock ID from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				return sqlResultSet->getBoolean("success");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::isUrlCrawled", e);
		}

		return false;
	}

	//! Locks a URL if it is lockable or still locked by the current thread.
	/*!
	 * \param urlId The ID of the URL to lock.
	 * \param lockTime Constant reference to a
	 *   string containing the time at which the
	 *   current lock by the thread for this URL
	 *   will end (or has ended). Constant
	 *   reference to an empty string, if the URL
	 *   has not yet been locked by the current
	 *   thread.
	 * \param lockTimeout The time for which to
	 *   lock the URL for the current thread, in
	 *   seconds.
	 *
	 * \returns A copy of a string containing the
	 *   time until which the URL has been locked
	 *   for the current thread, in the format @c
	 *   YYYY-MM-DD HH:MM:SS. A copy of an empty
	 *   string, if the URL could not be locked,
	 *   or its lock could not be renewed for the
	 *   current thread, e.g. because it has
	 *   already been locked by another thread
	 *   since the current URL lock expired.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if one of the
	 *   prepared SQL statements for locking a URL,
	 *   or for renewing a URL lock is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while locking the URL, or
	 *   renewing its URL lock.
	 *
	 * \sa getLockTime
	 */
	std::string Database::lockUrlIfOk(
			std::uint64_t urlId,
			const std::string& lockTime,
			std::uint32_t lockTimeout
	) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Crawler::Database::lockUrlIfOk():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(this->ps.addUrlLockIfOk == 0 || this->ps.renewUrlLockIfOk == 0) {
			throw Exception(
					"Module::Crawler::Database::lockUrlIfOk():"
					"Missing prepared SQL statement"
			);
		}

		if(lockTime.empty()) {
			// get prepared SQL statement for locking the URL
			auto& sqlStatement{this->getPreparedStatement(this->ps.addUrlLockIfOk)};

			// add URL lock to database
			try {
				// execute SQL query
				sqlStatement.setUInt64(sqlArg1, urlId);
				sqlStatement.setUInt64(sqlArg2, urlId);
				sqlStatement.setUInt(sqlArg3, lockTimeout);

				if(Database::sqlExecuteUpdate(sqlStatement) < 1) {
					return std::string(); // locking failed when no entries have been updated
				}
			}
			catch(const sql::SQLException &e) {
				Database::sqlException("Crawler::Database::lockUrlIfOk", e);
			}
		}
		else {
			// get prepared SQL statement for renewing the URL lock
			auto& sqlStatement{this->getPreparedStatement(this->ps.renewUrlLockIfOk)};

			// lock URL in database
			try {
				// execute SQL query
				sqlStatement.setUInt(sqlArg1, lockTimeout);
				sqlStatement.setString(sqlArg2, lockTime);
				sqlStatement.setUInt64(sqlArg3, urlId);
				sqlStatement.setString(sqlArg4, lockTime);

				if(Database::sqlExecuteUpdate(sqlStatement) < 1) {
					return std::string(); // locking failed when no entries have been updated
				}
			}
			catch(const sql::SQLException &e) {
				Database::sqlException("Crawler::Database::lockUrlIfOk", e);
			}
		}

		// get new expiration time of URL lock
		return this->getUrlLockTime(urlId);
	}

	//! Unlocks a URL in the database.
	/*!
	 * \param urlId The ID of the URL to unlock.
	 * \param lockTime Constant reference to a
	 *   string containing the time at which the
	 *   current lock by the thread for this URL
	 *   will end (or has ended).
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, i.e. the given
	 *   URL ID is zero, if no lock time has been
	 *   specified, or if the prepared SQL statement
	 *   for unlocking a URL is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while unlocking the URL.
	 */
	void Database::unLockUrlIfOk(std::uint64_t urlId, const std::string& lockTime) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Crawler::Database::unLockUrlIfOk():"
					" No URL has been specified"
			);
		}

		if(lockTime.empty()) {
			throw Exception(
					"Crawler::Database::unLockUrlIfOk():"
					" URL lock is missing"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.unLockUrlIfOk == 0) {
			throw Exception(
					"Crawler::Database::unLockUrlIfOk():"
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

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::unLockUrlIfOk", e);
		}
	}

	//! Sets the URL to crawled in the database, if it is still locked by the thread.
	/*!
	 * \param urlId The ID of the URL to set to
	 *   crawled.
	 * \param lockTime Constant reference to a
	 *   string containing the time at which the
	 *   current lock by the thread for this URL
	 *   will end (or has ended).
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, i.e. the given
	 *   URL ID is zero, if no lock time has been
	 *   specified, i.e. it references an empty
	 *   string, or if the prepared SQL statement for
	 *   setting a URL to crawled is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while setting the URL to
	 *   crawled.
	 */
	void Database::setUrlFinishedIfOk(std::uint64_t urlId, const std::string& lockTime) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Crawler::Database::setUrlFinishedIfOk():"
					" No URL has been specified"
			);
		}

		if(lockTime.empty()) {
			throw Exception(
					"Crawler::Database::setUrlFinishedIfOk():"
					" URL lock is missing"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.setUrlFinishedIfOk == 0) {
			throw Exception(
					"Crawler::Database::setUrlFinishedIfOk():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.setUrlFinishedIfOk)};

		// set URL as crawled
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);
			sqlStatement.setString(sqlArg2, lockTime);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::setUrlFinishedIfOk", e);
		}
	}

	/*
	 * CRAWLING
	 */

	//! Saves crawled content to the database.
	/*!
	 * \param urlId The ID of the URL that has
	 *   been crawled.
	 * \param response The HTTP status code that
	 *   has been received together with the
	 *   content, e.g. 200 for @c OK.
	 * \param type Constant reference to a string
	 *   containing the description of the content
	 *   type that has been received together with
	 *   the content, e.g. @c text/html.
	 * \param content Constant reference to a
	 *   string containing the crawled content to
	 *   be saved to the database.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, i.e. the given
	 *   URL ID is zero, or if the prepared SQL
	 *   statement for saving crawled content to the
	 *   database is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while saving the crawled
	 *   content to the database.
	 */
	void Database::saveContent(
			std::uint64_t urlId,
			std::uint32_t response,
			const std::string& type,
			const std::string& content
	) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Crawler::Database::saveContent():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.saveContent == 0) {
			throw Exception(
					"Crawler::Database::saveContent():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.saveContent)};

		// save content to database
		try {
			// execute SQL query if possible
			if(content.size() <= this->getMaxAllowedPacketSize()) {
				sqlStatement.setUInt64(sqlArg1, urlId);
				sqlStatement.setUInt(sqlArg2, response);
				sqlStatement.setString(sqlArg3, type);
				sqlStatement.setString(sqlArg4, content);

				Database::sqlExecute(sqlStatement);
			}
			else {
				// show warning about content size
				bool adjustServerSettings{false};

				std::ostringstream logStrStr;

				logStrStr.imbue(Helper::CommaLocale::locale());

				logStrStr	<< "WARNING: Some content could not be saved to the database, because its size ("
							<< content.size()
							<< " bytes) exceeds the ";

				if(content.size() > maxContentSize) {
					logStrStr << "MySQL maximum of " << maxContentSizeString << ".";
				}
				else {
					logStrStr	<< "current MySQL server maximum of "
								<< this->getMaxAllowedPacketSize() << " bytes.";

					adjustServerSettings = true;
				}

				this->log(this->getLoggingMin(), logStrStr.str());

				if(adjustServerSettings) {
					this->log(
							this->getLoggingMin(),
							"Adjust the server's 'max_allowed_packet' setting accordingly."
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::saveContent", e);
		}
	}

	//! Saves archived content to the database.
	/*!
	 * \param urlId The ID of the URL whose
	 *   archived version has been crawled.
	 * \param timeStamp The time stamp of the
	 *   archived content, i.e. when it has been
	 *   archived by the crawled archive.
	 * \param response The HTTP status code that
	 *   has been received together with the
	 *   content, e.g. 200 for @c OK.
	 * \param type Constant reference to a string
	 *   containing the description of the content
	 *   type that has been received together with
	 *   the content, e.g. @c text/html.
	 * \param content Constant reference to a
	 *   string containing the crawled content to
	 *   be saved to the database.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, i.e. the given
	 *   URL ID is zero, or if the prepared SQL
	 *   statement for saving archived content to the
	 *   database is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while saving the archived
	 *   content to the database.
	 */
	void Database::saveArchivedContent(
			std::uint64_t urlId,
			const std::string& timeStamp,
			std::uint32_t response,
			const std::string& type,
			const std::string& content
	) {
		// check arguments
		if(urlId == 0) {
			throw Exception(
					"Crawler::Database::saveArchivedContent():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.saveArchivedContent == 0) {
			throw Exception(
					"Crawler::Database::saveArchivedContent():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.saveArchivedContent)};

		try {
			// save archived content to database if possible
			if(content.size() <= this->getMaxAllowedPacketSize()) {
				// execute SQL query
				sqlStatement.setUInt64(sqlArg1, urlId);
				sqlStatement.setString(sqlArg2, timeStamp);
				sqlStatement.setUInt(sqlArg3, response);
				sqlStatement.setString(sqlArg4, type);
				sqlStatement.setString(sqlArg5, content);

				Database::sqlExecute(sqlStatement);
			}
			else {
				// show warning about content size
				bool adjustServerSettings{false};
				std::ostringstream logStrStr;

				logStrStr.imbue(Helper::CommaLocale::locale());

				logStrStr	<< "WARNING: Some content could not be saved to the database, because its size ("
							<< content.size()
							<< " bytes) exceeds the ";

				if(content.size() > maxContentSize) {
					logStrStr << "MySQL maximum of " << maxContentSizeString << ".";
				}
				else {
					logStrStr	<< "current MySQL server maximum of "
								<< this->getMaxAllowedPacketSize() << " bytes.";

					adjustServerSettings = true;
				}

				this->log(this->getLoggingMin(), logStrStr.str());

				if(adjustServerSettings) {
					this->log(
							this->getLoggingMin(),
							"Adjust the server's 'max_allowed_packet' setting accordingly."
					);
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::saveArchivedContent", e);
		}
	}

	//! Checks whether archived content for a URL with a specific timestamp already exists in the database.
	/*!
	 * \param urlId The ID of the URL whose
	 *   archived version has been crawled.
	 * \param timeStamp The time stamp of the
	 *   archived content, i.e. when it has been
	 *   archived by the crawled archive.
	 *
	 * \returns True, if archived content for the
	 *   specified URL with the given timestamp
	 *   already exists in the database. False, if
	 *   no such content has yet been saved to the
	 *   database.
	 *
	 * \throws Module::Crawler::Database::Exception
	 *   if no URL has been specified, i.e. the given
	 *   URL ID is zero, or if the prepared SQL
	 *   statement for checking for archived content
	 *   in the database is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking for archived
	 *   content in the database.
	 */

	// chck whether archived content with specific timestamp exists, throws Database::Exception
	bool Database::isArchivedContentExists(std::uint64_t urlId, const std::string& timeStamp) {
		bool result{false};

		// check argument
		if(urlId == 0) {
			throw Exception(
					"Crawler::Database::isArchivedContentExists():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.isArchivedContentExists == 0) {
			throw Exception(
					"Crawler::Database::isArchivedContentExists():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.isArchivedContentExists)};

		// get next URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);
			sqlStatement.setString(sqlArg2, timeStamp);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getBoolean("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::isArchivedContentExists", e);
		}

		return result;
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// generate a SQL query for adding a specific number of URLs, throws Database::Exception
	std::string Database::queryAddUrlsIfNotExist(std::size_t numberOfUrls, const std::string& hashQuery) {
		// check argument
		if(numberOfUrls == 0) {
			throw Exception(
					"Crawler::Database::queryUpdateOrAddUrls():"
					" No number of URLs has been specified"
			);
		}

		// generate INSERT INTO ... VALUES clause
		std::string sqlQueryString{
			"INSERT IGNORE INTO `"
		};

		sqlQueryString += this->urlListTable;
		sqlQueryString += "`(id, url, manual, hash) VALUES ";

		// generate placeholders for VALUES arguments
		for(std::uint32_t n{}; n < numberOfUrls; ++n) {
			sqlQueryString +=	"(" // begin of VALUES arguments
									" ("
										"SELECT id"
										" FROM"
										" ("
											"SELECT id, url"
											" FROM `";
			sqlQueryString += this->urlListTable;
			sqlQueryString += 				"`"
											" AS `";
			sqlQueryString += urlListTableAlias;
			sqlQueryString += std::to_string(n + 1);
			sqlQueryString +=				"`"
											" WHERE hash = ";
			sqlQueryString += hashQuery;
			sqlQueryString +=			" ) AS tmp2"
										" WHERE url = ?"
										" LIMIT 1"
									" ),"
									"?, "
									"?, ";
			sqlQueryString += hashQuery;
			sqlQueryString += 		"), ";
		}

		// remove last comma and space
		sqlQueryString.pop_back();
		sqlQueryString.pop_back();

		// return query
		return sqlQueryString;
	}

	// get all URLs from the URL list, throws Database::Exception
	std::queue<std::string> Database::getUrls() {
		std::queue<std::string> result;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getUrls == 0) {
			throw Exception(
					"Crawler::Database::getUrls():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getUrls)};

		// get URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet) {
				while(sqlResultSet->next()) {
					result.emplace(sqlResultSet->getString("url"));
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::getUrls", e);
		}

		return result;
	}

	// remove duplicates of the specified URL from the URL list (NOT its first occurence)
	//	return the number of deleted duplicates, throws Database::Exception
	std::uint32_t Database::removeDuplicates(const std::string& url) {
		int result{};

		// get ID of first occurence
		const auto first{this->getUrlId(url)};

		// check argument
		if(url.empty()) {
			throw Exception(
					"Crawler::Database::removeDuplicates():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.removeDuplicates == 0) {
			throw Exception(
					"Crawler::Database::removeDuplicates():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.removeDuplicates)};

		// remove URLs from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, first);
			sqlStatement.setString(sqlArg2, url);
			sqlStatement.setString(sqlArg3, url);

			result = Database::sqlExecuteUpdate(sqlStatement);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Crawler::Database::removeDuplicates", e);
		}

		if(result > 0) {
			return static_cast<std::uint32_t>(result);
		}

		return 0;
	}

} /* namespace crawlservpp::Module::Crawler */
