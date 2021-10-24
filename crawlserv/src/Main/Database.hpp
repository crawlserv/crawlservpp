/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * Class handling database access for the command-and-control server and its threads.
 * Thread-specific functionality is not implemented in this (parent) class.
 *
 * NOT THREAD-SAFE!
 * Use only one instance per thread.
 * Use instances of the child class Module::Database for module-specific functionality functionality instead.
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#ifndef MAIN_DATABASE_HPP_
#define MAIN_DATABASE_HPP_

// optional debugging options
//#define MAIN_DATABASE_DEBUG_REQUEST_COUNTER // enable database request counter for debugging purposes
//#define MAIN_DATABASE_DEBUG_DEADLOCKS		// enable documentation of deadlocks by writing hashes ('#') to stdout
#define MAIN_DATABASE_LOG_MOVING			// log the moving of websites from one data directory to another to stdout

#include "Exception.hpp"
#include "Version.hpp"

#include "../Data/Data.hpp"
#include "../Helper/Container.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Json.hpp"
#include "../Helper/Portability/locale.h"
#include "../Helper/Portability/mysqlcppconn.h"
#include "../Helper/Strings.hpp"
#include "../Helper/Utf8.hpp"
#include "../Helper/Versions.hpp"
#include "../Struct/ConfigProperties.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/TableColumn.hpp"
#include "../Struct/TableProperties.hpp"
#include "../Struct/TargetTableProperties.hpp"
#include "../Struct/ThreadDatabaseEntry.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/ThreadStatus.hpp"
#include "../Struct/UrlListProperties.hpp"
#include "../Struct/WebsiteProperties.hpp"
#include "../Timer/Simple.hpp"
#include "../Wrapper/DatabaseLock.hpp"
#include "../Wrapper/DatabaseTryLock.hpp"
#include "../Wrapper/PreparedSqlStatement.hpp"

#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <algorithm>	// std::find, std::find_if, std::for_each, std::remove, std::sort, std::transform, std::unique
#include <cctype>		// std::tolower
#include <chrono>		// std::chrono
#include <cmath>		// std::isnan
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint32_t, std::uint64_t
#include <fstream>		// std::ifstream
#include <functional>	// std::function
#include <iostream>		// std::cout, std::endl, std::flush
#include <locale>		// std::locale
#include <memory>		// std::unique_ptr
#include <mutex>		// std::lock_guard, std::mutex
#include <queue>		// std::queue
#include <sstream>		// std::istringstream, std::ostringstream
#include <stdexcept>	// std::logic_error
#include <string>		// std::getline, std::stoull, std::string, std::to_string
#include <string_view>	// std::string_view, std::string_view_literals
#include <thread>		// std::this_thread
#include <tuple>		// std::get<...>
#include <utility>		// std::pair, std::swap
#include <vector>		// std::vector

// optional header
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
#include <atomic>		// std::atomic
#endif

// forward-declaration for being friends
namespace crawlservpp::Wrapper {

	class Database;

} /* namespace crawlservpp::Wrapper */

namespace crawlservpp::Main {

	/*
	 * CONSTANTS
	 */

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! (Sub-)Directory for @c .sql files.
	inline constexpr auto sqlDir{"sql"sv};

	//! File extension for @c .sql files.
	inline constexpr auto sqlExtension{".sql"sv};

	//! Time-out on table lock in seconds.
	inline constexpr auto lockTimeOutSec{300};

	//! Idle time in milliseconds after which a re-connect to the database will be enforced.
	inline constexpr auto reconnectAfterIdleMs{600000};

	//! Sleep time in milliseconds before re-attempting to add a database lock.
	inline constexpr auto sleepOnLockMs{250};

	//! Recommended major MySQL Connector/C++ version.
	inline constexpr auto recommendedMySqlDriverMajorVer{8};

	//! Maximum size of database content in bytes (= 1 GiB).
	inline constexpr auto maxContentSize{1073741824};

	//! Maximum size of database content as string.
	inline constexpr auto maxContentSizeString{"1 GiB"sv};

	//! "www." prefix to be ignored when checking for a domain.
	inline constexpr auto wwwPrefix{"www."sv};

	//! The minimum number of tables per URL list.
	inline constexpr auto numUrlListTables{6};

	//! The MySQL keyword for a constraint, including the trailing space.
	inline constexpr auto sqlConstraint{"CONSTRAINT "sv};

