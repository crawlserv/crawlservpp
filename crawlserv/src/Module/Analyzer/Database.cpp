/*
 *
 * ---
 *
 *  Copyright (C) 2018-2020 Anselm Schmidt (ans[ät]ohai.su)
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

	// constructor: initialize database and set default values
	Database::Database(Module::Database& dbThread)
							: Wrapper::Database(dbThread),
							  timeoutTargetLock(0),
							  corpusSlicing(30),
							  targetTableId(0),
							  ps(_ps()) {}

	// destructor stub
	Database::~Database() {}

	// set target table name
	void Database::setTargetTable(const std::string& table) {
		this->targetTableName = table;
	}

	// set target table fields
	void Database::setTargetFields(const std::vector<std::string>& fields, const std::vector<std::string>& types) {
		this->targetFieldNames = fields;
		this->targetFieldTypes = types;

		// replace undefined types with empty strings
		//  NOTE: This will lead to an error in initTargetTable() if the corresponding names are not empty!
		while(this->targetFieldTypes.size() < this->targetFieldNames.size())
			this->targetFieldTypes.emplace_back();
	}

	// set time-out for target table lock
	void Database::setTimeoutTargetLock(size_t timeOut) {
		this->timeoutTargetLock = timeOut;
	}

	// set size of corpus chunks in percentage of the maximum package size allowed by the MySQL server
	void Database::setCorpusSlicing(unsigned char percentageOfMaxAllowedPackageSize) {
		this->corpusSlicing = percentageOfMaxAllowedPackageSize;
	}

	// set callback for checking whether the parent thread is still running (needed for corpus creation)
	void Database::setIsRunningCallback(IsRunningCallback isRunningCallback) {
		this->isRunning = isRunningCallback;
	}

	// create target table if it does not exists or add field columns if they do not exist
	// 	NOTE:	Needs to be called by algorithm class in order to get the required field names!
	//  throws Database::Exception
	void Database::initTargetTable(bool compressed) {
		// check options
		if(this->getOptions().websiteNamespace.empty())
			throw Exception("Analyzer::Database::initTargetTable(): No website specified");

		if(this->getOptions().urlListNamespace.empty())
			throw Exception("Analyzer::Database::initTargetTable(): No URL list specified");

		if(this->targetTableName.empty())
			throw Exception("Analyzer::Database::initTargetTable(): Name of result table is empty");

		if(
				std::find_if(
						this->targetFieldNames.begin(),
						this->targetFieldNames.end(),
						[](const auto& targetFieldName){
							return !targetFieldName.empty();
						}
				) == this->targetFieldNames.end()
		)
			throw Exception(
					"Analyzer::Database::initTargetTable(): No target fields specified (only empty strings)"
			);

		// create target table name
		this->targetTableFull = "crawlserv_"
								+ this->getOptions().websiteNamespace
								+ "_"
								+ this->getOptions().urlListNamespace
								+ "_analyzed_"
								+ this->targetTableName;

		// create table properties
		CustomTableProperties properties(
				"analyzed",
				this->getOptions().websiteId,
				this->getOptions().urlListId,
				this->targetTableName,
				this->targetTableFull,
				compressed
		);

		for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i) {
			if(!(i->empty())) {
				properties.columns.emplace_back(
						"analyzed__" + *i,
						this->targetFieldTypes.at(i - this->targetFieldNames.begin())
				);

				if(properties.columns.back().type.empty())
					throw Exception("Analyzer::Database::initTargetTable(): No type for target field \'" + *i + "\' specified");
			}
		}

		// add target table
		this->addTargetTable(properties);
	}

	// prepare SQL statements for analyzer, throws Main::Database::Exception
	void Database::prepare() {
		const unsigned short verbose = this->getLoggingVerbose();

		// create table prefix
		this->tablePrefix = "crawlserv_";

		if(!(this->getOptions().websiteNamespace.empty()))
			this->tablePrefix += this->getOptions().websiteNamespace + "_";

		if(!(this->getOptions().urlListNamespace.empty()))
			this->tablePrefix += this->getOptions().urlListNamespace + "_";

		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(7);

		try {
			// prepare SQL statements for analyzer
			if(!(this->ps.getCorpusFirst)) {
				this->log(verbose, "prepares getCorpus() [1/2]...");

				this->ps.getCorpusFirst = this->addPreparedStatement(
						"SELECT id, corpus, articlemap, datemap, sources"
						" FROM crawlserv_corpora"
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

			if(!(this->ps.getCorpusNext)) {
				this->log(verbose, "prepares getCorpus() [2/2]...");

				this->ps.getCorpusNext = this->addPreparedStatement(
						"SELECT id, corpus, articlemap, datemap, sources"
						" FROM crawlserv_corpora"
						" WHERE previous = ?"
						" LIMIT 1"
				);
			}

			if(!(this->ps.isCorpusChanged)) {
				this->log(verbose, "prepares isCorpusChanged() [1/4]...");

				this->ps.isCorpusChanged = this->addPreparedStatement(
						"SELECT EXISTS"
						" ("
							" SELECT *"
							" FROM crawlserv_corpora"
							" WHERE website = "	+ this->getWebsiteIdString() + ""
							" AND urllist = " + this->getUrlListIdString() +
							" AND source_type = ?"
							" AND source_table = ?"
							" AND source_field = ?"
							" AND created > ?"
						" )"
						" AS result"
				);
			}

			if(!(this->ps.isCorpusChangedParsing)) {
				this->log(verbose, "prepares isCorpusChanged() [2/4]...");

				this->ps.isCorpusChangedParsing = this->addPreparedStatement(
						"SELECT updated"
						" FROM crawlserv_parsedtables"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(!(this->ps.isCorpusChangedExtracting)) {
				this->log(verbose, "prepares isCorpusChanged() [3/4]...");

				this->ps.isCorpusChangedExtracting = this->addPreparedStatement(
						"SELECT updated"
						" FROM crawlserv_extractedtables"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(!(this->ps.isCorpusChangedAnalyzing)) {
				this->log(verbose, "prepares isCorpusChanged() [4/4]...");

				this->ps.isCorpusChangedAnalyzing = this->addPreparedStatement(
						"SELECT updated"
						" FROM crawlserv_analyzedtables"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(!(this->ps.deleteCorpus)) {
				this->log(verbose, "prepares createCorpus() [1/2]...");

				this->ps.deleteCorpus = this->addPreparedStatement(
						"DELETE"
						" FROM crawlserv_corpora"
						" WHERE website = " + this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND source_type = ?"
						" AND source_table = ?"
						" AND source_field = ?"
						" LIMIT 1"
				);
			}

			if(!(this->ps.addCorpus)) {
				this->log(verbose, "prepares createCorpus() [2/2]...");

				this->ps.addCorpus = this->addPreparedStatement(
						"INSERT INTO crawlserv_corpora"
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
							" sources"
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
							" ?"
						")"
				);
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Analyzer::Database::prepare", e); }
	}

	// prepare SQL statements for algorithm, throws Main::Database::Exception
	void Database::prepareAlgo(const std::vector<std::string>& statements, std::vector<unsigned short>& idsTo) {
		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(statements.size());

		// prepare SQL statements for algorithm
		this->log(
				this->getLoggingVerbose(),
				"prepares " + std::to_string(statements.size()) + " SQL statements for algorithm..."
		);

		for(const auto& statement : statements)
			idsTo.push_back(this->addPreparedStatement(statement));
	}

	// get prepared SQL statement for algorithm (wraps protected parent member function to the public)
	sql::PreparedStatement& Database::getPreparedAlgoStatement(unsigned short sqlStatementId) {
		return this->getPreparedStatement(sqlStatementId);
	}

	// get text corpus and save it to corpusTo - the corpus will be created if it is out-of-date or does not exist,
	//  throws Database::Exception
	void Database::getCorpus(
			const CorpusProperties& corpusProperties,
			const std::string& filterDateFrom,
			const std::string& filterDateTo,
			Data::Corpus& corpusTo,
			size_t& sourcesTo
	) {
		// check arguments
		if(corpusProperties.sourceTable.empty()) {
			this->log(this->getLoggingMin(), "WARNING: Name of source table is empty.");

			return;
		}

		if(corpusProperties.sourceField.empty()) {
			this->log(this->getLoggingMin(), "WARNING: Name of source field is empty.");

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
					+ corpusProperties.sourceField,
					this->isRunning
			);

			if(!(this->isRunning()))
				return;

			// check whether text corpus needs to be created
			if(this->isCorpusChanged(corpusProperties))
				this->createCorpus(corpusProperties, corpusTo, sourcesTo);
			else {
				// start timer
				Timer::Simple timer;

				// get all the chunks of the corpus from the database
				std::vector<std::string> chunks;
				std::vector<Struct::TextMap> articleMaps, dateMaps;

				// check connection
				this->checkConnection();

				// check prepared SQL statement
				if(!(this->ps.getCorpusFirst) || !(this->ps.getCorpusNext))
					throw Exception("Analyzer::Database::getCorpus(): Missing prepared SQL statement for getting the corpus");

				// get prepared SQL statements
				sql::PreparedStatement& sqlStatementFirst(this->getPreparedStatement(this->ps.getCorpusFirst));
				sql::PreparedStatement& sqlStatementNext(this->getPreparedStatement(this->ps.getCorpusNext));

				try {
					// execute SQL query for getting a chunk of the corpus
					size_t previous = 0;

					sqlStatementFirst.setUInt(1, corpusProperties.sourceType);
					sqlStatementFirst.setString(2, corpusProperties.sourceTable);
					sqlStatementFirst.setString(3, corpusProperties.sourceField);

					while(true) {
						SqlResultSetPtr sqlResultSet;

						if(previous) {
							sqlStatementNext.setUInt64(1, previous);

							SqlResultSetPtr(Database::sqlExecuteQuery(sqlStatementNext)).swap(sqlResultSet);
						}
						else
							SqlResultSetPtr(Database::sqlExecuteQuery(sqlStatementFirst)).swap(sqlResultSet);

						if(sqlResultSet && sqlResultSet->next()) {
							// get chunk
							chunks.emplace_back(sqlResultSet->getString("corpus"));

							if(!(sqlResultSet->isNull("articlemap")))
								// parse article map
								try {
									articleMaps.emplace_back(
											Helper::Json::parseTextMapJson(
													sqlResultSet->getString("articlemap")
											)
									);
								}
								catch(const JsonException& e) {
									throw Exception(
											"Analyzer::Database::getCorpus(): Could not parse article map - "
											+ e.whatStr()
									);
								}

							if(!(sqlResultSet->isNull("datemap")))
								// parse date map
								try {
									dateMaps.emplace_back(
											Helper::Json::parseTextMapJson(
													sqlResultSet->getString("datemap")
											)
									);
								}
								catch(const JsonException& e) {
									throw Exception(
											"Analyzer::Database::getCorpus(): Could not parse date map - "
											+ e.whatStr()
									);
								}

							if(!previous)
								sourcesTo = sqlResultSet->getUInt64("sources");

							previous = sqlResultSet->getUInt64("id");
						}
						else
							break;
					}
				}
				catch(const sql::SQLException &e) { this->sqlException("Analyzer::Database::getCorpus", e); }

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

	// public helper function: get the full name of a source table, throws Database::Exception
	std::string Database::getSourceTableName(unsigned short type, const std::string& name) {
		switch(type) {
		case Config::generalInputSourcesParsing:
			return this->tablePrefix + "parsed_" + name;

		case Config::generalInputSourcesExtracting:
			return this->tablePrefix + "extracted_" + name;

		case Config::generalInputSourcesAnalyzing:
			return this->tablePrefix + "analyzed_" + name;

		case Config::generalInputSourcesCrawling:
			return this->tablePrefix + "crawled";
		}

		throw Exception("Analyzer::Database::getSourceTableName(): Invalid source type for text corpus");
	}

	// public helper function: get the full name of a source column, throws Database::Exception
	std::string Database::getSourceColumnName(unsigned short type, const std::string& name) {
		switch(type) {
		case Config::generalInputSourcesParsing:
			if(name == "id")
				return "parsed_id";
			else if(name == "datetime")
				return "parsed_datetime";

			return "parsed__" + name;

		case Config::generalInputSourcesExtracting:
			return "extracted__" + name;

		case Config::generalInputSourcesAnalyzing:
			return "analyzed__" + name;

		case Config::generalInputSourcesCrawling:
			return name;
		}

		throw Exception("Analyzer::Database::getSourceTableName(): Invalid source type for text corpus");
	}

	// public helper function: check input tables and columns, throws Database::Exception
	void Database::checkSources(
			std::vector<unsigned char>& types,
			std::vector<std::string>& tables,
			std::vector<std::string>& columns
	) {
		// remove invalid sources
		for(size_t n = 1; n <= tables.size(); ++n) {
			if(!this->checkSource(types.at(n - 1), tables.at(n - 1), columns.at(n - 1))) {
				--n;

				types.erase(types.begin() + n);
				tables.erase(tables.begin() + n);
				columns.erase(columns.begin() + n);
			}
		}

		// check for valid sources
		if(types.empty() || tables.empty() || columns.empty())
			throw Exception("Analyzer::Database::checkSources(): No input sources available");
	}

	// public helper function: check input table and column
	bool Database::checkSource(
			unsigned short type,
			const std::string& table,
			const std::string& column
	) {
		// get full table name
		const std::string tableName(this->getSourceTableName(type, table));

		// check existence of table
		if(this->database.isTableExists(tableName)) {
			// get full column name
			const std::string columnName(this->getSourceColumnName(type, column));

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

	// helper function: check whether the basis for a specific corpus has changed since its creation,
	//  return true if corpus was not created yet, throws Database::Exception
	// NOTE: Corpora from raw crawling data will always be re-created!
	bool Database::isCorpusChanged(const CorpusProperties& corpusProperties) {
		bool result = true;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.isCorpusChanged))
			throw Exception("Analyzer::Database::isCorpusChanged():"
					" Missing prepared SQL statement for getting the corpus creation time");

		// get prepared SQL statement
		sql::PreparedStatement& corpusStatement(this->getPreparedStatement(this->ps.isCorpusChanged));

		unsigned short sourceStatement = 0;

		switch(corpusProperties.sourceType) {
		case Config::generalInputSourcesParsing:
			sourceStatement = this->ps.isCorpusChangedParsing;
			break;

		case Config::generalInputSourcesExtracting:
			sourceStatement = this->ps.isCorpusChangedExtracting;
			break;

		case Config::generalInputSourcesAnalyzing:
			sourceStatement = this->ps.isCorpusChangedAnalyzing;
			break;

		case Config::generalInputSourcesCrawling:
			return true; // always re-create corpus for crawling data
		}

		if(!sourceStatement)
			throw Exception(
					"Analyzer::Database::isCorpusChanged():"
					" Missing prepared SQL statement for creating text corpus from specified source type"
			);

		sql::PreparedStatement& tableStatement = this->getPreparedStatement(sourceStatement);

		try {
			// execute SQL query for getting the update time of the source table
			tableStatement.setString(1, corpusProperties.sourceTable);

			SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(tableStatement));

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				const std::string updateTime(sqlResultSet->getString("updated"));

				// execute SQL query for comparing the creation time of the corpus with the update time of the table
				corpusStatement.setUInt(1, corpusProperties.sourceType);
				corpusStatement.setString(2, corpusProperties.sourceTable);
				corpusStatement.setString(3, corpusProperties.sourceField);
				corpusStatement.setString(4, updateTime);

				sqlResultSet = SqlResultSetPtr(Database::sqlExecuteQuery(corpusStatement));

				// get result
				if(sqlResultSet && sqlResultSet->next())
					result = !(sqlResultSet->getBoolean("result"));
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Analyzer::Database::isCorpusChanged", e); }

		return result;
	}

	// create and add text corpus
	// 	if an old corpus exists, it will be deleted, when using parsed data, date and article maps will be created,
	//  throws Database::Exception
	void Database::createCorpus(
			const CorpusProperties& corpusProperties,
			Data::Corpus& corpusTo,
			size_t& sourcesTo
	) {
		// initialize values
		corpusTo.clear();

		sourcesTo = 0;

		// check connection
		this->checkConnection();

		// check prepared SQL statements
		if(!(this->ps.deleteCorpus))
			throw Exception(
					"Analyzer::Database::createCorpus(): Missing prepared SQL statement for deleting text corpus"
			);

		if(!(this->ps.addCorpus))
			throw Exception(
					"Analyzer::Database::createCorpus(): Missing prepared SQL statement for adding text corpus"
			);

		// get prepared SQL statements
		sql::PreparedStatement& deleteStatement = this->getPreparedStatement(this->ps.deleteCorpus);
		sql::PreparedStatement& addStatement = this->getPreparedStatement(this->ps.addCorpus);

		// check your sources
		this->checkSource(
				corpusProperties.sourceType,
				corpusProperties.sourceTable,
				corpusProperties.sourceField
		);

		// show warning when using raw crawled data and logging is enabled
		if(corpusProperties.sourceType == Config::generalInputSourcesCrawling) {
			this->log(this->getLoggingMin(), "WARNING: Corpus will always be re-created when created from raw crawled data.");
			this->log(this->getLoggingMin(), "It is highly recommended to use parsed data instead!");

			if(!corpusProperties.sourceTable.empty())
				this->log(this->getLoggingMin(), "WARNING: Source table name ignored.");

			if(!corpusProperties.sourceField.empty())
				this->log(this->getLoggingMin(), "WARNING: Source field name ignored.");
		}

		// start timing and write log entry
		Timer::Simple timer;

		const std::string tableName(
				this->getSourceTableName(
						corpusProperties.sourceType,
						corpusProperties.sourceTable
				)
		);
		const std::string columnName(
				this->getSourceColumnName(
						corpusProperties.sourceType,
						corpusProperties.sourceField
				)
		);

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
			deleteStatement.setUInt(1, corpusProperties.sourceType);
			deleteStatement.setString(2, corpusProperties.sourceTable);
			deleteStatement.setString(3, corpusProperties.sourceField);

			Database::sqlExecute(deleteStatement);

			// get texts and possibly parsed date/times and IDs from database
			Data::GetColumns data;

			data.table = tableName;

			data.columns.reserve(3);

			data.columns.emplace_back(columnName);

			if(corpusProperties.sourceType == Config::generalInputSourcesParsing) {
				data.columns.emplace_back("parsed_id");
				data.columns.emplace_back("parsed_datetime");

				data.order = "parsed_datetime";
			}

			data.type = DataType::_string;

			this->getCustomData(data);

			if(data.values.empty())
				throw Exception("Analyzer::Database::createCorpus(): Could not get requested data from database");

			// move received column data to vector(s) of strings
			std::vector<std::string> texts;
			std::vector<std::string> articleIds;
			std::vector<std::string> dateTimes;

			texts.reserve(data.values.at(0).size());

			if(data.values.size() > 1) {
				articleIds.reserve(data.values.at(1).size());

				if(data.values.size() > 2)
					dateTimes.reserve(data.values.at(2).size());
			}

			for(size_t n = 0; n < data.values.at(0).size(); ++n) {
				auto& text = data.values.at(0).at(n);

				if(!text._isnull && !text._s.empty()) {
					// add text as source
					++sourcesTo;

					// move text to vector
					texts.emplace_back(std::move(text._s));

					if(data.values.size() > 1) {
						// move article ID to vector
						auto& articleId = data.values.at(1).at(n);

						if(!articleId._isnull && !articleId._s.empty())
							articleIds.emplace_back(std::move(articleId._s));
						else
							articleIds.emplace_back();

						if(data.values.size() > 2) {
							// move date/time to vector
							auto& dateTime = data.values.at(2).at(n);

							if(!dateTime._isnull && !dateTime._s.empty())
								dateTimes.emplace_back(std::move(dateTime._s));
							else
								dateTimes.emplace_back();
						}
					}
				}
			}

			// create corpus (and delete the input data)
			if(data.values.size() > 1)
				corpusTo.create(texts, articleIds, dateTimes, true);
			else
				corpusTo.create(texts, true);

			// slice corpus into chunks for the database
			std::vector<std::string> chunks;
			std::vector<Struct::TextMap> articleMaps, dateMaps;

			corpusTo.copyChunks(
					static_cast<size_t>(
							this->getMaxAllowedPacketSize()
							* (static_cast<float>(this->corpusSlicing) / 100)
					),
					chunks,
					articleMaps,
					dateMaps
			);

			// add corpus chunks to the database
			size_t last = 0;

			for(size_t n = 0; n < chunks.size(); ++n) {
				addStatement.setUInt(1, corpusProperties.sourceType);
				addStatement.setString(2, corpusProperties.sourceTable);
				addStatement.setString(3, corpusProperties.sourceField);
				addStatement.setString(4, chunks.at(n));

				if(articleMaps.size() > n)
					addStatement.setString(5, Helper::Json::stringify(articleMaps.at(n)));
				else
					addStatement.setNull(5, 0);

				if(dateMaps.size() > n)
					addStatement.setString(6, Helper::Json::stringify(dateMaps.at(n)));
				else
					addStatement.setNull(6, 0);

				if(last)
					addStatement.setUInt64(7, last);
				else
					addStatement.setNull(7, 0);

				addStatement.setUInt64(8, sourcesTo);

				Database::sqlExecute(addStatement);

				last = this->getLastInsertedId();

				// free memory early
				std::string().swap(chunks.at(n));

				if(articleMaps.size() > n)
					Struct::TextMap().swap(articleMaps.at(n));

				if(dateMaps.size() > n)
					Struct::TextMap().swap(dateMaps.at(n));
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Analyzer::Database::createCorpus", e); }

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

} /* crawlservpp::Module::Analyzer */
