/*
 * Database.h
 *
 * This class provides database functionality for an analyzer thread by implementing the Module::DBWrapper interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_DATABASE_H_
#define MODULE_ANALYZER_DATABASE_H_

#include "Config.h"

#include "../DBThread.h"
#include "../DBWrapper.h"

#include "../../Main/Data.h"
#include "../../Helper/DateTime.h"
#include "../../Helper/Json.h"
#include "../../Timer/Simple.h"

#include "../../_extern/rapidjson/document.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <mysql_connection.h>

#include <chrono>
#include <locale>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace crawlservpp::Module::Analyzer {
	// for convenience
	typedef crawlservpp::Main::Data::Type DataType;

	// text maps are used to describe certain parts of a text defined by their positions and lengths with certain strings (words, dates etc.)
	typedef std::tuple<std::string, unsigned long, unsigned long> TextMapEntry;

	class Database : public crawlservpp::Module::DBWrapper {
	public:
		Database(crawlservpp::Module::DBThread& dbRef);
		virtual ~Database();

		// setters
		void setId(unsigned long analyzerId);
		void setWebsite(unsigned long websiteId);
		void setWebsiteNamespace(const std::string& websiteNamespace);
		void setUrlList(unsigned long listId);
		void setUrlListNamespace(const std::string& urlListNamespace);
		void setLogging(bool isLogging);
		void setVerbose(bool isVerbose);
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields, const std::vector<std::string>& types);

		// prepare target table and SQL statements for analyzer
		bool initTargetTable(bool compressed);
		bool prepare();

		// corpus functions
		void getCorpus(unsigned short sourceType, const std::string& sourceTable, const std::string& sourceField, std::string& corpusTo,
				unsigned long& sourcesTo, const std::string& filterDateFrom, const std::string& filterDateTo);

	protected:
		// options
		std::string idString;
		unsigned long website;
		std::string websiteIdString;
		std::string websiteName;
		unsigned long urlList;
		std::string listIdString;
		std::string urlListName;
		bool logging;
		bool verbose;
		std::string targetTableName;
		std::vector<std::string> targetFieldNames;
		std::vector<std::string> targetFieldTypes;

		// table namespace
		std::string tablePrefix;
		std::string targetTableFull;

		// corpus helper function
		bool isCorpusChanged(unsigned short sourceType, const std::string& sourceTable, const std::string& sourceField);
		void createCorpus(unsigned short sourceType, const std::string& sourceTable, const std::string& sourceField,
				std::string& corpusTo, std::string& dateMapTo, unsigned long& sourcesTo);

	private:
		// prepared SQL queries
		unsigned short psGetCorpus;
		unsigned short psIsCorpusChanged;
		unsigned short psIsCorpusChangedParsing;
		unsigned short psIsCorpusChangedExtracting;
		unsigned short psIsCorpusChangedAnalyzing;
		unsigned short psDeleteCorpus;
		unsigned short psAddCorpus;
	};
}

#endif /* MODULE_ANALYZER_DATABASE_H_ */
