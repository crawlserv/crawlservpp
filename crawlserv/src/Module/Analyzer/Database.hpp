/*
 * Database.h
 *
 * This class provides database functionality for an analyzer thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_DATABASE_H_
#define MODULE_ANALYZER_DATABASE_H_

#include "Config.h"

#include "../Database.h"

#include "../../Main/Data.h"
#include "../../Helper/DateTime.h"
#include "../../Helper/Json.h"
#include "../../Struct/CorpusProperties.h"
#include "../../Struct/TableColumn.h"
#include "../../Struct/TargetTableProperties.h"
#include "../../Timer/Simple.h"
#include "../../Wrapper/TargetTablesLock.h"
#include "../../Wrapper/Database.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <mysql_connection.h>

#include <chrono>
#include <functional>
#include <locale>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace crawlservpp::Module::Analyzer {
	class Database : public Wrapper::Database {
		// for convenience
		typedef Main::Data::Type DataType;
		typedef Main::Database::Exception DatabaseException;
		typedef Struct::TargetTableProperties CustomTableProperties;
		typedef Struct::CorpusProperties CorpusProperties;
		typedef Struct::TableColumn TableColumn;
		typedef Wrapper::TargetTablesLock TargetTablesLock;

		typedef std::function<bool()> CallbackIsRunning;
		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;

		// text maps are used to describe certain parts of a text
		//  defined by their positions and lengths with certain strings (words, dates etc.)
		typedef std::tuple<std::string, unsigned long, unsigned long> TextMapEntry;

	public:
		Database(Module::Database& dbRef);
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
		void setTimeoutTargetLock(unsigned long timeOut);

		// prepare target table and SQL statements for analyzer
		void initTargetTable(bool compressed, CallbackIsRunning isRunning);
		void prepare();

		// prepare and get custom SQL statements for algorithm
		void prepareAlgo(const std::vector<std::string>& statements, std::vector<unsigned short>& idsTo);
		sql::PreparedStatement& getPreparedAlgoStatement(unsigned short sqlStatementId);

		// corpus functions
		void getCorpus(const CorpusProperties& corpusProperties, std::string& corpusTo,
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
		unsigned long timeoutTargetLock;

		// table prefix, target table ID and name
		std::string tablePrefix;
		unsigned long targetTableId;
		std::string targetTableFull;

		// corpus helper function
		bool isCorpusChanged(const CorpusProperties& corpusProperties);
		void createCorpus(const CorpusProperties& corpusProperties,
				std::string& corpusTo, std::string& dateMapTo, unsigned long& sourcesTo);

	private:
		// IDs of prepared SQL statements
		struct {
			unsigned short getCorpus;
			unsigned short isCorpusChanged;
			unsigned short isCorpusChangedParsing;
			unsigned short isCorpusChangedExtracting;
			unsigned short isCorpusChangedAnalyzing;
			unsigned short deleteCorpus;
			unsigned short addCorpus;
			std::vector<unsigned short> algo;
		} ps;
	};
}

#endif /* MODULE_ANALYZER_DATABASE_H_ */
