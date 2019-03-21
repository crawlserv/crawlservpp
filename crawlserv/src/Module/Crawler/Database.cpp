/*
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
						  logging(true),
						  verbose(false),
						  urlCaseSensitive(true),
						  urlDebug(false),
						  urlStartupCheck(true),
						  ps(_ps()) {}

	// destructor stub
	Database::~Database() {}

	// convert thread ID to string for logging
	void Database::setId(unsigned long analyzerId) {
		std::ostringstream idStrStr;
		idStrStr << analyzerId;
		this->idString = idStrStr.str();
	}

	// set website namespace
	void Database::setNamespaces(const std::string& website, const std::string& urlList) {
		this->websiteName = website;
		this->urlListName = urlList;
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
		std::string crawledTable(this->urlListTable + "_crawled");

		// create SQL string for URL hashing
		std::string hashQuery("CRC32( ");
		if(this->urlCaseSensitive) hashQuery += "?";
		else hashQuery += "LOWER( ? )";
		hashQuery += " )";

		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(unsigned short));

		if(!(this->ps.getUrlId)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getUrlId()...");

			this->ps.getUrlId = this->addPreparedStatement(
					"SELECT `" + this->urlListTableAlias + "1`.id AS id"
					" FROM `" + this->urlListTable + "`"
					" AS `" + this->urlListTableAlias + "1`"
					" LEFT OUTER JOIN `" + this->crawlingTable + "`"
					" AS `" + this->crawlingTableAlias + "1`"
					" ON `" + this->urlListTableAlias + "1`.id"
					" = `" + this->crawlingTableAlias + "1`.url"
					" WHERE `" + this->urlListTableAlias + "1`.url = ?"
					" LIMIT 1"
			);
		}

		if(!(this->ps.getNextUrl)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getNextUrl()...");

			std::ostringstream sqlQueryStrStr;
			sqlQueryStrStr <<	"SELECT `" << this->urlListTableAlias << "1`.id AS id,"
								" `" << this->urlListTableAlias << "1`.url AS url"
								" FROM `" << this->urlListTable << "`"
								" AS `" << this->urlListTableAlias << "1`"
								" LEFT OUTER JOIN `" << this->crawlingTable << "`"
								" AS `" << this->crawlingTableAlias << "1`"
								" ON `" << this->urlListTableAlias << "1`.id"
								" = `" << this->crawlingTableAlias << "1`.url"
								" WHERE `" << this->urlListTableAlias << "1`.id > ?"
								" AND manual = FALSE";

			if(!(this->recrawl))
				sqlQueryStrStr <<
								" AND"
								" ("
									" `" << this->crawlingTableAlias << "1`.success IS NULL"
									" OR `" << this->crawlingTableAlias << "1`.success = FALSE"
								")";

			sqlQueryStrStr <<	" AND"
								" ("
									" `" << this->crawlingTableAlias << "1`.locktime IS NULL"
									" OR `" << this->crawlingTableAlias << "1`.locktime < NOW()"
								" )"
								" ORDER BY `" << this->urlListTableAlias << "1`.id"
								" LIMIT 1";

			this->ps.getNextUrl = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.addUrlIfNotExists)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares addUrlIfNotExists()...");

			this->ps.addUrlIfNotExists = this->addPreparedStatement(
					"INSERT IGNORE INTO `" + this->urlListTable + "`(id, url, hash, manual)"
						"VALUES ("
							" ("
								"SELECT id FROM"
								" ("
									"SELECT id, url FROM `" + this->urlListTable + "`"
									" AS `" + this->urlListTableAlias + "1`"
									" WHERE hash = " + hashQuery +
								" ) AS tmp2"
								" WHERE url = ?"
								" LIMIT 1"
							" ),"
							"?, " +
							hashQuery + ", "
							"?"
						")"
			);
		}

		if(!(this->ps.add10UrlsIfNotExist)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares addUrlsIfNotExist() [1/3]...");

			this->ps.add10UrlsIfNotExist = this->addPreparedStatement(this->queryAddUrlsIfNotExist(10, hashQuery));
		}

		if(!(this->ps.add100UrlsIfNotExist)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares addUrlsIfNotExist() [2/3]...");

			this->ps.add100UrlsIfNotExist = this->addPreparedStatement(this->queryAddUrlsIfNotExist(100, hashQuery));
		}

		if(!(this->ps.add1000UrlsIfNotExist)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares addUrlsIfNotExist() [3/3]...");

			this->ps.add1000UrlsIfNotExist = this->addPreparedStatement(this->queryAddUrlsIfNotExist(1000, hashQuery));
		}

		if(!(this->ps.getUrlPosition)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getUrlPosition()...");

			this->ps.getUrlPosition = this->addPreparedStatement(
					"SELECT COUNT(*) AS result"
					" FROM `" + this->urlListTable + "` WHERE id < ?"
			);
		}

		if(!(this->ps.getNumberOfUrls)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getNumberOfUrls()...");

			this->ps.getNumberOfUrls = this->addPreparedStatement(
					"SELECT COUNT(*) AS result"
					" FROM `" + this->urlListTable + "`"
			);
		}

		if(!(this->ps.getUrlLockTime)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares getUrlLock()...");

			this->ps.getUrlLockTime = this->addPreparedStatement(
					"SELECT locktime"
					" FROM `" + this->crawlingTable + "`"
					" WHERE url = ?"
					" LIMIT 1"
			);
		}

		if(!(this->ps.isUrlCrawled)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares isUrlCrawled()...");

			std::ostringstream sqlQueryStrStr;
			sqlQueryStrStr <<	"SELECT success"
								" FROM `" << this->crawlingTable << "`"
								" WHERE url = ?"
								" LIMIT 1";

			this->ps.isUrlCrawled = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.renewUrlLockIfOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares lockUrlIfOk() [1/2]...");

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
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares lockUrlIfOk() [2/2]...");

			std::ostringstream sqlQueryStrStr;
			sqlQueryStrStr <<	"INSERT INTO `" << this->crawlingTable << "`(id, url, locktime)"
								" VALUES"
								" ("
									" ("
										"SELECT id FROM `" << this->crawlingTable << "`"
										" AS " << this->crawlingTableAlias << "1"
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

			this->ps.addUrlLockIfOk = this->addPreparedStatement(sqlQueryStrStr.str());
		}

		if(!(this->ps.unLockUrlIfOk)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares unLockUrlIfOk()...");

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
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares setUrlFinishedIfOk()...");

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
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares saveContent()...");

			this->ps.saveContent = this->addPreparedStatement(
					"INSERT INTO `" + crawledTable + "`(url, response, type, content)"
					" VALUES (?, ?, ?, ?)"
			);
		}

		if(!(this->ps.saveArchivedContent)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares saveArchivedContent()...");

			this->ps.saveArchivedContent = this->addPreparedStatement(
					"INSERT INTO `" + crawledTable + "`(url, crawltime, archived, response, type, content)"
					" VALUES (?, ?, TRUE, ?, ?, ?)"
			);
		}

		if(!(this->ps.isArchivedContentExists)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares isArchivedContentExists()...");

			this->ps.isArchivedContentExists = this->addPreparedStatement(
					"SELECT EXISTS"
					" ("
						"SELECT *"
						" FROM `" + crawledTable + "`"
						" WHERE url = ? AND crawltime = ?"
					" )"
					" AS result"
			);
		}

		if(!(this->ps.urlDuplicationCheck) && (this->urlStartupCheck || this->urlDebug)) {
			if(this->verbose)
				this->log("[#" + this->idString + "] prepares urlDuplicationCheck()...");

			std::string groupBy;
			if(this->urlCaseSensitive) groupBy = "url";
			else groupBy = "LOWER(url)";

			this->ps.urlDuplicationCheck = this->addPreparedStatement(
					"SELECT CAST( " + groupBy + " AS BINARY ) AS url, COUNT( " + groupBy + " )"
					" FROM `" + this->urlListTable + "`"
					" GROUP BY CAST( " + groupBy + " AS BINARY ) HAVING COUNT( " + groupBy + " ) > 1"
			);
		}

		if(!(this->ps.urlHashCheck) && this->urlStartupCheck) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares urlHashCheck()...");

			std::string urlHash("CRC32( ");
			if(this->urlCaseSensitive) urlHash += "url";
			else urlHash += "LOWER( url )";
			urlHash.push_back(')');

			this->ps.urlHashCheck = this->addPreparedStatement(
					"SELECT url FROM `" + this->urlListTable + "`"
					" WHERE hash <> " + urlHash + " LIMIT 1"
			);
		}

		if(!(this->ps.urlEmptyCheck) && (this->urlStartupCheck || this->urlDebug)) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares urlHashCheck()...");
			this->ps.urlEmptyCheck = this->addPreparedStatement(
					"SELECT id FROM `" + this->urlListTable + "`"
					" WHERE url = '' LIMIT 1"
			);
		}
	}

	// get the ID of an URL (uses hash check for first checking the probable existence of the URL)
	unsigned long Database::getUrlId(const std::string& url) {
		// check argument
		if(url.empty())
			throw DatabaseException("Crawler::Database::getUrlId(): No URL specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlId))
			throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlId(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlId);

		// get ID of URL from database
		try {
			// execute SQL query for getting URL
			sqlStatement.setString(1, url);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())  {
				return sqlResultSet->getUInt64("id");
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::getUrlId", e); }

		return 0;
	}

	// get the next URL to crawl from database, return ID and URL or empty IdString if all URLs have been parsed
	Database::IdString Database::getNextUrl(unsigned long currentUrlId) {
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

	// add URL to database if it does not exist already, return whether it was added
	bool Database::addUrlIfNotExists(const std::string& urlString, bool manual) {
		// check argument
		if(urlString.empty())
			throw DatabaseException("Crawler::Database::addUrlIfNotExists(): No URL specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.addUrlIfNotExists))
			throw DatabaseException("Missing prepared SQL statement for Crawler::Database::addUrlIfNotExists(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.addUrlIfNotExists);

		// add URL to database and get resulting ID
		try {
			// execute SQL query
			sqlStatement.setString(1, urlString);
			sqlStatement.setString(2, urlString);
			sqlStatement.setString(3, urlString);
			sqlStatement.setString(4, urlString);
			sqlStatement.setBoolean(5, manual);

			return Database::sqlExecuteUpdate(sqlStatement) > 0;
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::addUrlIfNotExists", e); }

		return false; // not needed
	}

	// add URLs that do not exist already to the database, return the number of added URLs
	unsigned long Database::addUrlsIfNotExist(std::queue<std::string, std::deque<std::string>>& urls) {
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
			throw DatabaseException("Missing prepared SQL statement for Crawler::Database::addUrlsIfNotExist(...)");

		// get prepared SQL statements
		sql::PreparedStatement& sqlStatement1 = this->getPreparedStatement(this->ps.addUrlIfNotExists);
		sql::PreparedStatement& sqlStatement10 = this->getPreparedStatement(this->ps.add10UrlsIfNotExist);
		sql::PreparedStatement& sqlStatement100 = this->getPreparedStatement(this->ps.add100UrlsIfNotExist);
		sql::PreparedStatement& sqlStatement1000 = this->getPreparedStatement(this->ps.add1000UrlsIfNotExist);

		try {
			// add 1.000 URLs at once
			while(urls.size() >= 1000) {
				for(unsigned long n = 0; n < 1000; ++n) {
					sqlStatement1000.setString((n * 4) + 1, urls.front());
					sqlStatement1000.setString((n * 4) + 2, urls.front());
					sqlStatement1000.setString((n * 4) + 3, urls.front());
					sqlStatement1000.setString((n * 4) + 4, urls.front());

					urls.pop();
				}

				int added = Database::sqlExecuteUpdate(sqlStatement1000);
				if(added > 0) result += added;
			}

			// add 100 URLs at once
			while(urls.size() >= 100) {
				for(unsigned long n = 0; n < 100; ++n) {
					sqlStatement100.setString((n * 4) + 1, urls.front());
					sqlStatement100.setString((n * 4) + 2, urls.front());
					sqlStatement100.setString((n * 4) + 3, urls.front());
					sqlStatement100.setString((n * 4) + 4, urls.front());

					urls.pop();
				}

				int added = Database::sqlExecuteUpdate(sqlStatement100);
				if(added > 0) result += added;
			}

			// add 10 URLs at once
			while(urls.size() >= 10) {
				for(unsigned long n = 0; n < 10; ++n) {
					sqlStatement10.setString((n * 4) + 1, urls.front());
					sqlStatement10.setString((n * 4) + 2, urls.front());
					sqlStatement10.setString((n * 4) + 3, urls.front());
					sqlStatement10.setString((n * 4) + 4, urls.front());

					urls.pop();
				}

				int added = Database::sqlExecuteUpdate(sqlStatement10);
				if(added > 0) result += added;
			}

			// add remaining URLs
			while(urls.size()) {
				sqlStatement1.setString(1, urls.front());
				sqlStatement1.setString(2, urls.front());
				sqlStatement1.setString(3, urls.front());
				sqlStatement1.setString(4, urls.front());
				sqlStatement1.setBoolean(5, false);

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

	// get the position of the URL in the URL list
	unsigned long Database::getUrlPosition(unsigned long urlId) {
		unsigned long result = 0;

		// check argument
		if(!urlId)
			return 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlPosition))
			throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlPosition()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlPosition);

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
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

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
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				throw DatabaseException(
						"Crawler::Database::urlHashCheck():"
						" Corrupted hash for URL \'" + sqlResultSet->getString("url") + "\""
						" in `" + this->urlListTable + "`"
				);
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::urlHashCheck", e); }
	}

	// check for empty URLs in the URL list, throw DatabaseException if an empty URL exists
	void Database::urlEmptyCheck(const std::vector<std::string>& urlsAdded) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.urlEmptyCheck))
			throw DatabaseException("Missing prepared SQL statement for Crawler::Database::urlEmptyCheck()");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.urlEmptyCheck);

		// get number of URLs from database
		try {
			// execute SQL query
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				if(urlsAdded.size()) {
					// DEBUG
					std::cout << "\n Empty URL in `" + this->urlListTable + "` after adding:";

					for(auto i = urlsAdded.begin(); i != urlsAdded.end(); ++i) {
						std::cout << "\n \'" << *i << "\'";
					}

					std::cout << std::flush;
				}

				throw DatabaseException(
						"Crawler::Database::urlEmptyCheck():"
						" Empty URL in `" + this->urlListTable + "`"
				);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::urlHashCheck", e); }
	}

	// get the URL lock end time of a specific URL from database
	std::string Database::getUrlLockTime(unsigned long urlId) {
		// check argument
		if(!urlId)
			return std::string();

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getUrlLockTime))
			throw DatabaseException("Missing prepared SQL statement for Crawler::Database::getUrlLockTime(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getUrlLockTime);

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

	// get the URL lock ID for a specific URL from the database, return whether the URL has been crawled)
	bool Database::isUrlCrawled(unsigned long urlId) {
		// check arguments
		if(!urlId)
			throw DatabaseException("Crawler::Database::isUrlCrawled(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.isUrlCrawled))
			throw DatabaseException("Missing prepared SQL statement for Module::Crawler::Database::isUrlCrawled(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.isUrlCrawled);

		// get lock ID from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				return sqlResultSet->getBoolean("success");
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isUrlCrawled", e); }

		return false;
	}

	// lock a URL in the database if it is lockable (or is still locked) or return an empty string if locking was unsuccessful
	std::string Database::lockUrlIfOk(unsigned long urlId, const std::string& lockTime, unsigned long lockTimeout) {
		std::string dbg;

		// check argument
		if(!urlId)
			throw DatabaseException("Crawler::Database::lockUrlIfOk(): No URL ID specified");

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.addUrlLockIfOk) || !(this->ps.renewUrlLockIfOk))
			throw DatabaseException("Missing prepared SQL statement for Module::Crawler::Database::lockUrlIfOk(...)");

		if(lockTime.empty()) {
			// get prepared SQL statement for locking the URL
			sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.addUrlLockIfOk);

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
			sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.renewUrlLockIfOk);

			// lock URL in database
			dbg = "renewUrlLockIfOk";
			try {
				// execute SQL query
				sqlStatement.setUInt64(1, lockTimeout);
				sqlStatement.setString(2, lockTime);
				sqlStatement.setUInt64(3, urlId);
				sqlStatement.setString(4, lockTime);

				if(Database::sqlExecuteUpdate(sqlStatement) < 1)
					return std::string(); // locking failed when no entries have been updated
			}
			catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::lockUrlIfOk > " + dbg, e); }
		}

		// get new expiration time of URL lock
		return this->getUrlLockTime(urlId);
	}

	// unlock a URL in the database
	void Database::unLockUrlIfOk(unsigned long urlId, const std::string& lockTime) {
		// check argument
		if(!urlId)
			throw DatabaseException("Crawler::Database::unLockUrlIfOk(): No URL lock ID specified");
		if(lockTime.empty())
			throw DatabaseException("Crawler::Database::unLockUrlIfOk(): URL lock is missing");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.unLockUrlIfOk))
			throw DatabaseException("Missing prepared SQL statement for Crawler::Database::unLockUrlIfOk(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.unLockUrlIfOk);

		// unlock URL in database
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setString(2, lockTime);
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::unLockUrlIfOk", e); }
	}

	// set URL as crawled in the database
	void Database::setUrlFinishedIfOk(unsigned long urlId, const std::string& lockTime) {
		// check argument
		if(!urlId)
			throw DatabaseException("Crawler::Database::setUrlFinishedIfOk(): No crawling ID specified");
		if(lockTime.empty())
			throw DatabaseException("Crawler::Database::setUrlFinishedIfOk(): URL lock is missing");

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.setUrlFinishedIfOk))
			throw DatabaseException("Missing prepared SQL statement for Crawler::Database::setUrlFinishedIfOk(...)");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.setUrlFinishedIfOk);

		// set URL as crawled
		try {
			// execute SQL query
			sqlStatement.setUInt64(1, urlId);
			sqlStatement.setString(2, lockTime);
			Database::sqlExecute(sqlStatement);
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::setUrlFinishedIfOk", e); }
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
				Database::sqlExecute(sqlStatement);
			}
			else if(this->logging) {
				// show warning about content size
				bool adjustServerSettings = false;
				std::ostringstream logStrStr;
				logStrStr.imbue(std::locale(""));
				logStrStr << "[#" << this->idString << "] WARNING:"
						" Some content could not be saved to the database, because its size ("
						<< content.size() << " bytes) exceeds the ";
				if(content.size() > 1073741824) logStrStr << "mySQL maximum of 1 GiB.";
				else {
					logStrStr << "current mySQL server maximum of "
							<< this->getMaxAllowedPacketSize() << " bytes.";
					adjustServerSettings = true;
				}
				this->log(logStrStr.str());
				if(adjustServerSettings)
					this->log(
							"[#" + this->idString + "]"
							" Adjust the server's \'max_allowed_packet\' setting accordingly."
					);
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
				Database::sqlExecute(sqlStatement);
			}
			else if(this->logging) {
				// show warning about content size
				bool adjustServerSettings = false;
				std::ostringstream logStrStr;
				logStrStr.imbue(std::locale(""));
				logStrStr << "[#" << this->idString << "] WARNING:"
						" Some content could not be saved to the database, because its size ("
						<< content.size() << " bytes) exceeds the ";
				if(content.size() > 1073741824) logStrStr << "mySQL maximum of 1 GiB.";
				else {
					logStrStr << "current mySQL server maximum of "
							<< this->getMaxAllowedPacketSize() << " bytes.";
					adjustServerSettings = true;
				}
				this->log(logStrStr.str());
				if(adjustServerSettings)
					this->log(
							"[#" + this->idString + "]"
							" Adjust the server's \'max_allowed_packet\' setting accordingly."
					);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::saveArchivedContent", e); }
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
			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next())
				result = sqlResultSet->getBoolean("result");
		}
		catch(const sql::SQLException &e) { this->sqlException("Crawler::Database::isArchivedContentExists", e); }

		return result;
	}

	// generate a SQL query for adding a specific number of URLs
	std::string Database::queryAddUrlsIfNotExist(unsigned int numberOfUrls, const std::string& hashQuery) {
		// check argument
		if(!numberOfUrls)
			throw DatabaseException("Crawler::Database::queryUpdateOrAddUrls(): No number of URLs specified");

		// generate INSERT INTO ... VALUES clause
		std::ostringstream sqlQueryStr;
		sqlQueryStr << "INSERT IGNORE INTO `" << this->urlListTable << "`(id, url, hash) VALUES ";

		// generate placeholders
		for(unsigned int n = 0; n < numberOfUrls; ++n)
			sqlQueryStr << "(" // begin of VALUES arguments
							" ("
								"SELECT id FROM"
								" ("
									"SELECT id, url"
									" FROM `" << this->urlListTable << "`"
									" AS `" << this->urlListTableAlias << n + 1 << "`"
									" WHERE hash = " << hashQuery <<
								" ) AS tmp2 WHERE url = ? LIMIT 1"
							" ),"
							"?, " <<
							hashQuery <<
						"), "; // end of VALUES arguments

		// remove last comma and space
		std::string sqlQuery = sqlQueryStr.str();
		sqlQuery.pop_back();
		sqlQuery.pop_back();

		// return query
		return sqlQuery;
	}

} /* crawlservpp::Module::Crawler */