	//! The factor for converting seconds to milliseconds and vice versa.
	inline constexpr auto secToMs{1000};

	//! Time (in ms) to sleep on SQL deadlock.
	inline constexpr auto sleepOnDeadLockMs{250};

	//! Maximum number of columns in all associated tables associated with an URL list.
	inline constexpr auto maxColumnsUrlList{6};

	//! Number fo arguments needed for adding one URL.
	inline constexpr auto numArgsAddUrl{4};

	///@}
	///@name Constants for MySQL Queries
	///@{

	//! Ten at once.
	inline constexpr auto nAtOnce10{10};

	//! One hundred at once.
	inline constexpr auto nAtOnce100{100};

	//! Five hundred at once.
	inline constexpr auto nAtOnce500{500};

	//! First argument.
	inline constexpr auto sqlArg1{1};

	//! Second argument.
	inline constexpr auto sqlArg2{2};

	//! Third argument.
	inline constexpr auto sqlArg3{3};

	//! Fourth argument.
	inline constexpr auto sqlArg4{4};

	//! Fifth argument.
	inline constexpr auto sqlArg5{5};

	//! Sixth argument.
	inline constexpr auto sqlArg6{6};

	//! Seventh argument.
	inline constexpr auto sqlArg7{7};

	//! Eighth argument.
	inline constexpr auto sqlArg8{8};

	//! Ninth argument.
	inline constexpr auto sqlArg9{9};

	///@}
	///@name Constants for MySQL Connection Errors
	///@{

	//! Sort aborted.
	inline constexpr auto sqlSortAborted{1027};

	//! Too many connections.
	inline constexpr auto sqlTooManyConnections{1040};

	//! Cannot get host name.
	inline constexpr auto sqlCannotGetHostName{1042};

	//! Bad handshake.
	inline constexpr auto sqlBadHandShake{1043};

	//! Server shutdown.
	inline constexpr auto sqlServerShutDown{1053};

	//! Normal shutdown.
	inline constexpr auto sqlNormalShutdown{1077};

	//! Got signal.
	inline constexpr auto sqlGotSignal{1078};

	//! Shutdown complete.
	inline constexpr auto sqlShutDownComplete{1079};

	//! Forcing close of thread.
	inline constexpr auto sqlForcingCloseOfThread{1080};

	//! Cannot create IP socket.
	inline constexpr auto sqlCannotCreateIPSocket{1081};

	//! Aborted connection.
	inline constexpr auto sqlAbortedConnection{1152};

	//! Read error from connection pipe.
	inline constexpr auto sqlReadErrorFromConnectionPipe{1154};

	//! Packets out of order.
	inline constexpr auto sqlPacketsOutOfOrder{1156};

	//! Could not uncompress packets.
	inline constexpr auto sqlCouldNotUncompressPackets{1157};

	//! Error reading packets
	inline constexpr auto sqlErrorReadingPackets{1158};

	//! Timeout reading packets.
	inline constexpr auto sqlTimeOutReadingPackets{1159};

	//! Error writing packets.
	inline constexpr auto sqlErrorWritingPackets{1160};

	//! Timeout writing packets.
	inline constexpr auto sqlTimeOutWritingPackets{1161};

	//! New aborted connection-
	inline constexpr auto sqlNewAbortedConnection{1184};

	//! Network error reading from master.
	inline constexpr auto sqlNetErrorReadingFromMaster{1189};

	//! Network error writing to master.
	inline constexpr auto sqlNetErrorWritingToMaster{1190};

	//! More than the maximum number of user connections.
	inline constexpr auto sqlMoreThanMaxUserConnections{1203};

	//! Lock wait timeout exceeded.
	inline constexpr auto sqlLockWaitTimeOutExceeded{1205};

	//! Number of locks exceeds lock table size.
	inline constexpr auto sqlNumOfLocksExceedsLockTableSize{1206};

	//! Deadlock.
	inline constexpr auto sqlDeadLock{1213};

	//! Server error connecting to master.
	inline constexpr auto sqlServerErrorConnectingToMaster{1218};

	//! Query execution interrupted.
	inline constexpr auto sqlQueryExecutionInterrupted{1317};

	//! Unable to connect to foreign data source.
	inline constexpr auto sqlUnableToConnectToForeignDataSource{1429};

