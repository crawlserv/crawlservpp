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

#define MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS 300 // force re-connect if the connection has been idle that long

#include "Data.h"

#include "../Helper/FileSystem.h"
#include "../Struct/ConfigProperties.h"
#include "../Struct/DatabaseSettings.h"
#include "../Struct/QueryProperties.h"
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
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace crawlservpp::Main {
	class Database {
	public:
		Database(const crawlservpp::Struct::DatabaseSettings& dbSettings, const std::string& dbModule);
		virtual ~Database();

		// setter
		void setSleepOnError(unsigned long seconds);

		// getter
		const crawlservpp::Struct::DatabaseSettings& getSettings() const;

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
		std::vector<crawlservpp::Struct::ThreadDatabaseEntry> getThreads();
		unsigned long addThread(const std::string& threadModule, const crawlservpp::Struct::ThreadOptions& threadOptions);
		unsigned long getThreadRunTime(unsigned long threadId);
		unsigned long getThreadPauseTime(unsigned long threadId);
		void setThreadStatus(unsigned long threadId, bool threadPaused, const std::string& threadStatusMessage);
		void setThreadStatus(unsigned long threadId, const std::string& threadStatusMessage);
		void setThreadRunTime(unsigned long threadId, unsigned long threadRunTime);
		void setThreadPauseTime(unsigned long threadId, unsigned long threadPauseTime);
		void deleteThread(unsigned long threadId);

		// website functions
		unsigned long addWebsite(const crawlservpp::Struct::WebsiteProperties& websiteProperties);
		std::string getWebsiteDomain(unsigned long id);
		std::string getWebsiteNamespace(unsigned long websiteId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromUrlList(unsigned long listId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromConfig(unsigned long configId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromParsedTable(unsigned long tableId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromExtractedTable(unsigned long tableId);
		std::pair<unsigned long, std::string> getWebsiteNamespaceFromAnalyzedTable(unsigned long tableId);
		bool isWebsiteNamespace(const std::string& nameSpace);
		std::string duplicateWebsiteNamespace(const std::string& websiteNamespace);
		void updateWebsite(unsigned long websiteId, const crawlservpp::Struct::WebsiteProperties& websiteProperties);
		void deleteWebsite(unsigned long websiteId);
		unsigned long duplicateWebsite(unsigned long websiteId);

		// URL list functions
		unsigned long addUrlList(unsigned long websiteId, const crawlservpp::Struct::UrlListProperties& listProperties);
		std::vector<std::pair<unsigned long, std::string>> getUrlLists(unsigned long websiteId);
		std::string getUrlListNamespace(unsigned long listId);
		std::pair<unsigned long, std::string> getUrlListNamespaceFromParsedTable(unsigned long listId);
		std::pair<unsigned long, std::string> getUrlListNamespaceFromExtractedTable(unsigned long listId);
		std::pair<unsigned long, std::string> getUrlListNamespaceFromAnalyzedTable(unsigned long listId);
		bool isUrlListNamespace(unsigned long websiteId, const std::string& nameSpace);
		void updateUrlList(unsigned long listId, const crawlservpp::Struct::UrlListProperties& listProperties);
		void deleteUrlList(unsigned long listId);
		void resetParsingStatus(unsigned long listId);
		void resetExtractingStatus(unsigned long listId);
		void resetAnalyzingStatus(unsigned long listId);

		// query functions
		unsigned long addQuery(unsigned long websiteId, const crawlservpp::Struct::QueryProperties& queryProperties);
		void getQueryProperties(unsigned long queryId, crawlservpp::Struct::QueryProperties& queryPropertiesTo);
		void updateQuery(unsigned long queryId, const crawlservpp::Struct::QueryProperties& queryProperties);
		void deleteQuery(unsigned long queryId);
		unsigned long duplicateQuery(unsigned long queryId);

		// configuration functions
		unsigned long addConfiguration(unsigned long websiteId, const crawlservpp::Struct::ConfigProperties& configProperties);
		const std::string getConfiguration(unsigned long configId);
		void updateConfiguration(unsigned long configId, const crawlservpp::Struct::ConfigProperties& configProperties);
		void deleteConfiguration(unsigned long configId);
		unsigned long duplicateConfiguration(unsigned long configId);

		// table indexing functions
		void addParsedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		std::vector<std::pair<unsigned long, std::string>> getParsedTables(unsigned long listId);
		std::string getParsedTable(unsigned long tableId);
		void deleteParsedTable(unsigned long tableId);
		void addExtractedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		std::vector<std::pair<unsigned long, std::string>> getExtractedTables(unsigned long listId);
		std::string getExtractedTable(unsigned long tableId);
		void deleteExtractedTable(unsigned long tableId);
		void addAnalyzedTable(unsigned long websiteId, unsigned long listId, const std::string& tableName);
		std::vector<std::pair<unsigned long, std::string>> getAnalyzedTables(unsigned long listId);
		std::string getAnalyzedTable(unsigned long tableId);
		void deleteAnalyzedTable(unsigned long tableId);

		// table locking function
		void releaseLocks();

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

		// sub-structure for table columns
		struct Column {
			std::string _name;				// name of the column
			std::string _type;				// type of the column
			std::string _referenceTable;	// name of the referenced table
			std::string _referenceColumn;	// name of the referenced column

			Column(const std::string& name, const std::string& type, const std::string& referenceTable,
					const std::string& referenceColumn) {
				this->_name = name;
				this->_type = type;
				this->_referenceTable = referenceTable;
				this->_referenceColumn = referenceColumn;
			}
			Column(const std::string& name, const std::string& type) : Column(name, type, "", "") {}
		};

		// sub-class for database exceptions
		class Exception : public std::exception {
		public:
			Exception(const std::string& description) { this->_description = description; }
			const char * what() const throw() { return this->_description.c_str(); }
			const std::string& whatStr() const throw() { return this->_description; }
		private:
			std::string _description;
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
		void createTable(const std::string& tableName, const std::vector<Column>& columns, bool compressed);
		void addColumn(const std::string& tableName, const Column& column);
		void compressTable(const std::string& tableName);
		void deleteTableIfExists(const std::string& tableName);

	private:
		// private connection information
		const crawlservpp::Struct::DatabaseSettings settings;
		unsigned long maxAllowedPacketSize;
		unsigned long sleepOnError;
		std::string module;
#ifdef MAIN_DATABASE_RECONNECT_AFTER_IDLE_SECONDS
		crawlservpp::Timer::Simple reconnectTimer;
#endif

		// prepared SQL statements
		std::vector<crawlservpp::Wrapper::PreparedSqlStatement> preparedStatements;

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
