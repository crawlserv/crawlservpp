/*
 * Database.cpp
 *
 * This class provides database functionality for an analyzer thread by implementing the Module::DBWrapper interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#include "Database.h"

namespace crawlservpp::Module::Analyzer {

// constructor: initialize database and set default values
Database::Database(crawlservpp::Module::DBThread& dbThread)
		: crawlservpp::Module::DBWrapper(dbThread) {
	this->website = 0;
	this->urlList = 0;
	this->logging = false;
	this->verbose = false;

	this->psGetCorpus = 0;
	this->psIsCorpusChanged = 0;
	this->psIsCorpusChangedParsing = 0;
	this->psIsCorpusChangedExtracting = 0;
	this->psIsCorpusChangedAnalyzing = 0;
	this->psDeleteCorpus = 0;
	this->psAddCorpus = 0;
}

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
}

// create target table if it does not exists or add field columns if they do not exist
// NOTE: Needs to be called by algorithm class in order to get the required field names!
bool Database::initTargetTable(bool compressed) {
	// check options
	if(!(this->websiteName.length())) {
		if(this->logging) this->log("analyzer", "[#" + this->idString + "] ERROR: No website specified.");
		return false;
	}
	if(!(this->urlListName.length())) {
		if(this->logging) this->log("analyzer", "[#" + this->idString + "] ERROR: No URL list specified.");
		return false;
	}
	if(!(this->targetTableName.length())) {
		if(this->logging) this->log("analyzer", "[#" + this->idString + "] ERROR: Name of result table is empty.");
		return false;
	}

	bool emptyFields = true;
	for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i) {
		if(i->length()) {
			emptyFields = false;
			break;
		}
	}

	if(emptyFields) {
		if(this->logging) this->log("analyzer", "[#" + this->idString + "] ERROR: No target fields specified (only empty strings).");
		return false;
	}

	// create target table name
	this->targetTableFull = "crawlserv_" + this->websiteName + "_" + this->urlListName + "_analyzed_" + this->targetTableName;

	// check existence of target table
	if(this->isTableExists(this->targetTableFull)) {
		// table exists already: add columns that do not exist yet
		for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i) {
			if(i->length() && !(this->isColumnExists(this->targetTableFull, "analyzed__" + *i))) {
				std::string type = this->targetFieldTypes.at(i - this->targetFieldNames.begin());
				if(!type.length()) {
					if(this->logging)
						this->log("analyzer", "[#" + this->idString + "] ERROR: No type for target field \'" + *i + "\' specified.");
					return false;
				}
				this->execute("ALTER TABLE `" + this->targetTableFull + "` ADD `analyzed__" + *i + "` " + type);
			}
		}
		if(compressed) this->execute("ALTER TABLE `" + this->targetTableFull + "` ROW_FORMAT=COMPRESSED");
	}
	else {
		// table does not exist already: create table
		std::string sqlQuery = "CREATE TABLE `" + this->targetTableFull + "`(id SERIAL";
		for(auto i = this->targetFieldNames.begin(); i != this->targetFieldNames.end(); ++i) {
			std::string type = this->targetFieldTypes.at(i - this->targetFieldNames.begin());
			if(!type.length()) {
				if(this->logging)
					this->log("analyzer", "[#" + this->idString + "] ERROR: No type for target field \'" + *i + "\' specified.");
				return false;
			}
			sqlQuery += ", `analyzed__" + *i + "` " + type;
		}
		sqlQuery += ", PRIMARY KEY(id)) CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci";
		if(compressed) sqlQuery += ", ROW_FORMAT=COMPRESSED";
		this->execute(sqlQuery);

		// add target table to index
		this->addAnalyzedTable(this->website, this->urlList, this->targetTableName);
	}

	return true;
}

// prepare SQL statements for analyzer
bool Database::prepare() {
	// create table prefix
	this->tablePrefix = "crawlserv_" + this->websiteName + "_" + this->urlListName + "_";

	// check connection to database
	if(!(this->checkConnection())) {
		this->errorMessage = this->getDatabaseErrorMessage();
		return false;
	}

	// reserve memory
	this->reservePreparedStatements(7);

	try {
		// prepare SQL statements for analyzer
		if(!(this->psGetCorpus)) {
			if(verbose) this->log("analyzer", "[#" + this->idString + "] prepares getCorpus()...");
			this->psGetCorpus = this->addPreparedStatement("SELECT corpus, datemap, sources FROM crawlserv_corpora WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND source_type = ? AND source_table = ?"
					" AND source_field = ? ORDER BY created DESC LIMIT 1");
		}
		if(!(this->psIsCorpusChanged)) {
			if(verbose) this->log("analyzer", "[#" + this->idString + "] prepares isCorpusChanged() [1/4]...");
			this->psIsCorpusChanged = this->addPreparedStatement("SELECT EXISTS (SELECT * FROM crawlserv_corpora WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND source_type = ? AND source_table = ?"
					" AND source_field = ? AND created > ?) AS result");
		}
		if(!(this->psIsCorpusChangedParsing)) {
			if(verbose) this->log("analyzer", "[#" + this->idString + "] prepares isCorpusChanged() [2/4]...");
			this->psIsCorpusChangedParsing = this->addPreparedStatement("SELECT updated FROM crawlserv_parsedtables WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND name = ?");
		}
		if(!(this->psIsCorpusChangedExtracting)) {
			if(verbose) this->log("analyzer", "[#" + this->idString + "] prepares isCorpusChanged() [3/4]...");
			this->psIsCorpusChangedExtracting = this->addPreparedStatement("SELECT updated FROM crawlserv_extractedtables WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND name = ?");
		}
		if(!(this->psIsCorpusChangedAnalyzing)) {
			if(verbose) this->log("analyzer", "[#" + this->idString + "] prepares isCorpusChanged() [4/4]...");
			this->psIsCorpusChangedAnalyzing = this->addPreparedStatement("SELECT updated FROM crawlserv_analyzedtables WHERE website = "
					+ this->websiteIdString + " AND urllist = " + this->listIdString + " AND name = ?");
		}
		if(!(this->psDeleteCorpus)) {
			if(verbose) this->log("analyzer", "[#" + this->idString + "] prepares createCorpus() [1/2]...");
			this->psDeleteCorpus = this->addPreparedStatement("DELETE FROM crawlserv_corpora WHERE website = " + this->websiteIdString
					+ " AND urllist = " + this->listIdString + " AND source_type = ? AND source_table = ? AND source_field = ? LIMIT 1");
		}
		if(!(this->psAddCorpus)) {
			if(verbose) this->log("analyzer", "[#" + this->idString + "] prepares createCorpus() [2/2]...");
			this->psAddCorpus = this->addPreparedStatement("INSERT INTO crawlserv_corpora(website, urllist, source_type, source_table,"
					" source_field, corpus, datemap, sources) VALUES (" + this->websiteIdString + ", " + this->listIdString
					+ ", ?, ?, ?, ?, ?, ?)");
		}
	}
	catch(sql::SQLException &e) {
		// set error message
		std::ostringstream errorStrStr;
		errorStrStr << "prepare() SQL Error #" << e.getErrorCode() << " (State " << e.getSQLState() << "): " << e.what();
		this->errorMessage = errorStrStr.str();
		return false;
	}

	return true;
}

// get text corpus and save it to corpusTo - the corpus will be created if it is out-of-date or does not exist
void Database::getCorpus(unsigned short sourceType, const std::string& sourceTable, const std::string& sourceField, std::string& corpusTo,
		unsigned long& sourcesTo, const std::string& filterDateFrom, const std::string& filterDateTo) {
	sql::ResultSet * sqlResultSet = NULL;
	std::string dateMap;

	// check arguments
	if(!sourceTable.length()) {
		if(this->logging) this->log("analyzer", "[#" + this->idString + "] WARNING: Name of source table is empty.");
		return;
	}
	if(!sourceField.length()) {
		if(this->logging) this->log("analyzer", "[#" + this->idString + "] WARNING: Name of source field is empty.");
		return;
	}

	// check whether text corpus needs to be created
	if(this->isCorpusChanged(sourceType, sourceTable, sourceField)) {
		this->createCorpus(sourceType, sourceTable, sourceField, corpusTo, dateMap, sourcesTo);
	}
	else {
		// check prepared SQL statement
		if(!(this->psGetCorpus))
			throw std::runtime_error("Missing prepared SQL statement for getting the corpus");
		sql::PreparedStatement * sqlStatement = this->getPreparedStatement(this->psGetCorpus);

		// check connection
		if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

		try {
			// execute SQL query for getting the corpus
			sqlStatement->setUInt(1, sourceType);
			sqlStatement->setString(2, sourceTable);
			sqlStatement->setString(3, sourceField);
			sqlResultSet = sqlStatement->executeQuery();

			// get result
			if(sqlResultSet && sqlResultSet->next()) {
				corpusTo = sqlResultSet->getString("corpus");
				if(!(sqlResultSet->isNull("datemap"))) dateMap = sqlResultSet->getString("datemap");
				sourcesTo = sqlResultSet->getUInt64("sources");

				if(this->logging) {
					std::ostringstream logStrStr;
					logStrStr.imbue(std::locale(""));
					logStrStr << "[#" << this->idString << "] got text corpus of " << corpusTo.size() << " bytes.";
					this->log("analyzer", logStrStr.str());
				}
			}

			// delete result
			MAIN_DATABASE_DELETE(sqlResultSet);
		}
		catch(sql::SQLException &e) {
			// SQL error
			MAIN_DATABASE_DELETE(sqlResultSet);
			std::ostringstream errorStrStr;
			errorStrStr << "getCorpus() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") - " << e.what();
			throw std::runtime_error(errorStrStr.str());
		}
	}

	// filter corpus by dates
	if(filterDateFrom.length()) {
		if(dateMap.length()) {
			std::string filteredCorpus;
			filteredCorpus.reserve(corpusTo.length());

			// parse datemap
			rapidjson::Document document;
			if(document.Parse(dateMap.c_str()).HasParseError()) throw std::runtime_error("getCorpus(): Could not parse datemap of corpus");
			if(!document.IsArray()) throw std::runtime_error("getCorpus(): Invalid datemap (is not an array)");

			for(auto i = document.Begin(); i != document.End(); ++i) {
				rapidjson::Value::MemberIterator p = i->FindMember("p");
				rapidjson::Value::MemberIterator l = i->FindMember("l");
				rapidjson::Value::MemberIterator v = i->FindMember("v");
				if(p == i->MemberEnd() || !(p->value.IsUint64()))
					throw std::runtime_error("getCorpus(): Invalid datemap (could not find valid position)");
				if(l == i->MemberEnd() || !(l->value.IsUint64()))
					throw std::runtime_error("getCorpus(): Invalid datemap (could not find valid length)");
				if(v == i->MemberEnd() || !(v->value.IsString()))
					throw std::runtime_error("getCorpus(): Invalid datemap (could not find valid value)");

				if(crawlservpp::Helper::DateTime::isISODateInRange(v->value.GetString(), filterDateFrom, filterDateTo)) {
					filteredCorpus += corpusTo.substr(p->value.GetUint64(), l->value.GetUint64());
					filteredCorpus.push_back(' ');
				}
			}

			if(filteredCorpus.length()) filteredCorpus.pop_back();
			corpusTo.swap(filteredCorpus);

			if(this->logging) {
				std::ostringstream logStrStr;
				logStrStr.imbue(std::locale(""));
				logStrStr << "[#" << this->idString << "] filtered corpus to " << corpusTo.length() << " bytes.";
				this->log("analyzer", logStrStr.str());
			}
		}
		else throw std::runtime_error("getCorpus(): No datemap for corpus found");
	}
}

// helper function: check whether the basis for a specific corpus has changed since its creation, return true if corpus was not created yet
// NOTE: Corpora from raw crawling data will always be re-created!
bool Database::isCorpusChanged(unsigned short sourceType, const std::string& sourceTable, const std::string& sourceField) {
	sql::ResultSet * sqlResultSet = NULL;
	bool result = true;

	// check prepared SQL statements
	if(!(this->psIsCorpusChanged))
		throw std::runtime_error("Missing prepared SQL statement for getting the corpus creation time");
	sql::PreparedStatement * corpusStatement = this->getPreparedStatement(this->psIsCorpusChanged);

	sql::PreparedStatement * tableStatement = NULL;
	switch(sourceType) {
	case Config::generalInputSourcesParsing:
		if(!(this->psIsCorpusChangedParsing))
			throw std::runtime_error("Missing prepared SQL statement for getting the parsing table update time");
		tableStatement = this->getPreparedStatement(this->psIsCorpusChangedParsing);
		break;
	case Config::generalInputSourcesExtracting:
		if(!(this->psIsCorpusChangedExtracting))
			throw std::runtime_error("Missing prepared SQL statement for getting the extracting table update time");
		tableStatement = this->getPreparedStatement(this->psIsCorpusChangedExtracting);
		break;
	case Config::generalInputSourcesAnalyzing:
		if(!(this->psIsCorpusChangedAnalyzing))
			throw std::runtime_error("Missing prepared SQL statement for getting the annalyzing table update time");
		tableStatement = this->getPreparedStatement(this->psIsCorpusChangedAnalyzing);
		break;
	case Config::generalInputSourcesCrawling:
		return true; // always re-create corpus for crawling data
	}
	if(!tableStatement) throw std::runtime_error("Invalid source type for text corpus");

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	try {
		// execute SQL query for getting the update time of the source table
		tableStatement->setString(1, sourceTable);
		sqlResultSet = tableStatement->executeQuery();

		// get result
		if(sqlResultSet && sqlResultSet->next()) {
			std::string updateTime = sqlResultSet->getString("updated");

			// delete result
			MAIN_DATABASE_DELETE(sqlResultSet);

			// execute SQL query for comparing the creation time of the corpus with the update time of the table
			corpusStatement->setUInt(1, sourceType);
			corpusStatement->setString(2, sourceTable);
			corpusStatement->setString(3, sourceField);
			corpusStatement->setString(4, updateTime);
			sqlResultSet = corpusStatement->executeQuery();

			// get result
			if(sqlResultSet && sqlResultSet->next()) result = !(sqlResultSet->getBoolean("result"));

			// delete result
			MAIN_DATABASE_DELETE(sqlResultSet);
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		MAIN_DATABASE_DELETE(sqlResultSet);
		std::ostringstream errorStrStr;
		errorStrStr << "isCorpusChanged() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") - " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	return result;
}

// create and add text corpus (old corpus will be deleted if it exists, a datemap will be created when using parsed data)
void Database::createCorpus(unsigned short sourceType, const std::string& sourceTable, const std::string& sourceField,
		std::string& corpusTo, std::string& dateMapTo, unsigned long& sourcesTo) {
	std::vector<TextMapEntry> dateMap;
	TextMapEntry dateMapEntry;

	// initialize values
	corpusTo.clear();
	dateMapTo.clear();
	sourcesTo = 0;
	dateMapEntry = std::make_tuple("", 0, 0);

	// check prepared SQL statements
	if(!(this->psDeleteCorpus))
		throw std::runtime_error("Missing prepared SQL statement for deleting text corpus");
	sql::PreparedStatement * deleteStatement = this->getPreparedStatement(this->psDeleteCorpus);
	if(!(this->psAddCorpus))
		throw std::runtime_error("Missing prepared SQL statement for adding text corpus");
	sql::PreparedStatement * addStatement = this->getPreparedStatement(this->psAddCorpus);

	// check source type
	std::string tableName, columnName;
	switch(sourceType) {
	case Config::generalInputSourcesParsing:
		tableName = this->tablePrefix + "parsed_" + sourceTable;
		columnName = "parsed__" + sourceField;
		break;
	case Config::generalInputSourcesExtracting:
		tableName = this->tablePrefix + "extracted_" + sourceTable;
		columnName = "extracted__" + sourceField;
		break;
	case Config::generalInputSourcesAnalyzing:
		tableName = this->tablePrefix + "analyzed_" + sourceTable;
		columnName = "analyzed__" + sourceField;
		break;
	case Config::generalInputSourcesCrawling:
		tableName = this->tablePrefix + "crawled";
		columnName = "content";
		if(this->logging) {
			this->log("analyzer", "[#" + this->idString + "] WARNING: Corpus will always be re-created when created from raw crawled data.");
			this->log("analyzer", "[#" + this->idString + "]  It is highly recommended to use parsed data instead!");
			if(sourceTable.length()) this->log("analyzer", "[#" + this->idString + "] WARNING: Source table name ignored.");
			if(sourceField.length()) this->log("analyzer", "[#" + this->idString + "] WARNING: Source field name ignored.");
		}
		break;
	default:
		throw std::runtime_error("Invalid source type for text corpus");
	}

	// check connection
	if(!(this->checkConnection())) throw std::runtime_error(this->errorMessage);

	// start timing and write log entry
	crawlservpp::Timer::Simple timer;
	if(this->logging) this->log("analyzer", "[#" + this->idString + "] compiles text corpus from " + tableName + "." + columnName + "...");

	try {
		// execute SQL query for deleting old text corpus (if it exists)
		deleteStatement->setUInt(1, sourceType);
		deleteStatement->setString(2, sourceTable);
		deleteStatement->setString(3, sourceField);
		deleteStatement->execute();

		// get texts and possibly parsed datetimes from database
		crawlservpp::Main::Data::GetColumns data;
		data.table = tableName;
		data.columns.reserve(2);
		data.columns.push_back(columnName);
		if(sourceType == Config::generalInputSourcesParsing) data.columns.push_back("parsed_datetime");
		data.type = DataType::_string;
		data.order = "parsed_datetime";
		this->getCustomData(data);

		if(data.values.size()) {
			// go through all strings
			for(auto i = data.values.at(0).begin(); i != data.values.at(0).end(); ++i) {
				// check string
				if((!(i->_isnull)) && i->_s.length()) {
					// check for datetime (i.e. whether to create a datemap)
					if(data.values.size() > 1) {
						auto datetime = data.values.at(1).begin() + (i - data.values.at(0).begin());

						/*
						 * USAGE OF DATE MAP ENTRIES:
						 *
						 *		[std::string&] std::get<0>(DateMapEntry) -> date of the text part as string ("YYYY-MM-DD")
						 *		[unsigned long&] std::get<1>(DateMapEntry) -> position of the text part in the text corpus (starts with 1!)
						 * 		[unsigned long&] std::get<2>(DateMapEntry) -> length of the text part
						 *
						 */

						// check for current datetime
						if((!(datetime->_isnull)) && datetime->_s.length() > 9) {
							// found current datetime -> create date string
							std::string date = datetime->_s.substr(0, 10); // get only date (YYYY-MM-DD) from datetime

							// check whether a date is already set
							if(std::get<0>(dateMapEntry).length()) {
								// date is already set -> compare last with current date
								if(std::get<0>(dateMapEntry) == date) {
									// last date equals current date -> append last date
									std::get<2>(dateMapEntry) += i->_s.length() + 1; // include space before current string to add
								}
								else {
									// last date differs from current date -> conclude last date and start new date
									dateMap.push_back(dateMapEntry);
									dateMapEntry = std::make_tuple(date, corpusTo.length(), i->_s.length());
								}
							}
							else {
								// no date is set yet -> start new date
								dateMapEntry = std::make_tuple(date, corpusTo.length(), i->_s.length());
							}
						}
						else if(std::get<0>(dateMapEntry).length()) {
							// no valid datetime was found, but last date is set -> conclude last date
							dateMap.push_back(dateMapEntry);
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
			if(corpusTo.size()) corpusTo.pop_back();	// remove last space
			if(std::get<0>(dateMapEntry).length()) {	// check for unfinished date
				// conclude last date
				dateMap.push_back(dateMapEntry);
			}

			if(corpusTo.size() <= this->getMaxAllowedPacketSize()) {
				// add corpus to database if possible
				addStatement->setUInt(1, sourceType);
				addStatement->setString(2, sourceTable);
				addStatement->setString(3, sourceField);
				addStatement->setString(4, corpusTo);
				if(sourceType == Config::generalInputSourcesParsing) {
					dateMapTo = crawlservpp::Helper::Json::stringify(dateMap);
					addStatement->setString(5, dateMapTo);
				}
				else addStatement->setNull(5, 0);
				addStatement->setUInt64(6, sourcesTo);
				addStatement->execute();
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
				this->log("analyzer", logStrStr.str());
				if(adjustServerSettings)
					this->log("analyzer", "[#" + this->idString + "] Adjust the server's \'max_allowed_packet\' setting accordingly.");
			}
		}
	}
	catch(sql::SQLException &e) {
		// SQL error
		std::ostringstream errorStrStr;
		errorStrStr << "createCorpus() SQL Error #" << e.getErrorCode() << " (SQLState " << e.getSQLState() << ") - " << e.what();
		throw std::runtime_error(errorStrStr.str());
	}

	if(this->logging) {
		// write log entry
		std::ostringstream logStrStr;
		logStrStr.imbue(std::locale(""));
		logStrStr << "[#" << this->idString << "]  compiled text corpus of " << corpusTo.size() << " bytes in " + timer.tickStr() << ".";
		this->log("analyzer", logStrStr.str());
	}
}

}
