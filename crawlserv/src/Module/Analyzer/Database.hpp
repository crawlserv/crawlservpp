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

#include "../../Data/Corpus.hpp"
#include "../../Data/Data.hpp"
#include "../../Helper/Portability/mysqlcppconn.h"
#include "../../Main/Exception.hpp"
#include "../../Helper/Json.hpp"
#include "../../Struct/CorpusProperties.hpp"
#include "../../Struct/TableColumn.hpp"
#include "../../Struct/TargetTableProperties.hpp"
#include "../../Struct/TextMap.hpp"
#include "../../Timer/Simple.hpp"
#include "../../Wrapper/Database.hpp"
#include "../../Wrapper/DatabaseLock.hpp"

#include "../../_extern/rapidjson/include/rapidjson/document.h"

#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <algorithm>	// std::find_if
#include <chrono>		// std::chrono
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint16_t, std::uint64_t
#include <functional>	// std::function
#include <locale>		// std::locale
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <utility>		// std::move
#include <vector>		// std::vector

namespace crawlservpp::Module::Analyzer {

	class Database : public Wrapper::Database {
		// for convenience
		using JsonException = Helper::Json::Exception;

		using DataType = Data::Type;

		using CustomTableProperties = Struct::TargetTableProperties;
		using CorpusProperties = Struct::CorpusProperties;
		using TableColumn = Struct::TableColumn;

		using DatabaseLock = Wrapper::DatabaseLock<Database>;

		using IsRunningCallback = std::function<bool()>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;

	public:
		Database(Module::Database& dbRef);
		virtual ~Database();

		// setters
		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields, const std::vector<std::string>& types);
		void setTimeoutTargetLock(std::uint64_t timeOut);
		void setCorpusSlicing(std::uint8_t percentageOfMaxAllowedPackageSize);
		void setIsRunningCallback(IsRunningCallback isRunningCallback);

		// prepare target table and SQL statements for analyzer
		void initTargetTable(bool compressed);
		void prepare();

		// prepare and get custom SQL statements for algorithm
		void prepareAlgo(const std::vector<std::string>& statements, std::vector<std::uint16_t>& idsTo);
		sql::PreparedStatement& getPreparedAlgoStatement(std::uint16_t sqlStatementId);

		// corpus functions
		void getCorpus(
				const CorpusProperties& corpusProperties,
				const std::string& filterDateFrom,
				const std::string& filterDateTo,
				Data::Corpus& corpusTo,
				std::size_t& sourcesTo
		);

		// public helper functions
		std::string getSourceTableName(std::uint16_t type, const std::string& name);
		std::string getSourceColumnName(std::uint16_t type, const std::string& name);
		void checkSources(
				std::vector<std::uint8_t>& types,
				std::vector<std::string>& tables,
				std::vector<std::string>& columns
		);
		bool checkSource(
				std::uint16_t type,
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
		std::uint64_t timeoutTargetLock;
		std::uint8_t corpusSlicing;

		// table prefix, target table ID and its name
		std::string tablePrefix;
		std::uint64_t targetTableId;
		std::string targetTableFull;

		// corpus helper function
		bool isCorpusChanged(const CorpusProperties& corpusProperties);
		void createCorpus(
				const CorpusProperties& corpusProperties,
				Data::Corpus& corpusTo,
				std::size_t& sourcesTo
		);

	private:
		// IDs of prepared SQL statements
		struct _ps {
			std::uint16_t getCorpusFirst;
			std::uint16_t getCorpusNext;
			std::uint16_t isCorpusChanged;
			std::uint16_t isCorpusChangedParsing;
			std::uint16_t isCorpusChangedExtracting;
			std::uint16_t isCorpusChangedAnalyzing;
			std::uint16_t deleteCorpus;
			std::uint16_t addChunk;
			std::uint16_t measureChunk;
			std::uint16_t measureCorpus;

			std::vector<std::uint16_t> algo;
		} ps;

		// function for checking whether the parent thread is still running
		IsRunningCallback isRunning;
	};

} /* crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_DATABASE_HPP_ */
