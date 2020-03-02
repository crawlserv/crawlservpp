/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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
 * This class provides database functionality for a crawler thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module::Crawler {

	// constructor: initialize values
	Database::Database(Module::Database& dbThread)
						: Wrapper::Database(dbThread),
						  crawlingTableAlias("a"),
						  urlListTableAlias("b"),
						  recrawl(false),
						  urlCaseSensitive(true),
						  urlDebug(false),
						  urlStartupCheck(true),
						  ps(_ps()) {}

	// destructor stub
	Database::~Database() {}

	// enable or disable recrawling
	void Database::setRecrawl(bool isRecrawl) {
		this->recrawl = isRecrawl;
	}

	// enable or disable case-sensitive URLs for current URL list
	void Database::setUrlCaseSensitive(bool isUrlCaseSensitive) {
		this->urlCaseSensitive = isUrlCaseSensitive;

		// update case sensitivity in database
		this->setUrlListCaseSensitive(this->getOptions().urlListId, this->urlCaseSensitive);
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
		const unsigned short verbose = this->getLoggingVerbose();

		// create table names
		this->urlListTable = "crawlserv_" + this->getOptions().websiteNamespace + "_" + this->getOptions().urlListNamespace;
		this->crawlingTable = this->urlListTable + "_crawling";

		const std::string crawledTable(this->urlListTable + "_crawled");

		// create SQL strings for URL hashing
		std::string hashQuery("CRC32( ");

		if(this->urlCaseSensitive)
			hashQuery += "?";
		else
			hashQuery += "LOWER( ? )";

		hashQuery += " )";

		std::string urlHash("CRC32( ");

		if(this->urlCaseSensitive)
			urlHash += "url";
		else
			urlHash += "LOWER( url )";

		urlHash.push_back(')');

		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(unsigned short));

		if(!(this->ps.getUrlId)) {
			this->log(verbose, "prepares getUrlId()...");

			this->ps.getUrlId = this->addPreparedStatement(
					"SELECT id"
					" FROM "
					" ("
						"SELECT `" + this->urlListTableAlias + "1`.id"
						" AS id,"
						" `" + this->urlListTableAlias + "1`.url"
						" AS url"
						" FROM `" + this->urlListTable + "`"
						" AS `" + this->urlListTableAlias + "1`"
						" LEFT OUTER JOIN `" + this->crawlingTable + "`"
						" AS `" + this->crawlingTableAlias + "1`"
						" ON `" + this->urlListTableAlias + "1`.id"
						" = `" + this->crawlingTableAlias + "1`.url"
						" WHERE `" + this->urlListTableAlias + "1`.hash = " + hashQuery +
						" ORDER BY id"
					" ) AS tmp"
					" WHERE url = ?"
					" LIMIT 1"
			);
		}

		if(!(this->ps.getNextUrl)) {
			this->log(verbose, "prepares getNextUrl()...");

			std::string sqlQueryString(
					"SELECT `" + this->urlListTableAlias + "1`.id"
					" AS id,"
					" `" + this->urlListTableAlias + "1`.url"
					" AS url"
					" FROM `" + this->urlListTable + "`"
					" AS `" + this->urlListTableAlias + "1`"
					" LEFT OUTER JOIN `" + this->crawlingTable + "`"
					" AS `" + this->crawlingTableAlias + "1`"
					" ON `" + this->urlListTableAlias + "1`.id"
					" = `" + this->crawlingTableAlias + "1`.url"
					" WHERE `" + this->urlListTableAlias + "1`.id > ?"
					" AND manual = FALSE"
			);

			if(!(this->recrawl))
				sqlQueryString +=
								" AND"
								" ("
									" `" + this->crawlingTableAlias + "1`.success IS NULL"
									" OR `" + this->crawlingTableAlias + "1`.success = FALSE"
								")";

			sqlQueryString +=	" AND"
								" ("
									" `" + this->crawlingTableAlias + "1`.locktime IS NULL"
									" OR `" + this->crawlingTableAlias + "1`.locktime < NOW()"
								" )"
								" ORDER BY `" + this->urlListTableAlias + "1`.id"
								" LIMIT 1";

			this->ps.getNextUrl = this->addPreparedStatement(sqlQueryString);
		}

		if(!(this->ps.addUrlIfNotExists)) {
			this->log(verbose, "prepares addUrlIfNotExists()...");

			this->ps.addUrlIfNotExists = this->addPreparedStatement(
					"INSERT IGNORE INTO `" + this->urlListTable + "`(id, url, manual, hash)"
						"VALUES ("
							" ("
								"SELECT id"
								" FROM"
								" ("
									"SELECT id, url"
									" FROM `" + this->urlListTable + "`"
									" AS `" + this->urlListTableAlias + "1`"
									" WHERE hash = " + hashQuery +
								" ) AS tmp2"
								" WHERE url = ?"
								" LIMIT 1"
							" ),"
							"?, "
							"?, " +
							hashQuery +
						")"
			);
		}

		if(!(this->ps.add10UrlsIfNotExist)) {
			this->log(verbose, "prepares addUrlsIfNotExist() [1/3]...");

			this->ps.add10UrlsIfNotExist = this->addPreparedStatement(this->queryAddUrlsIfNotExist(10, hashQuery));
		}

		if(!(this->ps.add100UrlsIfNotExist)) {
			this->log(verbose, "prepares addUrlsIfNotExist() [2/3]...");

			this->ps.add100UrlsIfNotExist = this->addPreparedStatement(this->queryAddUrlsIfNotExist(100, hashQuery));
		}

		if(!(this->ps.add1000UrlsIfNotExist)) {
			this->log(verbose, "prepares addUrlsIfNotExist() [3/3]...");

			this->ps.add1000UrlsIfNotExist = this->addPreparedStatement(this->queryAddUrlsIfNotExist(1000, hashQuery));
		}

		if(!(this->ps.getUrlPosition)) {
			this->log(verbose, "prepares getUrlPosition()...");

			this->ps.getUrlPosition = this->addPreparedStatement(
					"SELECT COUNT(*)"
					" AS result"
					" FROM `" + this->urlListTable + "`"
					" WHERE id < ?"
			);
		}

		if(!(this->ps.getNumberOfUrls)) {
			this->log(verbose, "prepares getNumberOfUrls()...");

			this->ps.getNumberOfUrls = this->addPreparedStatement(
					"SELECT COUNT(*)"
					" AS result"
					" FROM `" + this->urlListTable + "`"
			);
		}

		if(!(this->ps.getUrlLockTime)) {
			this->log(verbose, "prepares getUrlLock()...");

			this->ps.getUrlLockTime = this->addPreparedStatement(
					"SELECT locktime"
					" FROM `" + this->crawlingTable + "`"
					" WHERE url = ?"
					" LIMIT 1"
			);
		}

		if(!(this->ps.isUrlCrawled)) {
			this->log(verbose, "prepares isUrlCrawled()...");

			this->ps.isUrlCrawled = this->addPreparedStatement(
					"SELECT success"
					" FROM `" + this->crawlingTable + "`"
					" WHERE url = ?"
					" LIMIT 1"
			);
		}

		if(!(this->ps.renewUrlLockIfOk)) {
			this->log(verbose, "prepares lockUrlIfOk() [1/2]...");

			this->ps.renewUrlLockIfOk = this->addPreparedStatement(
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
					" LIMIT 1"
			);
		}

		if(!(this->ps.addUrlLockIfOk)) {
			this->log(verbose, "prepares lockUrlIfOk() [2/2]...");

			this->ps.addUrlLockIfOk = this->addPreparedStatement(
					"INSERT INTO `" + this->crawlingTable + "`(id, url, locktime)"
					" VALUES"
					" ("
						" ("
							"SELECT id"
							" FROM `" + this->crawlingTable + "`"
							" AS " + this->crawlingTableAlias + "1"
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
						")"
			);
		}

		if(!(this->ps.unLockUrlIfOk)) {
			this->log(verbose, "prepares unLockUrlIfOk()...");

			this->ps.unLockUrlIfOk = this->addPreparedStatement(
					"UPDATE `" + this->crawlingTable + "`"
					" SET locktime = NULL"
					" WHERE url = ?"
					" AND"
					" ("
						" locktime IS NULL"
						" OR locktime <= ?"
						" OR locktime < NOW()"
					")"
					" LIMIT 1"
			);
		}

		if(!(this->ps.setUrlFinishedIfOk)) {
			this->log(verbose, "prepares setUrlFinishedIfOk()...");

			this->ps.setUrlFinishedIfOk = this->addPreparedStatement(
					"UPDATE `" + this->crawlingTable + "`"
					" SET success = TRUE, locktime = NULL"
					" WHERE url = ?"
					" AND "
					" ("
						" locktime <= ?"
						" OR locktime IS NULL"
						" OR locktime < NOW()"
					")"
					" LIMIT 1"
			);
		}

		if(!(this->ps.saveContent)) {
			this->log(verbose, "prepares saveContent()...");

			this->ps.saveContent = this->addPreparedStatement(
					"INSERT INTO `" + crawledTable + "`(url, response, type, content)"
					" VALUES (?, ?, ?, ?)"
			);
		}

		if(!(this->ps.saveArchivedContent)) {
			this->log(verbose, "prepares saveArchivedContent()...");

			this->ps.saveArchivedContent = this->addPreparedStatement(
					"INSERT INTO `" + crawledTable + "`(url, crawltime, archived, response, type, content)"
					" VALUES (?, ?, TRUE, ?, ?, ?)"
			);
		}

		if(!(this->ps.isArchivedContentExists)) {
			this->log(verbose, "prepares isArchivedContentExists()...");

			this->ps.isArchivedContentExists = this->addPreparedStatement(
					"SELECT EXISTS"
					" ("
						"SELECT *"
						" FROM `" + crawledTable + "`"
						" WHERE url = ?"
						" AND crawltime = ?"
					" )"
					" AS result"
			);
		}

		if(!(this->ps.urlDuplicationCheck) && (this->urlStartupCheck || this->urlDebug)) {
			this->log(verbose, "prepares urlDuplicationCheck()...");

			std::string groupBy;

			if(this->urlCaseSensitive)
				groupBy = "url";
			else
				groupBy = "LOWER(url)";

			this->ps.urlDuplicationCheck = this->addPreparedStatement(
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
					") > 1"
			);
		}

		if(!(this->ps.urlHashCheck)) {
			this->log(verbose, "prepares urlHashCheck() [1/2]...");

			this->ps.urlHashCheck = this->addPreparedStatement(
					"SELECT EXISTS"
					" ("
						" SELECT *"
						" FROM `" + this->urlListTable + "`"
						" WHERE hash <> " + urlHash + ""
					" )"
					" AS result"
			);
		}

		if(!(this->ps.urlHashCorrect)) {
			this->log(verbose, "prepares urlHashCheck() [1/2]...");

			this->ps.urlHashCorrect = this->addPreparedStatement(
					"UPDATE `" + this->urlListTable + "`"
					" SET hash = " + urlHash
			);
		}

		if(!(this->ps.urlEmptyCheck) && this->urlStartupCheck) {
			this->log(verbose, "prepares urlHashCheck()...");

			this->ps.urlEmptyCheck = this->addPreparedStatement(
					"SELECT id"
					" FROM `" + this->urlListTable + "`"
					" WHERE url = ''"
					" LIMIT 1"
			);
		}

		if(!(this->ps.getUrls) && this->urlStartupCheck) {
			this->log(verbose, "prepares getUrls()...");

			this->ps.getUrls = this->addPreparedStatement(
					"SELECT url"
					" FROM `" + this->urlListTable + "`"
			);
		}

		if(!(this->ps.removeDuplicates) && (this->urlStartupCheck || this->urlDebug)) {
			this->log(verbose, "prepares removeDuplicates()...");

			std::string urlComparison;

			if(this->urlCaseSensitive)
				urlComparison = "url LIKE ?";
			else
				urlComparison = "LOWER(url) LIKE LOWER(?)";

			this->ps.removeDuplicates = this->addPreparedStatement(
					" DELETE"
					"  FROM `" + this->urlListTable + "`"
					"  WHERE id > ?" // ignore the first occurence
					"  AND " + urlComparison
			);
		}
	}

	// get the ID of an URL (uses hash check for first checking the probable existence of the URL),
	//  throws Database::Exception
	unsigned long Database::getUrlId(const std::string& url) {
		// check argument
		if(url.empty())
			throw Exception("Crawler::Database::getUrlId(): No URL specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlId))
			throw Exception("Missing prepared SQL statement for Crawler::Database::getUrlId(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getUrlId));

		// get ID of URL from database
		try {
			// execute SQL query for getting URL
			sqlStatement.setString(1, url);
			sqlStatement.setString(2, url);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())  {
				return sqlResultSet->getUInt64("id");
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlId", e); }

		return 0;
	}

	// get the next URL to crawl from database, return ID and URL or empty IdString if all URLs have been parsed,
	//  throws Database::Exception
	Database::IdString Database::getNextUrl(unsigned long currentUrlId) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getNextUrl))
			throw Exception("Missing prepared SQL statement for Crawler::Database::getNextUrl(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getNextUrl));

		// get next URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, currentUrlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				return IdString(
						sqlResultSet->getUInt64("id"),
						sqlResultSet->getString("url")
				);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getNextUrl", e); }

		return IdString();
	}

	// add URL to database if it does not exist already, return whether it was added,
	//  throws Database::Exception
	bool Database::addUrlIfNotExists(const std::string& urlString, bool manual) {
		// check argument
		if(urlString.empty())
			throw Exception("Crawler::Database::addUrlIfNotExists(): No URL specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.addUrlIfNotExists))
			throw Exception("Missing prepared SQL statement for Crawler::Database::addUrlIfNotExists(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.addUrlIfNotExists));

		// add URL to database and get resulting ID
		try {
			// execute SQL query
			sqlStatement.setString(1, urlString);
			sqlStatement.setString(2, urlString);
			sqlStatement.setString(3, urlString);
			sqlStatement.setBoolean(4, manual);
			sqlStatement.setString(5, urlString);

			return Database::sqlExecuteUpdate(sqlStatement) > 0;
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::addUrlIfNotExists", e); }

		return false; // not needed
	}

	// add URLs that do not exist already to the database, return the number of added URLs,
	//  throws Database::Exception
	unsigned long Database::addUrlsIfNotExist(std::queue<std::string>& urls, bool manual) {
		unsigned long result = 0;

		// check argument
		if(urls.empty())
			return 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(
				!(this->ps.addUrlIfNotExists)
				|| !(this->ps.add10UrlsIfNotExist)
				|| !(this->ps.add100UrlsIfNotExist)
				|| !(this->ps.add1000UrlsIfNotExist)
		)
			throw Exception("Missing prepared SQL statement for Crawler::Database::addUrlsIfNotExist(...)");

		// get prepared SQL statements
		sql::PreparedStatement& sqlStatement1 = this->getPreparedStatement(this->ps.addUrlIfNotExists);
		sql::PreparedStatement& sqlStatement10 = this->getPreparedStatement(this->ps.add10UrlsIfNotExist);
		sql::PreparedStatement& sqlStatement100 = this->getPreparedStatement(this->ps.add100UrlsIfNotExist);
		sql::PreparedStatement& sqlStatement1000 = this->getPreparedStatement(this->ps.add1000UrlsIfNotExist);

		try {
			// add 1.000 URLs at once
			while(urls.size() >= 1000) {
				for(unsigned long n = 0; n < 1000; ++n) {
					sqlStatement1000.setString((n * 5) + 1, urls.front());
					sqlStatement1000.setString((n * 5) + 2, urls.front());
					sqlStatement1000.setString((n * 5) + 3, urls.front());
					sqlStatement1000.setBoolean((n * 5) + 4, manual);
					sqlStatement1000.setString((n * 5) + 5, urls.front());

					urls.pop();
				}

				int added = Database::sqlExecuteUpdate(sqlStatement1000);

				if(added > 0)
					result += added;
			}

			// add 100 URLs at once
			while(urls.size() >= 100) {
				for(unsigned long n = 0; n < 100; ++n) {
					sqlStatement100.setString((n * 5) + 1, urls.front());
					sqlStatement100.setString((n * 5) + 2, urls.front());
					sqlStatement100.setString((n * 5) + 3, urls.front());
					sqlStatement100.setBoolean((n * 5) + 4, manual);
					sqlStatement100.setString((n * 5) + 5, urls.front());

					urls.pop();
				}

				int added = Database::sqlExecuteUpdate(sqlStatement100);

				if(added > 0)
					result += added;
			}

			// add 10 URLs at once
			while(urls.size() >= 10) {
				for(unsigned long n = 0; n < 10; ++n) {
					sqlStatement10.setString((n * 5) + 1, urls.front());
					sqlStatement10.setString((n * 5) + 2, urls.front());
					sqlStatement10.setString((n * 5) + 3, urls.front());
					sqlStatement10.setBoolean((n * 5) + 4, manual);
					sqlStatement10.setString((n * 5) + 5, urls.front());

					urls.pop();
				}

				int added = Database::sqlExecuteUpdate(sqlStatement10);

				if(added > 0)
					result += added;
			}

			// add remaining URLs
			while(urls.size()) {
				sqlStatement1.setString(1, urls.front());
				sqlStatement1.setString(2, urls.front());
				sqlStatement1.setString(3, urls.front());
				sqlStatement1.setBoolean(4, manual);
				sqlStatement1.setString(5, urls.front());

				urls.pop();

				if(Database::sqlExecuteUpdate(sqlStatement1) > 0)
					++result;
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::addUrlsIfNotExist", e); }

		return result;
	}

	// add URL to database and return ID of newly added URL
	unsigned long Database::addUrlGetId(const std::string& urlString, bool manual) {
		unsigned long result = 0;

		// add URL to database
		this->addUrlIfNotExists(urlString, manual);

		// get resulting ID
		try {
			// get result
			result = this->getLastInsertedId();
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::addUrlGetId", e); }

		return result;
	}

	// get the position of the URL in the URL list, throws Database::Exception
	unsigned long Database::getUrlPosition(unsigned long urlId) {
		unsigned long result = 0;

		// check argument
		if(!urlId)
			return 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlPosition))
			throw Exception("Missing prepared SQL statement for Crawler::Database::getUrlPosition()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getUrlPosition));

		// get URL position of URL from database
		try {
			// disable locking as data consistency is not needed for calculating the approx. progress
			this->beginNoLock();

			// execute SQL query
			sqlStatement.setUInt64(1, urlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// re-enable locking
			this->endNoLock();

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlPosition", e); }

		return result;
	}

	// get the number of URLs in the URL list, throws Database::Exception
	unsigned long Database::getNumberOfUrls() {
		unsigned long result = 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getNumberOfUrls))
			throw Exception("Missing prepared SQL statement for Crawler::Database::getNumberOfUrls()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getNumberOfUrls));

		// get number of URLs from database
		try {
			// disable locking as data consistency is not needed for calculating the approx. progress
			this->beginNoLock();

			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// re-enable locking
			this->endNoLock();

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getUInt64("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getNumberOfUrls", e); }

		return result;
	}

	// check the URL list for duplicates, throw Exception if duplicates are found and removed, throws Database::Exception
	void Database::urlDuplicationCheck() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.urlDuplicationCheck))
			throw Exception("Missing prepared SQL statement for Crawler::Database::urlDuplicationCheck()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.urlDuplicationCheck));

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet) {
				const unsigned long numDuplicates = sqlResultSet->rowsCount();

				if(numDuplicates) {
					std::queue<std::string> duplicates;

					while(sqlResultSet->next())
						duplicates.push(sqlResultSet->getString("url"));

					while(!duplicates.empty()) {
						this->removeDuplicates(duplicates.front());

						duplicates.pop();
					}

					// throw exception on duplicates
					throw Exception(
							"Crawler::Database::urlDuplicationCheck(): removed "
							+ std::to_string(numDuplicates)
							+ " duplicate URL(s) in `"
							+ this->urlListTable + "`"
					);
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::urlDuplicationCheck", e); }
	}

	// check the hash values in the URL list, correct hash mismatches, throws Database::Exception
	void Database::urlHashCheck() {
		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.urlHashCheck))
			throw Exception("Missing prepared SQL statement for Crawler::Database::urlHashCheck()");

		if(!(this->ps.urlHashCorrect))
			throw Exception("Missing prepared SQL statement for Crawler::Database::urlHashCheck()");

		// get prepared SQL statement
		sql::PreparedStatement& checkStatement = this->getPreparedStatement(this->ps.urlHashCheck);

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(checkStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next() && sqlResultSet->getBoolean("result")) {
				// correct hash values
				sql::PreparedStatement& correctStatement = this->getPreparedStatement(this->ps.urlHashCorrect);

				int updated = Database::sqlExecuteUpdate(correctStatement);

				if(updated > 0) {
					std::ostringstream logStrStr;

					logStrStr << "corrected hash ";

					switch(updated) {
					case 1:
						logStrStr << "value for one URL.";

						break;

					default:
						logStrStr.imbue(std::locale(""));

						logStrStr << "values for " << updated << " URLs.";
					}

					this->log(this->getLoggingMin(), logStrStr.str());
				}
			}

		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::urlHashCheck", e); }
	}

	// check for empty URLs in the URL list, throws Exception if an empty URL exists,
	//  throws Database::Exception
	void Database::urlEmptyCheck() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.urlEmptyCheck))
			throw Exception("Missing prepared SQL statement for Crawler::Database::urlEmptyCheck()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.urlEmptyCheck));

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				throw Exception(
						"Crawler::Database::urlEmptyCheck():"
						" Empty URL in `"
						+ this->urlListTable
						+ "`"
				);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::urlHashCheck", e); }
	}

	// check for URLs containing invalid UTF-8 characters in the URL list, throws Exception if invalid URL exists,
	//  throws Database::Exception
	void Database::urlUtf8Check() {
		// get all URLs from URL list
		std::queue<std::string> urls(this->getUrls());

		// check URLs for invalid UTF-8
		while(!urls.empty()) {
			std::string err;

			if(!Helper::Utf8::isValidUtf8(urls.front(), err)) {
				if(err.empty())
					throw Exception(
							"Crawler::Database::urlUtf8Check(): URL containing invalid UTF-8 in `"
							+ this->urlListTable
							+ "` ["
							+ urls.front()
							+ "]"
					);
				else
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

	// get the URL lock end time of a specific URL from database, throws Database::Exception
	std::string Database::getUrlLockTime(unsigned long urlId) {
		// check argument
		if(!urlId)
			return std::string();

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlLockTime))
			throw Exception("Missing prepared SQL statement for Crawler::Database::getUrlLockTime(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getUrlLockTime));

		// get URL lock end time from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				return sqlResultSet->getString("locktime");
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlLockTime", e); }

		return std::string();
	}

	// get the URL lock ID for a specific URL from the database, return whether the URL has been crawled),
	//  throws Database::Exception
	bool Database::isUrlCrawled(unsigned long urlId) {
		// check arguments
		if(!urlId)
			throw Exception("Crawler::Database::isUrlCrawled(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.isUrlCrawled))
			throw Exception("Missing prepared SQL statement for Module::Crawler::Database::isUrlCrawled(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.isUrlCrawled));

		// get lock ID from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				return sqlResultSet->getBoolean("success");
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isUrlCrawled", e); }

		return false;
	}

	// lock a URL in the database if it is lockable (or is still locked) or return an empty string if locking was unsuccessful,
	//  throws Database::Exception
	std::string Database::lockUrlIfOk(
			unsigned long urlId,
			const std::string& lockTime,
			unsigned long lockTimeout
	) {
		// check argument
		if(!urlId)
			throw Exception("Crawler::Database::lockUrlIfOk(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.addUrlLockIfOk) || !(this->ps.renewUrlLockIfOk))
			throw Exception("Missing prepared SQL statement for Module::Crawler::Database::lockUrlIfOk(...)");

		if(lockTime.empty()) {
			// get prepared SQL statement for locking the URL
			sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.addUrlLockIfOk));

			// add URL lock to database
			try {
				// execute SQL query
				sqlStatement.setUInt64(1, urlId);
				sqlStatement.setUInt64(2, urlId);
				sqlStatement.setUInt64(3, lockTimeout);

				if(Database::sqlExecuteUpdate(sqlStatement) < 1)
					return std::string(); // locking failed when no entries have been updated
			}
			catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::lockUrlIfOk", e); }
		}
		else {
			// get prepared SQL statement for renewing the URL lock
			sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.renewUrlLockIfOk));

			// lock URL in database
			try {
				// execute SQL query
				sqlStatement.setUInt64(1, lockTimeout);
				sqlStatement.setString(2, lockTime);
				sqlStatement.setUInt64(3, urlId);
				sqlStatement.setString(4, lockTime);

				if(Database::sqlExecuteUpdate(sqlStatement) < 1)
					return std::string(); // locking failed when no entries have been updated
			}
			catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::lockUrlIfOk", e); }
		}

		// get new expiration time of URL lock
		return this->getUrlLockTime(urlId);
	}

	// unlock a URL in the database, throws Database::Exception
	void Database::unLockUrlIfOk(unsigned long urlId, const std::string& lockTime) {
		// check argument
		if(!urlId)
			throw Exception("Crawler::Database::unLockUrlIfOk(): No URL lock ID specified");

		if(lockTime.empty())
			throw Exception("Crawler::Database::unLockUrlIfOk(): URL lock is missing");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.unLockUrlIfOk))
			throw Exception("Missing prepared SQL statement for Crawler::Database::unLockUrlIfOk(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.unLockUrlIfOk));

		// unlock URL in database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setString(2, lockTime);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::unLockUrlIfOk", e); }
	}

	// set URL as crawled in the database, throws Database::Exception
	void Database::setUrlFinishedIfOk(unsigned long urlId, const std::string& lockTime) {
		// check argument
		if(!urlId)
			throw Exception("Crawler::Database::setUrlFinishedIfOk(): No crawling ID specified");

		if(lockTime.empty())
			throw Exception("Crawler::Database::setUrlFinishedIfOk(): URL lock is missing");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setUrlFinishedIfOk))
			throw Exception("Missing prepared SQL statement for Crawler::Database::setUrlFinishedIfOk(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.setUrlFinishedIfOk));

		// set URL as crawled
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setString(2, lockTime);

			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::setUrlFinishedIfOk", e); }
	}

	// save content to database, throws Database::Exception
	void Database::saveContent(
			unsigned long urlId,
			unsigned int response,
			const std::string& type,
			const std::string& content
	) {
		// check argument
		if(!urlId)
			throw Exception("Crawler::Database::saveContent(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.saveContent))
			throw Exception("Missing prepared SQL statement for Crawler::Database::saveContent(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.saveContent));

		// save content to database
		try {
			// execute SQL query if possible
			if(content.size() <= this->getMaxAllowedPacketSize()) {
				sqlStatement.setUInt64(1, urlId);
				sqlStatement.setUInt(2, response);
				sqlStatement.setString(3, type);
				sqlStatement.setString(4, content);

				Database::sqlExecute(sqlStatement);
			}
			else {
				// show warning about content size
				bool adjustServerSettings = false;
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr	<< "WARNING: Some content could not be saved to the database, because its size ("
							<< content.size()
							<< " bytes) exceeds the ";

				if(content.size() > 1073741824)
					logStrStr << "MySQL maximum of 1 GiB.";
				else {
					logStrStr	<< "current MySQL server maximum of "
								<< this->getMaxAllowedPacketSize() << " bytes.";

					adjustServerSettings = true;
				}

				this->log(this->getLoggingMin(), logStrStr.str());

				if(adjustServerSettings)
					this->log(this->getLoggingMin(), "Adjust the server's \'max_allowed_packet\' setting accordingly.");
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::saveContent", e); }
	}

	// save archived content to database, throws Database::Exception
	void Database::saveArchivedContent(
			unsigned long urlId,
			const std::string& timeStamp,
			unsigned int response,
			const std::string& type,
			const std::string& content
	) {
		// check arguments
		if(!urlId)
			throw Exception("Crawler::Database::saveArchivedContent(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.saveArchivedContent))
			throw Exception("Missing prepared SQL statement for Crawler::Database::saveArchivedContent(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.saveArchivedContent));

		try {
			// save archived content to database if possible
			if(content.size() <= this->getMaxAllowedPacketSize()) {
				// execute SQL query
				sqlStatement.setUInt64(1, urlId);
				sqlStatement.setString(2, timeStamp);
				sqlStatement.setUInt(3, response);
				sqlStatement.setString(4, type);
				sqlStatement.setString(5, content);

				Database::sqlExecute(sqlStatement);
			}
			else {
				// show warning about content size
				bool adjustServerSettings = false;
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr	<< "WARNING: Some content could not be saved to the database, because its size ("
							<< content.size()
							<< " bytes) exceeds the ";

				if(content.size() > 1073741824)
					logStrStr << "MySQL maximum of 1 GiB.";
				else {
					logStrStr	<< "current MySQL server maximum of "
								<< this->getMaxAllowedPacketSize() << " bytes.";

					adjustServerSettings = true;
				}

				this->log(this->getLoggingMin(), logStrStr.str());

				if(adjustServerSettings)
					this->log(this->getLoggingMin(), "Adjust the server's \'max_allowed_packet\' setting accordingly.");
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::saveArchivedContent", e); }
	}

	// chck whether archived content with specific timestamp exists, throws Database::Exception
	bool Database::isArchivedContentExists(unsigned long urlId, const std::string& timeStamp) {
		bool result = false;

		// check argument
		if(!urlId)
			throw Exception("Crawler::Database::isArchivedContentExists(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.isArchivedContentExists))
			throw Exception("Missing prepared SQL statement for Crawler::Database::isArchivedContentExists(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.isArchivedContentExists));

		// get next URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setString(2, timeStamp);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isArchivedContentExists", e); }

		return result;
	}

	// generate a SQL query for adding a specific number of URLs, throws Database::Exception
	std::string Database::queryAddUrlsIfNotExist(unsigned int numberOfUrls, const std::string& hashQuery) {
		// check argument
		if(!numberOfUrls)
			throw Exception("Crawler::Database::queryUpdateOrAddUrls(): No number of URLs specified");

		// generate INSERT INTO ... VALUES clause
		std::string sqlQueryString(
				"INSERT IGNORE INTO `" + this->urlListTable + "`(id, url, manual, hash) VALUES "
		);

		// generate placeholders
		for(unsigned int n = 0; n < numberOfUrls; ++n)
			sqlQueryString +=	"(" // begin of VALUES arguments
									" ("
										"SELECT id"
										" FROM"
										" ("
											"SELECT id, url"
											" FROM `" + this->urlListTable + "`"
											" AS `" + this->urlListTableAlias + std::to_string(n + 1) + "`"
											" WHERE hash = " + hashQuery +
										" ) AS tmp2"
										" WHERE url = ?"
										" LIMIT 1"
									" ),"
									"?, "
									"?, " +
									hashQuery +
									"), "; // end of VALUES arguments

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
		if(!(this->ps.getUrls))
			throw Exception("Missing prepared SQL statement for Crawler::Database::getUrls()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getUrls));

		// get URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet)
				while(sqlResultSet->next())
					result.emplace(sqlResultSet->getString("url"));
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrls", e); }

		return result;
	}

	// remove duplicates of the specified URL from the URL list (NOT its first occurence), throws Database::Exception
	void Database::removeDuplicates(const std::string& url) {
		// get ID of first occurence
		const unsigned long first = this->getUrlId(url);

		// check argument
		if(url.empty())
			throw Exception("Crawler::Database::removeDuplicates(): No URL specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.removeDuplicates))
			throw Exception("Missing prepared SQL statement for Crawler::Database::removeDuplicates()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.removeDuplicates));

		// remove URLs from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, first);
			sqlStatement.setString(2, url);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::removeDuplicates", e); }
	}

} /* crawlservpp::Module::Crawler */
