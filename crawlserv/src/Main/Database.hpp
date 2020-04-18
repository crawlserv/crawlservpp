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
 * A class to handle database access for crawlserv and its threads.
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

// hard-coded options
#define MAIN_DATABASE_SQL_DIRECTORY "sql"			// (sub-)directory for SQL files

#define MAIN_DATABASE_LOCK_TIMEOUT_SEC 300			// time-out on table lock
#define MAIN_DATABASE_RECONNECT_AFTER_IDLE_SEC 600	// force re-connect if the connection has been idle for that long
#define MAIN_DATABASE_SLEEP_ON_DEADLOCK_MS 250		// sleep before re-trying after MySQL deadlock
#define MAIN_DATABASE_SLEEP_ON_LOCK_MS 250			// sleep before re-attempting to add database lock

// optional debugging option
//#define MAIN_DATABASE_DEBUG_REQUEST_COUNTER		// enable database request counter for debugging purposes
//#define MAIN_DATABASE_DEBUG_DEADLOCKS				// enable documentation of deadlocks by writing hashes ('#') to stdout
#define MAIN_DATABASE_LOG_MOVING					// log the moving of websites from one data directory to another to stdout

#include "Exception.hpp"
#include "Version.hpp"

#include "../Data/Data.hpp"
#include "../Helper/FileSystem.hpp"
#include "../Helper/Json.hpp"
#include "../Helper/Portability/locale.h"
#include "../Helper/Portability/mysqlcppconn.h"
#include "../Helper/Strings.hpp"
#include "../Helper/Utf8.hpp"
#include "../Helper/Versions.hpp"
#include "../Struct/ConfigProperties.hpp"
#include "../Struct/TableProperties.hpp"
#include "../Struct/TargetTableProperties.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/TableColumn.hpp"
#include "../Struct/ThreadDatabaseEntry.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/ThreadStatus.hpp"
#include "../Struct/UrlListProperties.hpp"
#include "../Struct/WebsiteProperties.hpp"
#include "../Timer/Simple.hpp"
#include "../Wrapper/DatabaseLock.hpp"
#include "../Wrapper/PreparedSqlStatement.hpp"

#include "../_extern/rapidjson/include/rapidjson/document.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <algorithm>	// std::find, std::find_if, std::remove, std::sort, std::transform, std::unique
#include <cctype>		// ::tolower
#include <chrono>		// std::chrono
#include <cmath>		// std::round
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint64_t
#include <fstream>		// std::ifstream
#include <functional>	// std::function
#include <iostream>		// std::cout, std::endl, std::flush
#include <locale>		// std::locale
#include <memory>		// std::unique_ptr
#include <mutex>		// std::lock_guard, std::mutex
#include <queue>		// std::queue
#include <sstream>		// std::istringstream, std::ostringstream
#include <stdexcept>	// std::logic_error
#include <string>		// std::getline, std::stoul, std::string, std::to_string
#include <thread>		// std::this_thread
#include <tuple>		// std::get(std::tuple)
#include <utility>		// std::pair
#include <vector>		// std::vector

// optional header
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
#include <atomic>		// std::atomic
#endif

namespace crawlservpp::Wrapper {

	class Database;

}

namespace crawlservpp::Main {

	class Database {
		// for convenience
		using JsonException = Helper::Json::Exception;

		using ConfigProperties = Struct::ConfigProperties;
		using DatabaseSettings = Struct::DatabaseSettings;
		using QueryProperties = Struct::QueryProperties;
		using TableColumn = Struct::TableColumn;
		using TableProperties = Struct::TableProperties;
		using TargetTableProperties = Struct::TargetTableProperties;
		using ThreadDatabaseEntry = Struct::ThreadDatabaseEntry ;
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
		// allow wrapper and locking classes access to protected functions
		friend class Wrapper::Database;
		template<class DB> friend class Wrapper::DatabaseLock;

