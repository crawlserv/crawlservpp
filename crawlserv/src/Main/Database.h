/*
 * Database.h
 *
 * A class to handle database access for crawlserv and its threads.
 * Thread-specific functionality is not implemented in this (parent) class.
 *
 * NOT THREAD-SAFE!
 * Use instances of the child class DatabaseThread for thread-specific functionality
 * and child classes of DatabaseModule for module-specific functionality instead.
 *
 *  Created on: Sep 29, 2018
 *      Author: ans
 */

#ifndef MAIN_DATABASE_H_
#define MAIN_DATABASE_H_

// hard-coded options
#define MAIN_DATABASE_LOCK_TIMEOUT_SECONDS 300 // time-out on table lock
#define MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS 600 // force re-connect if the connection has been idle that long
#define MAIN_DATABASE_SLEEP_ON_LOCK_SECONDS 5 // sleep on target table lock

#include "Data.h"
#include "Exception.h"

#include "../Helper/FileSystem.h"
#include "../Struct/ConfigProperties.h"
#include "../Struct/TargetTableProperties.h"
#include "../Struct/DatabaseSettings.h"
#include "../Struct/QueryProperties.h"
#include "../Struct/TableColumn.h"
#include "../Struct/ThreadDatabaseEntry.h"
#include "../Struct/ThreadOptions.h"
#include "../Struct/UrlListProperties.h"
#include "../Struct/WebsiteProperties.h"
#include "../Timer/Simple.h"
#include "../Wrapper/PreparedSqlStatement.h"

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

namespace crawlservpp::Main {
	class Database {
		// for convenience
		typedef Struct::ConfigProperties ConfigProperties;
		typedef Struct::TargetTableProperties TargetTableProperties;
		typedef Struct::DatabaseSettings DatabaseSettings;
		typedef Struct::QueryProperties QueryProperties;
		typedef Struct::TableColumn TableColumn;
		typedef Struct::ThreadDatabaseEntry ThreadDatabaseEntry;
		typedef Struct::ThreadOptions ThreadOptions;
		typedef Struct::UrlListProperties UrlListProperties;
		typedef Struct::WebsiteProperties WebsiteProperties;
		typedef std::pair<unsigned long, std::string> IdString;
		typedef std::function<bool()> CallbackIsRunning;

	public:
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
		std::pair<unsigned long, std::string> getUrlListNamespaceFromTargetTable(const std::string& type, unsigned long listId);
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
		void lockTable(const std::string& tableName);
		void lockTables(const std::string& tableName1, const std::string& tableName2);
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

		// sub-class for safe in-scope table locks
		class TableLock {
		public:
			// constructor A: lock one table
			TableLock(Database& db, const std::string& tableName) : ref(db) {
				this->ref.lockTable(tableName);
			}

			// constructor B: lock two tables (and the aliases 'a' and 'b' for reading access to those tables)
			TableLock(Database& db, const std::string& tableName1, const std::string& tableName2) : ref(db) {
				this->ref.lockTables(tableName1, tableName2);
			}

			// destructor: try to unlock the table(s)
			~TableLock() {
				try { this->ref.unlockTables(); }
				catch(...) {}
			}

			// not moveable, not copyable
			TableLock(TableLock&) = delete;
			TableLock(TableLock&&) = delete;
			TableLock& operator=(TableLock&) = delete;
			TableLock& operator=(TableLock&&) = delete;

		private:
			// internal reference to the database connection of the server
			Database& ref;
		};

	private:
		// private connection information
		const DatabaseSettings settings;
		unsigned long maxAllowedPacketSize;
		unsigned long sleepOnError;
		std::string module;
#ifdef MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS
		Timer::Simple reconnectTimer;
#endif
		std::vector<std::pair<std::string, unsigned long>> targetTableLocks;

		// prepared SQL statements
		std::vector<Wrapper::PreparedSqlStatement> preparedStatements;

		// internal helper function
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
}

#endif /* MAIN_DATABASE_H_ */
