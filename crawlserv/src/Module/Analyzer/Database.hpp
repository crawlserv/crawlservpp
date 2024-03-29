/*
 *
 * ---
 *
 *  Copyright (C) 2022 Anselm Schmidt (ans[ät]ohai.su)
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
#include "../../Helper/CommaLocale.hpp"
#include "../../Helper/Container.hpp"
#include "../../Helper/Json.hpp"
#include "../../Helper/Memory.hpp"
#include "../../Helper/Portability/mysqlcppconn.h"
#include "../../Main/Exception.hpp"
#include "../../Struct/CorpusProperties.hpp"
#include "../../Struct/StatusSetter.hpp"
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

#include <algorithm>		// std::all_of, std::sort, std::upper_bound
#include <cstddef>			// std::size_t
#include <cstdint>			// std::uint8_t, std::uint16_t, std::uint64_t
#include <functional>		// std::function
#include <queue>			// std::queue
#include <sstream>			// std::ostringstream
#include <string>			// std::string, std::to_string
#include <unordered_map>	// std::unordered_map
#include <utility>			// std::move, std::pair
#include <vector>			// std::vector

namespace crawlservpp::Module::Analyzer {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The default percentage of the maximum package size allowed by the MySQL server to be used for the maximum size of the corpus.
	inline constexpr auto defaultCorpusSlicing{30};

	//! The factor used for corpus slicing percentage points (1/100).
	inline constexpr auto corpusSlicingFactor{1.F / 100};

	//! The maximum number of columns used when creating a text corpus.
	inline constexpr auto maxNumCorpusColumns{3};

	//! The progress with creating a corpus after the old corpus has been deleted.
	inline constexpr auto progressDeletedCorpus{0.05F};

	//! The progress with creating a corpus after the source texts have been received.
	inline constexpr auto progressReceivedSources{0.35F};

	//! The progress with creating a corpus after the data has been moved.
	inline constexpr auto progressMovedData{0.4F};

	//! The progress with creating a corpus after the server created the corpus.
	inline constexpr auto progressCreatedCorpus{0.6F};

	//! The progress with creating a corpus after the corpus has been sliced.
	inline constexpr auto progressSlicedCorpus{0.65F};

	//! The remaining progress, attributed to adding the corpus to the database.
	inline constexpr auto progressAddingCorpus{
		1.F - progressSlicedCorpus
	};

	//! The progress with getting an existing corpus after its contents have been received from the database.
	inline constexpr auto progressReceivedCorpus{0.8F};

	//! The progress of saving a savepoint after generating it.
	inline constexpr auto progressGeneratedSavePoint{0.1F};

	//! The remaining progress, attributed to saving a savepoint to the database.
	inline constexpr auto progressSavingSavePoint{
		1.F - progressGeneratedSavePoint
	};

	///@}
	///@name Constants for SQL Queries
	///@{

	//! First argument in a SQL query.
	inline constexpr auto sqlArg1{1};

	//! Second argument in a SQL query.
	inline constexpr auto sqlArg2{2};

	//! Third argument in a SQL query.
	inline constexpr auto sqlArg3{3};

	//! Fourth argument in a SQL query.
	inline constexpr auto sqlArg4{4};

	//! Fifth argument in a SQL query.
	inline constexpr auto sqlArg5{5};

	//! Sixth argument in a SQL query.
	inline constexpr auto sqlArg6{6};

	//! Seventh argument in a SQL query.
	inline constexpr auto sqlArg7{7};

	//! Eighth argument in a SQL query.
	inline constexpr auto sqlArg8{8};

	//! Ninth argument in a SQL query.
	inline constexpr auto sqlArg9{9};

	//! Tenth argument in a SQL query.
	inline constexpr auto sqlArg10{10};

	//! Eleventh argument in a SQL query.
	inline constexpr auto sqlArg11{11};

	//! Twelfth argument in a SQL query.
	inline constexpr auto sqlArg12{12};

	///@}
	///@name Constants for Table Columns
	///@{

	//! First column in a table.
	inline constexpr auto column1{0};

	//! Second column in a table.
	inline constexpr auto column2{1};

	//! Third column in a table.
	inline constexpr auto column3{2};

	//! One table column.
	inline constexpr auto numColumns1{1};

	//! Two table columns.
	inline constexpr auto numColumns2{2};

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
		using StatusSetter = Struct::StatusSetter;
		using TableColumn = Struct::TableColumn;
		using TextMap = Struct::TextMap;

		using DatabaseLock = Wrapper::DatabaseLock<Database>;

		using IsRunningCallback = std::function<bool()>;
		using SentenceMap = std::vector<std::pair<std::size_t, std::size_t>>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;
		using StringString = std::pair<std::string, std::string>;

	public:
		///@name Construction
		///@{

		explicit Database(Module::Database& dbThread);

		///@}
		///@name Analyzer-specific Setters
		///@{

		void setTargetTable(const std::string& table);
		void setTargetFields(const std::vector<StringString>& fields);
		void setCorpusSlicing(std::uint8_t percentageOfMaxAllowedPackageSize);
		void setIsRunningCallback(const IsRunningCallback& isRunningCallback);

		///@}
		///@name Target Table Initialization and Update
		///@{

		void initTargetTable(bool isCompressed, bool isDelete);
		void updateTargetTable();
		[[nodiscard]] std::string getTargetTableUpdated();

		///@}
		///@name Additional Tables
		///@{

		std::size_t addAdditionalTable(
				const std::string& name,
				const std::vector<StringString>& fields,
				bool isCompressed,
				bool isDelete
		);
		const std::string& getAdditionalTableName(std::size_t id) const;
		void updateAdditionalTable(std::size_t id);

		///@}
		///@name Prepared SQL Statements
		///@{

		void prepare();

		///@}
		///@name Text Corpus
		///@{

		[[nodiscard]] bool getCorpus(
				const CorpusProperties& corpusProperties,
				const std::string& filterDateFrom,
				const std::string& filterDateTo,
				Data::Corpus& corpusTo,
				std::size_t& sourcesTo,
				StatusSetter& statusSetter
		);
		[[nodiscard]] std::string getCorporaLastUpdated() const;

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
		std::uint64_t targetTableId{};

		//! The full name of the target table to be written to, including prefixes.
		std::string targetTableFull;

		//! The names and types of the target fields, i.e. the columns in the target table to be written to.
		std::vector<StringString> targetFields;

		//! The maximum size of the text corpus chunks, in percentage of the maximum package size allowed by the MySQL server.
		/*!
		 *  Must be between 1 and 99, i.e. between
		 *  one and ninety-nine percent.
		 */
		std::uint8_t corpusSlicing{defaultCorpusSlicing};

		//! The IDs and full names of additional tables to write data to.
		std::unordered_map<std::size_t, std::string> additionalTables;

	private:
		// IDs of prepared SQL statements
		struct _ps {
			std::size_t getCorpusInfo{};
			std::size_t checkCorpusSavePoint{};
			std::size_t getCorpusFirst{};
			std::size_t getCorpusSavePoint{};
			std::size_t getCorpusNext{};
			std::size_t isCorpusChanged{};
			std::size_t isCorpusChangedParsing{};
			std::size_t isCorpusChangedExtracting{};
			std::size_t isCorpusChangedAnalyzing{};
			std::size_t deleteCorpus{};
			std::size_t addChunkContinuous{};
			std::size_t addChunkTokenized{};
			std::size_t measureChunk{};
			std::size_t measureCorpus{};
			std::size_t updateTargetTable{};
			std::size_t getTargetTableUpdated{};
			std::size_t updateAdditionalTable{};
		} ps;

		// function for checking whether the parent thread is still running
		IsRunningCallback isRunning;

		// last update date/time over all corpus sources
		std::string corporaLastUpdated;

		// internal helper function
		bool checkSource(
				std::uint16_t type,
				const std::string& table,
				const std::string& column
		);

		// internal corpus functions
		[[nodiscard]] bool corpusIsChanged(
				const CorpusProperties& properties
		);
		void corpusCreate(
				const CorpusProperties& properties,
				Data::Corpus& corpusTo,
				std::size_t& sourcesTo,
				StatusSetter& statusSetter
		);
		void corpusLoad(
				CorpusProperties& properties,
				Data::Corpus& corpusTo,
				std::size_t& sourcesTo,
				StatusSetter& statusSetter
		);
		[[nodiscard]] std::string corpusFindSavePoint(
				CorpusProperties& properties,
				const std::string& corpusCreationTime
		);
		[[nodiscard]] bool corpusManipulate(
				const CorpusProperties& properties,
				Data::Corpus& corpusRef,
				std::size_t numSources,
				StatusSetter& statusSetter
		);
		void corpusSaveSavePoint(
				const CorpusProperties& properties,
				const Data::Corpus& corpus,
				std::size_t numSources,
				const std::string& savePoint,
				StatusSetter& statusSetter
		);
	};

} /* namespace crawlservpp::Module::Analyzer */

#endif /* MODULE_ANALYZER_DATABASE_HPP_ */
