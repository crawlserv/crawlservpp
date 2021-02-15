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
 * This class provides database functionality for an analyzer thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.hpp"

namespace crawlservpp::Module::Analyzer {

	/*
	 * CONSTRUCTION
	 */

	//! Constructor setting the database connection for the thread.
	/*!
	 * \param dbThread Reference to the database
	 * 	 connection used by the analyzer thread.
	 */
	Database::Database(Module::Database& dbThread)
							: Wrapper::Database(dbThread) {}

	/*
	 * SETTERS
	 */

	//! Sets the name of the target table.
	/*!
	 * \param table Constant reference to a
	 *   string containing the name of the
	 *   target table.
	 */
	void Database::setTargetTable(const std::string& table) {
		this->targetTableName = table;

		this->log(this->getLoggingMin(), "uses target table '" + table + "'.");
	}

	//! Sets the fields of the target table and their types.
	/*!
	 * The names and the types correspondend
	 *  to each other via the indices in their
	 *  respective vector.
	 *
	 * \note None of the fields or types might
	 *  be empty, and both vectors need to
	 *  contain the same number of elements.
	 *  Otherwise, a MySQL error will occur and
	 *  an exception will be thrown when
	 *  calling initTargetTable() to initialize
	 *  the target table of the analyzer.
	 *
	 * \param fields Constant reference to a
	 *   vector of pars of string containing
	 *   both the names and the SQL data types
	 *   of the fields in the target table.
	 */
	void Database::setTargetFields(const std::vector<StringString>& fields) {
		this->targetFields = fields;
	}

	//! Sets the size of corpus chunks, in percentage of the maximum package size allowed by the MySQL server.
	/*!
	 * \param percentageOfMaxAllowedPackageSize
	 *   Maximum size of the text corpus
	 *   chunks, in percentage of the maximum
	 *   package size allowed by the MySQL
	 *   server. Must be between 1 and 99.
	 */
	void Database::setCorpusSlicing(std::uint8_t percentageOfMaxAllowedPackageSize) {
		this->corpusSlicing = percentageOfMaxAllowedPackageSize;
	}

	//! Sets the callback function for checking whether the thread is still running.
	/*!
	 * This function is needed to interrupt
	 *  corpus creation in case the thread
	 *  is interrupted.
	 */
	void Database::setIsRunningCallback(const IsRunningCallback& isRunningCallback) {
		this->isRunning = isRunningCallback;
	}

	/*
	 * TARGET TABLE INITIALIZATION AND UPDATE
	 */

	//! Creates the target table, or adds the field columns, if they do not exist already.
	/*!
	 * \note Needs to be called by the
	 *   algorithm class in order to create
	 *   the full target table name and the
	 *   required target fields.
	 *
	 * \param compressed Set whether to
	 *   compress the data in the target
	 *   table.
	 * \param clear Set whether to clear
	 *   a previously existing target table.
	 *
	 * \throws Analyzer::Database::Exception
	 *   if no website or URL list has been
	 *   previously specified, the name of
	 *   the target table is empty, no
	 *   target fields have been specified,
	 *   or the data type for a target field
	 *   is missing.
	 *
	 * \sa setTargetTable, setTargetFields
	 *
	 */
	void Database::initTargetTable(bool compressed, bool clear) {
		// check options
		if(this->getOptions().websiteNamespace.empty()) {
			throw Exception(
					"Analyzer::Database::initTargetTable():"
					" No website has been specified"
			);
		}

		if(this->getOptions().urlListNamespace.empty()) {
			throw Exception(
					"Analyzer::Database::initTargetTable():"
					" No URL list has been specified"
			);
		}

		if(this->targetTableName.empty()) {
			throw Exception(
					"Analyzer::Database::initTargetTable():"
					" The name of the target table is empty"
			);
		}

		if(
				std::all_of(
						this->targetFields.cbegin(),
						this->targetFields.cend(),
						[](const auto& nameType) {
							return nameType.first.empty();
						}
				)
		) {
			throw Exception(
					"Analyzer::Database::initTargetTable():"
					" No target fields have been specified"
					" (only empty strings)"
			);
		}

		// create the name of the target table
		this->targetTableFull = "crawlserv_";
		this->targetTableFull += this->getOptions().websiteNamespace;
		this->targetTableFull += "_";
		this->targetTableFull += this->getOptions().urlListNamespace;
		this->targetTableFull += "_analyzed_";
		this->targetTableFull += this->targetTableName;

		// create the properties of the target table
		CustomTableProperties propertiesTarget(
				"analyzed",
				this->getOptions().websiteId,
				this->getOptions().urlListId,
				this->targetTableName,
				this->targetTableFull,
				compressed
		);

		for(const auto& field : this->targetFields) {
			if(field.first.empty()) {
				continue;
			}

			propertiesTarget.columns.emplace_back(
					"analyzed__" + field.first,
					field.second
			);

			if(propertiesTarget.columns.back().type.empty()) {
				throw Exception(
						"Analyzer::Database::initTargetTable():"
						" No type for target field '"
						+ field.first
						+ "' has been specified"
				);
			}
		}

		// add or update the target table
		this->targetTableId = this->addOrUpdateTargetTable(propertiesTarget);

		if(clear) {
			this->clearTable(this->targetTableFull);

			this->log(
					this->getLoggingMin(),
					"cleared target table '"
					+ this->targetTableName
					+ "'."
			);
		}
	}

	//! Updates the target table.
	/*!
	 * Sets the time that specifies, when the target
	 *  table has last been updated, to now – i.e.
	 *  the current database time.
	 *
	 * \throws Module::Analyzer::Database::Exception
	 *   if the prepared SQL statements for setting
	 *   the update time of the target table is
	 *   missing.
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
					"Analyzer::Database::updateTargetTable():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.updateTargetTable)};

		try {
			// execute SQL query
			if(Database::sqlExecuteUpdate(sqlStatement) > 0) {
				this->log(
						this->getLoggingMin(),
						"updated target table '"
						+ this->targetTableName
						+ "'."
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::updateTargetTable", e);
		}
	}

	/*
	 * ADDITIONAL TABLE INITIALIZATION AND UPDATE
	 */

	//! Creates an additioanl table, or adds its field columns, if they do not exist already.
	/*!
	 * \note Can be called by the algorithm
	 *   class to create another full table
	 *   name and the specified target fields.
	 *
	 * \param name The name of the additional
	 *   table.
	 * \param compressed Set whether to
	 *   compress the data in the table.
	 * \param clear Set whether to clear
	 *   a previously existing table.
	 *
	 * \returns The ID of the additional
	 *   table.
	 *
	 * \throws Analyzer::Database::Exception
	 *   if no website or URL list has been
	 *   previously specified, the name of
	 *   the additional table is empty, no
	 *   target fields have been specified,
	 *   or the data type for a target field
	 *   is missing.
	 */
	std::size_t Database::addAdditionalTable(
			const std::string& name,
			const std::vector<StringString>& fields,
			bool compressed,
			bool clear
	) {
		// check options
		if(this->getOptions().websiteNamespace.empty()) {
			throw Exception(
					"Analyzer::Database::addAdditionalTable():"
					" No website has been specified"
			);
		}

		if(this->getOptions().urlListNamespace.empty()) {
			throw Exception(
					"Analyzer::Database::addAdditionalTable():"
					" No URL list has been specified"
			);
		}

		if(name.empty()) {
			throw Exception(
					"Analyzer::Database::addAdditionalTable():"
					" The name of the additional table is empty"
			);
		}

		if(
				std::all_of(
						fields.cbegin(),
						fields.cend(),
						[](const auto& nameType) {
							return nameType.first.empty();
						}
				)
		) {
			throw Exception(
					"Analyzer::Database::addAdditionalTable():"
					" No table fields have been specified"
					" (only empty strings)"
			);
		}

		// create the name of the target table
		std::string fullTableName{"crawlserv_"};

		fullTableName += this->getOptions().websiteNamespace;
		fullTableName += "_";
		fullTableName += this->getOptions().urlListNamespace;
		fullTableName += "_analyzed_";
		fullTableName += name;

		// create the properties of the target table
		CustomTableProperties tableProperties(
				"analyzed",
				this->getOptions().websiteId,
				this->getOptions().urlListId,
				name,
				fullTableName,
				compressed
		);

		for(const auto& field : fields) {
			if(field.first.empty()) {
				continue;
			}

			tableProperties.columns.emplace_back(
					"analyzed__" + field.first,
					field.second
			);

			if(tableProperties.columns.back().type.empty()) {
				throw Exception(
						"Analyzer::Database::addAdditionalTable():"
						" No type for table field '"
						+ field.first
						+ "' has been specified"
				);
			}
		}

		// add or update the target table
		const std::size_t id{this->addOrUpdateTargetTable(tableProperties)};

		if(clear) {
			this->clearTable(fullTableName);

			this->log(this->getLoggingMin(), "cleared table '" + name + "'.");
		}

		this->additionalTables.emplace(id, fullTableName);

		return id;
	}