		// constructor and destructor
		Database(const DatabaseSettings& dbSettings, const std::string& dbModule);
		virtual ~Database();

		// setters
		void setSleepOnError(std::uint64_t seconds);
		void setTimeOut(std::uint64_t milliseconds);

		// getters
		const DatabaseSettings& getSettings() const;
		const std::string& getMysqlVersion() const;
		const std::string& getDataDir() const;
		std::uint64_t getMaxAllowedPacketSize() const;
		std::uint64_t getConnectionId() const;

		// initializing functions
		void connect();
		void initializeSql();
		void prepare();
		void update();

		// logging functions
		void log(const std::string& logEntry);
		void log(const std::string& logModule, const std::string& logEntry);
		std::uint64_t getNumberOfLogEntries(const std::string& logModule);
		void clearLogs(const std::string& logModule);

		// thread functions
		std::vector<ThreadDatabaseEntry> getThreads();
		std::uint64_t addThread(const ThreadOptions& threadOptions);
		std::uint64_t getThreadRunTime(std::uint64_t threadId);
		std::uint64_t getThreadPauseTime(std::uint64_t threadId);
		void setThreadStatus(
				std::uint64_t threadId,
				bool threadPaused,
				const std::string& threadStatusMessage
		);
		void setThreadStatus(std::uint64_t threadId, const std::string& threadStatusMessage);
		void setThreadRunTime(std::uint64_t threadId, std::uint64_t threadRunTime);
		void setThreadPauseTime(std::uint64_t threadId, std::uint64_t threadPauseTime);
		void deleteThread(std::uint64_t threadId);

		// website functions
		std::uint64_t addWebsite(const WebsiteProperties& websiteProperties);
		std::string getWebsiteDomain(std::uint64_t id);
		std::string getWebsiteNamespace(std::uint64_t websiteId);
		IdString getWebsiteNamespaceFromUrlList(std::uint64_t listId);
		IdString getWebsiteNamespaceFromConfig(std::uint64_t configId);
		IdString getWebsiteNamespaceFromTargetTable(const std::string& type, std::uint64_t tableId);
		bool isWebsiteNamespace(const std::string& nameSpace);
		std::string duplicateWebsiteNamespace(const std::string& websiteNamespace);
		std::string getWebsiteDataDirectory(std::uint64_t websiteId);
		std::uint64_t getChangedUrlsByWebsiteUpdate(std::uint64_t websiteId, const WebsiteProperties& websiteProperties);
		std::uint64_t getLostUrlsByWebsiteUpdate(std::uint64_t websiteId, const WebsiteProperties& websiteProperties);
		void updateWebsite(std::uint64_t websiteId, const WebsiteProperties& websiteProperties);
		void deleteWebsite(std::uint64_t websiteId);
		std::uint64_t duplicateWebsite(std::uint64_t websiteId, const Queries& queries);
		void moveWebsite(std::uint64_t websiteId, const WebsiteProperties& websiteProperties);

		// URL list functions
		std::uint64_t addUrlList(std::uint64_t websiteId, const UrlListProperties& listProperties);
		std::queue<IdString> getUrlLists(std::uint64_t websiteId);
		std::size_t mergeUrls(std::uint64_t listId, std::queue<std::string>& urls);
		std::queue<std::string> getUrls(std::uint64_t listId);
		std::queue<IdString> getUrlsWithIds(std::uint64_t listId);
		std::string getUrlListNamespace(std::uint64_t listId);
		IdString getUrlListNamespaceFromTargetTable(const std::string& type, std::uint64_t listId);
		bool isUrlListNamespace(std::uint64_t websiteId, const std::string& nameSpace);
		void updateUrlList(std::uint64_t listId, const UrlListProperties& listProperties);
		void deleteUrlList(std::uint64_t listId);
		std::size_t deleteUrls(std::uint64_t listId, std::queue<uint64_t>& urlIds);
		void resetParsingStatus(std::uint64_t listId);
		void resetExtractingStatus(std::uint64_t listId);
		void resetAnalyzingStatus(std::uint64_t listId);