	//! Cannot connect to server through socket.
	inline constexpr auto sqlCannotConnectToServerThroughSocket{2002};

	//! Cannot connect to server.
	inline constexpr auto sqlCannotConnectToServer{2003};

	//! Unknown server host.
	inline constexpr auto sqlUnknownServerHost{2005};

	//! Server has gone away.
	inline constexpr auto sqlServerHasGoneAway{2006};

	//! TCP error.
	inline constexpr auto sqlTCPError{2011};

	//! Error in server handshake.
	inline constexpr auto sqlErrorInServerHandshake{2012};

	//! Lost connection during query.
	inline constexpr auto sqlLostConnectionDuringQuery{2013};

	//! Client error connecting to slave.
	inline constexpr auto sqlClientErrorConnectingToSlave{2024};

	//! Client error connecting to master.
	inline constexpr auto sqlClientErrorConnectingToMaster{2025};

	//! SSL connection error.
	inline constexpr auto sqlSSLConnectionError{2026};

	//! Malformed packet.
	inline constexpr auto sqlMalformedPacket{2027};

	//! Invalid connection handle.
	inline constexpr auto sqlInvalidConnectionHandle{2048};

	///@}
	///@name Constants for Other MySQL Errors
	///@{

	//! Storage engine error.
	inline constexpr auto sqlStorageEngineError{1030};

	//! Insufficient privileges.
	inline constexpr auto sqlInsufficientPrivileges{1045};

	//! Wrong arguments.
	inline constexpr auto sqlWrongArguments{1210};

	//! Incorrect path.
	inline constexpr auto sqlIncorrectPath{1525};

	///@}

	/*
	 * DECLARATION
	 */

	//! Class handling database access for the command-and-control and its threads.
	/*!
	 * Thread-specific functionality is not implemented
	 *  in this (parent) class.
	 *
	 * \warning This class is not thread-safe!
	 *   Use only one instance per thread.
	 * \warning Use instances of the child class Module::Database
	 *  for module-specific functionality instead.
	 *
	 * \sa Module::Database, Wrapper::Database
	 */
	class Database {
		//! Allows access to module threads.
		friend class Wrapper::Database;

		//! Allows access for scoped locking.
		template<class DB> friend class Wrapper::DatabaseLock;

		//! Allows access for scoped optional locking.
		template<class DB> friend class Wrapper::DatabaseTryLock;

		// for convenience
		using JsonException = Helper::Json::Exception;

		using ConfigProperties = Struct::ConfigProperties;
		using DatabaseSettings = Struct::DatabaseSettings;
		using QueryProperties = Struct::QueryProperties;
		using TableColumn = Struct::TableColumn;
		using TableProperties = Struct::TableProperties;
		using TargetTableProperties = Struct::TargetTableProperties;
		using ThreadDatabaseEntry = Struct::ThreadDatabaseEntry;
		using ThreadOptions = Struct::ThreadOptions;
		using ThreadStatus = Struct::ThreadStatus;
		using UrlListProperties = Struct::UrlListProperties;
		using WebsiteProperties = Struct::WebsiteProperties;

		using IdPairs = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
		using IdString = std::pair<std::uint64_t, std::string>;
		using IsRunningCallback = std::function<bool()>;
		using SqlPreparedStatementPtr = std::unique_ptr<sql::PreparedStatement>;
		using SqlResultSetPtr = std::unique_ptr<sql::ResultSet>;
		using SqlStatementPtr = std::unique_ptr<sql::Statement>;
		using StringString = std::pair<std::string, std::string>;
		using StringQueueOfStrings = std::pair<std::string, std::queue<std::string>>;
		using TableNameWriteAccess = std::pair<std::string, bool>;
		using Queries = std::vector<std::pair<std::string, std::vector<StringString>>>;

	public:
		///@name Construction and Destruction
		///@{

		Database(const DatabaseSettings& dbSettings, const std::string& dbModule);
		virtual ~Database();

		///@}
		///@name Setters
		///@{

		void setSleepOnError(std::uint64_t seconds);
		void setTimeOut(std::uint64_t milliseconds);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] const DatabaseSettings& getSettings() const;
		[[nodiscard]] const std::string& getDriverVersion() const;
		[[nodiscard]] const std::string& getDataDir() const;
		[[nodiscard]] std::uint64_t getMaxAllowedPacketSize() const;
		[[nodiscard]] std::uint64_t getConnectionId() const;

