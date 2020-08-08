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
 * Interface to be inherited by the module threads.
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
#include "../Wrapper/DatabaseTryLock.hpp"

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

	//! %Wrapper class providing the database functionality of Module::Database to its child classes.
	/*!
	 * \sa Module::Analyzer::Database,
	 * 	 Module::Crawler::Database,
	 * 	 Module::Extractor::Database,
	 * 	 Module::Parser::Database
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
		//! Allows scoped locking of the database.
		template<class DB> friend class Wrapper::DatabaseLock;

		//! Allows optional scoped locking of the database.
		template<class DB> friend class Wrapper::DatabaseTryLock;

		///@name Construction and Destruction
		///@{

		explicit Database(Module::Database& dbThread);

		//! Default destructor.
		virtual ~Database() = default;

		///@}
		///@name Setters
		///@{

		void setLogging(std::uint8_t level, std::uint8_t min, std::uint8_t verbose);
		void setSleepOnError(std::uint64_t seconds);
		void setTimeOut(std::uint64_t milliseconds);

		///@}
		///@name Logging
		///@{

		void log(std::uint8_t level, const std::string& logEntry);
		void log(std::uint8_t level, std::queue<std::string>& logEntries);

		///@}
		///@name Websites
		///@{

		[[nodiscard]] std::string getWebsiteDomain(std::uint64_t websiteId);

		///@}
		///@name Queries
		///@{

		void getQueryProperties(std::uint64_t queryId, QueryProperties& queryPropertiesTo);

		///@}
		///@name Configurations
		///@{

		[[nodiscard]] std::string getConfiguration(std::uint64_t configId);

		///@}
		///@name Target Tables
		///@{

		std::uint64_t addTargetTable(const TargetTableProperties& properties);
		[[nodiscard]] std::queue<IdString> getTargetTables(const std::string& type, std::uint64_t listId);
		[[nodiscard]] std::uint64_t getTargetTableId(
				const std::string& type,
				std::uint64_t listId,
				const std::string& tableName
		);
		[[nodiscard]] std::string getTargetTableName(const std::string& type, std::uint64_t tableId);
		void deleteTargetTable(const std::string& type, std::uint64_t tableId);

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
		[[nodiscard]] bool isColumnExists(const std::string& tableName, const std::string& columnName);
		[[nodiscard]] std::string getColumnType(const std::string& tableName, const std::string& columnName);

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

		[[nodiscard]] static std::uint64_t getRequestCounter();

		///@}
		/**@name Copy and Move
		 * The class is neither copyable, nor moveable.
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
		///@name Database Connection
		///@{

		//! Reference to the database connection for the thread.
		Module::Database& database;

		///@}
		///@name Getters
		///@{

		[[nodiscard]] const ModuleOptions& getOptions() const;
		[[nodiscard]] const std::string& getWebsiteIdString() const;
		[[nodiscard]] const std::string& getUrlListIdString() const;
		[[nodiscard]] std::uint8_t getLoggingMin() const;
		[[nodiscard]] std::uint8_t getLoggingVerbose() const;
		[[nodiscard]] std::uint64_t getMaxAllowedPacketSize() const;

		///@}
		///@name Validation
		///@{

		void checkConnection();

		///@}
		///@name Helper Functions for Prepared SQL Statements
		///@{

		// wrappers for managing prepared SQL statements
		void reserveForPreparedStatements(std::size_t n);
		[[nodiscard]] std::size_t addPreparedStatement(const std::string& sqlQuery);
		[[nodiscard]] sql::PreparedStatement& getPreparedStatement(std::size_t id);

		///@}
		///@name Database Helper Functions
		///@{

		[[nodiscard]] std::uint64_t getLastInsertedId();
		static void addDatabaseLock(const std::string& name, const IsRunningCallback& isRunningCallback);
		static bool tryDatabaseLock(const std::string& name);
		static void removeDatabaseLock(const std::string& name);
		void createTable(const TableProperties& properties);
		void dropTable(const std::string& tableName);
		void addColumn(const std::string& tableName, const TableColumn& column);
		void compressTable(const std::string& tableName);

		///@}
		///@name URL List Helper Functions
		///@{

		void setUrlListCaseSensitive(std::uint64_t listId, bool isCaseSensitive);

		///@}
		///@name Exception Helper Functions
		///@{

		static void sqlException(const std::string& function, const sql::SQLException& e);

		///@}
		///@name Helper Functions for Executing SQL Queries
		///@{

		static bool sqlExecute(sql::PreparedStatement& sqlPreparedStatement);
		static sql::ResultSet * sqlExecuteQuery(sql::PreparedStatement& sqlPreparedStatement);
		static int sqlExecuteUpdate(sql::PreparedStatement& sqlPreparedStatement);

		///@}
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor setting the database connection.
	/*!
	 * \param dbThread Reference to the database
	 *   connection used by the thread.
	 */
	inline Database::Database(Module::Database& dbThread) : database(dbThread) {}

	//! \copydoc Module::Database::setLogging
	inline void Database::setLogging(std::uint8_t level, std::uint8_t min, std::uint8_t verbose) {
		this->database.setLogging(level, min, verbose);
	}

	//! \copydoc Main::Database::setSleepOnError
	inline void Database::setSleepOnError(std::uint64_t seconds) {
		this->database.setSleepOnError(seconds);
	}

	//! \copydoc Main::Database::setTimeOut
	inline void Database::setTimeOut(std::uint64_t milliseconds) {
		this->database.setTimeOut(milliseconds);
	}

	//! \copydoc Module::Database::log(std::uint8_t, const std::string&)
	inline void Database::log(std::uint8_t level, const std::string& logEntry) {
		this->database.log(level, logEntry);
	}

	//! \copydoc Module::Database::log(std::uint8_t, std::queue<std::string>&)
	inline void Database::log(std::uint8_t level, std::queue<std::string>& logEntries) {
		this->database.log(level, logEntries);
	}

	//! \copydoc Main::Database::getWebsiteDomain
	inline std::string Database::getWebsiteDomain(std::uint64_t websiteId) {
		return this->database.getWebsiteDomain(websiteId);
	}

	//! \copydoc Main::Database::getQueryProperties
	inline void Database::getQueryProperties(std::uint64_t queryId, QueryProperties& queryPropertiesTo) {
		this->database.getQueryProperties(queryId, queryPropertiesTo);
	}

	//! \copydoc Main::Database::getConfiguration
	inline std::string Database::getConfiguration(std::uint64_t configId) {
		return this->database.getConfiguration(configId);
	}

	//! \copydoc Main::Database::addTargetTable
	inline std::uint64_t Database::addTargetTable(const TargetTableProperties& properties) {
		return this->database.addTargetTable(properties);
	}

	//! \copydoc Main::Database::getTargetTables
	inline std::queue<Database::IdString> Database::getTargetTables(const std::string& type, std::uint64_t listId) {
		return this->database.getTargetTables(type, listId);
	}

	//! \copydoc Main::Database::getTargetTableId
	inline std::uint64_t Database::getTargetTableId(
			const std::string& type,
			std::uint64_t listId,
			const std::string& tableName
	) {
		return this->database.getTargetTableId(type, listId, tableName);
	}

	//! \copydoc Main::Database::getTargetTableName
	inline std::string Database::getTargetTableName(const std::string& type, std::uint64_t tableId) {
		return this->database.getTargetTableName(type, tableId);
	}

	//! \copydoc Main::Database::deleteTargetTable
	inline void Database::deleteTargetTable(const std::string& type, std::uint64_t tableId) {
		this->database.deleteTargetTable(type, tableId);
	}

	//! \copydoc Main::Database::beginNoLock
	inline void Database::beginNoLock() {
		this->database.beginNoLock();
	}

	//! \copydoc Main::Database::endNoLock
	inline void Database::endNoLock() {
		this->database.endNoLock();
	}

	//! \copydoc Main::Database::isTableEmpty
	inline bool Database::isTableEmpty(const std::string& tableName) {
		return this->database.isTableEmpty(tableName);
	}

	//! \copydoc Main::Database::isTableExists
	inline bool Database::isTableExists(const std::string& tableName) {
		return this->database.isTableExists(tableName);
	}

	//! \copydoc Main::Database::isColumnExists
	inline bool Database::isColumnExists(const std::string& tableName, const std::string& columnName) {
		return this->database.isColumnExists(tableName, columnName);
	}

	//! \copydoc Main::Database::getColumnType
	inline std::string Database::getColumnType(const std::string& tableName, const std::string& columnName) {
		return this->database.getColumnType(tableName, columnName);
	}

	//! \copydoc Main::Database::getCustomData(Data::GetValue&)
	inline void Database::getCustomData(Data::GetValue& data) {
		this->database.getCustomData(data);
	}

	//! \copydoc Main::Database::getCustomData(Data::GetFields&)
	inline void Database::getCustomData(Data::GetFields& data) {
		this->database.getCustomData(data);
	}

	//! \copydoc Main::Database::getCustomData(Data::GetFieldsMixed&)
	inline void Database::getCustomData(Data::GetFieldsMixed& data) {
		this->database.getCustomData(data);
	}

	//! \copydoc Main::Database::getCustomData(Data::GetColumn&)
	inline void Database::getCustomData(Data::GetColumn& data) {
		this->database.getCustomData(data);
	}

	//! \copydoc Main::Database::getCustomData(Data::GetColumns&)
	inline void Database::getCustomData(Data::GetColumns& data) {
		this->database.getCustomData(data);
	}

	//! \copydoc Main::Database::getCustomData(Data::GetColumnsMixed&)
	inline void Database::getCustomData(Data::GetColumnsMixed& data) {
		this->database.getCustomData(data);
	}

	//! \copydoc Main::Database::insertCustomData(const Data::InsertValue&)
	inline void Database::insertCustomData(const Data::InsertValue& data) {
		this->database.insertCustomData(data);
	}

	//! \copydoc Main::Database::insertCustomData(const Data::InsertFields&)
	inline void Database::insertCustomData(const Data::InsertFields& data) {
		this->database.insertCustomData(data);
	}

	//! \copydoc Main::Database::insertCustomData(const Data::InsertFieldsMixed&)
	inline void Database::insertCustomData(const Data::InsertFieldsMixed& data) {
		this->database.insertCustomData(data);
	}

	//! \copydoc Main::Database::updateCustomData(const Data::UpdateValue&)
	inline void Database::updateCustomData(const Data::UpdateValue& data) {
		this->database.updateCustomData(data);
	}

	//! \copydoc Main::Database::updateCustomData(const Data::UpdateFields&)
	inline void Database::updateCustomData(const Data::UpdateFields& data) {
		this->database.updateCustomData(data);
	}

	//! \copydoc Main::Database::updateCustomData(const Data::UpdateFieldsMixed&)
	inline void Database::updateCustomData(const Data::UpdateFieldsMixed& data) {
		this->database.updateCustomData(data);
	}

	//! \copydoc Main::Database::getRequestCounter
	inline std::uint64_t Database::getRequestCounter() {
		return Main::Database::getRequestCounter();
	}

	//! Gets the options of the module.
	/*!
	 * \returns A reference to the structure
	 *   containing the options for the module.
	 *
	 * \sa Struct::ModuleOptions
	 */
	inline const Struct::ModuleOptions& Database::getOptions() const {
		return this->database.options;
	}

	//! Gets the ID of the website used by the thread as string.
	/*!
	 * \returns A reference to the string
	 *   containing the ID of the website
	 *   used by the thread.
	 */
	inline const std::string& Database::getWebsiteIdString() const {
		return this->database.websiteIdString;
	}

	//! Gets the ID of the URL list used by the thread as string.
	/*!
	 * \returns A reference to the string
	 *   containing the ID of the URL list
	 *   used by the thread.
	 */
	inline const std::string& Database::getUrlListIdString() const {
		return this->database.urlListIdString;
	}

	//! Gets the minimal logging level.
	/*!
	 * \returns The minimum logging level,
	 *   in which logging is still not
	 *   deactivated.
	 */
	inline std::uint8_t Database::getLoggingMin() const {
		return this->database.loggingMin;
	}

	//! Gets the level for verbose logging.
	/*!
	 * \returns The logging level, in which
	 *   verbose logging is activated.
	 */
	inline std::uint8_t Database::getLoggingVerbose() const {
		return this->database.loggingVerbose;
	}

	//! \copydoc Main::Database::getMaxAllowedPacketSize
	inline std::uint64_t Database::getMaxAllowedPacketSize() const {
		return this->database.getMaxAllowedPacketSize();
	}

	//! \copydoc Main::Database::checkConnection
	inline void Database::checkConnection() {
		this->database.checkConnection();
	}

	//! \copydoc Main::Database::reserveForPreparedStatements
	inline void Database::reserveForPreparedStatements(std::size_t n) {
		this->database.reserveForPreparedStatements(n);
	}

	//! \copydoc Main::Database::addPreparedStatement
	inline std::size_t Database::addPreparedStatement(const std::string& sqlQuery) {
		return this->database.addPreparedStatement(sqlQuery);
	}

	//! \copydoc Main::Database::getPreparedStatement
	inline sql::PreparedStatement& Database::getPreparedStatement(std::size_t id) {
		return this->database.getPreparedStatement(id);
	}

	//! \copydoc Main::Database::getLastInsertedId
	inline std::uint64_t Database::getLastInsertedId() {
		return this->database.getLastInsertedId();
	}

	//! \copydoc Main::Database::addDatabaseLock
	inline void Database::addDatabaseLock(const std::string& name, const IsRunningCallback& isRunningCallback) {
		Main::Database::addDatabaseLock(name, isRunningCallback);
	}

	//! \copydoc Main::Database::tryDatabaseLock
	inline bool Database::tryDatabaseLock(const std::string& name) {
		return Main::Database::tryDatabaseLock(name);
	}

	//! \copydoc Main::Database::removeDatabaseLock
	inline void Database::removeDatabaseLock(const std::string& name) {
		Main::Database::removeDatabaseLock(name);
	}

	//! \copydoc Main::Database::createTable
	inline void Database::createTable(const TableProperties& properties) {
		this->database.createTable(properties);
	}

	//! \copydoc Main::Database::dropTable
	inline void Database::dropTable(const std::string& tableName) {
		this->database.dropTable(tableName);
	}

	//! \copydoc Main::Database::addColumn
	inline void Database::addColumn(const std::string& tableName, const TableColumn& column) {
		this->database.addColumn(tableName, column);
	}

	//! \copydoc Main::Database::compressTable
	inline void Database::compressTable(const std::string& tableName) {
		this->database.compressTable(tableName);
	}

	//! \copydoc Main::Database::setUrlListCaseSensitive
	inline void Database::setUrlListCaseSensitive(std::uint64_t listId, bool isCaseSensitive) {
		this->database.setUrlListCaseSensitive(listId, isCaseSensitive);
	}

	//! \copydoc Main::Database::sqlException
	inline void Database::sqlException(const std::string& function, const sql::SQLException& e) {
		Main::Database::sqlException(function, e);
	}

	//! Executes a prepared SQL statement.
	/*!
	 * \param sqlPreparedStatement Reference
	 *   to the prepared SQL statement to be
	 *   executed.
	 *
	 * \returns True, if the prepared SQL
	 *   statement returned a result set.
	 *   False, if the statement returned
	 *   nothing or an update count.
	 */
	inline bool Database::sqlExecute(sql::PreparedStatement& sqlPreparedStatement) {
		return Main::Database::sqlExecute(sqlPreparedStatement);
	}

	//! Executes a prepared SQL statement and returns the resulting set.
	/*!
	 * \param sqlPreparedStatement Reference
	 *   to the prepared SQL statement to be
	 *   executed.
	 *
	 * \returns A pointer to the result set retrieved
	 *   by executing the prepared SQL statement.
	 */
	inline sql::ResultSet * Database::sqlExecuteQuery(sql::PreparedStatement& sqlPreparedStatement) {
		return Main::Database::sqlExecuteQuery(sqlPreparedStatement);
	}

	//! Executes a prepared SQL statement and returns the number of affected rows.
	/*!
	 * \param sqlPreparedStatement Reference
	 *   to the prepared SQL statement to be
	 *   executed.
	 *
	 * \returns The number of rows affected by the
	 *   prepared SQL statement
	 */
	inline int Database::sqlExecuteUpdate(sql::PreparedStatement& sqlPreparedStatement) {
		return Main::Database::sqlExecuteUpdate(sqlPreparedStatement);
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_DATABASE_HPP_ */
