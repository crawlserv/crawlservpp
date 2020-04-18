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
 * Interface to be inherited by the thread modules.
 * Allows them access to the database by providing basic Database functionality as well as the option to add prepared SQL statements.
 *
 *  Created on: Oct 22, 2018
 *      Author: ans
 */

#ifndef WRAPPER_DATABASE_HPP_
#define WRAPPER_DATABASE_HPP_

#include "../Data/Data.hpp"
#include "../Helper/Portability/mysqlcppconn.h"
#include "../Module/Database.hpp"
#include "../Struct/DatabaseSettings.hpp"
#include "../Struct/ModuleOptions.hpp"
#include "../Struct/QueryProperties.hpp"
#include "../Struct/TableColumn.hpp"
#include "../Struct/TableProperties.hpp"
#include "../Struct/TargetTableProperties.hpp"
#include "../Wrapper/DatabaseLock.hpp"

#include <cppconn/prepared_statement.h>
#include <mysql_connection.h>

#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint64_t
#include <functional>	// std::function
#include <queue>		// std::queue
#include <string>		// std::string
#include <utility>		// std::pair

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	class Database {
		// for convenience
		using ModuleOptions = Struct::ModuleOptions;
		using QueryProperties = Struct::QueryProperties;
		using TableColumn = Struct::TableColumn;
		using TableProperties = Struct::TableProperties;
		using TargetTableProperties = Struct::TargetTableProperties;

		using IdString = std::pair<std::uint64_t, std::string>;
		using IsRunningCallback = std::function<bool()>;

	public:
		// allow locking class access to protected functions
		template<class DB> friend class Wrapper::DatabaseLock;

		// constructor and destructor
		Database(Module::Database& dbRef);
		~Database();

		// wrapper for setters
		void setLogging(unsigned short level, unsigned short min, unsigned short verbose);
		void setSleepOnError(std::uint64_t seconds);
		void setTimeOut(std::uint64_t milliseconds);

		// wrapper for logging function
		void log(unsigned short level, const std::string& logEntry);
		void log(unsigned short level, std::queue<std::string>& logEntries);

		// wrapper for website function
		std::string getWebsiteDomain(std::uint64_t websiteId);

		// wrapper for query function
		void getQueryProperties(std::uint64_t queryId, QueryProperties& queryPropertiesTo);

		// wrapper for configuration function
		std::string getConfiguration(std::uint64_t configId);

		// wrappers for database functions
		void beginNoLock();
		void endNoLock();

		// wrappers for target table functions
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

		// wrappers for general table functions
		bool isTableEmpty(const std::string& tableName);
		bool isTableExists(const std::string& tableName);
		bool isColumnExists(const std::string& tableName, const std::string& columnName);
		std::string getColumnType(const std::string& tableName, const std::string& columnName);

		// wrappers for custom data functions for algorithms
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

		// wrapper for static function
		static unsigned long long getRequestCounter();

		// not moveable, not copyable
		Database(Database&) = delete;
		Database(Database&&) = delete;
		Database& operator=(Database&) = delete;
		Database& operator=(Database&&) = delete;

	protected:
		// reference to the database connection by the thread
		Module::Database& database;

		// protected getters for general module options
		const ModuleOptions& getOptions() const;
		const std::string& getWebsiteIdString() const;
		const std::string& getUrlListIdString() const;
		std::uint8_t getLoggingMin() const;
		std::uint8_t getLoggingVerbose() const;

		// wrapper for validation function (changed from public to protected)
		void checkConnection();

		// wrapper for protected getter
		std::uint64_t getMaxAllowedPacketSize() const;

		// wrappers for managing prepared SQL statements
		void reserveForPreparedStatements(std::size_t numberOfAdditionalPreparedStatements);
		std::size_t addPreparedStatement(const std::string& sqlQuery);
		sql::PreparedStatement& getPreparedStatement(unsigned short id);

		// wrappers for database helper functions
		std::uint64_t getLastInsertedId();
		void addDatabaseLock(const std::string& name, IsRunningCallback isRunningCallback);
		void removeDatabaseLock(const std::string& name);
		void createTable(const TableProperties& properties);
		void addColumn(const std::string& tableName, const TableColumn& column);
		void compressTable(const std::string& tableName);
		void deleteTable(const std::string& tableName);

		// wrapper for URL list helper function
		void setUrlListCaseSensitive(std::uint64_t listId, bool isCaseSensitive);

		// wrapper for exception helper function
		void sqlException(const std::string& function, const sql::SQLException& e);

		// wrapper for static helper functions
		static bool sqlExecute(sql::PreparedStatement& sqlPreparedStatement);
		static sql::ResultSet * sqlExecuteQuery(sql::PreparedStatement& sqlPreparedStatement);
		static int sqlExecuteUpdate(sql::PreparedStatement& sqlPreparedStatement);
	};

	/*
	 * IMPLEMENTATION
	 */

	// constructor: initialize database
	inline Database::Database(Module::Database& dbRef) : database(dbRef) {}

	// destructor stub
	inline Database::~Database() {}

	// set current, minimal and verbose logging levels
	inline void Database::setLogging(unsigned short level, unsigned short min, unsigned short verbose) {
		this->database.setLogging(level, min, verbose);
	}

	// set the number of seconds to wait before (first and last) re-try on connection loss to MySQL server
	inline void Database::setSleepOnError(std::uint64_t seconds) {
		this->database.setSleepOnError(seconds);
	}

	// set the maximum amount of milliseconds for a query before it cancels execution (or 0 for none)
	//  NOTE: database connection needs to be established
	inline void Database::setTimeOut(std::uint64_t milliseconds) {
		this->database.setTimeOut(milliseconds);
	}

	// write thread-specific log entry to the database
	inline void Database::log(unsigned short level, const std::string& logEntry) {
		this->database.log(level, logEntry);
	}

	// write multiple thread-specific log entries to the databse
	inline void Database::log(unsigned short level, std::queue<std::string>& logEntries) {
		this->database.log(level, logEntries);
	}

	// get website domain from the database by its ID
	inline std::string Database::getWebsiteDomain(std::uint64_t websiteId) {
		return this->database.getWebsiteDomain(websiteId);
	}

	// get the properties of a query from the database by its ID
	inline void Database::getQueryProperties(std::uint64_t queryId, QueryProperties& queryPropertiesTo) {
		this->database.getQueryProperties(queryId, queryPropertiesTo);
	}

	// get a configuration from the database by its ID
	inline std::string Database::getConfiguration(std::uint64_t configId) {
		return this->database.getConfiguration(configId);
	}

	// disable locking
	inline void Database::beginNoLock() {
		this->database.beginNoLock();
	}

	// re-enable locking by ending (non-existing) transaction
	inline void Database::endNoLock() {
		this->database.endNoLock();
	}

	// add a target table of the specified type to the database if such a table does not exist already, return table ID
	inline std::uint64_t Database::addTargetTable(const TargetTableProperties& properties) {
		return this->database.addTargetTable(properties);
	}

	// get target tables of the specified type for an ID-specified URL list from the database
	inline std::queue<Database::IdString> Database::getTargetTables(const std::string& type, std::uint64_t listId) {
		return this->database.getTargetTables(type, listId);
	}

	// get the ID of a target table of the specified type from the database by its website ID, URL list ID and table name
	inline std::uint64_t Database::getTargetTableId(
			const std::string& type,
			std::uint64_t websiteId,
			std::uint64_t listId,
			const std::string& tableName
	) {
		return this->database.getTargetTableId(type, websiteId, listId, tableName);
	}

	// get the name of a target table of the specified type from the database by its ID
	inline std::string Database::getTargetTableName(const std::string& type, std::uint64_t tableId) {
		return this->database.getTargetTableName(type, tableId);
	}

	// delete target table of the specified type from the database by its ID
	inline void Database::deleteTargetTable(const std::string& type, std::uint64_t tableId) {
		this->database.deleteTargetTable(type, tableId);
	}

	// check whether a name-specified table in the database is empty
	inline bool Database::isTableEmpty(const std::string& tableName) {
		return this->database.isTableEmpty(tableName);
	}

	// check whether a specific table exists in the database
	inline bool Database::isTableExists(const std::string& tableName) {
		return this->database.isTableExists(tableName);
	}

	// check whether a specific column of a specific table exists in the database
	inline bool Database::isColumnExists(const std::string& tableName, const std::string& columnName) {
		return this->database.isColumnExists(tableName, columnName);
	}

	// get the type of a specific column of a specific table in the database
	inline std::string Database::getColumnType(const std::string& tableName, const std::string& columnName) {
		return this->database.getColumnType(tableName, columnName);
	}

	// get one custom value from one field of a row in the database
	inline void Database::getCustomData(Data::GetValue& data) {
		this->database.getCustomData(data);
	}

	// get custom values from multiple fields of a row in the database
	inline void Database::getCustomData(Data::GetFields& data) {
		this->database.getCustomData(data);
	}

	// get custom values from multiple fields of a row with different types in the database
	inline void Database::getCustomData(Data::GetFieldsMixed& data) {
		this->database.getCustomData(data);
	}

	// get custom values from one column in the database
	inline void Database::getCustomData(Data::GetColumn& data) {
		this->database.getCustomData(data);
	}

	// get custom values from multiple columns of the same type in the database
	inline void Database::getCustomData(Data::GetColumns& data) {
		this->database.getCustomData(data);
	}

	// get custom values from multiple columns of different types in the database
	inline void Database::getCustomData(Data::GetColumnsMixed& data) {
		this->database.getCustomData(data);
	}

	// insert one custom value into a row in the database
	inline void Database::insertCustomData(const Data::InsertValue& data) {
		this->database.insertCustomData(data);
	}

	// insert custom values into multiple fields of the same type into a row in the database
	inline void Database::insertCustomData(const Data::InsertFields& data) {
		this->database.insertCustomData(data);
	}

	// insert custom values into multiple fields of different types into a row in the database
	inline void Database::insertCustomData(const Data::InsertFieldsMixed& data) {
		this->database.insertCustomData(data);
	}

	// update one custom value in one field of a row in the database
	inline void Database::updateCustomData(const Data::UpdateValue& data) {
		this->database.updateCustomData(data);
	}

	// update custom values in multiple fields of a row with the same type in the database
	inline void Database::updateCustomData(const Data::UpdateFields& data) {
		this->database.updateCustomData(data);
	}

	// update custom values in multiple fields of a row with different types in the database
	inline void Database::updateCustomData(const Data::UpdateFieldsMixed& data) {
		this->database.updateCustomData(data);
	}

	// get database request counter or return zero if program was compiled without
	inline unsigned long long Database::getRequestCounter() {
		return Main::Database::getRequestCounter();
	}

	// get general module options
	inline const Struct::ModuleOptions& Database::getOptions() const {
		return this->database.options;
	}

	// get website ID as string
	inline const std::string& Database::getWebsiteIdString() const {
		return this->database.websiteIdString;
	}

	// get ID of URL list as string
	inline const std::string& Database::getUrlListIdString() const {
		return this->database.urlListIdString;
	}

	// get minimal logging level (where logging is still not deactivated)
	inline unsigned char Database::getLoggingMin() const {
		return this->database.loggingMin;
	}

	// get verbose logging level
	inline std::uint8_t Database::getLoggingVerbose() const {
		return this->database.loggingVerbose;
	}

	// check whether the connection to the database is still valid and try to re-connect if necesssary,
	//  throws Database::Exception
	//  WARNING: Afterwards, old references to prepared SQL statements may be invalid!
	inline void Database::checkConnection() {
		this->database.checkConnection();
	}

	// get the maximum allowed packet size
	inline std::uint64_t Database::getMaxAllowedPacketSize() const {
		return this->database.getMaxAllowedPacketSize();
	}

	// reserve memory for prepared SQL statements
	inline void Database::reserveForPreparedStatements(std::size_t numberOfAdditionalPreparedStatements) {
		this->database.reserveForPreparedStatements(numberOfAdditionalPreparedStatements);
	}

	// add prepared SQL statement and return the ID of the newly added prepared statement
	inline std::size_t Database::addPreparedStatement(const std::string& sqlQuery) {
		return this->database.addPreparedStatement(sqlQuery);
	}

	// get reference to prepared SQL statement by its ID
	//  WARNING: Do not run checkConnection() while using this reference!
	inline sql::PreparedStatement& Database::getPreparedStatement(unsigned short id) {
		return this->database.getPreparedStatement(id);
	}

	// get the last inserted ID from the database
	inline std::uint64_t Database::getLastInsertedId() {
		return this->database.getLastInsertedId();
	}

	// add a lock with a specific name to the database, wait if lock already exists
	inline void Database::addDatabaseLock(const std::string& name, IsRunningCallback isRunningCallback) {
		this->database.addDatabaseLock(name, isRunningCallback);
	}

	// remove lock with specific name from database
	inline void Database::removeDatabaseLock(const std::string& name) {
		this->database.removeDatabaseLock(name);
	}

	// add a table to the database (the primary key 'id' will be created automatically)
	inline void Database::createTable(const TableProperties& properties) {
		this->database.createTable(properties);
	}

	// add a column to a table in the database
	inline void Database::addColumn(const std::string& tableName, const TableColumn& column) {
		this->database.addColumn(tableName, column);
	}

	// compress a table in the database
	inline void Database::compressTable(const std::string& tableName) {
		this->database.compressTable(tableName);
	}

	// delete a table from the database if it exists
	inline void Database::deleteTable(const std::string& tableName) {
		this->database.deleteTable(tableName);
	}

	// set whether the specified URL list is case-sensitive
	inline void Database::setUrlListCaseSensitive(std::uint64_t listId, bool isCaseSensitive) {
		this->database.setUrlListCaseSensitive(listId, isCaseSensitive);
	}

	// catch SQL exception and re-throw it as specific Database::[X]Exception or generic Database::Exception
	inline void Database::sqlException(const std::string& function, const sql::SQLException& e) {
		this->database.sqlException(function, e);
	}

	// execute prepared SQL statement by reference
	inline bool Database::sqlExecute(sql::PreparedStatement& sqlPreparedStatement) {
		return Main::Database::sqlExecute(sqlPreparedStatement);
	}

	// execute prepared SQL statement by reference and fetch result
	inline sql::ResultSet * Database::sqlExecuteQuery(sql::PreparedStatement& sqlPreparedStatement) {
		return Main::Database::sqlExecuteQuery(sqlPreparedStatement);
	}

	// execute prepared SQL statement by reference and fetch updated rows
	inline int Database::sqlExecuteUpdate(sql::PreparedStatement& sqlPreparedStatement) {
		return Main::Database::sqlExecuteUpdate(sqlPreparedStatement);
	}

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_DATABASE_HPP_ */