		///@}
		///@name Initialization and Update
		///@{

		void connect();
		void initializeSql();
		void prepare();
		void update();

		///@}
		///@name Logging
		///@{

		void log(const std::string& logEntry);
		void log(const std::string& logModule, const std::string& logEntry);
		[[nodiscard]] std::uint64_t getNumberOfLogEntries(const std::string& logModule);
		void clearLogs(const std::string& logModule);

		///@}
		///@name Threads
		///@{

		[[nodiscard]] std::vector<ThreadDatabaseEntry> getThreads();
		std::uint64_t addThread(const ThreadOptions& threadOptions);
		[[nodiscard]] std::uint64_t getThreadRunTime(std::uint64_t threadId);
		[[nodiscard]] std::uint64_t getThreadPauseTime(std::uint64_t threadId);
		void setThreadStatus(
				std::uint64_t threadId,
				bool threadPaused,
				const std::string& threadStatusMessage
		);
		void setThreadStatus(std::uint64_t threadId, const std::string& threadStatusMessage);
		void setThreadRunTime(std::uint64_t threadId, std::uint64_t threadRunTime);
		void setThreadPauseTime(std::uint64_t threadId, std::uint64_t threadPauseTime);
		void deleteThread(std::uint64_t threadId);

		///@}
		///@name Websites
		///@{

		std::uint64_t addWebsite(const WebsiteProperties& websiteProperties);
		[[nodiscard]] std::string getWebsiteDomain(std::uint64_t id);
		[[nodiscard]] std::string getWebsiteNamespace(std::uint64_t websiteId);
		[[nodiscard]] std::uint64_t getWebsiteFromUrlList(std::uint64_t listId);
		[[nodiscard]] IdString getWebsiteNamespaceFromUrlList(std::uint64_t listId);
		[[nodiscard]] IdString getWebsiteNamespaceFromConfig(std::uint64_t configId);
		[[nodiscard]] IdString getWebsiteNamespaceFromTargetTable(
				const std::string& type,
				std::uint64_t tableId
		);
		[[nodiscard]] bool isWebsiteNamespace(const std::string& nameSpace);
		[[nodiscard]] std::string duplicateWebsiteNamespace(const std::string& websiteNamespace);
		[[nodiscard]] std::string getWebsiteDataDirectory(std::uint64_t websiteId);
		[[nodiscard]] std::uint64_t getChangedUrlsByWebsiteUpdate(
				std::uint64_t websiteId,
				const WebsiteProperties& websiteProperties
		);
		[[nodiscard]] std::uint64_t getLostUrlsByWebsiteUpdate(
				std::uint64_t websiteId,
				const WebsiteProperties& websiteProperties
		);
		void updateWebsite(std::uint64_t websiteId, const WebsiteProperties& websiteProperties);
		void deleteWebsite(std::uint64_t websiteId);
		std::uint64_t duplicateWebsite(std::uint64_t websiteId, const Queries& queries);
		void moveWebsite(std::uint64_t websiteId, const WebsiteProperties& websiteProperties);

		///@}
		///@name URL Lists
		///@{

		std::uint64_t addUrlList(std::uint64_t websiteId, const UrlListProperties& listProperties);
		[[nodiscard]] std::queue<IdString> getUrlLists(std::uint64_t websiteId);
		std::size_t mergeUrls(std::uint64_t listId, std::queue<std::string>& urls);
		[[nodiscard]] std::queue<std::string> getUrls(std::uint64_t listId);
		[[nodiscard]] std::queue<IdString> getUrlsWithIds(std::uint64_t listId);
		[[nodiscard]] std::string getUrlListNamespace(std::uint64_t listId);
		[[nodiscard]] IdString getUrlListNamespaceFromTargetTable(
				const std::string& type,
				std::uint64_t tableId
		);
		[[nodiscard]] bool isUrlListNamespace(std::uint64_t websiteId, const std::string& nameSpace);
		void updateUrlList(std::uint64_t listId, const UrlListProperties& listProperties);
		void deleteUrlList(std::uint64_t listId);
		std::size_t deleteUrls(std::uint64_t listId, std::queue<uint64_t>& urlIds);
		void resetParsingStatus(std::uint64_t listId);
		void resetExtractingStatus(std::uint64_t listId);
		void resetAnalyzingStatus(std::uint64_t listId);

