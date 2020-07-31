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
 * This class provides database functionality for analyzer threads
 *  by implementing the Wrapper::Database interface.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef MODULE_ANALYZER_DATABASE_HPP_
#define MODULE_ANALYZER_DATABASE_HPP_

#include "Config.hpp"

#include "../../Data/Corpus.hpp"
#include "../../Data/Data.hpp"
#include "../../Helper/Json.hpp"
#include "../../Helper/Portability/mysqlcppconn.h"
#include "../../Main/Exception.hpp"
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

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The default percentage of the maximum package size allowed by the MySQL server to be used for the maximum size of the corpus.
	constexpr auto defaultCorpusSlicing{30};

	//! The factor used for corpus slicing percentage points (1/100).
	constexpr auto corpusSlicingFactor{1.F / 100};

	//! The maximum number of columns used when creating a text corpus.
	constexpr auto maxNumCorpusColumns{3};

	//! The number of prepared SQL statements to reserve memory for.
	constexpr auto numPreparedStatements{10};

	///@}
	///@name Constants for SQL Queries
	///@{

	//! First argument in a SQL query.
	constexpr auto sqlArg1{1};

	//! Second argument in a SQL query.
	constexpr auto sqlArg2{2};

	//! Third argument in a SQL query.
	constexpr auto sqlArg3{3};

	//! Fourth argument in a SQL query.
	constexpr auto sqlArg4{4};

	//! Fifth argument in a SQL query.
	constexpr auto sqlArg5{5};

	//! Sixth argument in a SQL query.
	constexpr auto sqlArg6{6};

	//! Seventh argument in a SQL query.
	constexpr auto sqlArg7{7};

	//! Eighth argument in a SQL query.
	constexpr auto sqlArg8{8};

	//! Ninth argument in a SQL query.
	constexpr auto sqlArg9{9};

	///@}
	///@name Constants for Table Columns
	///@{

	//! First column in a table.
	constexpr auto column1{0};

	//! Second column in a table.
	constexpr auto column2{1};

	//! Third column in a table.
	constexpr auto column3{2};

	//! One table column.
	constexpr auto numColumns1{1};

	//! Two table columns.
	constexpr auto numColumns2{2};

	///@}

	/*
	 * DECLARATION
	 */

	//! Class providing database functionality for analyzer threads by implementing Wrapper::Database.
	class Database final : public Wrapper::Database {
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
		///@name Construction
		///@{

		explicit Database(Module::Database& dbThread);

		///@}
		///@name Analyzer-specific Setters
		///@{

		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<std::string>& fields, const std::vector<std::string>& types);
		void setCorpusSlicing(std::uint8_t percentageOfMaxAllowedPackageSize);
		void setIsRunningCallback(const IsRunningCallback& isRunningCallback);

		///@}
		///@name Target Table Initialization
		///@{

		void initTargetTable(bool compressed);

		///@}
		///@name Prepared SQL Statements
		///@{

		void prepare();
		void prepareAlgo(const std::vector<std::string>& statements, std::vector<std::uint16_t>& idsTo);
		[[nodiscard]] sql::PreparedStatement& getPreparedAlgoStatement(std::uint16_t sqlStatementId);

		///@}
		///@name Text Corpus
		///@{

		void getCorpus(
				const CorpusProperties& corpusProperties,
				const std::string& filterDateFrom,
				const std::string& filterDateTo,
				Data::Corpus& corpusTo,
				std::size_t& sourcesTo
		);

		///@}
		///@name Analyzer-specific Helpers
		///@{

		[[nodiscard]] std::string getSourceTableName(std::uint16_t type, const std::string& name) const;
		[[nodiscard]] static std::string getSourceColumnName(std::uint16_t type, const std::string& name);
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

		///@}

		//! Class for analyzer-specific database exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		///@name Analyzer Properties
		///@{

		//! The prefix used for tables in the MySQL database.
		std::string tablePrefix;

		//! The name of the target table to be written to.
		std::string targetTableName;

		//! The ID of the target table to be written to.
		std::uint64_t targetTableId{0};

		//! The full name of the target table to be written to, including prefixes.
		std::string targetTableFull;

		//! The names of the target fields, i.e. the columns in the target table, to be written to.
		std::vector<std::string> targetFieldNames;

		//! The types of the target fields, i.e. the columns in the target table, to be written to.
		std::vector<std::string> targetFieldTypes;

		//! The maximum size of the text corpus chunks, in percentage of the maximum package size allowed by the MySQL server.
		/*!
		 *  Must be between 1 and 99, i.e. between
		 *  one and ninety-nine percent.
		 */
		std::uint8_t corpusSlicing{defaultCorpusSlicing};

		///@}
		///@name Text Corpus Helpers
		///@{

		[[nodiscard]] bool isCorpusChanged(const CorpusProperties& corpusProperties);
		void createCorpus(
				const CorpusProperties& corpusProperties,
				Data::Corpus& corpusTo,
				std::size_t& sourcesTo
		);

		///@}

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
		} ps{};

		// function for checking whether the parent thread is still running
		IsRunningCallback isRunning;
	};

} /* namespace crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_DATABASE_HPP_ */