	//! Gets the full name of an additional table.
	/*!
	 * \param id The ID of the additional table.
	 *
	 * \returns A constant reference to a string
	 *   containing the full name of the
	 *   additional table in the database.
	 *
	 * \throws Main::Database::Exception if the
	 *   given ID does not identify an additional
	 *   table.
	 *
	 * \sa addAdditionalTable
	 */
	const std::string& Database::getAdditionalTableName(std::size_t id) const {
		// check argument
		const auto it{
			this->additionalTables.find(id)
		};

		if(it == this->additionalTables.end()) {
			throw Exception(
					"Analyzer::Database::getAdditionalTableName():"
					" Invalid additional table ID '" + std::to_string(id) + "'"
			);
		}

		return it->second;
	}

	//! Updates an additional table.
	/*!
	 * Sets the time that specifies, when the
	 *  table has last been updated, to now – i.e.
	 *  the current database time.
	 *
	 * \param id The ID of the additional table.
	 *
	 * \throws Module::Analyzer::Database::Exception
	 *   if the prepared SQL statements for setting
	 *   the update time of additional tables is
	 *   missing or the given ID does not identify
	 *   an additional table.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while setting the update time
	 *   of the additional table in the database.
	 *
	 * \sa addAdditionalTable
	 */
	void Database::updateAdditionalTable(std::size_t id) {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.updateAdditionalTable == 0) {
			throw Exception(
					"Analyzer::Database::updateAdditionalTable():"
					" Missing prepared SQL statement"
			);
		}

		// check argument
		const auto it{
			this->additionalTables.find(id)
		};