		// query functions
		std::uint64_t addQuery(std::uint64_t websiteId, const QueryProperties& queryProperties);
		void getQueryProperties(std::uint64_t queryId, QueryProperties& queryPropertiesTo);
		void updateQuery(std::uint64_t queryId, const QueryProperties& queryProperties);
		void moveQuery(std::uint64_t queryId, std::uint64_t toWebsiteId);
		void deleteQuery(std::uint64_t queryId);
		std::uint64_t duplicateQuery(std::uint64_t queryId);

		// configuration functions
		std::uint64_t addConfiguration(
				std::uint64_t websiteId,
				const ConfigProperties& configProperties
		);
		const std::string getConfiguration(std::uint64_t configId);
		void updateConfiguration(std::uint64_t configId, const ConfigProperties& configProperties);
		void deleteConfiguration(std::uint64_t configId);
		std::uint64_t duplicateConfiguration(std::uint64_t configId);

		// target table functions
		std::uint64_t addTargetTable(const TargetTableProperties& properties);
		std::queue<IdString> getTargetTables(const std::string& type, std::uint64_t listId);
		std::uint64_t getTargetTableId(
				const std::string& type,
				std::uint64_t websiteId,
				std::uint64_t listId,
				const std::string& tableName
		);
		std::string getTargetTableName(const std::string& type, std::uint64_t tableId);
		void deleteTargetTable(const std::string& type, std::uint64_t tableId);

		// validation functions
		void checkConnection();
		bool isWebsite(std::uint64_t websiteId);
		bool isUrlList(std::uint64_t urlListId);
		bool isUrlList(std::uint64_t websiteId, std::uint64_t urlListId);
		bool isQuery(std::uint64_t queryId);
		bool isQuery(std::uint64_t websiteId, std::uint64_t queryId);
		bool isConfiguration(std::uint64_t configId);
		bool isConfiguration(std::uint64_t websiteId, std::uint64_t configId);

		// database functions
		void beginNoLock();
		void endNoLock();
		bool checkDataDir(const std::string& dir);

		// general table functions
		bool isTableEmpty(const std::string& tableName);
		bool isTableExists(const std::string& tableName);
		bool isColumnExists(const std::string& tableName, const std::string& columnName);
		std::string getColumnType(const std::string& tableName, const std::string& columnName);
		void lockTables(std::queue<TableNameWriteAccess>& locks);
		void unlockTables();
		void startTransaction(const std::string& isolationLevel);
		void endTransaction(bool success);

		// custom data functions for algorithms
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

#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		static unsigned long long getRequestCounter() { return Database::requestCounter.load(); }
#else
		static unsigned long long getRequestCounter() { return 0; }
#endif

		// classes for general and specific database exceptions
		MAIN_EXCEPTION_CLASS();
		MAIN_EXCEPTION_SUBCLASS(ConnectionException);
		MAIN_EXCEPTION_SUBCLASS(IncorrectPathException);
		MAIN_EXCEPTION_SUBCLASS(StorageEngineException);
		MAIN_EXCEPTION_SUBCLASS(PrivilegesException);
		MAIN_EXCEPTION_SUBCLASS(WrongArgumentsException);

		// not moveable, not copyable
		Database(Database&) = delete;
		Database(Database&&) = delete;
		Database& operator=(Database&) = delete;
		Database& operator=(Database&&) = delete;

	protected:
		// shared connection information
		std::unique_ptr<sql::Connection> connection;
		static sql::Driver * driver;

		// helper functions for prepared SQL statements
		void reserveForPreparedStatements(std::size_t numberOfAdditionalPreparedStatements);
		std::size_t addPreparedStatement(const std::string& sqlQuery);
		sql::PreparedStatement& getPreparedStatement(std::size_t id);

