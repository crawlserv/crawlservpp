/*
 * Database.hpp
 *
 * This class provides database functionality for an analyzer thread by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_DATABASE_HPP_
#define MODULE_ANALYZER_DATABASE_HPP_

#include "Config.hpp"

#include "../../Main/Data.hpp"
#include "../../Helper/DateTime.hpp"
#include "../../Helper/Json.hpp"
#include "../../Struct/CorpusProperties.hpp"
#include "../../Struct/TableColumn.hpp"
#include "../../Struct/TargetTableProperties.hpp"
#include "../../Timer/Simple.hpp"
#include "../../Wrapper/Database.hpp"
#include "../../Wrapper/TargetTablesLock.hpp"

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
#include <queue>
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
		void setUrlList(unsigned long listId);
		void setNamespaces(const std::string& website, const std::string& urlList);
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

		// public helper functions
		std::string getSourceTableName(unsigned short type, const std::string& name);
		std::string getSourceColumnName(unsigned short type, const std::string& name);
		void checkSources(	std::vector<unsigned short>& types,
							std::vector<std::string>& tables,
							std::vector<std::string>& columns,
							bool logging);
		bool checkSource(	unsigned short type,
							const std::string& table,
							const std::string& column,
							bool logging);

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

} /* crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_DATABASE_HPP_ */