		if(it == this->additionalTables.end()) {
			throw Exception(
					"Analyzer::Database::updateAdditionalTable():"
					" Invalid additional table ID '" + std::to_string(id) + "'"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{this->getPreparedStatement(this->ps.updateAdditionalTable)};

		try {
			// execute SQL query
			sqlStatement.setUInt64(sqlArg1, id);

			if(Database::sqlExecuteUpdate(sqlStatement) > 0) {
				this->log(this->getLoggingMin(), "updated table '" + it->second + "'.");
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::updateAdditionalTable", e);
		}
	}

	/*
	 * PREPARED SQL STATEMENTS
	 */

	//! Prepares the SQL statements for the analyzer.
	/*!
	 * \note The target table needs to be
	 *   prepared first.
	 *
	 * \throws Main::Database::Exception if
	 *   a MySQL error occurs during the
	 *   preparation of the SQL statements.
	 *
	 */
	void Database::prepare() {
		const auto verbose{this->getLoggingVerbose()};

		// create table prefix
		this->tablePrefix = "crawlserv_";

		if(!(this->getOptions().websiteNamespace.empty())) {
			this->tablePrefix += this->getOptions().websiteNamespace;

			this->tablePrefix.push_back('_');
		}

		if(!(this->getOptions().urlListNamespace.empty())) {
			this->tablePrefix += this->getOptions().urlListNamespace;

			this->tablePrefix.push_back('_');
		}

		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(sizeof(this->ps) / sizeof(std::size_t));

		try {
			// prepare SQL statements for analyzer
			this->log(verbose, "prepares getCorpus() [1/5]...");

			this->addPreparedStatement(
					"SELECT created"
					" FROM `crawlserv_corpora`"
					" WHERE website = "	+ this->getWebsiteIdString() +
					" AND urllist = " + this->getUrlListIdString() +
					" AND source_type = ?"
					" AND source_table LIKE ?"
					" AND source_field LIKE ?"
					" AND previous IS NULL"
					" ORDER BY created DESC"
					" LIMIT 1",
					this->ps.getCorpusInfo
			);

			this->log(verbose, "prepares getCorpus() [2/5]...");

			this->addPreparedStatement(
					"SELECT EXISTS("
						" SELECT 1"
						" FROM `crawlserv_corpora`"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND source_type = ?"
						" AND source_table LIKE ?"
						" AND source_field LIKE ?"
						" AND created LIKE ?"
						" AND savepoint LIKE ?"
						" AND previous IS NULL"
					") AS result",
					this->ps.checkCorpusSavePoint
			);

			this->log(verbose, "prepares getCorpus() [3/5]...");

			this->addPreparedStatement(
					"SELECT id, corpus, articlemap, datemap, sources, chunks"
					" FROM `crawlserv_corpora`"
					" USE INDEX(urllist)"
					" WHERE website = "	+ this->getWebsiteIdString() +
					" AND urllist = " + this->getUrlListIdString() +
					" AND source_type = ?"
					" AND source_table LIKE ?"
					" AND source_field LIKE ?"
					" AND created LIKE ?"
					" AND savepoint IS NULL"
					" AND previous IS NULL"
					" ORDER BY created DESC"
					" LIMIT 1",
					this->ps.getCorpusFirst
			);

			this->log(verbose, "prepares getCorpus() [4/5]...");

			this->addPreparedStatement(
					"SELECT id, corpus, articlemap, datemap, sentencemap, sources, chunks, words"
					" FROM `crawlserv_corpora`"
					" USE INDEX(urllist)"
					" WHERE website = "	+ this->getWebsiteIdString() +
					" AND urllist = " + this->getUrlListIdString() +
					" AND source_type = ?"
					" AND source_table LIKE ?"
					" AND source_field LIKE ?"
					" AND created LIKE ?"
					" AND savepoint LIKE ?"
					" AND previous IS NULL"
					" ORDER BY created DESC"
					" LIMIT 1",
					this->ps.getCorpusSavePoint
			);

			this->log(verbose, "prepares getCorpus() [5/5]...");

			this->addPreparedStatement(
					"SELECT id, corpus, articlemap, datemap, sentencemap, words"
					" FROM `crawlserv_corpora`"
					" WHERE previous = ?"
					" LIMIT 1",
					this->ps.getCorpusNext
			);

			this->log(verbose, "prepares isCorpusChanged() [1/4]...");

			this->addPreparedStatement(
					"SELECT EXISTS"
					" ("
						" SELECT *"
						" FROM `crawlserv_corpora`"
						" USE INDEX(urllist)"
						" WHERE website = "	+ this->getWebsiteIdString() + ""
						" AND urllist = " + this->getUrlListIdString() +
						" AND source_type = ?"
						" AND source_table LIKE ?"
						" AND source_field LIKE ?"
						" AND ("
							" savepoint IS NULL"
							" OR savepoint LIKE LEFT(?, LENGTH(savepoint))"
						" )"
						" AND created > ?"
					" )"
					" AS result",
					this->ps.isCorpusChanged
			);

			this->log(verbose, "prepares isCorpusChanged() [2/4]...");

			this->addPreparedStatement(
					"SELECT updated"
					" FROM `crawlserv_parsedtables`"
					" USE INDEX(urllist)"
					" WHERE website = "	+ this->getWebsiteIdString() +
					" AND urllist = " + this->getUrlListIdString() +
					" AND name = ?",
					this->ps.isCorpusChangedParsing
			);

			this->log(verbose, "prepares isCorpusChanged() [3/4]...");

			this->addPreparedStatement(
					"SELECT updated"
					" FROM `crawlserv_extractedtables`"
					" USE INDEX(urllist)"
					" WHERE website = "	+ this->getWebsiteIdString() +
					" AND urllist = " + this->getUrlListIdString() +
					" AND name = ?",
					this->ps.isCorpusChangedExtracting
			);

			this->log(verbose, "prepares isCorpusChanged() [4/4]...");

			this->addPreparedStatement(
					"SELECT updated"
					" FROM `crawlserv_analyzedtables`"
					" USE INDEX(urllist)"
					" WHERE website = "	+ this->getWebsiteIdString() +
					" AND urllist = " + this->getUrlListIdString() +
					" AND name = ?",
					this->ps.isCorpusChangedAnalyzing
			);

			this->log(verbose, "prepares createCorpus() [1/5]...");

			this->addPreparedStatement(
					"DELETE"
					" FROM `crawlserv_corpora`"
					" WHERE website = " + this->getWebsiteIdString() +
					" AND urllist = " + this->getUrlListIdString() +
					" AND source_type = ?"
					" AND source_table LIKE ?"
					" AND source_field LIKE ?",
					this->ps.deleteCorpus
			);

			this->log(verbose, "prepares createCorpus() [2/5]...");

			this->addPreparedStatement(
					"INSERT INTO `crawlserv_corpora`"
					" ("
						" website,"
						" urllist,"
						" source_type,"
						" source_table,"
						" source_field,"
						" corpus,"
						" articlemap,"
						" datemap,"
						" previous,"
						" sources,"
						" chunks"
					") "
					"VALUES"
					" (" +
						this->getWebsiteIdString() + ", " +
						this->getUrlListIdString() + ","
						" ?,"
						" ?,"
						" ?,"
						" ?,"
						" CONVERT( ? USING utf8mb4 ),"
						" CONVERT( ? USING utf8mb4 ),"
						" ?,"
						" ?,"
						" ?"
					")",
					this->ps.addChunkContinuous
			);

			this->log(verbose, "prepares createCorpus() [3/5]...");

			this->addPreparedStatement(
					"INSERT INTO `crawlserv_corpora`"
					" ("
						" website,"
						" urllist,"
						" source_type,"
						" source_table,"
						" source_field,"
						" savepoint,"
						" corpus,"
						" articlemap,"
						" datemap,"
						" sentencemap,"
						" previous,"
						" sources,"
						" chunks,"
						" words"
					") "
					"VALUES"
					" (" +
						this->getWebsiteIdString() + ", " +
						this->getUrlListIdString() + ","
						" ?,"
						" ?,"
						" ?,"
						" ?,"
						" ?,"
						" CONVERT( ? USING utf8mb4 ),"
						" CONVERT( ? USING utf8mb4 ),"
						" CONVERT( ? USING utf8mb4 ),"
						" ?,"
						" ?,"
						" ?,"
						" ?"
					")",
					this->ps.addChunkTokenized
			);

			this->log(verbose, "prepares createCorpus() [4/5]...");

			this->addPreparedStatement(
					"UPDATE `crawlserv_corpora`"
					" SET chunk_length = CHAR_LENGTH(corpus),"
					" chunk_size = LENGTH(corpus)"
					" WHERE id = ?"
					" LIMIT 1",
					this->ps.measureChunk
			);

			this->log(verbose, "prepares createCorpus() [5/5]...");

			this->addPreparedStatement(
					"UPDATE `crawlserv_corpora` AS dest,"
					" ("
						"SELECT SUM(chunk_size) AS size, SUM(chunk_length) AS length"
						" FROM `crawlserv_corpora`"
						" USE INDEX(urllist)"
						" WHERE website = " + this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND source_type = ?"
						" AND source_table LIKE ?"
						" AND source_field LIKE ?"
						" LIMIT 1"
					") AS src"
					" SET"
					" dest.length = src.length,"
					" dest.size = src.size"
					" WHERE dest.website = " + this->getWebsiteIdString() +
					" AND dest.urllist = " + this->getUrlListIdString() +
					" AND dest.source_type = ?"
					" AND dest.source_table LIKE ?"
					" AND dest.source_field LIKE ?",
					this->ps.measureCorpus
			);

			this->log(verbose, "prepares updateTargetTable()...");

			this->addPreparedStatement(
					"UPDATE crawlserv_analyzedtables"
					" SET updated = CURRENT_TIMESTAMP"
					" WHERE id = " + std::to_string(this->targetTableId) +
					" LIMIT 1",
					this->ps.updateTargetTable
			);

			this->log(verbose, "prepares updateAdditionalTable()...");

			this->addPreparedStatement(
					"UPDATE crawlserv_analyzedtables"
					" SET updated = CURRENT_TIMESTAMP"
					" WHERE id = ?"
					" LIMIT 1",
					this->ps.updateAdditionalTable
			);
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::prepare", e);
		}
	}

	/*
	 * TEXT CORPUS
	 */

	//! Gets the text corpus after creating it if it is out-of-date or does not yet exist.
	/*!
	 * \param corpusProperties Constant reference
	 *   to structure containing the properties of
	 *   the text corpus.
	 * \param filterDateFrom Constant reference to
	 *   a string contaning the starting date from
	 *   which on the text corpus should be
	 *   created, or to an empty string if the
	 *   source of the corpus should not be
	 *   filtered by a starting date. Can only be
	 *   used if the source of the corpus is parsed
	 *   data.
	 * \param filterDateTo Constant reference to
	 *   a string contaning the ending date until
	 *   which the text corpus should be created,
	 *   or to an empty string if the source of
	 *   the corpus should not be filtered by an
	 *   ending date. Can only be used if the
	 *   source of the corpus is parsed data.
	 * \param corpusTo Reference to which the
	 *   resulting text corpus should be written.
	 * \param sourcesTo Reference to which the
	 *   number of sources used when creating the
	 *   text corpus should be written.
	 * \param statusSetter Data needed to keep
	 *   the status of the thread updated.
	 *
	 * \returns True, if the thread is still
	 *   running. False otherwise.
	 *
	 * \throws Module::Analyzer::Exception if a
	 *   prepared SQL statement is missing, or
	 *   an article or date map could not be parsed.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while getting or creating
	 *   the text corpus.
	 */
	bool Database::getCorpus(
			const CorpusProperties& corpusProperties,
			const std::string& filterDateFrom,
			const std::string& filterDateTo,
			Data::Corpus& corpusTo,
			std::size_t& sourcesTo,
			StatusSetter& statusSetter
	) {
		// check arguments
		if(corpusProperties.sourceTable.empty()) {
			this->log(
					this->getLoggingMin(),
					"WARNING:"
					" The name of the source table is empty."
			);

			return statusSetter.isRunning();
		}

		if(corpusProperties.sourceColumn.empty()) {
			this->log(
					this->getLoggingMin(),
					"WARNING:"
					" The name of the source field is empty."
			);

			return statusSetter.isRunning();
		}

		// initialize values
		corpusTo.clear();

		sourcesTo = 0;

		// copy properties
		CorpusProperties properties(corpusProperties);

		{ // wait for source table lock
			DatabaseLock(
					*this,
					"corpusCreation."
					+ std::to_string(properties.sourceType)
					+ "."
					+ properties.sourceTable
					+ "."
					+ properties.sourceColumn,
					this->isRunning
			);

			if(!(this->isRunning())) {
				return false;
			}

			// check whether text corpus needs to be created
			if(this->corpusIsChanged(properties)) {
				this->corpusCreate(properties, corpusTo, sourcesTo, statusSetter);
			}
			else {
				this->corpusLoad(properties, corpusTo, sourcesTo, statusSetter);
			}
		}

		if(!(this->isRunning())) {
			return false;
		}

		// run missing manipulators on corpus
		if(!this->corpusManipulate(properties, corpusTo, sourcesTo, statusSetter)) {
			return false;
		}

		// start timer
		Timer::Simple timer;

		// filter corpus by date(s) if necessary
		if(corpusTo.filterByDate(filterDateFrom, filterDateTo)) {
			// log new corpus size
			std::ostringstream logStrStr;

			logStrStr.imbue(std::locale(""));

			logStrStr	<< "filtered corpus (by date) to "
						<< corpusTo.size()
						<< " bytes in "
						<< timer.tickStr()
						<< ".";

			this->log(this->getLoggingMin(), logStrStr.str());
		}

		return statusSetter.isRunning();
	}

	/*
	 * PUBLIC HELPERS
	 */

	//! Public helper function getting the full name of a source table.
	/*!
	 * \param type The type of the source for
	 *   which the full name of its table will
	 *   be retrieved.
	 * \param name Constant reference to a
	 *   string containing the name of the
	 *   source table whose full name will
	 *   be retrieved.
	 *
	 * \returns A copy of the full name of the
	 *   source table, including table prefixes.
	 *
	 * \throws Analyzer::Database::Exception if
	 *   the given source type is invalid.
	 *
	 * \sa generalInputSourcesParsing,
	 *   generalInputSourcesExtracting,
	 *   generalInputSourcesAnalyzing,
	 *   generalInputSourcesCrawling
	 */
	std::string Database::getSourceTableName(std::uint16_t type, const std::string& name) const {
		switch(type) {
		case generalInputSourcesParsing:
			return this->tablePrefix + "parsed_" + name;

		case generalInputSourcesExtracting:
			return this->tablePrefix + "extracted_" + name;

		case generalInputSourcesAnalyzing:
			return this->tablePrefix + "analyzed_" + name;

		case generalInputSourcesCrawling:
			return this->tablePrefix + "crawled";

		default:
			throw Exception(
					"Analyzer::Database::getSourceTableName():"
					" Invalid source type for text corpus"
			);
		}
	}

	//! Public helper function getting the full name of a source column.
	/*!
	 * \param type The type of the source for
	 *   which the full name of a column will
	 *   be retrieved.
	 * \param name Constant reference to a string
	 *   containing the name of the source column
	 *   whose full name will be retrieved.
	 *
	 * \returns A copy of the full name of the
	 *   source column.
	 *
	 * \throws Analyzer::Database::Exception if
	 *   the given source type is invalid.
	 *
	 * \sa generalInputSourcesParsing,
	 *   generalInputSourcesExtracting,
	 *   generalInputSourcesAnalyzing,
	 *   generalInputSourcesCrawling
	 */
	std::string Database::getSourceColumnName(std::uint16_t type, const std::string& name) {
		switch(type) {
		case generalInputSourcesParsing:
			if(name == "id") {
				return "parsed_id";
			}
			else if(name == "datetime") {
				return "parsed_datetime";
			}

			return "parsed__" + name;

		case generalInputSourcesExtracting:
			return "extracted__" + name;

		case generalInputSourcesAnalyzing:
			return "analyzed__" + name;

		case generalInputSourcesCrawling:
			return name;

		default:
			throw Exception(
					"Analyzer::Database::getSourceColumnName():"
					" Invalid source type for text corpus"
			);
		}
	}

	//! Public helper function checking the given data sources.
	/*!
	 * Removes all invalid sources.
	 *
	 * The types, table names, and column
	 *  names of the sources need to
	 *  correspond to each other via the
	 *  indices of their respective vectors.
	 *
	 * \param types Constant reference to a
	 *   vector containing the types of the
	 *   sources to be checked.
	 * \param tables Constant reference to
	 *   a vector of strings containing the
	 *   names of the source tables to be
	 *   checked.
	 * \param columns Constant reference to
	 *   a vector of strings containing the
	 *   names of the source columns to be
	 *   checked.
	 *
	 * \throws Analyzer::Database::Exception if
	 *   no sources have been specified.
	 *
	 * \sa generalInputSourcesParsing,
	 *   generalInputSourcesExtracting,
	 *   generalInputSourcesAnalyzing,
	 *   generalInputSourcesCrawling,
	 *   checkSource
	 */
	void Database::checkSources(
			std::vector<std::uint8_t>& types,
			std::vector<std::string>& tables,
			std::vector<std::string>& columns
	) {
		// remove invalid sources
		for(std::size_t n{1}; n <= tables.size(); ++n) {
			if(
					!(this->checkSource(
							types.at(n - 1),
							tables.at(n - 1),
							columns.at(n - 1)
					))
			) {
				--n;

				types.erase(types.begin() + n);
				tables.erase(tables.begin() + n);
				columns.erase(columns.begin() + n);
			}
		}

		// check for valid sources
		if(types.empty() || tables.empty() || columns.empty()) {
			throw Exception(
					"Analyzer::Database::checkSources():"
					" No sources have been specified"
			);
		}
	}

	/*
	 * INTERNAL HELPER FUNCTION (private)
	 */

	// check the given data source
	bool Database::checkSource(
			std::uint16_t type,
			const std::string& table,
			const std::string& column
	) {
		// get full table name
		const auto tableName{this->getSourceTableName(type, table)};

		// check existence of table
		if(this->database.isTableExists(tableName)) {
			// get full column name
			const auto columnName{getSourceColumnName(type, column)};

			// check existence of column
			if(!(this->database.isColumnExists(tableName, columnName))) {
				this->log(
						this->getLoggingMin(),
						"WARNING: Non-existing column `"
						+ columnName
						+ "` in input table `"
						+ tableName
						+ "` ignored"
				);

				return false;
			}
		}
		else {
			this->log(
					this->getLoggingMin(),
					"WARNING: Non-existing input table `"
					+ tableName
					+ "` ignored"
			);

			return false;
		}

		return true;
	}

	/*
	 * INTERNAL CORPUS FUNCTIONS (private)
	 */

	// checks whether the source of the corpus has changed, throws Corpus::Exception
	bool Database::corpusIsChanged(const CorpusProperties& properties) {
		bool result{true};

		// check connection to database
		this->checkConnection();

		// check prepared SQL statements
		if(this->ps.isCorpusChanged == 0) {
			throw Exception(
					"Analyzer::Database::isCorpusChanged():"
					" Missing prepared SQL statement"
					" for getting the corpus creation time"
			);
		}

		// get prepared SQL statement
		auto& corpusStatement{
			this->getPreparedStatement(this->ps.isCorpusChanged)
		};

		std::uint16_t sourceStatement{};

		switch(properties.sourceType) {
		case generalInputSourcesParsing:
			sourceStatement = this->ps.isCorpusChangedParsing;

			break;

		case generalInputSourcesExtracting:
			sourceStatement = this->ps.isCorpusChangedExtracting;

			break;

		case generalInputSourcesAnalyzing:
			sourceStatement = this->ps.isCorpusChangedAnalyzing;

			break;

		case generalInputSourcesCrawling:
			return true; // always re-create corpus for crawling data

		default:
			throw Exception(
					"Analyzer::Database::isCorpusChanged():"
					" Invalid source type for the text corpus"
			);
		}

		if(sourceStatement == 0) {
			throw Exception(
					"Analyzer::Database::isCorpusChanged():"
					" Missing prepared SQL statement for checking"
					" the source of a text corpus from the"
					" specified source type"
			);
		}

		auto& tableStatement{
			this->getPreparedStatement(sourceStatement)
		};

		try {
			// execute SQL query for getting the update time of the source table
			tableStatement.setString(sqlArg1, properties.sourceTable);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(tableStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				const std::string updateTime{sqlResultSet->getString("updated")};

				// execute SQL query for comparing the creation time of the corpus with the update time of the table
				corpusStatement.setUInt(sqlArg1, properties.sourceType);
				corpusStatement.setString(sqlArg2, properties.sourceTable);
				corpusStatement.setString(sqlArg3, properties.sourceColumn);

				if(
						properties.wordManipulators.empty()
						&& properties.sentenceManipulators.empty()
				) {
					corpusStatement.setNull(sqlArg4, 0);
				}
				else {
					std::string lastSavePoint;

					for(std::size_t n{}; n < properties.wordManipulators.size(); ++n) {
						lastSavePoint += 'w';
						lastSavePoint += std::to_string(properties.wordManipulators[n]);
						lastSavePoint += '[';

						if(properties.wordModels.size() > n) {
							lastSavePoint += properties.wordModels[n];
						}

						lastSavePoint += ']';
					}

					for(std::size_t n{}; n < properties.sentenceManipulators.size(); ++n) {
						lastSavePoint += 's';
						lastSavePoint += std::to_string(properties.sentenceManipulators[n]);
						lastSavePoint += '[';

						if(properties.sentenceModels.size() > n) {
							lastSavePoint += properties.sentenceModels[n];
						}

						lastSavePoint += ']';
					}

					corpusStatement.setString(sqlArg4, lastSavePoint);
				}

				corpusStatement.setString(sqlArg5, updateTime);

				SqlResultSetPtr(Database::sqlExecuteQuery(corpusStatement)).swap(sqlResultSet);

				// get result
				if(sqlResultSet && sqlResultSet->next()) {
					result = !(sqlResultSet->getBoolean("result"));
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::isCorpusChanged", e);
		}

		return result;
	}

	// creates corpus from scratch
	void Database::corpusCreate(
			const CorpusProperties& properties,
			Data::Corpus& corpusTo,
			std::size_t& sourcesTo,
			StatusSetter& statusSetter
	) {
		// initialize values
		corpusTo.clear();

		auto savePoints{
			properties.savePoints
		};

		std::sort(savePoints.begin(), savePoints.end());

		// check connection to database
		this->checkConnection();

		// check prepared SQL statements
		if(this->ps.deleteCorpus == 0) {
			throw Exception(
					"Analyzer::Database::corpusCreate():"
					" Missing prepared SQL statement"
					" for deleting the text corpus"
			);
		}

		if(this->ps.addChunkContinuous == 0) {
			throw Exception(
					"Analyzer::Database::corpusCreate():"
					" Missing prepared SQL statement"
					" for adding a continuous corpus chunk"
					" to the database"
			);
		}

		if(this->ps.measureChunk == 0) {
			throw Exception(
					"Analyzer::Database::corpusCreate():"
					" Missing prepared SQL statement"
					" for measuring a text corpus chunk"
			);
		}

		if(this->ps.measureCorpus == 0) {
			throw Exception(
					"Analyzer::Database::corpusCreate():"
					" Missing prepared SQL statement"
					" for measuring the text corpus"
			);
		}

		// get prepared SQL statements
		auto& deleteStatement{
			this->getPreparedStatement(this->ps.deleteCorpus)
		};
		auto& addStatementContinuous{
			this->getPreparedStatement(this->ps.addChunkContinuous)
		};
		auto& measureChunkStatement{
			this->getPreparedStatement(this->ps.measureChunk)
		};
		auto& measureCorpusStatement{
			this->getPreparedStatement(this->ps.measureCorpus)
		};

		// check your sources
		this->checkSource(
				properties.sourceType,
				properties.sourceTable,
				properties.sourceColumn
		);

		// show warning when using raw crawled data and logging is enabled
		if(properties.sourceType == generalInputSourcesCrawling) {
			this->log(
					this->getLoggingMin(),
					"WARNING:"
					" Corpus will always be re-created"
					" when created from raw crawled data."
			);
			this->log(
					this->getLoggingMin(),
					"It is highly recommended"
					" to use parsed data instead!"
			);

			if(!properties.sourceTable.empty()) {
				this->log(
						this->getLoggingMin(),
						"WARNING:"
						" Source table name ignored."
				);
			}

			if(!properties.sourceColumn.empty()) {
				this->log(
						this->getLoggingMin(),
						"WARNING:"
						" Source field name ignored."
				);
			}
		}

		// start timing and write log entry
		Timer::Simple timer;

		const auto tableName{
			this->getSourceTableName(
					properties.sourceType,
					properties.sourceTable
			)
		};
		const auto columnName{
			getSourceColumnName(
					properties.sourceType,
					properties.sourceColumn
			)
		};

		this->log(
				this->getLoggingMin(),
				"compiles text corpus from "
				+ tableName
				+ "."
				+ columnName
				+ "..."
		);

		bool saveCorpus{
			!savePoints.empty()
			&& savePoints.at(0) == 0 /* (save points are unsigned and sorted) */
		};

		try {
			if(saveCorpus) {
				// execute SQL query for deleting old text corpus (if it exists)
				deleteStatement.setUInt(sqlArg1, properties.sourceType);
				deleteStatement.setString(sqlArg2, properties.sourceTable);
				deleteStatement.setString(sqlArg3, properties.sourceColumn);

				Database::sqlExecute(deleteStatement);

				if(!statusSetter.update(progressDeletedCorpus, false)) {
					return;
				}
			}

			// get texts and possibly parsed date/times and IDs from database
			Data::GetColumns data;

			data.table = tableName;

			if(properties.sourceType == generalInputSourcesParsing) {
				data.columns.reserve(maxNumCorpusColumns);
			}

			data.columns.emplace_back(columnName);

			if(properties.sourceType == generalInputSourcesParsing) {
				data.columns.emplace_back("parsed_id");
				data.columns.emplace_back("parsed_datetime");

				data.order.reserve(2);

				data.order.emplace_back("parsed_datetime");
				data.order.emplace_back("parsed_id");
			}

			data.type = DataType::_string;

			this->getCustomData(data);

			if(data.values.empty()) {
				throw Exception(
						"Analyzer::Database::corpusCreate():"
						" Could not get requested data"
						" from database"
				);
			}

			if(!statusSetter.update(progressReceivedSources, false)) {
				return;
			}

			// move received column data to vector(s) of strings
			std::vector<std::string> texts;
			std::vector<std::string> articleIds;
			std::vector<std::string> dateTimes;

			texts.reserve(data.values.at(column1).size());

			if(data.values.size() > numColumns1) {
				articleIds.reserve(data.values.at(column2).size());

				if(data.values.size() > numColumns2) {
					dateTimes.reserve(data.values.at(column3).size());
				}
			}

			for(auto it{data.values.at(column1).begin()}; it != data.values.at(column1).end(); ++it) {
				const auto index{
					it - data.values.at(column1).begin()
				};
				auto& text{
					data.values.at(column1).at(index)
				};

				if(!text._isnull && !text._s.empty()) {
					// add text as source
					++sourcesTo;

					// move text to vector
					texts.emplace_back(std::move(text._s));

					if(data.values.size() > numColumns1) {
						// move article ID to vector
						auto& articleId{data.values.at(column2).at(index)};

						if(!articleId._isnull && !articleId._s.empty()) {
							articleIds.emplace_back(std::move(articleId._s));
						}
						else {
							articleIds.emplace_back();
						}

						if(data.values.size() > numColumns2) {
							// move date/time to vector
							auto& dateTime{data.values.at(column3).at(index)};

							if(!dateTime._isnull && !dateTime._s.empty()) {
								dateTimes.emplace_back(std::move(dateTime._s));
							}
							else {
								dateTimes.emplace_back();
							}
						}
					}
				}
			}

			if(!statusSetter.update(progressMovedData, false)) {
				return;
			}

			// create corpus (and delete the input data)
			if(data.values.size() > numColumns1) {
				corpusTo.create(texts, articleIds, dateTimes, true);
			}
			else {
				corpusTo.create(texts, true);
			}

			if(!statusSetter.update(progressCreatedCorpus, false)) {
				return;
			}

			if(saveCorpus) {
				// slice continuous corpus into chunks for the database
				std::vector<std::string> chunks;
				std::vector<TextMap> articleMaps;
				std::vector<TextMap> dateMaps;

				corpusTo.copyChunksContinuous(
						static_cast<std::size_t>(
								this->getMaxAllowedPacketSize()
								* (
										static_cast<float>(this->corpusSlicing)
										* corpusSlicingFactor
								)
						),
						chunks,
						articleMaps,
						dateMaps
				);

				if(!statusSetter.update(progressSlicedCorpus, false)) {
					return;
				}

				// add corpus chunks to the database
				std::uint64_t last{};

				for(std::size_t n{}; n < chunks.size(); ++n) {
					addStatementContinuous.setUInt(sqlArg1, properties.sourceType);
					addStatementContinuous.setString(sqlArg2, properties.sourceTable);
					addStatementContinuous.setString(sqlArg3, properties.sourceColumn);
					addStatementContinuous.setString(sqlArg4, chunks.at(n));

					if(articleMaps.size() > n) {
						addStatementContinuous.setString(sqlArg5, Helper::Json::stringify(articleMaps.at(n)));
					}
					else {
						addStatementContinuous.setNull(sqlArg5, 0);
					}

					if(dateMaps.size() > n) {
						addStatementContinuous.setString(sqlArg6, Helper::Json::stringify(dateMaps.at(n)));
					}
					else {
						addStatementContinuous.setNull(sqlArg6, 0);
					}

					if(last > 0) {
						addStatementContinuous.setUInt64(sqlArg7, last);
					}
					else {
						addStatementContinuous.setNull(sqlArg7, 0);
					}

					addStatementContinuous.setUInt64(sqlArg8, sourcesTo);
					addStatementContinuous.setUInt64(sqlArg9, chunks.size());

					Database::sqlExecute(addStatementContinuous);

					last = this->getLastInsertedId();

					// free memory early
					std::string().swap(chunks.at(n));

					if(articleMaps.size() > n) {
						TextMap().swap(articleMaps.at(n));
					}

					if(dateMaps.size() > n) {
						TextMap().swap(dateMaps.at(n));
					}

					// measure chunk
					measureChunkStatement.setUInt64(sqlArg1, last);

					Database::sqlExecute(measureChunkStatement);

					statusSetter.update(
							progressSlicedCorpus
							+ progressAddingCorpus
							* (
									static_cast<float>(n + 1)
									/ chunks.size()
							),
							false
					);
				}
			}

			statusSetter.finish();
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::corpusCreate", e);
		}

		if(saveCorpus) {
			// check connection to database
			this->checkConnection();

			try {
				// measure corpus
				measureCorpusStatement.setUInt(sqlArg1, properties.sourceType);
				measureCorpusStatement.setString(sqlArg2, properties.sourceTable);
				measureCorpusStatement.setString(sqlArg3, properties.sourceColumn);
				measureCorpusStatement.setUInt(sqlArg4, properties.sourceType);
				measureCorpusStatement.setString(sqlArg5, properties.sourceTable);
				measureCorpusStatement.setString(sqlArg6, properties.sourceColumn);

				Database::sqlExecute(measureCorpusStatement);
			}
			catch(const sql::SQLException &e) {
				// log and ignore errors when measuring corpus (total text might be too long)
				this->log(
						this->getLoggingMin(),
						"WARNING: Could not measure corpus - "
						+ std::string(e.what()));
			}
		}

		// write log entry if necessary
		std::ostringstream logStrStr;

		logStrStr.imbue(std::locale(""));

		logStrStr	<< "compiled text corpus of "
					<< corpusTo.size()
					<< " bytes in "
					<< timer.tickStr()
					<< ".";

		this->log(this->getLoggingMin(), logStrStr.str());
	}

	// loads a corpus from the database
	void Database::corpusLoad(
			CorpusProperties& properties,
			Data::Corpus& corpusTo,
			std::size_t& sourcesTo,
			StatusSetter& statusSetter
	) {
		// start timer
		Timer::Simple timer;

		// get all the chunks of the corpus from the database
		std::vector<std::string> chunks;
		std::vector<std::size_t> wordNums;
		std::vector<TextMap> articleMaps;
		std::vector<TextMap> dateMaps;
		std::vector<SentenceMap> sentenceMaps;
		std::string savePoint;

		// check connection to database
		this->checkConnection();

		// check prepared SQL statements
		if(
				this->ps.getCorpusInfo == 0
				|| this->ps.getCorpusFirst == 0
				|| this->ps.getCorpusSavePoint == 0
				|| this->ps.getCorpusNext == 0
		) {
			throw Exception(
					"Analyzer::Database::corpusLoad():"
					" Missing prepared SQL statement(s)"
					" for getting the corpus"
			);
		}


		// get prepared SQL statements
		auto& sqlStatementInfo{
			this->getPreparedStatement(this->ps.getCorpusInfo)
		};
		auto& sqlStatementFirst{
			this->getPreparedStatement(this->ps.getCorpusFirst)
		};
		auto& sqlStatementSavePoint{
			this->getPreparedStatement(this->ps.getCorpusSavePoint)
		};
		auto& sqlStatementNext{
			this->getPreparedStatement(this->ps.getCorpusNext)
		};

		try {
			// execute SQL query for getting the creation date of the corpus
			sqlStatementInfo.setUInt(sqlArg1, properties.sourceType);
			sqlStatementInfo.setString(sqlArg2, properties.sourceTable);
			sqlStatementInfo.setString(sqlArg3, properties.sourceColumn);

			SqlResultSetPtr sqlResultSet{
				Database::sqlExecuteQuery(sqlStatementInfo)
			};

			if(!sqlResultSet || !(sqlResultSet->next())) {
				throw Exception(
						"Analyzer::Database::corpusLoad():"
						" Could not get creation date of corpus"
				);
			}

			std::string corpusCreationTime{
				sqlResultSet->getString("created")
			};

			// find suitable save point, if it exists
			savePoint = this->corpusFindSavePoint(
					properties,
					corpusCreationTime
			);

			// execute SQL query for getting a chunk of the corpus
			std::uint64_t count{};
			std::uint64_t total{};
			std::uint64_t previous{};

			if(savePoint.empty()) {
				sqlStatementFirst.setUInt(sqlArg1, properties.sourceType);
				sqlStatementFirst.setString(sqlArg2, properties.sourceTable);
				sqlStatementFirst.setString(sqlArg3, properties.sourceColumn);
				sqlStatementFirst.setString(sqlArg4, corpusCreationTime);
			}
			else {
				sqlStatementSavePoint.setUInt(sqlArg1, properties.sourceType);
				sqlStatementSavePoint.setString(sqlArg2, properties.sourceTable);
				sqlStatementSavePoint.setString(sqlArg3, properties.sourceColumn);
				sqlStatementSavePoint.setString(sqlArg4, corpusCreationTime);
				sqlStatementSavePoint.setString(sqlArg5, savePoint);
			}

			while(true) {
				if(previous > 0) {
					sqlStatementNext.setUInt64(sqlArg1, previous);

					SqlResultSetPtr(Database::sqlExecuteQuery(sqlStatementNext)).swap(sqlResultSet);
				}
				else if(savePoint.empty()) {
					SqlResultSetPtr(Database::sqlExecuteQuery(sqlStatementFirst)).swap(sqlResultSet);
				}
				else {
					SqlResultSetPtr(Database::sqlExecuteQuery(sqlStatementSavePoint)).swap(sqlResultSet);
				}

				if(!sqlResultSet || !(sqlResultSet->next())) {
					break;
				}

				if(previous == 0) {
					// first chunk: save sources and reserve memory
					sourcesTo = sqlResultSet->getUInt64("sources");

					total = sqlResultSet->getUInt64("chunks");

					chunks.reserve(total);
				}

				// get text of chunk
				chunks.emplace_back(sqlResultSet->getString("corpus"));

				if(!savePoint.empty()) {
					if(sqlResultSet->isNull("words")) {
						throw Exception(
								"Analyzer::Database::corpusLoad():"
								" Could not get number of words"
								" in a corpus chunk"
						);
					}

					wordNums.push_back(
							sqlResultSet->getUInt64("words")
					);
				}

				if(!(sqlResultSet->isNull("articlemap"))) {
					// parse article map
					try {
						articleMaps.emplace_back(
								Helper::Json::parseTextMapJson(
										sqlResultSet->getString(
												"articlemap"
										).c_str()
								)
						);
					}
					catch(const JsonException& e) {
						throw Exception(
								"Analyzer::Database::corpusLoad():"
								" Could not parse article map - "
								+ std::string(e.view())
						);
					}
				}

				if(!(sqlResultSet->isNull("datemap"))) {
					// parse date map
					try {
						dateMaps.emplace_back(
								Helper::Json::parseTextMapJson(
										sqlResultSet->getString("datemap").c_str()
								)
						);
					}
					catch(const JsonException& e) {
						throw Exception(
								"Analyzer::Database::corpusLoad():"
								" Could not parse date map - "
								+ std::string(e.view())
						);
					}
				}

				if(!savePoint.empty()) {
					if(sqlResultSet->isNull("sentencemap")) {
						throw Exception(
								"Analyzer::Database::corpusLoad():"
								" Could not get sentence map"
								" for a corpus chunk"
						);
					}


					// parse sentence map
					try {
						sentenceMaps.emplace_back(
								Helper::Json::parsePosLenPairsJson(
										sqlResultSet->getString("sentencemap").c_str()
								)
						);
					}
					catch(const JsonException& e) {
						throw Exception(
								"Analyzer::Database::corpusLoad():"
								" Could not parse sentence map - "
								+ std::string(e.view())
						);
					}
				}

				previous = sqlResultSet->getUInt64("id");

				++count;

				if(total > 0) {
					if(!statusSetter.update(
							static_cast<float>(count)
							/ total
							* progressReceivedCorpus,
							false
					)) {
						return;
					}
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::corpusLoad", e);
		}

		// combine chunks to corpus (and delete the input data)
		if(savePoint.empty()) {
			corpusTo.combineContinuous(
					chunks,
					articleMaps,
					dateMaps,
					true
			);
		}
		else {
			corpusTo.combineTokenized(
					chunks,
					wordNums,
					articleMaps,
					dateMaps,
					sentenceMaps,
					true
			);
		}

		// log the size of the combined corpus and the time it took to receive it
		std::ostringstream logStrStr;

		logStrStr.imbue(std::locale(""));

		logStrStr	<< "got text corpus of "
					<< corpusTo.size()
					<< " bytes in "
					<< timer.tickStr()
					<< ".";

		this->log(this->getLoggingMin(), logStrStr.str());

		statusSetter.finish();
	}

	// find an already tokenized corpus for the given properties and update them accordingly, return empty string if none is found
	std::string Database::corpusFindSavePoint(
			CorpusProperties& properties,
			const std::string& corpusCreationTime
	) {
		std::string savePoint;
		std::string result;
		std::size_t numSentenceManipulators{};
		std::size_t numWordManipulators{};

		// check connection to database
		this->checkConnection();

		// check prepared SQL statement
		if(this->ps.checkCorpusSavePoint == 0) {
			throw Exception(
					"Analyzer::Database::corpusFindSavePoint():"
					" Missing prepared SQL statement"
			);
		}

		// get prepared SQL statement
		auto& sqlStatement{
			this->getPreparedStatement(this->ps.checkCorpusSavePoint)
		};

		try {
			sqlStatement.setUInt(sqlArg1, properties.sourceType);
			sqlStatement.setString(sqlArg2, properties.sourceTable);
			sqlStatement.setString(sqlArg3, properties.sourceColumn);
			sqlStatement.setString(sqlArg4, corpusCreationTime);

			for(
					auto it{properties.sentenceManipulators.cbegin()};
					it != properties.sentenceManipulators.cend();
					++it
			) {
				const auto index{
					static_cast<std::size_t>(
							it
							- properties.sentenceManipulators.cbegin()
					)
				};

				savePoint += "s";
				savePoint += std::to_string(*it);
				savePoint += "[";
				savePoint += properties.sentenceModels.at(index);
				savePoint += "]";

				// execute SQL query for
				sqlStatement.setString(sqlArg5, savePoint);

				SqlResultSetPtr sqlResultSet{
					Database::sqlExecuteQuery(sqlStatement)
				};

				// get result
				if(sqlResultSet && sqlResultSet->next()) {
					if(sqlResultSet->getBoolean("result")) {
						result = savePoint;

						numSentenceManipulators = index + 1;
					}
				}
			}

			for(
					auto it{properties.wordManipulators.cbegin()};
					it != properties.wordManipulators.cend();
					++it
			) {
				const auto index{
					static_cast<std::size_t>(
							it
							- properties.wordManipulators.cbegin()
					)
				};

				savePoint += "w";
				savePoint += std::to_string(*it);
				savePoint += "[";
				savePoint += properties.wordModels.at(index);
				savePoint += "]";

				// execute SQL query for
				sqlStatement.setString(sqlArg5, savePoint);

				SqlResultSetPtr sqlResultSet{
					Database::sqlExecuteQuery(sqlStatement)
				};

				// get result
				if(sqlResultSet && sqlResultSet->next()) {
					if(sqlResultSet->getBoolean("result")) {
						result = savePoint;

						numWordManipulators = index + 1;
					}
				}
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::corpusFindSavePoint", e);
		}

		// remove manipulators that have already been run on save point
		if(numWordManipulators > 0) {
			properties.sentenceManipulators.clear();

			properties.wordManipulators.erase(
					properties.wordManipulators.begin(),
					properties.wordManipulators.begin() + numWordManipulators
			);
		}
		else if(numSentenceManipulators > 0) {
			properties.sentenceManipulators.erase(
					properties.sentenceManipulators.begin(),
					properties.sentenceManipulators.begin() + numSentenceManipulators
			);
		}

		// remove save points before already run manipulators
		properties.savePoints.erase(
				properties.savePoints.begin(),
				std::upper_bound(
						properties.savePoints.begin(),
						properties.savePoints.end(),
						numSentenceManipulators + numWordManipulators
				)
		);

		return result;
	}

	// run remaining manipulators on corpus
	bool Database::corpusManipulate(
			const CorpusProperties& properties,
			Data::Corpus& corpusRef,
			std::size_t numSources,
			StatusSetter& statusSetter
	) {
		// tokenize and manipulate corpus, from savepoint to savepoint
		std::string savePointName;
		std::size_t done{};

		if(!statusSetter.change("Preprocessing corpus...")) {
			return false;
		}

		for(const auto savePoint : properties.savePoints) {
			if(savePoint == 0) {
				continue;
			}

			if(
					savePoint
					> properties.sentenceManipulators.size()
					+ properties.wordManipulators.size()
			) {
				continue;
			}

			std::vector<std::uint16_t> sentenceManipulators;
			std::vector<std::string> sentenceModels;
			std::vector<std::uint16_t> wordManipulators;
			std::vector<std::string> wordModels;

			for(auto manipulator{done}; manipulator < savePoint; ++manipulator) {
				if(manipulator >= properties.sentenceManipulators.size()) {
					// add word manipulator
					const auto index{
						manipulator - properties.sentenceManipulators.size()
					};

					wordManipulators.push_back(
							properties.wordManipulators.at(index)
					);
					wordModels.push_back(
							properties.wordModels.at(index)
					);

					savePointName += "w";
					savePointName += std::to_string(wordManipulators.back());
					savePointName += "[";
					savePointName += wordModels.back();
					savePointName += "]";
				}
				else {
					// add sentence manipulator
					sentenceManipulators.push_back(
							properties.sentenceManipulators.at(manipulator)
					);
					sentenceModels.push_back(
							properties.sentenceModels.at(manipulator)
					);

					savePointName += "s";
					savePointName += std::to_string(sentenceManipulators.back());
					savePointName += "[";
					savePointName += sentenceModels.back();
					savePointName += "]";
				}

				++done;
			}

			if(!corpusRef.tokenize(
					sentenceManipulators,
					sentenceModels,
					wordManipulators,
					wordModels,
					properties.freeMemoryEvery,
					statusSetter
			)) {
				return false;
			}

			// save savepoint in database
			statusSetter.change("Saving processed corpus to the database...");

			this->corpusSaveSavePoint(
					properties,
					corpusRef,
					numSources,
					savePointName,
					statusSetter
			);
		}

		// run remaining manipulators, if necessary (result will not be saved to the database)
		const auto total{
			properties.sentenceManipulators.size()
			+ properties.wordManipulators.size()
		};

		if(done < total) {
			std::vector<std::uint16_t> sentenceManipulators;
			std::vector<std::string> sentenceModels;
			std::vector<std::uint16_t> wordManipulators;
			std::vector<std::string> wordModels;

			for(auto manipulator{done}; manipulator < total; ++manipulator) {
				if(manipulator >= properties.sentenceManipulators.size()) {
					// add word manipulator
					const auto index{
						manipulator - properties.sentenceManipulators.size()
					};

					wordManipulators.push_back(
							properties.wordManipulators.at(index)
					);
					wordModels.push_back(
							properties.wordModels.at(index)
					);
				}
				else {
					// add sentence manipulator
					sentenceManipulators.push_back(
							properties.sentenceManipulators.at(manipulator)
					);
					sentenceModels.push_back(
							properties.sentenceModels.at(manipulator)
					);
				}
			}

			statusSetter.change("Preprocessing corpus...");

			return corpusRef.tokenize(
					sentenceManipulators,
					sentenceModels,
					wordManipulators,
					wordModels,
					properties.freeMemoryEvery,
					statusSetter
			);
		}

		if(!corpusRef.isTokenized()) {
			// tokenize without manipulators
			return corpusRef.tokenizeCustom({}, {}, properties.freeMemoryEvery, statusSetter);
		}

		return statusSetter.isRunning();
	}

	// save corpus savepoint, throws Database::Exception
	void Database::corpusSaveSavePoint(
			const CorpusProperties& properties,
			const Data::Corpus& corpus,
			std::size_t numSources,
			const std::string& savePoint,
			StatusSetter& statusSetter
	) {
		// slice tokenized corpus into chunks for the database
		std::vector<std::string> chunks;
		std::vector<TextMap> articleMaps;
		std::vector<TextMap> dateMaps;
		std::vector<SentenceMap> sentenceMaps;
		std::vector<std::size_t> wordNums;

		corpus.copyChunksTokenized(
				static_cast<std::size_t>(
						this->getMaxAllowedPacketSize()
						* (
								static_cast<float>(this->corpusSlicing)
								* corpusSlicingFactor
						)
				),
				chunks,
				wordNums,
				articleMaps,
				dateMaps,
				sentenceMaps
		);

		// update status
		statusSetter.update(progressGeneratedSavePoint, false);

		// check connection to database
		this->checkConnection();

		// check prepared SQL statements
		if(ps.addChunkTokenized == 0) {
			throw Exception(
					"Analyzer::Database::corpusSaveSavePoint():"
					" Missing prepared SQL statement(s)"
					" for adding a tokenized chunk to the corpus"
			);
		}

		// get prepared SQL statement
		auto& addStatementTokenized{
			this->getPreparedStatement(this->ps.addChunkTokenized)
		};

		try {
			// save tokenized and sliced corpus to database
			std::uint64_t last{};

			for(std::size_t n{}; n < chunks.size(); ++n) {
				addStatementTokenized.setUInt(sqlArg1, properties.sourceType);
				addStatementTokenized.setString(sqlArg2, properties.sourceTable);
				addStatementTokenized.setString(sqlArg3, properties.sourceColumn);
				addStatementTokenized.setString(sqlArg4, savePoint);
				addStatementTokenized.setString(sqlArg5, chunks.at(n));

				if(articleMaps.size() > n) {
					addStatementTokenized.setString(
							sqlArg6,
							Helper::Json::stringify(articleMaps.at(n))
					);
				}
				else {
					addStatementTokenized.setNull(sqlArg6, 0);
				}

				if(dateMaps.size() > n) {
					addStatementTokenized.setString(
							sqlArg7,
							Helper::Json::stringify(dateMaps.at(n))
					);
				}
				else {
					addStatementTokenized.setNull(sqlArg7, 0);
				}

				if(sentenceMaps.size() > n) {
					addStatementTokenized.setString(
							sqlArg8,
							Helper::Json::stringify(sentenceMaps.at(n))
					);
				}
				else {
					addStatementTokenized.setNull(sqlArg8, 0);
				}

				if(last > 0) {
					addStatementTokenized.setUInt64(sqlArg9, last);
				}
				else {
					addStatementTokenized.setNull(sqlArg9, 0);
				}

				addStatementTokenized.setUInt64(sqlArg10, numSources);
				addStatementTokenized.setUInt64(sqlArg11, chunks.size());
				addStatementTokenized.setUInt64(sqlArg12, wordNums.at(n));

				Database::sqlExecute(addStatementTokenized);

				last = this->getLastInsertedId();

				// free memory early
				std::string().swap(chunks.at(n));

				if(articleMaps.size() > n) {
					TextMap().swap(articleMaps.at(n));
				}

				if(dateMaps.size() > n) {
					TextMap().swap(dateMaps.at(n));
				}

				statusSetter.update(
						progressGeneratedSavePoint
						+ progressSavingSavePoint
						* (
								static_cast<float>(n + 1)
								/ chunks.size()
						),
						false
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::corpusSaveSavePoint", e);
		}
	}

} /* namespace crawlservpp::Module::Analyzer */