		// database helper functions
		std::uint64_t getLastInsertedId();
		void resetAutoIncrement(const std::string& tableName);
		void addDatabaseLock(const std::string& name, IsRunningCallback isRunningCallback);
		void removeDatabaseLock(const std::string& name);
		void createTable(const TableProperties& properties);
		void dropTable(const std::string& name);
		void addColumn(const std::string& tableName, const TableColumn& column);
		void compressTable(const std::string& tableName);
		void deleteTable(const std::string& tableName);
		void checkDirectory(const std::string& dir);
		std::queue<std::string> cloneTable(const std::string& tableName, const std::string& dataDir);

		// URL list helper functions
		bool isUrlListCaseSensitive(std::uint64_t listId);
		void setUrlListCaseSensitive(std::uint64_t listId, bool isCaseSensitive);

		// exception helper function
		void sqlException(const std::string& function, const sql::SQLException& e);

		// wrapper template for executing SQL query
		template<class T, class... Args>
		static bool sqlExecute(T& statement, Args... args) {
			bool result = false;

			while(true) { // retry on deadlock
				try {
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
					++Database::requestCounter;
#endif

					result = statement.execute(args...);

					break;
				}
				catch(const sql::SQLException &e) {
					if(e.getErrorCode() != 1213)
						throw; // no deadlock: re-throw exception
				}

#ifdef MAIN_DATABASE_DEBUG_DEADLOCKS
				unsigned long long counter = Database::getRequestCounter();

				std::cout << "#";

				if(counter)
					std::cout << counter;

				std::cout << std::flush;
#endif

#ifdef MAIN_DATABASE_SLEEP_ON_DEADLOCK_MS
				std::this_thread::sleep_for(
						std::chrono::milliseconds(
								MAIN_DATABASE_SLEEP_ON_DEADLOCK_MS
						)
				);
#endif
			}

			return result;
		}

		// wrapper template for executing SQL query and returning the result
		template<class T, class... Args>
		static sql::ResultSet * sqlExecuteQuery(T& statement, Args... args) {
			sql::ResultSet * resultPtr = nullptr;

			while(true) { // retry on deadlock
				try {
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
					++Database::requestCounter;
#endif

					resultPtr = statement.executeQuery(args...);

					break;
				}
				catch(const sql::SQLException &e) {
					if(e.getErrorCode() != 1213)
						throw; // no deadlock: re-throw exception
				}

#ifdef MAIN_DATABASE_DEBUG_DEADLOCKS
				unsigned long long counter = Database::getRequestCounter();

				std::cout << "#";

				if(counter)
					std::cout << counter;

				std::cout << std::flush;
#endif

#ifdef MAIN_DATABASE_SLEEP_ON_DEADLOCK_MS
				std::this_thread::sleep_for(
						std::chrono::milliseconds(
								MAIN_DATABASE_SLEEP_ON_DEADLOCK_MS
						)
				);
#endif
			}

			return resultPtr;
		}

		// wrapper template for executing SQL query and returning the number of affected rows
		template<class T, class... Args>
		static int sqlExecuteUpdate(T& statement, Args... args) {
			int result = 0;

			while(true) { // retry on deadlock
				try {
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
					++Database::requestCounter;
#endif

					result = statement.executeUpdate(args...);

					break;
				}
				catch(const sql::SQLException &e) {
					if(e.getErrorCode() != 1213)
						throw; // no deadlock: re-throw exception
				}

#ifdef MAIN_DATABASE_DEBUG_DEADLOCKS
				unsigned long long counter = Database::getRequestCounter();

				std::cout << "#";

				if(counter)
					std::cout << counter;

				std::cout << std::flush;
#endif

#ifdef MAIN_DATABASE_SLEEP_ON_DEADLOCK_MS
				std::this_thread::sleep_for(
						std::chrono::milliseconds(
								MAIN_DATABASE_SLEEP_ON_DEADLOCK_MS
						)
				);
#endif
			}

			return result;
		}

