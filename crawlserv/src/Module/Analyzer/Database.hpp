/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
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

#include "../../Helper/Portability/mysqlcppconn.h"
#include "../../Main/Data.hpp"
#include "../../Main/Exception.hpp"
#include "../../Helper/DateTime.hpp"
#include "../../Helper/Json.hpp"
#include "../../Struct/CorpusProperties.hpp"
#include "../../Struct/TableColumn.hpp"
#include "../../Struct/TargetTableProperties.hpp"
#include "../../Timer/Simple.hpp"
#include "../../Wrapper/Database.hpp"

#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <algorithm>	// std::find_if
#include <chrono>		// std::chrono
#include <locale>		// std::locale
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <tuple>		// std::get(std::tuple), std::make_tuple, std::tuple
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer {

	class Database : public Wrapper::Database {
		// for convenience
		typedef Helper::Json::Exception JsonException;
		typedef Main::Data::Type DataType;
		typedef Struct::TargetTableProperties CustomTableProperties;
		typedef Struct::CorpusProperties CorpusProperties;
		typedef Struct::TableColumn TableColumn;

		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;

		// text maps are used to describe certain parts of a text
		//  defined by their positions and lengths with certain strings (words, dates etc.)
		typedef std::tuple<std::string, unsigned long, unsigned long> TextMapEntry;

	public:
		Database(Module::Database& dbRef);
		virtual ~Database();

		// setters
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields, const std::vector<std::string>& types);
		void setTimeoutTargetLock(unsigned long timeOut);

		// prepare target table and SQL statements for analyzer
		void initTargetTable(bool compressed);
		void prepare();

		// prepare and get custom SQL statements for algorithm
		void prepareAlgo(const std::vector<std::string>& statements, std::vector<unsigned short>& idsTo);
		sql::PreparedStatement& getPreparedAlgoStatement(unsigned short sqlStatementId);

		// corpus functions
		void getCorpus(
				const CorpusProperties& corpusProperties,
				std::string& corpusTo,
				unsigned long& sourcesTo,
				const std::string& filterDateFrom,
				const std::string& filterDateTo
		);

		// public helper functions
		std::string getSourceTableName(unsigned short type, const std::string& name);
		std::string getSourceColumnName(unsigned short type, const std::string& name);
		void checkSources(
				std::vector<unsigned char>& types,
				std::vector<std::string>& tables,
				std::vector<std::string>& columns
		);
		bool checkSource(
				unsigned short type,
				const std::string& table,
				const std::string& column
		);

		// class for Analyzer::Database exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		// options
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
		void createCorpus(
				const CorpusProperties& corpusProperties,
				std::string& corpusTo,
				std::string& dateMapTo,
				unsigned long& sourcesTo
		);

	private:
		// IDs of prepared SQL statements
		struct _ps {
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
