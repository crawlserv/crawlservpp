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
 * Database.h
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

#include "Data.hpp"
#include "Exception.hpp"
#include "Version.hpp"

#include "../Helper/FileSystem.hpp"
#include "../Helper/Json.hpp"
#include "../Helper/Portability/locale.h"
#include "../Helper/Portability/mysqlcppconn.h"
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

#include <algorithm>	// std::find, std::find_if, std::remove, std::transform
#include <chrono>		// std::chrono
#include <cmath>		// std::round
#include <fstream>		// std::ifstream
#include <functional>	// std::function
#include <iostream>		// std::cout, std::endl, std::flush
#include <locale>		// std::locale
#include <memory>		// std::unique_ptr
#include <mutex>		// std::lock_guard, std::mutex
#include <queue>		// std::queue
#include <sstream>		// std::ostringstream
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
		typedef Helper::Json::Exception JsonException;
		typedef Struct::ConfigProperties ConfigProperties;
		typedef Struct::TableProperties TableProperties;
		typedef Struct::TargetTableProperties TargetTableProperties;
		typedef Struct::DatabaseSettings DatabaseSettings;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::TableColumn TableColumn;
		typedef Struct::ThreadDatabaseEntry ThreadDatabaseEntry;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::ThreadStatus ThreadStatus;
		typedef Struct::UrlListProperties UrlListProperties;
		typedef Struct::WebsiteProperties WebsiteProperties;

		typedef std::vector<std::pair<unsigned long, unsigned long>> IdPairs;
		typedef std::pair<unsigned long, std::string> IdString;
		typedef std::function<bool()> IsRunningCallback;
		typedef std::unique_ptr<sql::PreparedStatement> SqlPreparedStatementPtr;
		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;
		typedef std::unique_ptr<sql::Statement> SqlStatementPtr;
		typedef std::pair<std::string, std::string> StringString;
		typedef std::vector<std::pair<std::string, std::vector<StringString>>> Queries;

	public:
		// allow wrapper and locking class access to protected functions
		friend class Wrapper::Database;
		template<class DB> friend class Wrapper::DatabaseLock;

		// constructor and destructor
		Database(const DatabaseSettings& dbSettings, const std::string& dbModule);
		virtual ~Database();

		// setters
		void setSleepOnError(unsigned long seconds);
		void setTimeOut(unsigned long milliseconds);

		// getters
		const DatabaseSettings& getSettings() const;
		const std::string& getMysqlVersion() const;
		const std::string& getDataDir() const;
		unsigned long getMaxAllowedPacketSize() const;

		// initializing functions
		void connect();
		void initializeSql();
		void prepare();
		void update();

		// logging functions
		void log(const std::string& logEntry);
		void log(const std::string& logModule, const std::string& logEntry);
		unsigned long getNumberOfLogEntries(const std::string& logModule);
		void clearLogs(const std::string& logModule);

		// thread functions
		std::vector<ThreadDatabaseEntry> getThreads();
		unsigned long addThread(const ThreadOptions& threadOptions);
		unsigned long getThreadRunTime(unsigned long threadId);
		unsigned long getThreadPauseTime(unsigned long threadId);
		void setThreadStatus(
				unsigned long threadId,
				bool threadPaused,
				const std::string& threadStatusMessage
		);
		void setThreadStatus(unsigned long threadId, const std::string& threadStatusMessage);
		void setThreadRunTime(unsigned long threadId, unsigned long threadRunTime);
		void setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime);
		void deleteThread(unsigned long threadId);

		// website functions
		unsigned long addWebsite(const WebsiteProperties& websiteProperties);
		std::string getWebsiteDomain(unsigned long id);
		std::string getWebsiteNamespace(unsigned long websiteId);
		IdString getWebsiteNamespaceFromUrlList(unsigned long listId);
		IdString getWebsiteNamespaceFromConfig(unsigned long configId);
		IdString getWebsiteNamespaceFromTargetTable(const std::string& type, unsigned long tableId);
		bool isWebsiteNamespace(const std::string& nameSpace);
		std::string duplicateWebsiteNamespace(const std::string& websiteNamespace);
		std::string getWebsiteDataDirectory(unsigned long websiteId);
		unsigned long getChangedUrlsByWebsiteUpdate(unsigned long websiteId, const WebsiteProperties& websiteProperties);
		unsigned long getLostUrlsByWebsiteUpdate(unsigned long websiteId, const WebsiteProperties& websiteProperties);
		void updateWebsite(unsigned long websiteId, const WebsiteProperties& websiteProperties);
		void deleteWebsite(unsigned long websiteId);
		unsigned long duplicateWebsite(unsigned long websiteId, const Queries& queries);

		// URL list functions
		unsigned long addUrlList(unsigned long websiteId, const UrlListProperties& listProperties);
		std::queue<IdString> getUrlLists(unsigned long websiteId);
		unsigned long mergeUrls(unsigned long listId, std::queue<std::string>& urls);
		std::queue<std::string> getUrls(unsigned long listId);
		std::queue<IdString> getUrlsWithIds(unsigned long listId);
		std::string getUrlListNamespace(unsigned long listId);
		IdString getUrlListNamespaceFromTargetTable(const std::string& type, unsigned long listId);
		bool isUrlListNamespace(unsigned long websiteId, const std::string& nameSpace);
		void updateUrlList(unsigned long listId, const UrlListProperties& listProperties);
		void deleteUrlList(unsigned long listId);
		void resetParsingStatus(unsigned long listId);
		void resetExtractingStatus(unsigned long listId);
		void resetAnalyzingStatus(unsigned long listId);

		// query functions
		unsigned long addQuery(unsigned long websiteId, const QueryProperties& queryProperties);
		void getQueryProperties(unsigned long queryId, QueryProperties& queryPropertiesTo);
		void updateQuery(unsigned long queryId, const QueryProperties& queryProperties);
		void deleteQuery(unsigned long queryId);
		unsigned long duplicateQuery(unsigned long queryId);

		// configuration functions
		unsigned long addConfiguration(
				unsigned long websiteId,
				const ConfigProperties& configProperties
		);
		const std::string getConfiguration(unsigned long configId);
		void updateConfiguration(unsigned long configId, const ConfigProperties& configProperties);
		void deleteConfiguration(unsigned long configId);
		unsigned long duplicateConfiguration(unsigned long configId);

		// target table functions
		unsigned long addTargetTable(const TargetTableProperties& properties);
		std::queue<IdString> getTargetTables(const std::string& type, unsigned long listId);
		unsigned long getTargetTableId(
				const std::string& type,
				unsigned long websiteId,
				unsigned long listId,
				const std::string& tableName
		);
		std::string getTargetTableName(const std::string& type, unsigned long tableId);
		void deleteTargetTable(const std::string& type, unsigned long tableId);

		// validation functions
		void checkConnection();
		bool isWebsite(unsigned long websiteId);
		bool isUrlList(unsigned long urlListId);
		bool isUrlList(unsigned long websiteId, unsigned long urlListId);
		bool isQuery(unsigned long queryId);
		bool isQuery(unsigned long websiteId, unsigned long queryId);
		bool isConfiguration(unsigned long configId);
		bool isConfiguration(unsigned long websiteId, unsigned long configId);

		// database functions
		void beginNoLock();
		void endNoLock();

		// general table functions
		bool isTableEmpty(const std::string& tableName);
		bool isTableExists(const std::string& tableName);
		bool isColumnExists(const std::string& tableName, const std::string& columnName);
		std::string getColumnType(const std::string& tableName, const std::string& columnName);

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
		static unsigned long long getRequestCounter() { return Database::requestCounter; }
