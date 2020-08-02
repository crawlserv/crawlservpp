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
	 *   vector of strings containing the
	 *   names of the fields in the target
	 *   table.
	 * \param types Constant reference to a
	 *   vector of strings containing the
	 *   data types of the fields in the
	 *   target table
	 */
	void Database::setTargetFields(const std::vector<std::string>& fields, const std::vector<std::string>& types) {
		this->targetFieldNames = fields;
		this->targetFieldTypes = types;

		// replace undefined types with empty strings
		//  NOTE: This will lead to an error in initTargetTable() if the corresponding names are not empty!
		while(this->targetFieldTypes.size() < this->targetFieldNames.size()) {
			this->targetFieldTypes.emplace_back();
		}
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
	 * INITIALIZATION
	 */

	//! Creates the target table, or adds the field columns, if they do not exist already.
	/*!
	 * \note Needs to be called by the
	 *   algorithm class in order to create
	 *   the fill target table name and the
	 *   required target fields.
	 *
	 * \param compressed Set whether to
	 *   compress the data in the target
	 *   table.
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
	void Database::initTargetTable(bool compressed) {
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
				std::find_if(
						this->targetFieldNames.cbegin(),
						this->targetFieldNames.cend(),
						[](const auto& targetFieldName){
							return !targetFieldName.empty();
						}
				) == this->targetFieldNames.cend()
		) {
			throw Exception(
					"Analyzer::Database::initTargetTable():"
					" No target fields have been specified (only empty strings)"
			);
		}

		// create target table name
		this->targetTableFull = "crawlserv_";
		this->targetTableFull += this->getOptions().websiteNamespace;
		this->targetTableFull += "_";
		this->targetTableFull += this->getOptions().urlListNamespace;
		this->targetTableFull += "_analyzed_";
		this->targetTableFull += this->targetTableName;

		// create table properties
		CustomTableProperties properties(
				"analyzed",
				this->getOptions().websiteId,
				this->getOptions().urlListId,
				this->targetTableName,
				this->targetTableFull,
				compressed
		);

		for(auto it{this->targetFieldNames.cbegin()}; it != this->targetFieldNames.cend(); ++it) {
			if(!(it->empty())) {
				const auto index{it - this->targetFieldNames.cbegin()};

				properties.columns.emplace_back(
						"analyzed__" + *it,
						this->targetFieldTypes.at(index)
				);

				if(properties.columns.back().type.empty()) {
					throw Exception(
							"Analyzer::Database::initTargetTable():"
							" No type for target field \'"
							+ *it
							+ "\' has been specified"
					);
				}
			}
		}

		// add target table
		this->addTargetTable(properties);
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
			this->tablePrefix += this->getOptions().websiteNamespace + "_";
		}

		if(!(this->getOptions().urlListNamespace.empty())) {
			this->tablePrefix += this->getOptions().urlListNamespace + "_";
		}

		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(numPreparedStatements);

		try {
			// prepare SQL statements for analyzer
			if(this->ps.getCorpusFirst == 0) {
				this->log(verbose, "prepares getCorpus() [1/2]...");

				this->ps.getCorpusFirst = this->addPreparedStatement(
						"SELECT id, corpus, articlemap, datemap, sources, chunks"
						" FROM `crawlserv_corpora`"
						" USE INDEX(urllist)"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND source_type = ?"
						" AND source_table LIKE ?"
						" AND source_field LIKE ?"
						" AND previous IS NULL"
						" ORDER BY created DESC"
						" LIMIT 1"
				);
			}

			if(this->ps.getCorpusNext == 0) {
				this->log(verbose, "prepares getCorpus() [2/2]...");

				this->ps.getCorpusNext = this->addPreparedStatement(
						"SELECT id, corpus, articlemap, datemap"
						" FROM `crawlserv_corpora`"
						" WHERE previous = ?"
						" LIMIT 1"
				);
			}

			if(this->ps.isCorpusChanged == 0) {
				this->log(verbose, "prepares isCorpusChanged() [1/4]...");

				this->ps.isCorpusChanged = this->addPreparedStatement(
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
							" AND created > ?"
						" )"
						" AS result"
				);
			}

			if(this->ps.isCorpusChangedParsing == 0) {
				this->log(verbose, "prepares isCorpusChanged() [2/4]...");

				this->ps.isCorpusChangedParsing = this->addPreparedStatement(
						"SELECT updated"
						" FROM `crawlserv_parsedtables`"
						" USE INDEX(urllist)"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(this->ps.isCorpusChangedExtracting == 0) {
				this->log(verbose, "prepares isCorpusChanged() [3/4]...");

				this->ps.isCorpusChangedExtracting = this->addPreparedStatement(
						"SELECT updated"
						" FROM `crawlserv_extractedtables`"
						" USE INDEX(urllist)"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(this->ps.isCorpusChangedAnalyzing == 0) {
				this->log(verbose, "prepares isCorpusChanged() [4/4]...");

				this->ps.isCorpusChangedAnalyzing = this->addPreparedStatement(
						"SELECT updated"
						" FROM `crawlserv_analyzedtables`"
						" USE INDEX(urllist)"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(this->ps.deleteCorpus == 0) {
				this->log(verbose, "prepares createCorpus() [1/4]...");

				this->ps.deleteCorpus = this->addPreparedStatement(
						"DELETE"
						" FROM `crawlserv_corpora`"
						" WHERE website = " + this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND source_type = ?"
						" AND source_table LIKE ?"
						" AND source_field LIKE ?"
						" LIMIT 1"
				);
			}

			if(this->ps.addChunk == 0) {
				this->log(verbose, "prepares createCorpus() [2/4]...");

				this->ps.addChunk = this->addPreparedStatement(
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
							" ?, "
							" ?,"
							" ?,"
							" ?,"
							" CONVERT( ?  USING utf8mb4 ),"
							" ?,"
							" ?,"
							" ?,"
							" ?"
						")"
				);
			}

			if(this->ps.measureChunk == 0) {
				this->log(verbose, "prepares createCorpus() [3/4]...");

				this->ps.measureChunk = this->addPreparedStatement(
						"UPDATE `crawlserv_corpora`"
						" SET chunk_length = CHAR_LENGTH(corpus),"
						" chunk_size = LENGTH(corpus)"
						" WHERE id = ?"
						" LIMIT 1"
				);
			}

			if(this->ps.measureCorpus == 0) {
				this->log(verbose, "prepares createCorpus() [4/4]...");

				this->ps.measureCorpus = this->addPreparedStatement(
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
						" AND dest.source_field LIKE ?"
				);
			}
		}
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::prepare", e);
		}
	}

	//! Prepares custom SQL statements for the algorithm.
	/*!
	 * \param statements Constant reference to
	 *   a vector of strings containing the
	 *   SQL statements to prepare.
	 * \param idsTo Reference to a vector to
	 *   which the IDs of the prepared SQL
	 *   statements will be written,
	 *
	 * \throws Main::Database::Exception if
	 *   a MySQL error occurs during the
	 *   preparation of the SQL statements.
	 */
	void Database::prepareAlgo(const std::vector<std::string>& statements, std::vector<uint16_t>& idsTo) {
		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(statements.size());

		// prepare SQL statements for algorithm
		this->log(
				this->getLoggingVerbose(),
				"prepares "
				+ std::to_string(statements.size())
				+ " SQL statements for the algorithm..."
		);

		for(const auto& statement : statements) {
			idsTo.push_back(this->addPreparedStatement(statement));
		}
	}

	//! Gets a prepared SQL statement for the algorithm.
	/*!
	 * \note Wraps the protected parent member
	 *  Wrapper::Database::getPreparedStatement
	 *  to the public for usage by the algorithm.
	 *
	 * \param sqlStatementId ID of the prepared
	 *   SQL statement to retrieve.
	 *
	 * \returns A reference to the prepared
	 *   SQL statement specified.
	 *
	 * \throws Main::Database::Exception if a MySQL
	 *    error occured while retrieving the
	 *    prepared SQL statement.
	 */
	sql::PreparedStatement& Database::getPreparedAlgoStatement(uint16_t sqlStatementId) {
		return this->getPreparedStatement(sqlStatementId);
	}

	/*
	 * TEXT CORPUS
	 */

	//! Gets the text corpus after creating it if it is out-of-date or does not yet exist.
	/*!
	 * \param corpusProperties Constant reference to
	 *   a structure containing the properties of
	 *   the text corpus.
	 * \param filterDateFrom Constant reference to a
	 *   string contaning the starting date from
	 *   which on the text corpus should be created,
	 *   or to an empty string if the source of the
	 *   corpus should not be filtered by a
	 *   starting date. Can only be used if the
	 *   source of the corpus is parsed data.
	 * \param filterDateTo Constant reference to a
	 *   string contaning the ending date until
	 *   which the text corpus should be created,
	 *   or to an empty string if the source of the
	 *   corpus should not be filtered by an
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
	 * \throws Module::Analyzer::Exception if a
	 *   prepared SQL statement is missing, or
	 *   an article or date map could not be parsed.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while getting or creating
	 *   the text corpus.
	 */
	void Database::getCorpus(
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

			return;
		}

		if(corpusProperties.sourceColumn.empty()) {
			this->log(
					this->getLoggingMin(),
					"WARNING:"
					" The name of the source field is empty."
			);

			return;
		}

		// initialize values
		corpusTo.clear();

		sourcesTo = 0;

		{ // wait for source table lock
			DatabaseLock(
					*this,
					"corpusCreation."
					+ std::to_string(corpusProperties.sourceType)
					+ "."
					+ corpusProperties.sourceTable
					+ "."
					+ corpusProperties.sourceColumn,
					this->isRunning
			);

			if(!(this->isRunning())) {
				return;
			}

			// check whether text corpus needs to be created
			if(this->isCorpusChanged(corpusProperties)) {
				this->createCorpus(corpusProperties, corpusTo, sourcesTo, statusSetter);
			}
			else {
				// start timer
				Timer::Simple timer;

				// get all the chunks of the corpus from the database
				std::vector<std::string> chunks;
				std::vector<Struct::TextMap> articleMaps;
				std::vector<Struct::TextMap> dateMaps;

				// check connection
				this->checkConnection();

				// check prepared SQL statements
				if(this->ps.getCorpusFirst == 0 || this->ps.getCorpusNext == 0) {
					throw Exception(
							"Analyzer::Database::getCorpus():"
							" Missing prepared SQL statement(s) for getting the corpus"
					);
				}

				// get prepared SQL statements
				auto& sqlStatementFirst{
					this->getPreparedStatement(this->ps.getCorpusFirst)
				};
				auto& sqlStatementNext{
					this->getPreparedStatement(this->ps.getCorpusNext)
				};

				try {
					// execute SQL query for getting a chunk of the corpus
					std::uint64_t count{0};
					std::uint64_t total{0};
					std::uint64_t previous{0};

					sqlStatementFirst.setUInt(sqlArg1, corpusProperties.sourceType);
					sqlStatementFirst.setString(sqlArg2, corpusProperties.sourceTable);
					sqlStatementFirst.setString(sqlArg3, corpusProperties.sourceColumn);

					while(true) {
						SqlResultSetPtr sqlResultSet;

						if(previous > 0) {
							sqlStatementNext.setUInt64(sqlArg1, previous);

							SqlResultSetPtr(Database::sqlExecuteQuery(sqlStatementNext)).swap(sqlResultSet);
						}
						else {
							SqlResultSetPtr(Database::sqlExecuteQuery(sqlStatementFirst)).swap(sqlResultSet);
						}

						if(sqlResultSet && sqlResultSet->next()) {
							if(previous == 0) {
								// first chunk: save sources and reserve memory
								sourcesTo = sqlResultSet->getUInt64("sources");

								total = sqlResultSet->getUInt64("chunks");

								chunks.reserve(total);
							}

							// get text of chunk
							chunks.emplace_back(sqlResultSet->getString("corpus"));

							if(!(sqlResultSet->isNull("articlemap"))) {
								// parse article map
								try {
									articleMaps.emplace_back(
											Helper::Json::parseTextMapJson(
													sqlResultSet->getString("articlemap").c_str()
											)
									);
								}
								catch(const JsonException& e) {
									throw Exception(
											"Analyzer::Database::getCorpus():"
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
											"Analyzer::Database::getCorpus():"
											" Could not parse date map - "
											+ std::string(e.view())
									);
								}
							}

							previous = sqlResultSet->getUInt64("id");

							++count;

							if(total > 0) {
								statusSetter.update(
										static_cast<float>(count)
										/ total
										* progressReceivedCorpus,
										false
								);
							}
						}
						else {
							break;
						}
					}
				}
				catch(const sql::SQLException &e) {
					Database::sqlException("Analyzer::Database::getCorpus", e);
				}

				// combine chunks to corpus (and delete the input data)
				corpusTo.combine(chunks, articleMaps, dateMaps, true);

				// log size of text combined corpus and time it took to receive it
				std::ostringstream logStrStr;

				logStrStr.imbue(std::locale(""));

				logStrStr	<< "got text corpus of "
							<< corpusTo.size()
							<< " bytes in "
							<< timer.tickStr()
							<< ".";

				this->log(this->getLoggingMin(), logStrStr.str());
			}
		}

		// filter corpus by date(s) if necessary
		if(corpusTo.filterByDate(filterDateFrom, filterDateTo)) {
			// log new corpus size
			std::ostringstream logStrStr;

			logStrStr.imbue(std::locale(""));

			logStrStr	<< "filtered corpus to "
						<< corpusTo.size()
						<< " bytes.";

			this->log(this->getLoggingMin(), logStrStr.str());
		}
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

	//! Public helper function checking the given data source.
	/*!
	 * \param type Constant reference to
	 *   the type of the source to be checked.
	 * \param table Constant reference to a
	 *   string containing the name of the
	 *   source table to be checked.
	 * \param column Constant reference to a
	 *   string containing the name of the
	 *   source column to be checked.
	 *
	 * \returns True, if the given source is
	 *   valid. False otherwise.
	 *
	 * \sa generalInputSourcesParsing,
	 *   generalInputSourcesExtracting,
	 *   generalInputSourcesAnalyzing,
	 *   generalInputSourcesCrawling,
	 *   checkSources
	 */
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
	 * TEXT CORPUS HELPERS (protected)
	 */

	//! Checks whether the source of a text corpus has changed since the corpus has been created.
	/*!
	 * \param corpusProperties Constant reference
	 *   to a structure containing the properties
	 *   of the text corpus whose source will be
	 *   checked.
	 *
	 * \returns True, if the source of the given
	 *   text corpus has changed since the corpus
	 *   has been created, if the text corpus has
	 *   not been created yet, or if the source of
	 *   the text corpus is raw crawling data, in
	 *   which case the corpus will always be re-
	 *   created. False, if the source of the text
	 *   corpus has not changed, and the corpus can
	 *   be used as previously generated.
	 *
	 * \throws Module::Analyzer::Exception if a
	 *   prepared SQL statement for checking the
	 *   source of the corpus is missing, or
	 *   the source specified for the text corpus
	 *   is invalid.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while checking the source
	 *   of the given text corpus.
	 */
	bool Database::isCorpusChanged(const CorpusProperties& corpusProperties) {
		bool result{true};

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(this->ps.isCorpusChanged == 0) {
			throw Exception(
					"Analyzer::Database::isCorpusChanged():"
					" Missing prepared SQL statement for getting the corpus creation time"
			);
		}

		// get prepared SQL statement
		auto& corpusStatement{
			this->getPreparedStatement(this->ps.isCorpusChanged)
		};

		std::uint16_t sourceStatement{0};

		switch(corpusProperties.sourceType) {
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

		auto& tableStatement{this->getPreparedStatement(sourceStatement)};

		try {
			// execute SQL query for getting the update time of the source table
			tableStatement.setString(sqlArg1, corpusProperties.sourceTable);

			SqlResultSetPtr sqlResultSet{Database::sqlExecuteQuery(tableStatement)};

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				const std::string updateTime{sqlResultSet->getString("updated")};

				// execute SQL query for comparing the creation time of the corpus with the update time of the table
				corpusStatement.setUInt(sqlArg1, corpusProperties.sourceType);
				corpusStatement.setString(sqlArg2, corpusProperties.sourceTable);
				corpusStatement.setString(sqlArg3, corpusProperties.sourceColumn);
				corpusStatement.setString(sqlArg4, updateTime);

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

	//! Generates and adds a text corpus to the database.
	/*!
	 * If an old corpus exists for the specified
	 *  source, it will be deleted.
	 *
	 * When using parsed data, a date and an
	 *  article map will be created for the
	 *  generated text corpus.
	 *
	 * \param corpusProperties Constant reference
	 *   to a structure containing the properties
	 *   of the text corpus to generate.
	 * \param corpusTo Reference to which the
	 *   newly generated text corpus will be
	 *   written.
	 * \param sourcesTo Reference to which the
	 *   number of sources used for the newly
	 *   generated text corpus will be written.
	 * \param statusSetter Data needed to keep
	 *   the status of the thread updated.
	 *
	 * \throws Module::Analyzer::Exception if any
	 *   of the prepared SQL statements for
	 *   deleting, adding, and measuring the newly
	 *   generatd text corpus is missing, or if
	 *   underlying source data could not be
	 *   retrieved from the database.
	 * \throws Main::Database::Exception if a MySQL
	 *   error occured while deleting, adding, or
	 *   measuring the newly generated text corpus.
	 */
	void Database::createCorpus(
			const CorpusProperties& corpusProperties,
			Data::Corpus& corpusTo,
			std::size_t& sourcesTo,
			StatusSetter& statusSetter
	) {
		// initialize values
		corpusTo.clear();

		sourcesTo = 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(this->ps.deleteCorpus == 0) {
			throw Exception(
					"Analyzer::Database::createCorpus():"
					" Missing prepared SQL statement for deleting the text corpus"
			);
		}

		if(this->ps.addChunk == 0) {
			throw Exception(
					"Analyzer::Database::createCorpus():"
					" Missing prepared SQL statement for adding a chunk to text corpus"
			);
		}

		if(this->ps.measureCorpus == 0) {
			throw Exception(
					"Analyzer::Database::createCorpus():"
					" Missing prepared SQL statement for measuring the text corpus"
			);
		}

		// get prepared SQL statements
		auto& deleteStatement{
			this->getPreparedStatement(this->ps.deleteCorpus)
		};
		auto& addStatement{
			this->getPreparedStatement(this->ps.addChunk)
		};
		auto& measureChunkStatement{
			this->getPreparedStatement(this->ps.measureChunk)
		};
		auto& measureCorpusStatement{
			this->getPreparedStatement(this->ps.measureCorpus)
		};

		// check your sources
		this->checkSource(
				corpusProperties.sourceType,
				corpusProperties.sourceTable,
				corpusProperties.sourceColumn
		);

		// show warning when using raw crawled data and logging is enabled
		if(corpusProperties.sourceType == generalInputSourcesCrawling) {
			this->log(
					this->getLoggingMin(),
					"WARNING:"
					" Corpus will always be re-created when created from raw crawled data."
			);
			this->log(
					this->getLoggingMin(),
					"It is highly recommended to use parsed data instead!"
			);

			if(!corpusProperties.sourceTable.empty()) {
				this->log(
						this->getLoggingMin(),
						"WARNING:"
						" Source table name ignored."
				);
			}

			if(!corpusProperties.sourceColumn.empty()) {
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
					corpusProperties.sourceType,
					corpusProperties.sourceTable
			)
		};
		const auto columnName{
			getSourceColumnName(
					corpusProperties.sourceType,
					corpusProperties.sourceColumn
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

		try {
			// execute SQL query for deleting old text corpus (if it exists)
			deleteStatement.setUInt(sqlArg1, corpusProperties.sourceType);
			deleteStatement.setString(sqlArg2, corpusProperties.sourceTable);
			deleteStatement.setString(sqlArg3, corpusProperties.sourceColumn);

			Database::sqlExecute(deleteStatement);

			statusSetter.update(progressDeletedCorpus, false);

			// get texts and possibly parsed date/times and IDs from database
			Data::GetColumns data;

			data.table = tableName;

			if(corpusProperties.sourceType == generalInputSourcesParsing) {
				data.columns.reserve(maxNumCorpusColumns);
			}

			data.columns.emplace_back(columnName);

			if(corpusProperties.sourceType == generalInputSourcesParsing) {
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
						"Analyzer::Database::createCorpus():"
						" Could not get requested data from database"
				);
			}

			statusSetter.update(progressReceivedSources, false);

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
				const auto index{it - data.values.at(column1).begin()};
				auto& text{data.values.at(column1).at(index)};

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

			statusSetter.update(progressMovedData, false);

			// create corpus (and delete the input data)
			if(data.values.size() > numColumns1) {
				corpusTo.create(texts, articleIds, dateTimes, true);
			}
			else {
				corpusTo.create(texts, true);
			}

			statusSetter.update(progressCreatedCorpus, false);

			// slice corpus into chunks for the database
			std::vector<std::string> chunks;
			std::vector<Struct::TextMap> articleMaps;
			std::vector<Struct::TextMap> dateMaps;

			corpusTo.copyChunks(
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

			statusSetter.update(progressSlicedCorpus, false);

			// add corpus chunks to the database
			std::uint64_t last{0};

			for(std::size_t n{0}; n < chunks.size(); ++n) {
				addStatement.setUInt(sqlArg1, corpusProperties.sourceType);
				addStatement.setString(sqlArg2, corpusProperties.sourceTable);
				addStatement.setString(sqlArg3, corpusProperties.sourceColumn);
				addStatement.setString(sqlArg4, chunks.at(n));

				if(articleMaps.size() > n) {
					addStatement.setString(sqlArg5, Helper::Json::stringify(articleMaps.at(n)));
				}
				else {
					addStatement.setNull(sqlArg5, 0);
				}

				if(dateMaps.size() > n) {
					addStatement.setString(sqlArg6, Helper::Json::stringify(dateMaps.at(n)));
				}
				else {
					addStatement.setNull(sqlArg6, 0);
				}

				if(last > 0) {
					addStatement.setUInt64(sqlArg7, last);
				}
				else {
					addStatement.setNull(sqlArg7, 0);
				}

				addStatement.setUInt64(sqlArg8, sourcesTo);
				addStatement.setUInt64(sqlArg9, chunks.size());

				Database::sqlExecute(addStatement);

				last = this->getLastInsertedId();

				// free memory early
				std::string().swap(chunks.at(n));

				if(articleMaps.size() > n) {
					Struct::TextMap().swap(articleMaps.at(n));
				}

				if(dateMaps.size() > n) {
					Struct::TextMap().swap(dateMaps.at(n));
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
		catch(const sql::SQLException &e) {
			Database::sqlException("Analyzer::Database::createCorpus", e);
		}

		// check connection to MySQL server
		this->checkConnection();

		try {
			// measure corpus
			measureCorpusStatement.setUInt(sqlArg1, corpusProperties.sourceType);
			measureCorpusStatement.setString(sqlArg2, corpusProperties.sourceTable);
			measureCorpusStatement.setString(sqlArg3, corpusProperties.sourceColumn);
			measureCorpusStatement.setUInt(sqlArg4, corpusProperties.sourceType);
			measureCorpusStatement.setString(sqlArg5, corpusProperties.sourceTable);
			measureCorpusStatement.setString(sqlArg6, corpusProperties.sourceColumn);

			Database::sqlExecute(measureCorpusStatement);
		}
		catch(const sql::SQLException &e) {
			// log and ignore errors when measuring corpus (total text might be too long)
			this->log(
					this->getLoggingMin(),
					"WARNING: Could not measure corpus - "
					+ std::string(e.what()));
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

} /* namespace crawlservpp::Module::Analyzer */