		// wrapper template for executing SQL query by unique pointer
		template<class T, class... Args>
		static bool sqlExecute(std::unique_ptr<T>& statement, Args... args) {
			return sqlExecute(*statement, args...);
		}

		// wrapper template for executing SQL query by unique pointer and returning the result
		template<class T, class... Args>
		static sql::ResultSet * sqlExecuteQuery(std::unique_ptr<T>& statement, Args... args) {
			return sqlExecuteQuery(*statement, args...);
		}

		// wrapper template for executing SQL query by unique pointer and returning the number of affected rows
		template<class T, class... Args>
		static int sqlExecuteUpdate(std::unique_ptr<T>& statement, Args... args) {
			return sqlExecuteUpdate(*statement, args...);
		}

		// wrapper class for in-scope table locks
		class TableLock {
		public:
			// constructor: lock the table(s)
			TableLock(Main::Database& db, std::queue<TableNameWriteAccess>& locks) : ref(db), active(false) {
				this->ref.lockTables(locks);

				this->active = true;
			}

			// destructor: unlock the table(s) if lock is active
			virtual ~TableLock() {
				if(this->active) {
					this->ref.unlockTables();

					this->active = false;
				}
			}

			// not moveable, not copyable
			TableLock(TableLock&) = delete;
			TableLock(TableLock&&) = delete;
			TableLock& operator=(TableLock&) = delete;
			TableLock& operator=(TableLock&&) = delete;

		private:
			Main::Database& ref;	// reference to database
			bool active;			// lock is active
		};

		// wrapper class for in-scope transactions
		class Transaction {
		public:
			// constructor #1: lock the table(s) using specified isolation level
			Transaction(
					Main::Database& db,
					const std::string& isolationLevel
			) : ref(db), active(false), successful(false) {
				this->ref.startTransaction(isolationLevel);

				this->active = true;
			}

			// constructor #2: lock the table(s) using the default isolation level
			Transaction(
					Main::Database& db
			) : ref(db), active(false), successful(false) {
				this->ref.startTransaction("");

				this->active = true;
			}

			// transaction was successful
			void success() {
				this->successful = true;
			}

			// destructor: unlock the table(s) if lock is active
			virtual ~Transaction() {
				if(this->active) {
					this->ref.endTransaction(this->successful);

					this->active = false;
				}
			}

			// not moveable, not copyable
			Transaction(Transaction&) = delete;
			Transaction(Transaction&&) = delete;
			Transaction& operator=(Transaction&) = delete;
			Transaction& operator=(Transaction&&) = delete;

		private:
			Main::Database& ref;	// reference to database
			bool active;			// transaction is active
			bool successful;		// transaction was successful
		};

	private:
		// private connection information
		const DatabaseSettings settings;	// database settings
		std::uint64_t connectionId;			// MySQL connection ID
		std::uint64_t maxAllowedPacketSize;	// maximum packet size
		std::uint64_t sleepOnError;			// number of seconds to sleep on database error
		std::string mysqlVersion;			// MySQL version
		std::string dataDir;				// main data directory
		std::vector<std::string> dirs;		// all known data directories
		std::string module;					// module for which the database connection was established

		// optional private variables
#ifdef MAIN_DATABASE_RECONNECT_AFTER_IDLE_SEC
		Timer::Simple reconnectTimer;		// timer for reconnecting to the database
#endif
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		static std::atomic<unsigned long long> requestCounter; // MySQL request counter
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
		std::string sqlEscapeString(const std::string& in);

		// IDs of prepared SQL statements
		struct _ps {
			std::size_t log;
			std::size_t lastId;
			std::size_t setThreadStatus;
			std::size_t setThreadStatusMessage;
		} ps;
	};

} /* crawlservpp::Main */

#endif /* MAIN_DATABASE_HPP_ */
