/*
 * Database.cpp
 *
 * This class provides database functionality for an analyzer thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

namespace crawlservpp::Module::Analyzer {

// constructor: initialize database and set default values
Database::Database(Module::Database& dbThread) : Wrapper::Database(dbThread), website(0), urlList(0),
		logging(false), verbose(false), timeoutTargetLock(0), targetTableId(0), ps({0}) {}

// destructor stub
Database::~Database() {}

// convert thread ID to string for logging
void Database::setId(unsigned long analyzerId) {
	std::ostringstream idStrStr;
	idStrStr << analyzerId;
	this->idString = idStrStr.str();
}

// set website ID (and convert it to string for SQL requests)
void Database::setWebsite(unsigned long websiteId) {
	std::ostringstream idStrStr;
	idStrStr << websiteId;
	this->website = websiteId;
	this->websiteIdString = idStrStr.str();
}

// set website namespace
void Database::setWebsiteNamespace(const std::string& websiteNamespace) {
	this->websiteName = websiteNamespace;
}

// set URL list ID (and convert it to string for SQL requests)
void Database::setUrlList(unsigned long listId) {
	std::ostringstream idStrStr;
	idStrStr << listId;
	this->urlList = listId;
	this->listIdString = idStrStr.str();
}

// set URL list namespace
void Database::setUrlListNamespace(const std::string& urlListNamespace) {
	this->urlListName = urlListNamespace;
}

// enable or disable logging
void Database::setLogging(bool isLogging) {
	this->logging = isLogging;
}

// enable or disable verbose logging
void Database::setVerbose(bool isVerbose) {
	this->verbose = isVerbose;
}

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
	while(this->targetFieldTypes.size() < this->targetFieldNames.size()) {
		this->targetFieldTypes.emplace_back();
	}
}

// set time-out for target table lock
void Database::setTimeoutTargetLock(unsigned long timeOut) {
	this->timeoutTargetLock = timeOut;
}

// create target table if it does not exists or add field columns if they do not exist
// 	NOTE:	Needs to be called by algorithm class in order to get the required field names!
//  		Waiting for other locks to be released requires a callback function to get the running status of the thread.
//  throws Main::Database::Exception
void Database::initTargetTable(bool compressed, CallbackIsRunning isRunning) {
	// check options
	if(this->websiteName.empty())
		throw DatabaseException("Analyzer::Database::initTargetTable(): No website specified");
	if(this->urlListName.empty())
		throw DatabaseException("Analyzer::Database::initTargetTable(): No URL list specified");
	if(this->targetTableName.empty())
		throw DatabaseException("Analyzer::Database::initTargetTable(): Name of result table is empty");

	bool emptyFields = true;
	for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i) {
		if(!(i->empty())) {
			emptyFields = false;
			break;
		}
	}
	if(emptyFields)
		throw DatabaseException("Analyzer::Database::initTargetTable(): No target fields specified (only empty strings)");

	// create target table name
	this->targetTableFull = "crawlserv_" + this->websiteName + "_" + this->urlListName + "_analyzed_" + this->targetTableName;

	// create table properties
	CustomTableProperties properties("analyzed", this->website, this->urlList, this->targetTableName, this->targetTableFull, false);
	for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i) {
		if(!(i->empty())) {
			properties.columns.emplace_back("analyzed__" + *i, this->targetFieldTypes.at(i - this->targetFieldNames.begin()));
			if(properties.columns.back().type.empty())
				throw DatabaseException("Analyzer::Database::initTargetTable(): No type for target field \'" + *i + "\' specified");
		}
	}

	{ // lock analyzing tables
		TargetTablesLock(*this, "analyzed", this->website, this->urlList, this->timeoutTargetLock, isRunning);
		this->addTargetTable(properties);
	} // analyzing tables unlocked
}

// prepare SQL statements for analyzer, throws Main::Database::Exception
void Database::prepare() {
	// create table prefix
	this->tablePrefix = "crawlserv_" + this->websiteName + "_" + this->urlListName + "_";

	// check connection to database
	this->checkConnection();

	// reserve memory
	this->reserveForPreparedStatements(7);

	try {
		// prepare SQL statements for analyzer
		if(!(this->ps.getCorpus)) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares getCorpus()...");
			this->ps.getCorpus = this->addPreparedStatement("SELECT corpus, datemap, sources FROM crawlserv_corpora WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND source_type = ? AND source_table = ?"
					" AND source_field = ? ORDER BY created DESC LIMIT 1");
		}
		if(!(this->ps.isCorpusChanged)) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares isCorpusChanged() [1/4]...");
			this->ps.isCorpusChanged = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM crawlserv_corpora WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND source_type = ? AND source_table = ?"
					" AND source_field = ? AND created > ?) AS result");
		}
		if(!(this->ps.isCorpusChangedParsing)) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares isCorpusChanged() [2/4]...");
			this->ps.isCorpusChangedParsing = this->addPreparedStatement("SELECT updated FROM crawlserv_parsedtables WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND name = ?");
		}
		if(!(this->ps.isCorpusChangedExtracting)) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares isCorpusChanged() [3/4]...");
			this->ps.isCorpusChangedExtracting = this->addPreparedStatement("SELECT updated FROM crawlserv_extractedtables WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND name = ?");
		}
		if(!(this->ps.isCorpusChangedAnalyzing)) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares isCorpusChanged() [4/4]...");
			this->ps.isCorpusChangedAnalyzing = this->addPreparedStatement("SELECT updated FROM crawlserv_analyzedtables WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND name = ?");
		}
		if(!(this->ps.deleteCorpus)) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares createCorpus() [1/2]...");
			this->ps.deleteCorpus = this->addPreparedStatement("DELETE FROM crawlserv_corpora WHERE website = " + this->websiteIdString
					+ " AND urllist = " + this->listIdString + " AND source_type = ? AND source_table = ? AND source_field = ? LIMIT 1");
		}
		if(!(this->ps.addCorpus)) {
			if(this->verbose) this->log("[#" + this->idString + "] prepares createCorpus() [2/2]...");
			this->ps.addCorpus = this->addPreparedStatement("INSERT INTO crawlserv_corpora(website, urllist, source_type, source_table,"
					" source_field, corpus, datemap, sources) VALUES (" + this->websiteIdString + ", " + this->listIdString
					+ ", ?, ?, ?, ?, ?, ?)");
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
	if(this->verbose) {
		std::ostringstream logStrStr;
		logStrStr << "[#" << this->idString << "] prepares " << statements.size() << " SQL statements for algorithm...";
		this->log(logStrStr.str());
	}
	for(auto i = statements.begin(); i != statements.end(); ++i)
		idsTo.push_back(this->addPreparedStatement(*i));
}

// get prepared SQL statement for algorithm (wraps protected parent member function to the public)
sql::PreparedStatement& Database::getPreparedAlgoStatement(unsigned short sqlStatementId) {
	return this->getPreparedStatement(sqlStatementId);
}

// get text corpus and save it to corpusTo - the corpus will be created if it is out-of-date or does not exist
void Database::getCorpus(const CorpusProperties& corpusProperties, std::string& corpusTo,
		unsigned long& sourcesTo, const std::string& filterDateFrom, const std::string& filterDateTo) {
	std::string dateMap;

	// check arguments
	if(corpusProperties.sourceTable.empty()) {
		if(this->logging) this->log("[#" + this->idString + "] WARNING: Name of source table is empty.");
		return;
	}
	if(corpusProperties.sourceField.empty()) {
		if(this->logging) this->log("[#" + this->idString + "] WARNING: Name of source field is empty.");
		return;
	}

	// check whether text corpus needs to be created
	if(this->isCorpusChanged(corpusProperties)) {
		this->createCorpus(corpusProperties, corpusTo, dateMap, sourcesTo);
	}
	else {
		// check connection
		this->checkConnection();

		// check prepared SQL statement
		if(!(this->ps.getCorpus))
			throw DatabaseException("Analyzer::Database::getCorpus(): Missing prepared SQL statement for getting the corpus");

		// get prepared SQL statement
		sql::PreparedStatement& sqlStatement = this->getPreparedStatement(this->ps.getCorpus);

		try {
			// execute SQL query for getting the corpus
			sqlStatement.setUInt(1, corpusProperties.sourceType);
			sqlStatement.setString(2, corpusProperties.sourceTable);
			sqlStatement.setString(3, corpusProperties.sourceField);
			std::unique_ptr<sql::ResultSet> sqlResultSet(sqlStatement.executeQuery());

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				corpusTo = sqlResultSet->getString("corpus");
				if(!(sqlResultSet->isNull("datemap"))) dateMap = sqlResultSet->getString("datemap");
				sourcesTo = sqlResultSet->getUInt64("sources");

				if(this->logging) {
					std::ostringstream logStrStr;
					logStrStr.imbue(std::locale(""));
					logStrStr << "[#" << this->idString << "] got text corpus of " << corpusTo.size() << " bytes.";
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
			if(document.Parse(dateMap.c_str()).HasParseError())
				throw DatabaseException("Analyzer::Database::getCorpus(): Could not parse datemap of corpus");
			if(!document.IsArray())
				throw DatabaseException("Analyzer::Database::getCorpus(): Invalid datemap (is not an array)");

			for(auto i = document.Begin(); i != document.End(); ++i) {
				rapidjson::Value::MemberIterator p = i->FindMember("p");
				rapidjson::Value::MemberIterator l = i->FindMember("l");
				rapidjson::Value::MemberIterator v = i->FindMember("v");
				if(p == i->MemberEnd() || !(p->value.IsUint64()))
					throw DatabaseException("Analyzer::Database::getCorpus(): Invalid datemap (could not find valid position)");
				if(l == i->MemberEnd() || !(l->value.IsUint64()))
					throw DatabaseException("Analyzer::Database::getCorpus(): Invalid datemap (could not find valid length)");
				if(v == i->MemberEnd() || !(v->value.IsString()))
					throw DatabaseException("Analyzer::Database::getCorpus(): Invalid datemap (could not find valid value)");

				if(Helper::DateTime::isISODateInRange(v->value.GetString(), filterDateFrom, filterDateTo)) {
					filteredCorpus += corpusTo.substr(p->value.GetUint64(), l->value.GetUint64());
					filteredCorpus.push_back(' ');
				}
			}

			if(!filteredCorpus.empty()) filteredCorpus.pop_back();
			corpusTo.swap(filteredCorpus);

			if(this->logging) {
				std::ostringstream logStrStr;
				logStrStr.imbue(std::locale(""));
				logStrStr << "[#" << this->idString << "] filtered corpus to " << corpusTo.length() << " bytes.";
				this->log(logStrStr.str());
			}
		}
		else throw DatabaseException("Analyzer::Database::getCorpus(): No datemap for corpus found");
	}
}

// helper function: check whether the basis for a specific corpus has changed since its creation,
//  return true if corpus was not created yet
// NOTE: Corpora from raw crawling data will always be re-created!
bool Database::isCorpusChanged(const CorpusProperties& corpusProperties) {
	bool result = true;

	// check connection
	this->checkConnection();

	// check prepared SQL statements
	if(!(this->ps.isCorpusChanged))
		throw DatabaseException("Analyzer::Database::isCorpusChanged():"
				" Missing prepared SQL statement for getting the corpus creation time");

	// get prepared SQL statement
	sql::PreparedStatement& corpusStatement = this->getPreparedStatement(this->ps.isCorpusChanged);

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
	if(!sourceStatement) throw DatabaseException("Analyzer::Database::isCorpusChanged():"
			" Missing prepared SQL statement for creating text corpus from specified source type");
	sql::PreparedStatement& tableStatement = this->getPreparedStatement(sourceStatement);

	try {
		// execute SQL query for getting the update time of the source table
		tableStatement.setString(1, corpusProperties.sourceTable);
		std::unique_ptr<sql::ResultSet> sqlResultSet(tableStatement.executeQuery());

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			std::string updateTime = sqlResultSet->getString("updated");

			// execute SQL query for comparing the creation time of the corpus with the update time of the table
			corpusStatement.setUInt(1, corpusProperties.sourceType);
			corpusStatement.setString(2, corpusProperties.sourceTable);
			corpusStatement.setString(3, corpusProperties.sourceField);
			corpusStatement.setString(4, updateTime);
			sqlResultSet = std::unique_ptr<sql::ResultSet>(corpusStatement.executeQuery());

			// get result
			if(sqlResultSet && sqlResultSet->next()) result = !(sqlResultSet->getBoolean("result"));
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Analyzer::Database::isCorpusChanged", e); }

	return result;
}

// create and add text corpus (old corpus will be deleted if it exists, a datemap will be created when using parsed data)
void Database::createCorpus(const CorpusProperties& corpusProperties, std::string& corpusTo,
		std::string& dateMapTo, unsigned long& sourcesTo) {
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
		throw DatabaseException("Analyzer::Database::createCorpus(): Missing prepared SQL statement for deleting text corpus");
	if(!(this->ps.addCorpus))
		throw DatabaseException("Analyzer::Database::createCorpus(): Missing prepared SQL statement for adding text corpus");

	// get prepared SQL statements
	sql::PreparedStatement& deleteStatement = this->getPreparedStatement(this->ps.deleteCorpus);
	sql::PreparedStatement& addStatement = this->getPreparedStatement(this->ps.addCorpus);

	// check source type
	std::string tableName, columnName;
	switch(corpusProperties.sourceType) {
	case Config::generalInputSourcesParsing:
		tableName = this->tablePrefix + "parsed_" + corpusProperties.sourceTable;
		columnName = "parsed__" + corpusProperties.sourceField;
		break;
	case Config::generalInputSourcesExtracting:
		tableName = this->tablePrefix + "extracted_" + corpusProperties.sourceTable;
		columnName = "extracted__" + corpusProperties.sourceField;
		break;
	case Config::generalInputSourcesAnalyzing:
		tableName = this->tablePrefix + "analyzed_" + corpusProperties.sourceTable;
		columnName = "analyzed__" + corpusProperties.sourceField;
		break;
	case Config::generalInputSourcesCrawling:
		tableName = this->tablePrefix + "crawled";
		columnName = "content";
		if(this->logging) {
			this->log("[#" + this->idString + "] WARNING: Corpus will always be re-created when created from raw crawled data.");
			this->log("[#" + this->idString + "]  It is highly recommended to use parsed data instead!");
			if(!corpusProperties.sourceTable.empty())
				this->log("[#" + this->idString + "] WARNING: Source table name ignored.");
			if(!corpusProperties.sourceField.empty())
				this->log("[#" + this->idString + "] WARNING: Source field name ignored.");
		}
		break;
	default:
		throw DatabaseException("Analyzer::Database::createCorpus(): Invalid source type for text corpus");
	}

	// start timing and write log entry
	Timer::Simple timer;
	if(this->logging) this->log("[#" + this->idString + "] compiles text corpus from " + tableName + "." + columnName + "...");

	try {
		// execute SQL query for deleting old text corpus (if it exists)
		deleteStatement.setUInt(1, corpusProperties.sourceType);
		deleteStatement.setString(2, corpusProperties.sourceTable);
		deleteStatement.setString(3, corpusProperties.sourceField);
		deleteStatement.execute();

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

		if(!data.values.empty()) {
			// go through all strings
			for(auto i = data.values.at(0).begin(); i != data.values.at(0).end(); ++i) {
				// check string
				if((!(i->_isnull)) && !(i->_s.empty())) {
					// check for datetime (i.e. whether to create a datemap)
					if(data.values.size() > 1) {
						auto datetime = data.values.at(1).begin() + (i - data.values.at(0).begin());

						/*
						 * USAGE OF DATE MAP ENTRIES:
						 *
						 *	[std::string&] std::get<0>(DateMapEntry) -> date of the text part as string ("YYYY-MM-DD")
						 *	[unsigned long&] std::get<1>(DateMapEntry) -> position of the text part in the text corpus (starts with 1!)
						 * 	[unsigned long&] std::get<2>(DateMapEntry) -> length of the text part
						 *
						 */

						// check for current datetime
						if((!(datetime->_isnull)) && datetime->_s.length() > 9) {
							// found current datetime -> create date string
							std::string date = datetime->_s.substr(0, 10); // get only date (YYYY-MM-DD) from datetime

							// check whether a date is already set
							if(!std::get<0>(dateMapEntry).empty()) {
								// date is already set -> compare last with current date
								if(std::get<0>(dateMapEntry) == date) {
									// last date equals current date -> append last date
									std::get<2>(dateMapEntry) += i->_s.length() + 1; // include space before current string to add
								}
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
					sourcesTo++;
				}
			}

			// finish up data
			if(!corpusTo.empty()) corpusTo.pop_back();	// remove last space
			if(!std::get<0>(dateMapEntry).empty()) {	// check for unfinished date
				// conclude last date
				dateMap.emplace_back(dateMapEntry);
			}

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
				else addStatement.setNull(5, 0);
				addStatement.setUInt64(6, sourcesTo);
				addStatement.execute();
			}
			else if(this->logging) {
				// show warning about text corpus size
				bool adjustServerSettings = false;
				std::ostringstream logStrStr;
				logStrStr.imbue(std::locale(""));
				logStrStr << "[#" << this->idString << "] WARNING: The text corpus cannot be saved to the database, because its size ("
						<< corpusTo.size() << " bytes) exceeds the ";
				if(corpusTo.size() > 1073741824) logStrStr << "mySQL maximum of 1 GiB.";
				else {
					logStrStr << "current mySQL server maximum of " << this->getMaxAllowedPacketSize() << " bytes.";
					adjustServerSettings = true;
				}
				this->log(logStrStr.str());
				if(adjustServerSettings)
					this->log("[#" + this->idString + "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
			}
		}
	}
	catch(const sql::SQLException &e) { this->sqlException("Analyzer::Database::createCorpus", e); }

	if(this->logging) {
		// write log entry
		std::ostringstream logStrStr;
		logStrStr.imbue(std::locale(""));
		logStrStr << "[#" << this->idString << "]  compiled text corpus of " << corpusTo.size() << " bytes in " + timer.tickStr() << ".";
		this->log(logStrStr.str());
	}
}

}
