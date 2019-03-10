/*
 * Database.h
 *
 * A class to handle database access for crawlserv and its threads.
 * Thread-specific functionality is not implemented in this (parent) class.
 *
 * NOT THREAD-SAFE!
 * Use instances of the child class Module::Database for thread-specific functionality functionality instead.
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#ifndef MAIN_DATABASE_HPP_
#define MAIN_DATABASE_HPP_

// hard-coded options
#define MAIN_DATABASE_LOCK_TIMEOUT_SECONDS 300 // time-out on table lock
#define MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS 600 // force re-connect if the connection has been idle that long
#define MAIN_DATABASE_SLEEP_ON_LOCK_SECONDS 5 // sleep on target table lock

// optional debugging option
#define MAIN_DATABASE_DEBUG_REQUEST_COUNTER // enable database request counter for debugging purposes

#include "Data.hpp"
#include "Exception.hpp"

#include "../Helper/FileSystem.hpp"
#include "../Struct/ConfigProperties.hpp"
#include "../Struct/TargetTableProperties.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/TableColumn.hpp"
#include "../Struct/TableLockProperties.hpp"
#include "../Struct/ThreadDatabaseEntry.hpp"
#include "../Struct/ThreadOptions.hpp"
#include "../Struct/UrlListProperties.hpp"
#include "../Struct/WebsiteProperties.hpp"
#include "../Timer/Simple.hpp"
#include "../Wrapper/PreparedSqlStatement.hpp"
#include "../Wrapper/TableLock.hpp"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <mysql_connection.h>

#include <experimental/filesystem>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <memory>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

// optional header
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
#include <atomic>
#endif

namespace crawlservpp::Main {

	class Database {
		// for convenience
		typedef Struct::ConfigProperties ConfigProperties;
		typedef Struct::TargetTableProperties TargetTableProperties;
		typedef Struct::DatabaseSettings DatabaseSettings;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::TableColumn TableColumn;
		typedef Struct::TableLockProperties TableLockProperties;
		typedef Struct::ThreadDatabaseEntry ThreadDatabaseEntry;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::UrlListProperties UrlListProperties;
		typedef Struct::WebsiteProperties WebsiteProperties;
		typedef Wrapper::TableLock<Main::Database> TableLock;

		typedef std::function<bool()> CallbackIsRunning;
		typedef std::pair<unsigned long, std::string> IdString;
		typedef std::unique_ptr<sql::PreparedStatement> SqlPreparedStatementPtr;
		typedef std::unique_ptr<sql::ResultSet> SqlResultSetPtr;
		typedef std::unique_ptr<sql::Statement> SqlStatementPtr;

	public:
		// allow TableLock access to protected locking functions
		template<class DB> friend class Wrapper::TableLock;
		friend class TargetTablesLock;

		// constructor and destructor
		Database(const DatabaseSettings& dbSettings, const std::string& dbModule);
		virtual ~Database();

		// setter
		void setSleepOnError(unsigned long seconds);

		// getter
		const DatabaseSettings& getSettings() const;

		// initializing functions
		void connect();
		void initializeSql();
		void prepare();

		// logging functions
		void log(const std::string& logEntry);
		void log(const std::string& logModule, const std::string& logEntry);
		unsigned long getNumberOfLogEntries(const std::string& logModule);
		void clearLogs(const std::string& logModule);

		// thread functions
		std::vector<ThreadDatabaseEntry> getThreads();
		unsigned long addThread(const std::string& threadModule, const ThreadOptions& threadOptions);
		unsigned long getThreadRunTime(unsigned long threadId);
		unsigned long getThreadPauseTime(unsigned long threadId);
		void setThreadStatus(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage);
		void setThreadStatus(unsigned long threadId, const std::string& threadStatusMessage);
		void setThreadRunTime(unsigned long threadId, unsigned long threadRunTime);
		void setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime);
		void deleteThread(unsigned long threadId);

		// website functions
		unsigned long addWebsite(const WebsiteProperties& websiteProperties);
		std::string getWebsiteDomain(unsigned long id);
		std::string getWebsiteNamespace(unsigned long websiteId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromUrlList(unsigned long listId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromConfig(unsigned long configId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromTargetTable(const std::string& type, unsigned long tableId);
		bool isWebsiteNamespace(const std::string& nameSpace);
		std::string duplicateWebsiteNamespace(const std::string& websiteNamespace);
		void updateWebsite(unsigned long websiteId, const WebsiteProperties& websiteProperties);
		void deleteWebsite(unsigned long websiteId);
		unsigned long duplicateWebsite(unsigned long websiteId);

		// URL list functions
		unsigned long addUrlList(unsigned long websiteId, const UrlListProperties& listProperties);
		std::queue<IdString> getUrlLists(unsigned long websiteId);
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
		unsigned long addConfiguration(unsigned long websiteId, const ConfigProperties& configProperties);
		const std::string getConfiguration(unsigned long configId);
		void updateConfiguration(unsigned long configId, const ConfigProperties& configProperties);
		void deleteConfiguration(unsigned long configId);
		unsigned long duplicateConfiguration(unsigned long configId);

		// target table functions
		void lockTargetTables(const std::string& type, unsigned long websiteId, unsigned long listId,
				unsigned long timeOut, CallbackIsRunning running);
		unsigned long addTargetTable(const TargetTableProperties& properties);
		std::queue<IdString> getTargetTables(const std::string& type, unsigned long listId);
		unsigned long getTargetTableId(const std::string& type, unsigned long websiteId, unsigned long listId,
				const std::string& tableName);
		std::string getTargetTableName(const std::string& type, unsigned long tableId);
		void deleteTargetTable(const std::string& type, unsigned long tableId);
		void unlockTargetTables(const std::string& type);

		// validation functions
		void checkConnection();
		bool isWebsite(unsigned long websiteId);
		bool isUrlList(unsigned long urlListId);
		bool isUrlList(unsigned long websiteId, unsigned long urlListId);
		bool isQuery(unsigned long queryId);
		bool isQuery(unsigned long websiteId, unsigned long queryId);
		bool isConfiguration(unsigned long configId);
		bool isConfiguration(unsigned long websiteId, unsigned long configId);

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

		// static inline function

#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		static unsigned long long getRequestCounter() { return Database::requestCounter; }
#else
		static unsigned long long getRequestCounter() { return 0; }
#endif

		// sub-classes for database exceptions
		class Exception : public Main::Exception { // general Database exception
		public:
			Exception(const std::string& description) : Main::Exception(description) {}
		};

		class ConnectionException : public Exception { // connection exception (used to handle a lost database connection)
		public:
			ConnectionException(const std::string& description) : Exception(description) {}
		};

		// not moveable, not copyable
		Database(Database&) = delete;
		Database(Database&&) = delete;
		Database& operator=(Database&) = delete;
		Database& operator=(Database&&) = delete;

	protected:
		// shared connection information
		std::unique_ptr<sql::Connection> connection;
		static sql::Driver * driver;
		bool tablesLocked;

		// protected getter
		unsigned long getMaxAllowedPacketSize() const;

		// helper functions for prepared SQL statements
		void reserveForPreparedStatements(unsigned short numberOfAdditionalPreparedStatements);
		unsigned short addPreparedStatement(const std::string& sqlQuery);
		sql::PreparedStatement& getPreparedStatement(unsigned short id);

		// database helper functions
		unsigned long getLastInsertedId();
		void resetAutoIncrement(const std::string& tableName);
		void lockTable(const TableLockProperties& lockProperties);
		void lockTables(const TableLockProperties& lockProperties1, const TableLockProperties& lockProperties2);
		void unlockTables();
		bool isTableEmpty(const std::string& tableName);
		bool isTableExists(const std::string& tableName);
		bool isColumnExists(const std::string& tableName, const std::string& columnName);
		std::string getColumnType(const std::string& tableName, const std::string& columnName);
		void createTable(const std::string& tableName, const std::vector<TableColumn>& columns, bool compressed);
		void addColumn(const std::string& tableName, const TableColumn& column);
		void compressTable(const std::string& tableName);
		void deleteTable(const std::string& tableName);

		// exception helper function
		void sqlException(const std::string& function, const sql::SQLException& e);

		// static inline wrapper functions
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		static bool sqlExecute(sql::PreparedStatement& sqlPreparedStatement) {
			bool result = sqlPreparedStatement.execute();
			Database::requestCounter++;
			return result;
		}
		static bool sqlExecute(SqlPreparedStatementPtr& sqlPreparedStatement) {
			bool result = sqlPreparedStatement->execute();
			Database::requestCounter++;
			return result;
		}
		static bool sqlExecute(SqlStatementPtr& sqlStatement, const std::string& sqlQuery) {
			bool result = sqlStatement->execute(sqlQuery);
			Database::requestCounter++;
			return result;
		}
		static sql::ResultSet * sqlExecuteQuery(sql::PreparedStatement& sqlPreparedStatement) {
			sql::ResultSet * resultPtr = sqlPreparedStatement.executeQuery();
			Database::requestCounter++;
			return resultPtr;
		}
		static sql::ResultSet * sqlExecuteQuery(SqlPreparedStatementPtr& sqlPreparedStatement) {
			sql::ResultSet * resultPtr = sqlPreparedStatement->executeQuery();
			Database::requestCounter++;
			return resultPtr;
		}
		static sql::ResultSet * sqlExecuteQuery(SqlStatementPtr& sqlStatement, const std::string& sqlQuery) {
			sql::ResultSet * resultPtr = sqlStatement->executeQuery(sqlQuery);
			Database::requestCounter++;
			return resultPtr;
		}
		static int sqlExecuteUpdate(sql::PreparedStatement& sqlPreparedStatement) {
			int result = sqlPreparedStatement.executeUpdate();
			Database::requestCounter++;
			return result;
		}
		static int sqlExecuteUpdate(SqlPreparedStatementPtr& sqlPreparedStatement) {
			int result = sqlPreparedStatement->executeUpdate();
			Database::requestCounter++;
			return result;
		}
		static int sqlExecuteUpdate(SqlStatementPtr& sqlStatement, const std::string& sqlQuery) {
			int result = sqlStatement->executeUpdate(sqlQuery);
			Database::requestCounter++;
			return result;
		}

#else
		static bool sqlExecute(sql::PreparedStatement& sqlPreparedStatement) {
			return sqlPreparedStatement.execute();
		}
		static bool sqlExecute(SqlPreparedStatementPtr& sqlPreparedStatement) {
			return sqlPreparedStatement->execute();
		}
		static bool sqlExecute(SqlStatementPtr& sqlStatement, const std::string& sqlQuery) {
			return sqlStatement->execute(sqlQuery);
		}
		static sql::ResultSet * sqlExecuteQuery(sql::PreparedStatement& sqlPreparedStatement) {
			return sqlPreparedStatement.executeQuery();
		}
		static sql::ResultSet * sqlExecuteQuery(SqlPreparedStatementPtr& sqlPreparedStatement) {
			return sqlPreparedStatement->executeQuery();
		}
		static sql::ResultSet * sqlExecuteQuery(SqlStatementPtr& sqlStatement, const std::string& sqlQuery) {
			return sqlStatement->executeQuery(sqlQuery);
		}
		static int sqlExecuteUpdate(sql::PreparedStatement& sqlPreparedStatement) {
			return sqlPreparedStatement.executeUpdate();
		}
		static int sqlExecuteUpdate(SqlPreparedStatementPtr& sqlPreparedStatement) {
			return sqlPreparedStatement->executeUpdate();
		}
		static int sqlExecuteUpdate(SqlStatementPtr& sqlStatement, const std::string& sqlQuery) {
			return sqlStatement->executeUpdate(sqlQuery);
		}
#endif

	private:
		// private connection information
		const DatabaseSettings settings;
		unsigned long maxAllowedPacketSize;
		unsigned long sleepOnError;
		std::string module;
		std::vector<std::pair<std::string, unsigned long>> targetTableLocks;

		// optional private variables
#ifdef MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS
		Timer::Simple reconnectTimer;
#endif
#ifdef MAIN_DATABASE_DEBUG_REQUEST_COUNTER
		static std::atomic<unsigned long long> requestCounter;
#endif

		// prepared SQL statements
		std::vector<Wrapper::PreparedSqlStatement> preparedStatements;

		// internal helper functions
		void run(const std::string& sqlFile);
		void execute(const std::string& sqlQuery);

		// IDs of prepared SQL statements
		struct {
			unsigned short log;
			unsigned short lastId;
			unsigned short setThreadStatus;
			unsigned short setThreadStatusMessage;
		} ps;
	};

} /* crawlservpp::Main */

#endif /* MAIN_DATABASE_HPP_ */