		///@}
		///@name Queries
		///@{

		std::uint64_t addQuery(std::uint64_t websiteId, const QueryProperties& queryProperties);
		void getQueryProperties(std::uint64_t queryId, QueryProperties& queryPropertiesTo);
		void updateQuery(std::uint64_t queryId, const QueryProperties& queryProperties);
		void moveQuery(std::uint64_t queryId, std::uint64_t toWebsiteId);
		void deleteQuery(std::uint64_t queryId);
		std::uint64_t duplicateQuery(std::uint64_t queryId);

		///@}
		///@name Configurations
		///@{

		std::uint64_t addConfiguration(
				std::uint64_t websiteId,
				const ConfigProperties& configProperties
		);
		[[nodiscard]] std::string getConfiguration(std::uint64_t configId);
		void updateConfiguration(std::uint64_t configId, const ConfigProperties& configProperties);
		void deleteConfiguration(std::uint64_t configId);
		std::uint64_t duplicateConfiguration(std::uint64_t configId);

		///@}
		///@name Target Tables
		///@{

		std::uint64_t addOrUpdateTargetTable(const TargetTableProperties& properties);
		[[nodiscard]] std::queue<IdString> getTargetTables(
				const std::string& type,
				std::uint64_t listId
		);
		[[nodiscard]] std::uint64_t getTargetTableId(
				const std::string& type,
				std::uint64_t listId,
				const std::string& tableName
		);
		[[nodiscard]] std::string getTargetTableName(std::string_view type, std::uint64_t tableId);
		void deleteTargetTable(const std::string& type, std::uint64_t tableId);

		///@}
		///@name Validation
		///@{

		void checkConnection();
		[[nodiscard]] bool isWebsite(std::uint64_t websiteId);
		[[nodiscard]] bool isUrlList(std::uint64_t urlListId);
		[[nodiscard]] bool isUrlList(std::uint64_t websiteId, std::uint64_t urlListId);
		[[nodiscard]] bool isQuery(std::uint64_t queryId);
		[[nodiscard]] bool isQuery(std::uint64_t websiteId, std::uint64_t queryId);
		[[nodiscard]] bool isConfiguration(std::uint64_t configId);
		[[nodiscard]] bool isConfiguration(std::uint64_t websiteId, std::uint64_t configId);
		[[nodiscard]] bool isTargetTable(
				std::string_view type,
				std::uint64_t websiteId,
				std::uint64_t urlListId,
				std::uint64_t tableID
		);
		[[nodiscard]] bool checkDataDir(const std::string& dir);

		///@}
		///@name Locking
		///@{

		void beginNoLock();
		void endNoLock();

		///@}
		///@name Tables
		///@{

		[[nodiscard]] bool isTableEmpty(const std::string& tableName);
		[[nodiscard]] bool isTableExists(const std::string& tableName);
		[[nodiscard]] bool isColumnExists(
				const std::string& tableName,
				const std::string& columnName
		);
		[[nodiscard]] std::string getColumnType(
				const std::string& tableName,
				const std::string& columnName
		);
		void readTableAsStrings(
				const std::string& tableName,
				std::vector<std::vector<std::string>>& contentsTo,
				bool includeColumnNames
		);
		void lockTables(std::queue<TableNameWriteAccess>& tableLocks);
		void unlockTables();
		void startTransaction(const std::string& isolationLevel);
		void endTransaction(bool success);

		///@}
		///@name Custom Data
		///@{

		void getCustomData(Data::GetValue& data);
		void getCustomData(Data::GetFields& data);
		void getCustomData(Data::GetFieldsMixed& data);
		void getCustomData(Data::GetColumn& data);
		void getCustomData(Data::GetColumns& data);
		void getCustomData(Data::GetColumnsMixed& data);
		void insertCustomData(const Data::InsertValue& data);
		void insertCustomData(const Data::InsertFields& data);
		void insertCustomData(const Data::InsertFieldsMixed& data);
		void updateCustomData(const Data::UpdateValue& data);
		void updateCustomData(const Data::UpdateFields& data);
		void updateCustomData(const Data::UpdateFieldsMixed& data);

		///@}
		///@name Request Counter
		///@{

#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		static std::uint64_t getRequestCounter() {
			return Database::requestCounter.load();
		}
