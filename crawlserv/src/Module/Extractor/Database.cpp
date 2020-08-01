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
 * This class provides database functionality for an extractor thread
 *  by implementing the Wrapper::Database interface.
 *
 *  Created on: May 9, 2019
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module::Extractor {

	/*
	 * CONSTRUCTION
	 */

	//! Constructor setting the database connection for the thread.
	/*!
	 * \param dbThread Reference to the database
	 * 	 connection used by the extractor thread.
	 */
	Database::Database(Module::Database& dbThread)
							: Wrapper::Database(dbThread) {}

	/*
	 * SETTERS
	 */

	//! Sets the maximum cache size for URLs.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the extractor.
	 *
	 * \param setCacheSize The maximum number
	 *   of URLs that can be cached.
	 *
	 * \sa prepare
	 */
	void Database::setCacheSize(std::uint64_t setCacheSize) {
		this->cacheSize = setCacheSize;
	}

	//! Sets whether to re-extract data from already processed URLs.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the extractor.
	 *
	 * \param isReExtract Set to true, and data
	 *   from already processed URLs will be
	 *   re-extracted.
	 *
	 * \sa prepare
	 */
	void Database::setReExtract(bool isReExtract) {
		this->reExtract = isReExtract;
	}

	//! Sets whether to extract data from custom URLs.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the extractor.
	 *
	 * \param isExtractCustom Set to true, and
	 *   data will be extracted from custom URLs
	 *   as well.
	 *
	 * \sa prepare
	 */
	void Database::setExtractCustom(bool isExtractCustom) {
		this->extractCustom = isExtractCustom;
	}

	//! Sets whether raw crawled data is used as source for the data to be extracted.
	/*!
	 * \note Needs to be set before preparing
	 *   the SQL statements for the extractor.
	 *
	 * \param isRawContentIsSource Set to true,
	 *   if raw crawled data will be the source
	 *   of the extracted data.
	 *
	 * \sa prepare
	 */
	void Database::setRawContentIsSource(bool isRawContentIsSource) {
		this->rawContentIsSource = isRawContentIsSource;
	}

	//! Sets the tables and columns of the parsed data sources.
	/*!
	 * \note Need to be set before preparing
	 *   the SQL statements for the extractor.
	 *
	 * \warning Uses std::queue::swap() – do
	 *   not use the argument after the call!
	 *
	 * \param tablesAndColumns Reference to a
	 *   queue containing the tables and columns
	 *   to be used as sources for the parsed
	 *   data. Will be invalidated by the call.
	 *
	 * \sa prepare
	 */
	void Database::setSources(std::queue<StringString>& tablesAndColumns) {
		this->sources.swap(tablesAndColumns);
	}

	//! Sets the name of the target table.
	/*!
	 * \note Needs to be set before
	 *   initializing the target table.
	 *
	 * \param table Constant reference to a
	 *   string containing the name of the
	 *   table to which the extracted data
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
	 *   columns to which the extracted data
	 *   will be written.
	 *
	 * \sa setTargetTable, initTargetTable
	 */
	void Database::setTargetFields(const std::vector<std::string>& fields) {
		this->targetFieldNames = fields;
	}

	//! Sets the name of the linked table.
	/*!
	 * \note Needs to be set before
	 *   initializing the target table.
	 *
	 * \param table Constant reference to a
	 *   string containing the name of the
	 *   table to which the linked data
	 *   will be written.
	 *
	 * \sa setLinkedFields, initTargetTable
	 */
	void Database::setLinkedTable(const std::string& table) {
		this->linkedTableName = table;
	}

	//! Sets the mname of the linked field.
	/*!
	 * The name of the linked field must
	 *  exist in the target table.
	 *
	 * \note Needs to be set before
	 *   initializing the target table.
	 *
	 * \param field Constant reference to a
	 *   string containing the name of the
	 *   extracted field to link to the
	 *   linked data.
	 *
	 * \sa setLinkedTable
	 */
	void Database::setLinkedField(const std::string& field) {
		this->linkedField = field;
	}

	//! Sets the columns of the linked table.
	/*!
	 * \note Needs to be set before
	 *   initializing the target table.
	 *
	 * \param fields Constant reference to a
	 *   vector containing the names of the
	 *   columns to which the linked data
	 *   will be written.
	 *
	 * \sa setLinkedTable, initTargetTable
	 */
	void Database::setLinkedFields(const std::vector<std::string>& fields) {
		this->linkedFieldNames = fields;
	}

	//! Sets whether existing datasets with the same ID will be overwritten.
	/*!
	 * \note Needs to be set before
	 *   initializing the target table.
	 *
	 * \param isOverwrite Set to true, and
	 *   datasets with the same ID will be
	 *   overwritten.
	 *
	 * \sa prepare
	 */
	void Database::setOverwrite(bool isOverwrite) {
		this->overwrite = isOverwrite;
	}

	//! Sets whether existing linked datasets with the same ID will be overwritten.
	/*!
	 * \note Needs to be set before
	 *   initializing the target table.
	 *
	 * \param isOverwrite Set to true, and
	 *   linked datasets with the same ID
	 *   will be overwritten.
	 *
	 * \sa prepare
	 */
	void Database::setOverwriteLinked(bool isOverwrite) {
		this->overwriteLinked = isOverwrite;
	}

	/*
	 * TARGET TABLE INITIALIZATION
	 */

	//! Creates the target table, if it does not exist, or adds target columns needed by the extractor.
	/*!
	 * If the target table does not exist,
	 *  it will be created. If the target
	 *  table exists, those target columns,
	 *  that it does not contain already,
	 *  will be added to the existing table.
	 *
	 * If necessary, the linked table will
	 *  also be created, or updated.
	 *
	 * \throws Module::Extractor::Exception
	 *   if the column used to link data
	 *   to the target table does not exist.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding the new
	 *   target table, updating the existing
	 *   target table, adding the new linked
	 *   table, or updating the existing
	 *   linked table in the database.
	 *
	 *  \sa setTargetTable, setTargetFields,
	 *    setLinkedTable, setLinkedFields,
	 *    Main::Database::addTargetTable
	 */
	void Database::initTargetTables() {
		const auto& options{this->getOptions()};

		// create table names
		this->urlListTable = "crawlserv_"
				+ options.websiteNamespace
				+ "_"
				+ options.urlListNamespace;
		this->extractingTable = this->urlListTable
				+ "_extracting";
		this->targetTableFull = this->urlListTable
				+ "_extracted_"
				+ this->targetTableName;

		// check whether linked table is necessary
		this->linked = !(this->linkedTableName.empty())
				&& !(this->linkedField.empty());

		if(this->linked) {
			// find linked field
			if(this->linkedField != "id" && this->linkedField != "datetime") {
				bool found{false};

				for(const auto& fieldName : this->targetFieldNames) {
					if(fieldName == this->linkedField) {
						found = true;

						break;
					}

					++(this->linkedIndex);
				}

				if(!found) {
					throw Exception(
							"Module::Extractor::Database::initTargetTables():"
							" Linked column '" + this->linkedField + "' does not exist"
					);
				}
			}

			// create table name for linked target table
			this->linkedTableFull = this->urlListTable
					+ "_extracted_"
					+ this->linkedTableName;

			// create properties for linked target table
			TargetTableProperties propertiesLinked(
					"extracted",
					this->getOptions().websiteId,
					this->getOptions().urlListId,
					this->linkedTableName,
					this->linkedTableFull,
					true
			);

			propertiesLinked.columns.reserve(
					minLinkedColumns
					+ this->linkedFieldNames.size()
			);

			propertiesLinked.columns.emplace_back("extracted_id", "TEXT NOT NULL");
			propertiesLinked.columns.emplace_back("hash", "INT UNSIGNED DEFAULT 0 NOT NULL", true);

			for(const auto& linkedFieldName : this->linkedFieldNames) {
				if(!linkedFieldName.empty()) {
					propertiesLinked.columns.emplace_back(
							"extracted__"
							+ linkedFieldName,
							"LONGTEXT"
					);
				}
			}

			// add or update linked target table
			this->linkedTableId = this->addTargetTable(propertiesLinked);
		}

		// create table properties for target table
		TargetTableProperties propertiesTarget(
				"extracted",
				this->getOptions().websiteId,
				this->getOptions().urlListId,
				this->targetTableName,
				this->targetTableFull,
				true
		);

		propertiesTarget.columns.reserve(
				minTargetColumns
				+ this->targetFieldNames.size()
				+ (this->linked ? 1 : 0) // column for 'link'
		);

		propertiesTarget.columns.emplace_back(
				"content",
				"BIGINT UNSIGNED NOT NULL",
				this->urlListTable
				+ "_crawled",
				"id"
		);

		propertiesTarget.columns.emplace_back("extracted_id", "TEXT NOT NULL");
		propertiesTarget.columns.emplace_back("hash", "INT UNSIGNED DEFAULT 0 NOT NULL", true);
		propertiesTarget.columns.emplace_back("extracted_datetime", "DATETIME DEFAULT NULL");

		for(const auto& targetFieldName : this->targetFieldNames) {
			if(!targetFieldName.empty()) {
				propertiesTarget.columns.emplace_back(
						"extracted__"
						+ targetFieldName,
						"LONGTEXT"
				);
			}
		}

		if(this->linked) {
			propertiesTarget.columns.emplace_back(
					"link",
					"BIGINT UNSIGNED DEFAULT NULL",
					this->linkedTableFull,
					"id"
			);
		}

		// add or update target table
		this->targetTableId = this->addTargetTable(propertiesTarget);
	}

	/*
	 * PREPARED SQL STATEMENTS
	 */

	//! Prepares the SQL statements needed by the extractor.
	/*!
	 * \note The target tables need to be prepared
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
		this->reserveForPreparedStatements(
				sizeof(ps) / sizeof(std::uint16_t) + this->sources.size()
		);

		// prepare SQL statements
		if(this->ps.fetchUrls == 0) {
			this->log(verbose, "prepares fetchUrls()...");

			std::string sqlQueryString{
									"SELECT tmp1.id, tmp1.url FROM"
									" ("
										" SELECT `"
			};

			sqlQueryString +=					 this->urlListTable;
			sqlQueryString +=					 "`.id,"
												 " `";
			sqlQueryString += 						this->urlListTable;
			sqlQueryString +=						"`.url"
										" FROM `";
			sqlQueryString +=					this->urlListTable;
			sqlQueryString +=					"`"
										" WHERE `";
			sqlQueryString +=					this->urlListTable;
			sqlQueryString +=					"`.id > ?";

			if(!(this->extractCustom)) {
				sqlQueryString +=		" AND `";
				sqlQueryString +=				this->urlListTable;
				sqlQueryString +=				"`.manual = FALSE";
			}

			sqlQueryString +=			" AND EXISTS"
										" ("
											" SELECT *"
											" FROM `";
			sqlQueryString +=						this->urlListTable;
			sqlQueryString +=						"_parsing`"
											" WHERE `";
			sqlQueryString +=						this->urlListTable;
			sqlQueryString +=						"_parsing`.url"
											" = `";
			sqlQueryString +=						this->urlListTable;
			sqlQueryString +=						"`.id"
											" AND `";
			sqlQueryString +=						this->urlListTable;
			sqlQueryString +=						"_parsing`.success"
										" )"
										" ORDER BY `";
			sqlQueryString +=						this->urlListTable;
			sqlQueryString +=						"`.id"
									" ) AS tmp1"
									" LEFT OUTER JOIN "
									" ("
										" SELECT url, MAX(locktime)"
										" AS locktime";

			if(!(this->reExtract)) {
				sqlQueryString += 		", MAX(success)"
										 " AS success";
			}

			sqlQueryString +=			" FROM `";
			sqlQueryString +=					this->extractingTable;
			sqlQueryString +=					"`"
										" WHERE target = ";
			sqlQueryString +=					std::to_string(this->targetTableId);
			sqlQueryString +=			" AND url > ?"
										" AND"
										" ("
											"locktime >= NOW()";

			if(!(this->reExtract)) {
				sqlQueryString +=			" OR success = TRUE";
			}

			sqlQueryString +=			" )"
										" GROUP BY url"
									" ) AS tmp2"
									" ON tmp1.id = tmp2.url"
									" WHERE tmp2.locktime IS NULL";

			if(!(this->reExtract)) {
				sqlQueryString +=	" AND tmp2.success IS NULL";
			}

			if(this->cacheSize > 0) {
				sqlQueryString +=	" LIMIT ";
				sqlQueryString +=			std::to_string(this->cacheSize);
			}

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
					" FROM `" + this->extractingTable + "`"
					" WHERE target = " + std::to_string(this->targetTableId) +
					" AND url = ?"
					" GROUP BY url"
					" LIMIT 1"
			);
		}

		if(this->ps.renewUrlLockIfOk == 0) {
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

		if(this->ps.unLockUrlIfOk == 0) {
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

		if(this->ps.checkExtractingTable == 0) {
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

		if(this->ps.getContent == 0) {
			this->log(verbose, "prepares getContent()...");

			if(this->rawContentIsSource) {
				this->ps.getContent = this->addPreparedStatement(
						"SELECT id, content FROM `" + this->urlListTable + "_crawled`"
						" WHERE url = ?"
						" ORDER BY crawltime DESC"
						" LIMIT 1"
				);
			}
			else {
				this->ps.getContent = this->addPreparedStatement(
						"SELECT id FROM `" + this->urlListTable + "_crawled`"
						" WHERE url = ?"
						" ORDER BY crawltime DESC"
						" LIMIT 1"
				);
			}
		}

		if(this->ps.setUrlFinishedIfLockOk == 0) {
			this->log(verbose, "prepares setUrlFinished() [1/4]...");

			this->ps.setUrlFinishedIfLockOk = this->addPreparedStatement(
					this->querySetUrlsFinishedIfLockOk(1)
			);
		}

		if(this->ps.set10UrlsFinishedIfLockOk == 0) {
			this->log(verbose, "prepares setUrlFinished() [2/4]...");

			this->ps.set10UrlsFinishedIfLockOk = this->addPreparedStatement(
					this->querySetUrlsFinishedIfLockOk(nAtOnce10)
			);
		}

		if(this->ps.set100UrlsFinishedIfLockOk == 0) {
			this->log(verbose, "prepares setUrlFinished() [3/4]...");

			this->ps.set100UrlsFinishedIfLockOk = this->addPreparedStatement(
					this->querySetUrlsFinishedIfLockOk(nAtOnce100)
			);
		}

		if(this->ps.set1000UrlsFinishedIfLockOk == 0) {
			this->log(verbose, "prepares setUrlFinished() [4/4]...");

			this->ps.set1000UrlsFinishedIfLockOk = this->addPreparedStatement(
					this->querySetUrlsFinishedIfLockOk(nAtOnce1000)
			);
		}

		if(this->ps.updateOrAddEntry == 0) {
			this->log(verbose, "prepares updateOrAddEntries() [1/4]...");

			this->ps.updateOrAddEntry = this->addPreparedStatement(
					this->queryUpdateOrAddEntries(1)
			);
		}

		if(this->ps.updateOrAdd10Entries == 0) {
			this->log(verbose, "prepares updateOrAddEntries() [2/4]...");

			this->ps.updateOrAdd10Entries = this->addPreparedStatement(
					this->queryUpdateOrAddEntries(nAtOnce10)
			);
		}

		if(this->ps.updateOrAdd100Entries == 0) {
			this->log(verbose, "prepares updateOrAddEntries() [3/4]...");

			this->ps.updateOrAdd100Entries = this->addPreparedStatement(
					this->queryUpdateOrAddEntries(nAtOnce100)
			);
		}

		if(this->ps.updateOrAdd1000Entries == 0) {
			this->log(verbose, "prepares updateOrAddEntries() [4/4]...");

			this->ps.updateOrAdd1000Entries = this->addPreparedStatement(
					this->queryUpdateOrAddEntries(nAtOnce1000)
			);
		}

		if(this->ps.updateOrAddLinked == 0) {
			this->log(verbose, "prepares updateOrAddLinked() [1/4]...");

			this->ps.updateOrAddLinked = this->addPreparedStatement(
					this->queryUpdateOrAddLinked(1)
			);
		}

		if(this->ps.updateOrAdd10Linked == 0) {
			this->log(verbose, "prepares updateOrAddLinked() [2/4]...");

			this->ps.updateOrAdd10Linked = this->addPreparedStatement(
					this->queryUpdateOrAddLinked(nAtOnce10)
			);
		}

		if(this->ps.updateOrAdd100Linked == 0) {
			this->log(verbose, "prepares updateOrAddLinked() [3/4]...");

			this->ps.updateOrAdd100Linked = this->addPreparedStatement(
					this->queryUpdateOrAddLinked(nAtOnce100)
			);
		}

		if(this->ps.updateOrAdd1000Linked == 0) {
			this->log(verbose, "prepares updateOrAddLinked() [4/4]...");

			this->ps.updateOrAdd1000Linked = this->addPreparedStatement(
					this->queryUpdateOrAddLinked(nAtOnce1000)
			);
		}

		if(this->ps.updateTargetTable == 0) {
			this->log(verbose, "prepares updateTargetTable()...");

			std::string queryString{
				"UPDATE `crawlserv_extractedtables`"
					" SET updated = CURRENT_TIMESTAMP"
					" WHERE id = "
			};

			queryString += std::to_string(this->targetTableId);

			if(this->linkedTableId > 0) {
				queryString += " OR id = ";
				queryString += std::to_string(this->linkedTableId);
				queryString += " LIMIT 2";
			}
			else {
				queryString += " LIMIT 1";
			}

			this->ps.updateTargetTable = this->addPreparedStatement(queryString);
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
	 * \throws Module::Extractor::Database::Exception
	 *   if one of the prepared SQL statements for
	 *   fetching and locking URLs is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while fetching and locking
	 *   the URLs.
	 */
	std::string Database::fetchUrls(
			std::uint64_t lastId,
			std::queue<IdString>& cache,
			std::uint32_t lockTimeout
	) {
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
					"Extractor:Database::fetchUrls():"
					" Missing prepared SQL statement"
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

			SqlResultSetPtr sqlResultSetFetch{
				Database::sqlExecuteQuery(sqlStatementFetch)
			};

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
			Database::sqlException("Extractor:Database::fetchUrls", e);
		}

		// number of arguments for setting a lock
		constexpr std::uint8_t numArgsLock{3};

		// set 1,000 locks at once
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
	 * \throws Module::Extractor::Database::Exception
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
					"Extractor:Database::getUrlPosition():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getUrlPosition == 0) {
			throw Exception(
					"Extractor:Database::getUrlPosition():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{
			this->getPreparedStatement(this->ps.getUrlPosition)
		};

		// get URL position of URL from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getUInt64("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Extractor:Database::getUrlPosition", e);
		}

		return result;
	}

	//! Gets the number of URLs in the URL list.
	/*!
	 * \returns The number of URLs in the current
	 *   URL list, or zero if the URL list is
	 *   empty.
	 *
	 * \throws Module::Extractor::Database::Exception
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
		if(this->ps.getNumberOfUrls == 0) {
			throw Exception(
					"Extractor:Database::getNumberOfUrls():"
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
			Database::sqlException("Extractor:Database::getNumberOfUrls", e);
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
	 * \throws Module::Extractor::Database::Exception
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
					"Extractor:Database::getLockTime():"
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
			Database::sqlException("Extractor:Database::getLockTime", e);
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
	 * \throws Module::Extractor::Database::Exception
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
					"Extractor:Database::getUrlLockTime():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{
			this->getPreparedStatement(this->ps.getUrlLockTime)
		};

		// get URL lock end time from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatement)
			};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				result = sqlResultSet->getString("locktime");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Extractor:Database::getUrlLockTime", e);
		}

		return result;
	}

	//! Locks a URL in the database, if it is lockable, or extends its locking time, if it is still locked by the extractor.
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
	 * \throws Module::Extractor::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for locking a URL is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while locking the URL, or
	 *   renewing its URL lock.
	 */
	std::string Database::renewUrlLockIfOk(
			std::uint64_t urlId,
			const std::string& lockTime,
			std::uint32_t lockTimeout
	) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Extractor:Database::renewUrlLockIfOk():"
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
					"Module::Extractor:Database::renewUrlLockIfOk():"
					" Missing prepared SQL statement "
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
			Database::sqlException("Crawler::Database::renewUrlLockIfOk", e);
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
	 * \throws Module::Extractor::Database::Exception
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
					"Extractor:Database::unLockUrlIfOk():"
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
			Database::sqlException("Extractor:Database::unLockUrlIfOk", e);
		}

		return false;
	}

	//! Unlocks multiple URLs in the database at once.
	/*!
	 * \note The SQL statements needed for
	 *   unlocking the URLs will only be created
	 *   shortly before query execution, as it
	 *   should only be used once (on shutdown
	 *   on the extractor). During normal
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
			Database::sqlException("Extractor:Database::unLockUrlsIfOk", e);
		}

		lockTime.clear();
	}

	/*
	 * EXTRACTING
	 */

	//! Checks the extracting table.
	/*!
	 * Deletes duplicate URL locks.
	 *
	 * \returns The number of duplicate URL locks
	 *   that have been deleted. Zero, if no
	 *   duplicate locks have been found.
	 *
	 * \throws Module::Extractor::Database::Exception
	 *   if the prepared SQL statement for checking
	 *   the table is missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the table.
	 */
	std::uint32_t Database::checkExtractingTable() {
		// check connection
		this->checkConnection();

		// check prepared SQL statement for locking the URL
		if(this->ps.checkExtractingTable == 0) {
			throw Exception(
					"Extractor:Database::checkExtractingTable():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement for locking the URL
		auto& sqlStatement{
			this->getPreparedStatement(this->ps.checkExtractingTable)
		};

		// lock URL in database if not locked (and not extracted yet when re-extracting is deactivated)
		try {
			// execute SQL query
			auto result{Database::sqlExecuteUpdate(sqlStatement)};

			if(result > 0) {
				return result;
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Extractor:Database::checkExtractingTable", e);
		}

		return 0;
	}

	//! Gets the latest content stored in the database for a specific URL.
	/*!
	 * \param urlId ID of the URL whose latest
	 *   content will be retrieved from the
	 *   database.
	 * \param contentTo Reference to a pair, to
	 *   which the content and its ID will be
	 *   written.
	 *
	 * \returns True, if the requested content
	 *   for the given URL has been retrieved, even
	 *   when it is empty. False, if no content has
	 *   been stored for the URL in the database.
	 *
	 * \throws Module::Extractor::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for retrieving the latest
	 *   content for a URL from the database is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while trying to retrieve the
	 *   content from the database.
	 */
	bool Database::getContent(std::uint64_t urlId, IdString& contentTo) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Extractor:Database::getContent():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.getContent == 0) {
			throw Exception(
					"Extractor:Database::getContent():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.getContent)};

		// get URL latest content from database
		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				contentTo.first = sqlResultSet->getUInt64("id");

				if(this->rawContentIsSource) {
					contentTo.second = sqlResultSet->getString("content");
				}

				return true;
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Extractor:Database::getContent", e);
		}

		return false;
	}

	//! Gets parsed data from the given source stored in the database for a specific URL.
	/*!
	 * \note The source index is determined by the
	 *   order in which the sources have been added,
	 *   starting at zero.
	 *
	 * \param urlId ID of the URL whose parsed data
	 *   will be retrieved from the database.
	 * \param sourceIndex Zero-based index of the
	 *   source from which to retrieve the data, as
	 *   specified in the tables and columns passed
	 *   to setSources().
	 * \param resultTo Reference to a string, to
	 *   which the retrieved data will be
	 *   written. Will be left unchanged, if the
	 *   specified data has not been found.
	 *
	 * \throws Module::Extractor::Database::Exception
	 *   if no URL has been specified, i.e. the
	 *   given URL ID is zero, or if the prepared
	 *   SQL statement for retrieving parsed data
	 *   for the given URL from the specified source
	 *   is missing, e.g. if the source index is
	 *   invalid.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while trying to retrieve the
	 *   data from the database.
	 */
	void Database::getLatestParsedData(std::uint64_t urlId, std::size_t sourceIndex, std::string& resultTo) {
		// check argument
		if(urlId == 0) {
			throw Exception(
					"Extractor::Database::getLatestParsedData():"
					" No URL has been specified"
			);
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(
				sourceIndex >= this->psGetLatestParsedData.size()
				|| this->psGetLatestParsedData.at(sourceIndex) == 0
		) {
			throw Exception(
					"Extractor::Database::getLatestParsedData():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{
			this->getPreparedStatement(
					this->psGetLatestParsedData.at(sourceIndex)
			)
		};

		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, urlId);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(sqlStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				resultTo = sqlResultSet->getString("result");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Extractor:Database::getLatestParsedData", e);
		}
	}

	//! Adds extracted data to the database, or updates data that already exists.
	/*!
	 * Only updates data, if the extractor is set
	 *  to overwrite existing data via
	 *  setOverwrite().
	 *
	 * \param entries Reference to a queue
	 *   containing the data to add. If empty,
	 *   nothing will be done. The queue will be
	 *   emptied as the data will be processed,
	 *   even when some or all of the data has not
	 *   been added or updated, because it already
	 *   exists and the extractor has been set not
	 *   to overwrite data via setOverwrite().
	 *
	 * \throws Module::Extractor::Database::Exception
	 *   if any of the prepared SQL statements for
	 *   adding and updating extracted data is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding or updating
	 *   the extracted data in the database.
	 */
	void Database::updateOrAddEntries(std::queue<DataEntry>& entries) {
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
					"Extractor:Database::updateOrAddEntries():"
					" Missing prepared SQL statement(s)"
			);
		}

		// get prepared SQL statements
		auto& sqlStatement1{this->getPreparedStatement(this->ps.updateOrAddEntry)};
		auto& sqlStatement10{this->getPreparedStatement(this->ps.updateOrAdd10Entries)};
		auto& sqlStatement100{this->getPreparedStatement(this->ps.updateOrAdd100Entries)};
		auto& sqlStatement1000{this->getPreparedStatement(this->ps.updateOrAdd1000Entries)};

		// number of arguments when adding an entry (without custom fields)
		constexpr auto numArgsAdd{4};

		// number of additional arguments when data is linked
		constexpr auto numArgsLinked{2};

		// number of additional arguments when overwriting existing entries
		constexpr auto numArgsOverwrite{3};

		// count fields
		auto fields{
			std::count_if(
					this->targetFieldNames.cbegin(),
					this->targetFieldNames.cend(),
					[](const auto& fieldName) {
						return !fieldName.empty();
					}
			) + minTargetColumns
			+ (this->linked ? numArgsLinked : 0)
		};

		if(this->overwrite) {
			fields += numArgsOverwrite;
		}

		try {
			// add 1,000 entries at once
			while(entries.size() >= nAtOnce1000) {
				for(std::uint16_t n{0}; n < nAtOnce1000; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					std::size_t counter{0};

					// set standard values
					if(this->overwrite) {
						sqlStatement1000.setUInt64(n * fields + sqlArg1, entries.front().contentId);
						sqlStatement1000.setString(n * fields + sqlArg2, entries.front().dataId);
						sqlStatement1000.setString(n * fields + sqlArg3, entries.front().dataId);

						counter += numArgsOverwrite;
					}

					sqlStatement1000.setUInt64(n * fields + counter + sqlArg1, entries.front().contentId);
					sqlStatement1000.setString(n * fields + counter + sqlArg2, entries.front().dataId);
					sqlStatement1000.setString(n * fields + counter + sqlArg3, entries.front().dataId);

					if(entries.front().dateTime.empty()) {
						sqlStatement1000.setNull(n * fields + counter + sqlArg4, 0);
					}
					else {
						sqlStatement1000.setString(n * fields + counter + sqlArg4, entries.front().dateTime);
					}

					// set custom values
					counter += numArgsAdd;

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->targetFieldNames.at(index).empty())) {
							sqlStatement1000.setString(n * fields + counter + sqlArg1, *it);

							++counter;
						}
					}

					// set linked data
					if(this->linked) {
						std::string linkedData;

						if(this->linkedField == "id") {
							linkedData = entries.front().dataId;
						}
						else if(this->linkedField == "datetime") {
							linkedData = entries.front().dateTime;
						}
						else {
							linkedData = entries.front().fields.at(this->linkedIndex);
						}

						sqlStatement1000.setString(n * fields + counter + sqlArg1, linkedData);
						sqlStatement1000.setString(n * fields + counter + sqlArg2, linkedData);

						//counter += 2;
					}

					// remove entry
					entries.pop();
				}

				// execute SQL query
				Database::sqlExecute(sqlStatement1000);
			}

			// add 100 entries at once
			while(entries.size() >= nAtOnce100) {
				for(std::uint8_t n{0}; n < nAtOnce100; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					std::size_t counter{0};

					// set standard values
					if(this->overwrite) {
						sqlStatement100.setUInt64(n * fields + sqlArg1, entries.front().contentId);
						sqlStatement100.setString(n * fields + sqlArg2, entries.front().dataId);
						sqlStatement100.setString(n * fields + sqlArg3, entries.front().dataId);

						counter += numArgsOverwrite;
					}

					sqlStatement100.setUInt64(n * fields + counter + sqlArg1, entries.front().contentId);
					sqlStatement100.setString(n * fields + counter + sqlArg2, entries.front().dataId);
					sqlStatement100.setString(n * fields + counter + sqlArg3, entries.front().dataId);

					if(entries.front().dateTime.empty()) {
						sqlStatement100.setNull(n * fields + counter + sqlArg4, 0);
					}
					else {
						sqlStatement100.setString(n * fields + counter + sqlArg4, entries.front().dateTime);
					}

					// set custom values
					counter += numArgsAdd;

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->targetFieldNames.at(index).empty())) {
							sqlStatement100.setString(n * fields + counter + sqlArg1, *it);

							++counter;
						}
					}

					// set linked data
					if(this->linked) {
						// set linked data
						std::string linkedData;

						if(this->linkedField == "id") {
							linkedData = entries.front().dataId;
						}
						else if(this->linkedField == "datetime") {
							linkedData = entries.front().dateTime;
						}
						else {
							linkedData = entries.front().fields.at(this->linkedIndex);
						}

						sqlStatement100.setString(n * fields + counter + sqlArg1, linkedData);
						sqlStatement100.setString(n * fields + counter + sqlArg2, linkedData);

						//counter += 2;
					}

					// remove entry
					entries.pop();
				}

				// execute SQL query
				Database::sqlExecute(sqlStatement100);
			}

			// add 10 entries at once
			while(entries.size() >= nAtOnce10) {
				for(std::uint8_t n{0}; n < nAtOnce10; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					std::size_t counter{0};

					// set standard values
					if(this->overwrite) {
						sqlStatement10.setUInt64(n * fields + sqlArg1, entries.front().contentId);
						sqlStatement10.setString(n * fields + sqlArg2, entries.front().dataId);
						sqlStatement10.setString(n * fields + sqlArg3, entries.front().dataId);

						counter += numArgsOverwrite;
					}

					sqlStatement10.setUInt64(n * fields + counter + sqlArg1, entries.front().contentId);
					sqlStatement10.setString(n * fields + counter + sqlArg2, entries.front().dataId);
					sqlStatement10.setString(n * fields + counter + sqlArg3, entries.front().dataId);

					if(entries.front().dateTime.empty()) {
						sqlStatement10.setNull(n * fields + counter + sqlArg4, 0);
					}
					else {
						sqlStatement10.setString(n * fields + counter + sqlArg4, entries.front().dateTime);
					}

					// set custom values
					counter += numArgsAdd;

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->targetFieldNames.at(index).empty())) {
							sqlStatement10.setString(n * fields + counter + sqlArg1, *it);

							++counter;
						}
					}

					// set linked data
					if(this->linked) {
						// set linked data
						std::string linkedData;

						if(this->linkedField == "id") {
							linkedData = entries.front().dataId;
						}
						else if(this->linkedField == "datetime") {
							linkedData = entries.front().dateTime;
						}
						else {
							linkedData = entries.front().fields.at(this->linkedIndex);
						}

						sqlStatement10.setString(n * fields + counter + sqlArg1, linkedData);
						sqlStatement10.setString(n * fields + counter + sqlArg2, linkedData);

						//counter += 2;
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

				std::size_t counter{0};

				// set standard values
				if(this->overwrite) {
					sqlStatement1.setUInt64(sqlArg1, entries.front().contentId);
					sqlStatement1.setString(sqlArg2, entries.front().dataId);
					sqlStatement1.setString(sqlArg3, entries.front().dataId);

					counter += numArgsOverwrite;
				}

				sqlStatement1.setUInt64(counter + sqlArg1, entries.front().contentId);
				sqlStatement1.setString(counter + sqlArg2, entries.front().dataId);
				sqlStatement1.setString(counter + sqlArg3, entries.front().dataId);

				if(entries.front().dateTime.empty()) {
					sqlStatement1.setNull(counter + sqlArg4, 0);
				}
				else {
					sqlStatement1.setString(counter + sqlArg4, entries.front().dateTime);
				}

				// set custom values
				counter += numArgsAdd;

				for(
						auto it{entries.front().fields.cbegin()};
						it != entries.front().fields.cend();
						++it
				) {
					const auto index{it - entries.front().fields.cbegin()};
					if(!(this->targetFieldNames.at(index).empty())) {
						sqlStatement1.setString(counter + sqlArg1, *it);

						++counter;
					}
				}

				// set linked data
				if(this->linked) {
					// set linked data
					std::string linkedData;

					if(this->linkedField == "id") {
						linkedData = entries.front().dataId;
					}
					else if(this->linkedField == "datetime") {
						linkedData = entries.front().dateTime;
					}
					else {
						linkedData = entries.front().fields.at(this->linkedIndex);
					}

					sqlStatement1.setString(counter + sqlArg1, linkedData);
					sqlStatement1.setString(counter + sqlArg2, linkedData);

					//counter += 2;
				}

				// remove entry
				entries.pop();

				// execute SQL query
				Database::sqlExecute(sqlStatement1);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Extractor:Database::updateOrAddEntries", e);
		}
	}

	//! Adds linked data to the database, or updates data that already exists.
	/*!
	 * Only updates data, if the extractor is set
	 *  to overwrite existing linked data via
	 *  setOverwriteLinked().
	 *
	 * \param entries Reference to a queue
	 *   containing the data to add. If empty,
	 *   nothing will be done. The queue will be
	 *   emptied as the data will be processed,
	 *   even when some or all of the data has not
	 *   been added or updated, because it already
	 *   exists and the extractor has been set not
	 *   to overwrite data via setOverwriteLinked().
	 *
	 * \throws Module::Extractor::Database::Exception
	 *   if any of the prepared SQL statements for
	 *   adding and updating linked data is
	 *   missing.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while adding or updating
	 *   the linked data in the database.
	 */
	void Database::updateOrAddLinked(std::queue<DataEntry>& entries) {
		// check argument
		if(entries.empty()) {
			return;
		}

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(
				this->ps.updateOrAddLinked == 0
				|| this->ps.updateOrAdd10Linked == 0
				|| this->ps.updateOrAdd100Linked == 0
				|| this->ps.updateOrAdd1000Linked == 0
		) {
			throw Exception(
					"Extractor:Database::updateOrAddLinked():"
					" Missing prepared SQL statement(s)"
			);
		}

		// get prepared SQL statements
		auto& sqlStatement1{this->getPreparedStatement(this->ps.updateOrAddLinked)};
		auto& sqlStatement10{this->getPreparedStatement(this->ps.updateOrAdd10Linked)};
		auto& sqlStatement100{this->getPreparedStatement(this->ps.updateOrAdd100Linked)};
		auto& sqlStatement1000{this->getPreparedStatement(this->ps.updateOrAdd1000Linked)};

		// number of arguments when adding an entry (without custom fields)
		constexpr auto numArgsAdd{2};

		// number of additional arguments when overwriting existing entries
		constexpr auto numArgsOverwrite{2};

		// count fields
		auto fields{
			std::count_if(
					this->linkedFieldNames.cbegin(),
					this->linkedFieldNames.cend(),
					[](const auto& fieldName) {
						return !fieldName.empty();
					}
			) + minLinkedColumns
		};

		if(this->overwriteLinked) {
			fields += numArgsOverwrite;
		}

		try {
			// add 1,000 entries at once
			while(entries.size() >= nAtOnce1000) {
				for(std::uint16_t n{0}; n < nAtOnce1000; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					std::size_t counter{0};

					// set standard values
					if(this->overwriteLinked) {
						sqlStatement1000.setString(n * fields + sqlArg1, entries.front().dataId);
						sqlStatement1000.setString(n * fields + sqlArg2, entries.front().dataId);

						counter += numArgsOverwrite;
					}

					sqlStatement1000.setString(n * fields + counter + sqlArg1, entries.front().dataId);
					sqlStatement1000.setString(n * fields + counter + sqlArg2, entries.front().dataId);

					// set custom values
					counter += numArgsAdd;

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->linkedFieldNames.at(index).empty())) {
							sqlStatement1000.setString(n * fields + counter + sqlArg1, *it);

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
			while(entries.size() >= nAtOnce100) {
				for(std::uint8_t n{0}; n < nAtOnce100; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					std::size_t counter{0};

					// set standard values
					if(this->overwriteLinked) {
						sqlStatement100.setString(n * fields + sqlArg1, entries.front().dataId);
						sqlStatement100.setString(n * fields + sqlArg2, entries.front().dataId);

						counter += numArgsOverwrite;
					}

					sqlStatement100.setString(n * fields + counter + sqlArg1, entries.front().dataId);
					sqlStatement100.setString(n * fields + counter + sqlArg2, entries.front().dataId);

					// set custom values
					counter += numArgsAdd;

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->linkedFieldNames.at(index).empty())) {
							sqlStatement100.setString(n * fields + counter + sqlArg1, *it);

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
			while(entries.size() >= nAtOnce10) {
				for(std::uint8_t n{0}; n < nAtOnce10; ++n) {
					// check entry
					this->checkEntrySize(entries.front());

					std::size_t counter{0};

					// set standard values
					if(this->overwriteLinked) {
						sqlStatement10.setString(n * fields + sqlArg1, entries.front().dataId);
						sqlStatement10.setString(n * fields + sqlArg2, entries.front().dataId);

						counter += numArgsOverwrite;
					}

					sqlStatement10.setString(n * fields + counter + sqlArg1, entries.front().dataId);
					sqlStatement10.setString(n * fields + counter + sqlArg2, entries.front().dataId);

					// set custom values
					counter += numArgsAdd;

					for(
							auto it{entries.front().fields.cbegin()};
							it != entries.front().fields.cend();
							++it
					) {
						const auto index{it - entries.front().fields.cbegin()};

						if(!(this->linkedFieldNames.at(index).empty())) {
							sqlStatement10.setString(n * fields + counter + sqlArg1, *it);

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

				std::size_t counter{0};

				// set standard values
				if(this->overwriteLinked) {
					sqlStatement1.setString(sqlArg1, entries.front().dataId);
					sqlStatement1.setString(sqlArg2, entries.front().dataId);

					counter += numArgsOverwrite;
				}

				sqlStatement1.setString(counter + sqlArg1, entries.front().dataId);
				sqlStatement1.setString(counter + sqlArg2, entries.front().dataId);

				// set custom values
				counter += numArgsAdd;

				for(
						auto it{entries.front().fields.cbegin()};
						it != entries.front().fields.cend();
						++it
				) {
					const auto index{it - entries.front().fields.cbegin()};
					if(!(this->linkedFieldNames.at(index).empty())) {
						sqlStatement1.setString(counter + sqlArg1, *it);

						++counter;
					}
				}

				// remove entry
				entries.pop();

				// execute SQL query
				Database::sqlExecute(sqlStatement1);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Extractor:Database::updateOrAddLinked", e);
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
	 * \throws Module::Extractor::Database::Exception
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
					"Extractor:Database::setUrlsFinishedIfLockOk():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statements
		auto& sqlStatement1{this->getPreparedStatement(this->ps.setUrlFinishedIfLockOk)};
		auto& sqlStatement10{this->getPreparedStatement(this->ps.set10UrlsFinishedIfLockOk)};
		auto& sqlStatement100{this->getPreparedStatement(this->ps.set100UrlsFinishedIfLockOk)};
		auto& sqlStatement1000{this->getPreparedStatement(this->ps.set1000UrlsFinishedIfLockOk)};

		// number of arguments to set a URL to finished
		constexpr auto numArgsFinish{2};

		// set URLs to finished in database
		try {

			// set 1,000 URLs at once
			while(finished.size() > nAtOnce1000) {
				for(std::uint16_t n{0}; n < nAtOnce1000; ++n) {
					sqlStatement1000.setUInt64(n * numArgsFinish + sqlArg1, finished.front().first);
					sqlStatement1000.setString(n * numArgsFinish + sqlArg2, finished.front().second);

					finished.pop();
				}

				Database::sqlExecute(sqlStatement1000);
			}

			// set 100 URLs at once
			while(finished.size() > nAtOnce100) {
				for(std::uint8_t n{0}; n < nAtOnce100; ++n) {
					sqlStatement100.setUInt64(n * numArgsFinish + sqlArg1, finished.front().first);
					sqlStatement100.setString(n * numArgsFinish + sqlArg2, finished.front().second);

					finished.pop();
				}

				Database::sqlExecute(sqlStatement100);
			}

			// set 10 URLs at once
			while(finished.size() > nAtOnce10) {
				for(std::uint8_t n{0}; n < nAtOnce10; ++n) {
					sqlStatement10.setUInt64(n * numArgsFinish + sqlArg1, finished.front().first);
					sqlStatement10.setString(n * numArgsFinish + sqlArg2, finished.front().second);

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
			Database::sqlException("Extractor:Database::setUrlsFinishedIfLockOk", e);
		}
	}

	//! Updates the target table.
	/*!
	 * Sets the time that specifies, when the target
	 *  table has last been updated, to now – i.e.
	 *  the current database time.
	 *
	 * If a linked table is specified, it will also
	 *  be updated.
	 *
	 * \throws Module::Extractor::Database::Exception
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
					"Extractor:Database::updateTargetTable():"
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
			Database::sqlException("Extractor:Database::updateTargetTable", e);
		}
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// check the value sizes in a extracting entry and remove values that are too large for the database
	bool Database::checkEntrySize(DataEntry& entry) {
		// check data sizes
		const auto max{this->getMaxAllowedPacketSize()};
		std::size_t tooLarge{0};

		if(entry.dataId.size() > max) {
			tooLarge = entry.dataId.size();

			entry.dataId.clear();
		}

		if(
				entry.dateTime.size() > max
				&& entry.dateTime.size() > tooLarge
		) {
			tooLarge = entry.dateTime.size();

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
							" because the size of an extracted value ("
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
						"Adjust the server's \'max_allowed_packet\' setting accordingly."
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
					"Extractor::Database::queryLockUrls():"
					" No URLs have been specified"
			);
		}

		// create INSERT INTO clause
		std::string sqlQueryString{
							"INSERT INTO `"
		};

		sqlQueryString +=		this->extractingTable;
		sqlQueryString +=		"`(id, target, url, locktime)"
								" VALUES";

		// create VALUES clauses
		for(std::size_t n{1}; n <= numberOfUrls; ++n) {
			sqlQueryString +=	" ("
									" ("
										"SELECT id FROM `";
			sqlQueryString +=				this->extractingTable;
			sqlQueryString +=				"`"
										" AS `";
			sqlQueryString +=				extractingTableAlias;
			sqlQueryString +=				std::to_string(n);
			sqlQueryString +=				"`"
										" WHERE target = ";
			sqlQueryString +=				std::to_string(this->targetTableId);
			sqlQueryString +=			" AND url = ?"
										" ORDER BY id DESC"
										" LIMIT 1"
									" ),"
									" ";
			sqlQueryString +=			std::to_string(this->targetTableId);
			sqlQueryString +=			","
									" ?,"
									" ?"
								" )";

			if(n < numberOfUrls) {
				sqlQueryString += ", ";
			}
		}

		// create ON DUPLICATE KEY UPDATE clause
		sqlQueryString +=		" ON DUPLICATE KEY"
								" UPDATE locktime = VALUES(locktime)";

		return sqlQueryString;
	}

	// generate SQL query for updating or adding a specific number of extracted entries, throws Database::Exception
	std::string Database::queryUpdateOrAddEntries(std::size_t numberOfEntries) {
		// check argument
		if(numberOfEntries == 0) {
			throw Exception(
					"Extractor::Database::queryUpdateOrAddEntries():"
					" No entries have been specified"
			);
		}

		// create INSERT INTO clause
		std::string sqlQueryString{
							"INSERT INTO `"
		};

		sqlQueryString +=			this->targetTableFull;
		sqlQueryString +=			"`"
								" (";

		if(this->overwrite) {
			sqlQueryString +=		" id,";
		}

		sqlQueryString += 			" content,"
									" extracted_id,"
									" hash,"
									" extracted_datetime";

		std::size_t counter{0};

		for(const auto& targetFieldName : this->targetFieldNames) {
			if(!targetFieldName.empty()) {
				sqlQueryString += 		", `extracted__" + targetFieldName + "`";

				++counter;
			}
		}

		if(this->linked) {
			sqlQueryString +=		" , link";
		}

		sqlQueryString += 			")"
								" VALUES ";

		// create placeholder list (including existence check)
		for(std::size_t n{1}; n <= numberOfEntries; ++n) {
			sqlQueryString +=	"( ";

			if(this->overwrite) {
				sqlQueryString +=	"("
										"SELECT id"
										" FROM "
										" ("
											" SELECT id, extracted_id"
											" FROM `";
				sqlQueryString +=				this->targetTableFull;
				sqlQueryString +=				"`"
											" AS `";
				sqlQueryString +=				targetTableAlias;
				sqlQueryString +=				"`"
											" WHERE content = ?"
											" AND hash = CRC32( ? )"
										" ) AS tmp"
										" WHERE extracted_id LIKE ?"
										" LIMIT 1"
									"),";
			}


			sqlQueryString +=			"?, ?, CRC32( ? ), ?";

			for(std::size_t c{0}; c < counter; ++c) {
				sqlQueryString +=	 		", ?";
			}

			if(this->linked) {
				sqlQueryString +=	", ("
										"SELECT id"
										" FROM"
										" ("
											"SELECT id, extracted_id"
											" FROM `";
				sqlQueryString +=				this->linkedTableFull;
				sqlQueryString +=				"`"
											" AS `";
				sqlQueryString +=				linkedTableAlias;
				sqlQueryString +=				"`"
											" WHERE hash = CRC32( ? )"
										" ) AS tmp"
										" WHERE extracted_id LIKE ?"
										" LIMIT 1"
									")";
			}

			sqlQueryString +=	")";

			if(n < numberOfEntries) {
				sqlQueryString += ", ";
			}
		}

		// create ON DUPLICATE KEY UPDATE clause
		if(this->overwrite) {
			sqlQueryString +=		" ON DUPLICATE KEY UPDATE"
								" hash = VALUES(hash),"
								" extracted_datetime = VALUES(extracted_datetime)";

			for(const auto& targetFieldName : this->targetFieldNames) {
				if(!targetFieldName.empty()) {
					sqlQueryString +=	", `extracted__";
					sqlQueryString += targetFieldName;
					sqlQueryString += "` = VALUES(`extracted__";
					sqlQueryString += targetFieldName;
					sqlQueryString += "`)";
				}
			}

			if(this->linked) {
				sqlQueryString += ", link = VALUES(link)";
			}
		}

		// return query
		return sqlQueryString;
	}

	// generate SQL query for updating or adding a specific number of linked entries, throws Database::Exception
	std::string Database::queryUpdateOrAddLinked(std::size_t numberOfEntries) {
		if(!(this->linked)) {
			throw Exception(
					"Extractor::Database::queryUpdateOrAddLinked():"
					" No linked data available"
			);
		}

		// check argument
		if(numberOfEntries == 0) {
			throw Exception(
					"Extractor::Database::queryUpdateOrAddLinked():"
					" No entries have been specified"
			);
		}

		// create INSERT INTO clause
		std::string sqlQueryString{
							"INSERT INTO `"
		};

		sqlQueryString +=			this->linkedTableFull;
		sqlQueryString +=			"`"
								" (";

		if(this->overwriteLinked) {
			sqlQueryString +=		" id,";
		}

		sqlQueryString += 			" extracted_id,"
									" hash";

		std::size_t counter{0};

		for(const auto& linkedFieldName : this->linkedFieldNames) {
			if(!linkedFieldName.empty()) {
				sqlQueryString += 		", `extracted__" + linkedFieldName + "`";

				++counter;
			}
		}

		sqlQueryString += 			")"
								" VALUES ";

		// create placeholder list (including existence check)
		for(std::size_t n{1}; n <= numberOfEntries; ++n) {
			sqlQueryString +=	"( ";

			if(this->overwriteLinked) {
				sqlQueryString +=	"("
										"SELECT id"
										" FROM "
										" ("
											" SELECT id, extracted_id"
											" FROM `";
				sqlQueryString +=				this->linkedTableFull;
				sqlQueryString +=				"`"
											" AS `";
				sqlQueryString +=				linkedTableAlias;
				sqlQueryString +=				"`"
											" WHERE hash = CRC32( ? )"
										" ) AS tmp"
										" WHERE extracted_id LIKE ?"
										" LIMIT 1"
									"),";
			}


			sqlQueryString +=			" ?, CRC32( ? )";

			for(std::size_t c{0}; c < counter; ++c) {
				sqlQueryString +=	 		", ?";
			}

			sqlQueryString +=	")";

			if(n < numberOfEntries) {
				sqlQueryString += ", ";
			}
		}

		// create ON DUPLICATE KEY UPDATE clause
		if(this->overwriteLinked) {
			sqlQueryString +=	" ON DUPLICATE KEY UPDATE"
								" hash = VALUES(hash)";

			for(const auto& linkedFieldName : this->linkedFieldNames) {
				if(!linkedFieldName.empty()) {
					sqlQueryString +=	", `extracted__";
					sqlQueryString += linkedFieldName;
					sqlQueryString += "` = VALUES(`extracted__";
					sqlQueryString += linkedFieldName;
					sqlQueryString += "`)";
				}
			}
		}

		// return query
		return sqlQueryString;
	}

	// generate SQL query for setting a specific number of URLs to finished if they haven't been locked since extracting,
	//  throws Database::Exception
	std::string Database::querySetUrlsFinishedIfLockOk(std::size_t numberOfUrls) {
		// check arguments
		if(numberOfUrls == 0) {
			throw Exception(
					"Extractor::Database::querySetUrlsFinishedIfLockOk():"
					" No URLs have been specified"
			);
		}

		// create UPDATE SET clause
		std::string sqlQueryString{
				"UPDATE `"
		};

		sqlQueryString += this->extractingTable;
		sqlQueryString += "`"
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
		std::string sqlQueryString{
			"UPDATE `"
		};

		sqlQueryString += this->extractingTable;
		sqlQueryString +=
			"`"
			" SET locktime = NULL"
			" WHERE target = ";
		sqlQueryString += std::to_string(this->targetTableId);
		sqlQueryString +=
			" AND"
			" (";

		for(std::size_t n{1}; n <= numberOfUrls; ++n) {
			sqlQueryString += " url = ?";

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

} /* namespace crawlservpp::Module::Extractor */
