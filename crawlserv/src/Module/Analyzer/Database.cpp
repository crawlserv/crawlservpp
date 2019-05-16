/*
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
	void Database::setTimeoutTargetLock(unsigned long timeOut) {
		this->timeoutTargetLock = timeOut;
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
		// check connection to database
		this->checkConnection();

		// reserve memory
		this->reserveForPreparedStatements(7);

		try {
			// prepare SQL statements for analyzer
			if(!(this->ps.getCorpus)) {
				if(this->isVerbose())
					this->log("prepares getCorpus()...");

				this->ps.getCorpus = this->addPreparedStatement(
						"SELECT corpus, datemap, sources FROM crawlserv_corpora"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND source_type = ?"
						" AND source_table = ?"
						" AND source_field = ?"
						" ORDER BY created DESC"
						" LIMIT 1"
				);
			}

			if(!(this->ps.isCorpusChanged)) {
				if(this->isVerbose())
					this->log("prepares isCorpusChanged() [1/4]...");

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
				if(this->isVerbose())
					this->log("prepares isCorpusChanged() [2/4]...");

				this->ps.isCorpusChangedParsing = this->addPreparedStatement(
						"SELECT updated FROM crawlserv_parsedtables"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(!(this->ps.isCorpusChangedExtracting)) {
				if(this->isVerbose())
					this->log("prepares isCorpusChanged() [3/4]...");

				this->ps.isCorpusChangedExtracting = this->addPreparedStatement(
						"SELECT updated FROM crawlserv_extractedtables"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(!(this->ps.isCorpusChangedAnalyzing)) {
				if(this->isVerbose())
					this->log("prepares isCorpusChanged() [4/4]...");

				this->ps.isCorpusChangedAnalyzing = this->addPreparedStatement(
						"SELECT updated FROM crawlserv_analyzedtables"
						" WHERE website = "	+ this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND name = ?"
				);
			}

			if(!(this->ps.deleteCorpus)) {
				if(this->isVerbose())
					this->log("prepares createCorpus() [1/2]...");

				this->ps.deleteCorpus = this->addPreparedStatement(
						"DELETE FROM crawlserv_corpora"
						" WHERE website = " + this->getWebsiteIdString() +
						" AND urllist = " + this->getUrlListIdString() +
						" AND source_type = ?"
						" AND source_table = ?"
						" AND source_field = ?"
						" LIMIT 1"
				);
			}

			if(!(this->ps.addCorpus)) {
				if(this->isVerbose())
					this->log("prepares createCorpus() [2/2]...");

				this->ps.addCorpus = this->addPreparedStatement(
						"INSERT INTO crawlserv_corpora"
							"(website, urllist, source_type, source_table, source_field, corpus, datemap, sources)"
							" VALUES (" + this->getWebsiteIdString() + ", " + this->getUrlListIdString() + ", ?, ?, ?, ?, ?, ?)"
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
		if(this->isVerbose())
			this->log(
					"prepares "
					+ std::to_string(statements.size())
					+ " SQL statements for algorithm..."
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
			std::string& corpusTo,
			unsigned long& sourcesTo,
			const std::string& filterDateFrom,
			const std::string& filterDateTo
	) {
		std::string dateMap;

		// check arguments
		if(corpusProperties.sourceTable.empty()) {
			if(this->isLogging())
				this->log("WARNING: Name of source table is empty.");

			return;
		}

		if(corpusProperties.sourceField.empty()) {
			if(this->isLogging())
				this->log("WARNING: Name of source field is empty.");

			return;
		}

		// check whether text corpus needs to be created
		if(this->isCorpusChanged(corpusProperties))
			this->createCorpus(corpusProperties, corpusTo, dateMap, sourcesTo);
		else {
			// check connection
			this->checkConnection();

			// check prepared SQL statement
			if(!(this->ps.getCorpus))
				throw Exception("Analyzer::Database::getCorpus(): Missing prepared SQL statement for getting the corpus");

			// get prepared SQL statement
			sql::PreparedStatement& sqlStatement(this->getPreparedStatement(this->ps.getCorpus));

			try {
				// execute SQL query for getting the corpus
				sqlStatement.setUInt(1, corpusProperties.sourceType);
				sqlStatement.setString(2, corpusProperties.sourceTable);
				sqlStatement.setString(3, corpusProperties.sourceField);

				SqlResultSetPtr sqlResultSet(Database::sqlExecuteQuery(sqlStatement));

				// get result
				if(sqlResultSet && sqlResultSet->next()) {
					corpusTo = sqlResultSet->getString("corpus");

					if(!(sqlResultSet->isNull("datemap")))
						dateMap = sqlResultSet->getString("datemap");

					sourcesTo = sqlResultSet->getUInt64("sources");

					if(this->isLogging()) {
						std::ostringstream logStrStr;

						logStrStr.imbue(std::locale(""));

						logStrStr	<< "got text corpus of "
									<< corpusTo.size()
									<< " bytes.";

						this->log(logStrStr.str());
					}
				}
			}
			catch(const sql::SQLException &e) { this->sqlException("Analyzer::Database::getCorpus", e); }
		}

		// filter corpus by dates
		if(!filterDateFrom.empty()) {
			if(!dateMap.empty()) {
				std::string filteredCorpus;

				filteredCorpus.reserve(corpusTo.length());

				// parse datemap
				rapidjson::Document document;

				try {
					document = Helper::Json::parseRapid(dateMap);
				}
				catch(const JsonException& e) {
					throw Exception(
							"Analyzer::Database::getCorpus(): Could not parse datemap of corpus - "
							+ e.whatStr()
					);
				}

				if(!document.IsArray())
					throw Exception("Analyzer::Database::getCorpus(): Invalid datemap (is not an array)");

				for(const auto& element : document.GetArray()) {
					if(!element.IsObject())
						throw Exception(
								"Analyzer::Database::getCorpus(): Invalid datemap (an array element is not an object"
						);

					auto p = element.FindMember("p");
					auto l = element.FindMember("l");
					auto v = element.FindMember("v");

					if(p == element.MemberEnd() || !(p->value.IsUint64()))
						throw Exception(
								"Analyzer::Database::getCorpus(): Invalid datemap (could not find valid position)"
						);

					if(l == element.MemberEnd() || !(l->value.IsUint64()))
						throw Exception(
								"Analyzer::Database::getCorpus(): Invalid datemap (could not find valid length)"
						);

					if(v == element.MemberEnd() || !(v->value.IsString()))
						throw Exception(
								"Analyzer::Database::getCorpus(): Invalid datemap (could not find valid value)"
						);

					if(Helper::DateTime::isISODateInRange(v->value.GetString(), filterDateFrom, filterDateTo)) {
						filteredCorpus += corpusTo.substr(p->value.GetUint64(), l->value.GetUint64());

						filteredCorpus.push_back(' ');
					}
				}

				if(!filteredCorpus.empty())
					filteredCorpus.pop_back();

				corpusTo.swap(filteredCorpus);

				if(this->isLogging()) {
					std::ostringstream logStrStr;

					logStrStr.imbue(std::locale(""));

					logStrStr	<< "filtered corpus to "
								<< corpusTo.length()
								<< " bytes.";

					this->log(logStrStr.str());
				}
			}
			else
				throw Exception("Analyzer::Database::getCorpus(): No datemap for corpus found");
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
			std::vector<std::string>& columns,
			bool logging
	) {
		// remove invalid sources
		for(unsigned long n = 1; n <= tables.size(); ++n) {
			if(!this->checkSource(types.at(n - 1), tables.at(n - 1), columns.at(n - 1), logging)) {
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
			const std::string& column,
			bool logging
	) {
		// get full table name
		const std::string tableName(this->getSourceTableName(type, table));

		// check existence of table
		if(this->database.isTableExists(tableName)) {
			// get full column name
			const std::string columnName(this->getSourceColumnName(type, column));

			// check existence of column
			if(!(this->database.isColumnExists(tableName, columnName))) {
				if(logging)
					this->log(
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
			if(logging)
				this->log(
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

	// create and add text corpus (old corpus will be deleted if it exists, a datemap will be created when using parsed data),
	//  throws Database::Exception
	void Database::createCorpus(
			const CorpusProperties& corpusProperties,
			std::string& corpusTo,
			std::string& dateMapTo,
			unsigned long& sourcesTo
	) {
		std::vector<TextMapEntry> dateMap;
		TextMapEntry dateMapEntry;

		// initialize values
		corpusTo.clear();
		dateMapTo.clear();

		sourcesTo = 0;

		dateMapEntry = std::make_tuple("", 0, 0);

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
				corpusProperties.sourceField,
				this->isLogging()
		);

		// show warning when using raw crawled data and logging is enabled
		if(
				corpusProperties.sourceType == Config::generalInputSourcesCrawling
				&& this->isLogging()
		) {
			if(this->isLogging()) {
				this->log("WARNING: Corpus will always be re-created when created from raw crawled data.");

				this->log("It is highly recommended to use parsed data instead!");

				if(!corpusProperties.sourceTable.empty())
					this->log("WARNING: Source table name ignored.");

				if(!corpusProperties.sourceField.empty())
					this->log("WARNING: Source field name ignored.");
			}
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

		if(this->isLogging())
			this->log(
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

			// get texts and possibly parsed datetimes from database
			Main::Data::GetColumns data;

			data.table = tableName;
			data.columns.reserve(2);
			data.columns.emplace_back(columnName);

			if(corpusProperties.sourceType == Config::generalInputSourcesParsing) {
				data.columns.emplace_back("parsed_datetime");
				data.order = "parsed_datetime";
			}

			data.type = DataType::_string;

			this->getCustomData(data);

			// go through all strings
			for(auto i = data.values.at(0).begin(); i != data.values.at(0).end(); ++i) {
				// check string
				if((!(i->_isnull)) && !(i->_s.empty())) {
					// check for datetime (i.e. whether to create a datemap)
					if(data.values.size() > 1) {
						const auto datetime = data.values.at(1).begin() + (i - data.values.at(0).begin());

						/*
						 * USAGE OF DATE MAP ENTRIES:
						 *
						 *	[std::string&] std::get<0>(DateMapEntry)
						 *		-> date of the text part as string ("YYYY-MM-DD")
						 *	[unsigned long&] std::get<1>(DateMapEntry)
						 *		-> position of the text part in the text corpus (starts with 1!)
						 * 	[unsigned long&] std::get<2>(DateMapEntry)
						 * 		-> length of the text part
						 *
						 */

						// check for current datetime
						if((!(datetime->_isnull)) && datetime->_s.length() > 9) {
							// found current datetime -> create date string
							const std::string date(datetime->_s, 10); // get only date (YYYY-MM-DD) from datetime

							// check whether a date is already set
							if(!std::get<0>(dateMapEntry).empty()) {
								// date is already set -> compare last with current date
								if(std::get<0>(dateMapEntry) == date)
									// last date equals current date -> append last date
									std::get<2>(dateMapEntry) += i->_s.length() + 1; // include space before current string to add
								else {
									// last date differs from current date -> conclude last date and start new date
									dateMap.emplace_back(dateMapEntry);

									dateMapEntry = std::make_tuple(date, corpusTo.length(), i->_s.length());
								}
							}
							else {
								// no date is set yet -> start new date
								dateMapEntry = std::make_tuple(date, corpusTo.length(), i->_s.length());
							}
						}
						else if(!std::get<0>(dateMapEntry).empty()) {
							// no valid datetime was found, but last date is set -> conclude last date
							dateMap.emplace_back(dateMapEntry);

							dateMapEntry = std::make_tuple("", 0, 0);
						}
					}

					// concatenate corpus text
					corpusTo += i->_s;

					corpusTo.push_back(' ');

					++sourcesTo;
				}

				// finish up data
				if(!corpusTo.empty())
					corpusTo.pop_back(); // remove last space

				if(!std::get<0>(dateMapEntry).empty()) // check for unfinished date
					// conclude last date
					dateMap.emplace_back(dateMapEntry);

				if(corpusTo.size() <= this->getMaxAllowedPacketSize()) {
					// add corpus to database if possible
					addStatement.setUInt(1, corpusProperties.sourceType);
					addStatement.setString(2, corpusProperties.sourceTable);
					addStatement.setString(3, corpusProperties.sourceField);
					addStatement.setString(4, corpusTo);

					if(corpusProperties.sourceType == Config::generalInputSourcesParsing) {
						dateMapTo = Helper::Json::stringify(dateMap);

						addStatement.setString(5, dateMapTo);
					}
					else
						addStatement.setNull(5, 0);

					addStatement.setUInt64(6, sourcesTo);

					Database::sqlExecute(addStatement);
				}
				else if(this->isLogging()) {
					// show warning about text corpus size
					bool adjustServerSettings = false;
					std::ostringstream logStrStr;

					logStrStr.imbue(std::locale(""));

					logStrStr	<< "WARNING: The text corpus cannot be saved to the database, because its size ("
								<< corpusTo.size()
								<< " bytes) exceeds the ";

					if(corpusTo.size() > 1073741824)
						logStrStr << "mySQL maximum of 1 GiB.";
					else {
						logStrStr << "current mySQL server maximum of " << this->getMaxAllowedPacketSize() << " bytes.";

						adjustServerSettings = true;
					}

					this->log(logStrStr.str());

					if(adjustServerSettings)
						this->log("Adjust the server's \'max_allowed_packet\' setting accordingly.");
				}
			}
		}
		catch(const sql::SQLException &e) { this->sqlException("Analyzer::Database::createCorpus", e); }

		if(this->isLogging()) {
			// write log entry
			std::ostringstream logStrStr;

			logStrStr.imbue(std::locale(""));

			logStrStr	<< "compiled text corpus of "
						<< corpusTo.size()
						<< " bytes in "
						<< timer.tickStr()
						<< ".";

			this->log(logStrStr.str());
		}
	}

} /* crawlservpp::Module::Analyzer */