#else
		//! Gets the number of SQL requests performed since the start of the application.
		/*!
		 * \note By default, the request counter
		 *   should be deactivated and the
		 *   function always return zero.
		 *
		 * \returns The number of SQL requests
		 *   performed since the start of the
		 *   application or zero, if the
		 *   request counter had not been
		 *   activated on compilation.
		 */
		static std::uint64_t getRequestCounter() {
			return 0;
		}
#endif

		///@}

		//! %Wrapper class for in-scope transactions.
		class Transaction {
		public:
			///@name Construction and Destruction
			///@{

			//! Constructor starting the transaction using the specified isolation level.
			/*!
			 * \param db Reference to the database
			 *   connection to be used.
			 * \param isolationLevel Constant reference
			 *   to a string containing the isolation
			 *   level to be used.
			 */
			Transaction(
					Main::Database& db,
					const std::string& isolationLevel
			) : ref(db), active(false), successful(false) {
				this->ref.startTransaction(isolationLevel);

				this->active = true;
			}

			//! Constructor starting the transaction using the default isolation level.
			/*!
			 * \param db Reference to the database
			 *   connection to be used.
			 */
			explicit Transaction(
					Main::Database& db
			) : ref(db), active(false), successful(false) {
				this->ref.startTransaction("");

				this->active = true;
			}

			//! Destructor committing the transaction on success.
			virtual ~Transaction() {
				if(this->active) {
					try {
						this->ref.endTransaction(this->successful);
					}
					catch(...) {} // ignore exceptions

					this->active = false;
				}
			}

			///@}
			///@name Setter
			///@{

			//! Sets the state of the transaction to successful.
			/*!
			 * The transaction will be committed
			 *  on destruction.
			 */
			void success() {
				this->successful = true;
			}

			///@}
			/**@name Copy and Move
			 * The class is neither copyable, nor moveable.
			 */
			///@{

			//! Deleted copy constructor.
			Transaction(Transaction&) = delete;

			//! Deleted copy assignment operator.
			Transaction& operator=(Transaction&) = delete;

			//! Deleted move constructor.
			Transaction(Transaction&&) = delete;

			//! Deleted move assignment operator.
			Transaction& operator=(Transaction&&) = delete;

			///@}

		private:
			Main::Database& ref;	// reference to database
			bool active;			// transaction is active
			bool successful;		// transaction was successful
		};

		//! Class for generic database exceptions.
		MAIN_EXCEPTION_CLASS();

		//! Class for database connection exceptions.
		MAIN_EXCEPTION_SUBCLASS(ConnectionException);

		//! Class for incorrect path exceptions.
		MAIN_EXCEPTION_SUBCLASS(IncorrectPathException);

		//! Class for storage engine exceptions.
		MAIN_EXCEPTION_SUBCLASS(StorageEngineException);

		//! Class for insufficient privileges exceptions.
		MAIN_EXCEPTION_SUBCLASS(PrivilegesException);

		//! Class for wrong arguments exceptions.
		MAIN_EXCEPTION_SUBCLASS(WrongArgumentsException);

		/**@name Copy and Move
		 * The class is neither copyable, nor movable.
		 */
		///@{

		//! Deleted copy constructor.
		Database(Database&) = delete;

		//! Deleted copy assignment operator.
		Database& operator=(Database&) = delete;

		//! Deleted move constructor.
		Database(Database&&) = delete;

		//! Deleted move assignment operator.
		Database& operator=(Database&&) = delete;

		///@}

	protected:
		///@name Shared Connection Information
		///@{

		//! %Database connection.
		std::unique_ptr<sql::Connection> connection;

		//! Pointer to the MySQL database driver.
		static sql::Driver * driver;

		///@}
		///@name Helper Functions for Prepared SQL Statements
		///@{

		void reserveForPreparedStatements(std::size_t n);
		void addPreparedStatement(const std::string& sqlQuery, std::size_t& id);
		void clearPreparedStatement(std::size_t& id);
		[[nodiscard]] sql::PreparedStatement& getPreparedStatement(std::size_t id);

		///@}
		///@name Database Helper Functions
		///@{

		[[nodiscard]] std::uint64_t getLastInsertedId();
		void resetAutoIncrement(const std::string& tableName);
		static void addDatabaseLock(
				const std::string& name,
				const IsRunningCallback& isRunningCallback
		);
		static bool tryDatabaseLock(const std::string& name);
		static void removeDatabaseLock(const std::string& name);
		void checkDirectory(const std::string& dir);

		///@}
		///@name Table Helper Functions
		///@{

		void createTable(const TableProperties& properties);
		void clearTable(const std::string& tableName);
		void dropTable(const std::string& tableName);
		void addColumn(const std::string& tableName, const TableColumn& column);
		void compressTable(const std::string& tableName);
		std::queue<std::string> cloneTable(
				const std::string& tableName,
				const std::string& destDir
		);

		///@}
		///@name URL List Helper Functions
		///@{

		[[nodiscard]] bool isUrlListCaseSensitive(std::uint64_t listId);
		void setUrlListCaseSensitive(std::uint64_t listId, bool isCaseSensitive);

		///@}
		///@name Exception Helper Function
		///@{

		static void sqlException(const std::string& function, const sql::SQLException& e);

		///@}
		///@name Helper Functions for Executing SQL Queries
		///@{

		//! Template function for executing a SQL query.
		/*!
		 * \param statement Reference to the SQL statement.
		 *   Can be either a prepared statement or a @c
		 *   sql::Statement.
		 * \param args Optional arguments to the underlying
		 *   function executing the query. When a @c
		 *   sql::Statement is used, this should be a null-
		 *   terminated string containing the text of the
		 *   SQL query.
		 *
		 * \returns True, if the query returned a result set.
		 *   False, if the query returned nothing or an
		 *   update count.
		 */
		template<class T, class... Args>
		static bool sqlExecute(T& statement, Args... args) {
			bool result{false};

			while(true) { // retry on deadlock
				try {
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
					++Database::requestCounter;
#endif

					result = statement.execute(args...);

					break;
				}
				catch(const sql::SQLException &e) {
					if(e.getErrorCode() != sqlDeadLock) {
						throw; // no deadlock: re-throw exception
					}
				}

#ifdef MAIN_DATABASE_DEBUG_DEADLOCKS
				const auto counter{Database::getRequestCounter()};

				std::cout << "#";

				if(counter > 0) {
					std::cout << counter;
				}

				std::cout << std::flush;
#endif

				if(sleepOnDeadLockMs > 0) {
					std::this_thread::sleep_for(
							std::chrono::milliseconds(
									sleepOnDeadLockMs
							)
					);
				}
			}

			return result;
		}

		//! Template function for executing a SQL query and returning the resulting set.
		/*!
		 * \param statement Reference to the SQL statement.
		 *   Can be either a prepared statement or a @c
		 *   sql::Statement.
		 * \param args Optional arguments to the underlying
		 *   function executing the query. When a @c
		 *   sql::Statement is used, this should be a null-
		 *   terminated string containing the text of the
		 *   SQL query.
		 *
		 * \returns A pointer to the result set retrieved
		 *   by executing the SQL query.
		 */
		template<class T, class... Args>
		static sql::ResultSet * sqlExecuteQuery(T& statement, Args... args) {
			sql::ResultSet * resultPtr{nullptr};

			while(true) { // retry on deadlock
				try {
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
					++Database::requestCounter;
#endif

					resultPtr = statement.executeQuery(args...);

					break;
				}
				catch(const sql::SQLException &e) {
					if(e.getErrorCode() != sqlDeadLock) {
						throw; // no deadlock: re-throw exception
					}
				}

#ifdef MAIN_DATABASE_DEBUG_DEADLOCKS
				const auto counter{Database::getRequestCounter()};

				std::cout << "#";

				if(counter > 0) {
					std::cout << counter;
				}

				std::cout << std::flush;
#endif

				if(sleepOnDeadLockMs > 0) {
					std::this_thread::sleep_for(
							std::chrono::milliseconds(
									sleepOnDeadLockMs
							)
					);
				}
			}

			return resultPtr;
		}

		//! Template function for executing a SQL query and returning the number of affected rows.
		/*!
		 * \param statement Reference to the SQL statement.
		 *   Can be either a prepared statement or a
		 *   @c sql::Statement.
		 * \param args Optional arguments to the underlying
		 *   function executing the query. When a @c
		 *   sql::Statement is used, this should be a null-
		 *   terminated string containing the text of the
		 *   query.
		 *
		 * \returns The number of rows affected by the
		 *   SQL query.
		 */
		template<class T, class... Args>
		static int sqlExecuteUpdate(T& statement, Args... args) {
			int result{};

			while(true) { // retry on deadlock
				try {
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
					++Database::requestCounter;
#endif

					result = statement.executeUpdate(args...);

					break;
				}
				catch(const sql::SQLException &e) {
					if(e.getErrorCode() != sqlDeadLock) {
						throw; // no deadlock: re-throw exception
					}
				}

#ifdef MAIN_DATABASE_DEBUG_DEADLOCKS
				const auto counter{Database::getRequestCounter()};

				std::cout << "#";

				if(counter > 0) {
					std::cout << counter;
				}

				std::cout << std::flush;
#endif

				if(sleepOnDeadLockMs > 0) {
					std::this_thread::sleep_for(
							std::chrono::milliseconds(
									sleepOnDeadLockMs
							)
					);
				}
			}

			return result;
		}

		//! Template function for executing a SQL query by unique pointer.
		/*!
		 * \param statement Reference to the unique pointer.
		 *   to the SQL statement.
		 * \param args Optional arguments to the underlying
		 *   function executing the query. When a @c
		 *   sql::Statement is used, this should be a null-
		 *   terminated string containing the text of the
		 *   SQL query.
		 *
		 * \returns True, if the query returned a result set.
		 *   False, if the query returned nothing or an
		 *   update count.
		 */
		template<class T, class... Args>
		static bool sqlExecute(std::unique_ptr<T>& statement, Args... args) {
			return sqlExecute(*statement, args...);
		}

		//! Template function for executing a SQL query by unique pointer and returning the resulting set.
		/*!
		 * \param statement Reference to the unique pointer.
		 *   to the SQL statement.
		 * \param args Optional arguments to the underlying
		 *   function executing the query. When a @c
		 *   sql::Statement is used, this should be a null-
		 *   terminated string containing the text of the
		 *   SQL query.
		 *
		 * \returns A pointer to the result set retrieved
		 *   by executing the SQL query.
		 */
		template<class T, class... Args>
		static sql::ResultSet * sqlExecuteQuery(std::unique_ptr<T>& statement, Args... args) {
			return sqlExecuteQuery(*statement, args...);
		}

		//! Template function for executing a SQL query by unique pointer and returning the number of affected rows.
		/*!
		 * \param statement Reference to the unique pointer.
		 *   to the SQL statement.
		 * \param args Optional arguments to the underlying
		 *   function executing the query. When a @c
		 *   sql::Statement is used, this should be a null-
		 *   terminated string containing the text of the
		 *   SQL query.
		 *
		 * \returns The number of rows affected by the
		 *   SQL query.
		 */
		template<class T, class... Args>
		static int sqlExecuteUpdate(std::unique_ptr<T>& statement, Args... args) {
			return sqlExecuteUpdate(*statement, args...);
		}

		///@}

	private:
		// private connection information
		const DatabaseSettings settings;		// database settings
		std::uint64_t connectionId{};			// MySQL connection ID
		std::uint64_t maxAllowedPacketSize{};	// maximum packet size
		std::uint64_t sleepOnError{};			// number of seconds to sleep on database error
		std::string driverVersion;				// MySQL Connector/C++ version
		std::string dataDir;					// main data directory
		std::vector<std::string> dirs;			// all known data directories
		std::string module;						// module for which the database connection was established
		Timer::Simple reconnectTimer;			// timer for reconnecting to the database

		// optional private variables
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		static std::atomic<std::uint64_t> requestCounter; // MySQL request counter
#endif

		// locking state
		static std::mutex lockAccess;
		static std::vector<std::string> locks;

		// prepared SQL statements
		std::vector<Wrapper::PreparedSqlStatement> preparedStatements;

		// internal helper functions
		void run(const std::string& sqlFile);
		void execute(const std::string& sqlQuery);
		int executeUpdate(const std::string& sqlQuery);
		[[nodiscard]] std::string sqlEscapeString(const std::string& in);

		// IDs of prepared SQL statements
		struct _ps {
			std::size_t log{};
			std::size_t lastId{};
			std::size_t setThreadStatus{};
			std::size_t setThreadStatusMessage{};
		} ps;
	};

} /* namespace crawlservpp::Main */

#endif /* MAIN_DATABASE_HPP_ */