#else
		static unsigned long long getRequestCounter() { return 0; }
#endif

		// classes for general and specific database exceptions
		MAIN_EXCEPTION_CLASS();
		MAIN_EXCEPTION_SUBCLASS(ConnectionException);
		MAIN_EXCEPTION_SUBCLASS(IncorrectPathException);
		MAIN_EXCEPTION_SUBCLASS(StorageEngineException);
		MAIN_EXCEPTION_SUBCLASS(PrivilegesException);

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
		void reserveForPreparedStatements(unsigned short numberOfAdditionalPreparedStatements);
		unsigned short addPreparedStatement(const std::string& sqlQuery);
		sql::PreparedStatement& getPreparedStatement(unsigned short id);

		// database helper functions
		unsigned long getLastInsertedId();
		void resetAutoIncrement(const std::string& tableName);
		void addDatabaseLock(const std::string& name, IsRunningCallback isRunningCallback);
		void removeDatabaseLock(const std::string& name);
		void createTable(const TableProperties& properties);
		void dropTable(const std::string& name);
		void addColumn(const std::string& tableName, const TableColumn& column);
		void compressTable(const std::string& tableName);
		void deleteTable(const std::string& tableName);
		void checkDirectory(const std::string& dir);

		// URL list helper functions
		bool isUrlListCaseSensitive(unsigned long listId);
		void setUrlListCaseSensitive(unsigned long listId, bool isCaseSensitive);

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

	private:
		// private connection information
		const DatabaseSettings settings;
		unsigned long maxAllowedPacketSize;
		unsigned long sleepOnError;
		std::string mysqlVersion;
		std::string dataDir;
		std::string module;

		// optional private variables
#ifdef MAIN_DATABASE_RECONNECT_AFTER_IDLE_SEC
		Timer::Simple reconnectTimer;
#endif
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		static std::atomic<unsigned long long> requestCounter;
#endif

		// locking state
		static std::mutex lockAccess;
		static std::vector<std::string> locks;

		// prepared SQL statements
		std::vector<Wrapper::PreparedSqlStatement> preparedStatements;

		// internal helper functions
		void run(const std::string& sqlFile);
		void execute(const std::string& sqlQuery);
		std::string sqlEscapeString(const std::string& in);

		// IDs of prepared SQL statements
		struct _ps {
			unsigned short log;
			unsigned short lastId;
			unsigned short setThreadStatus;
			unsigned short setThreadStatusMessage;
		} ps;
	};

} /* crawlservpp::Main */

#endif /* MAIN_DATABASE_HPP_ */
