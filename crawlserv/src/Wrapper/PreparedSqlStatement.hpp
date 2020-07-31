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
 * PreparedSqlStatement.hpp
 *
 * RAII wrapper for prepared SQL statement pointer.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_PREPAREDSQLSTATEMENT_HPP_
#define WRAPPER_PREPAREDSQLSTATEMENT_HPP_

#include "../Helper/Portability/mysqlcppconn.h"

#include <cppconn/prepared_statement.h>
#include <mysql_connection.h>

#include <memory>		// std::unique_ptr
#include <stdexcept>	// std::runtime_error
#include <string>		// std::string
#include <string_view>	// std::string_view
#include <utility>		// std::move

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for prepared MySQL statements.
	/*!
	 * Prepares the statement on creation and clears it
	 *  on destruction using a smart pointer, avoiding
	 *  memory leaks.
	 *
	 * Additionally stores the underlying SQL statement as
	 *  string, to be able to re-create it on connection loss.
	 *
	 * At the moment, this class is used exclusively by the
	 *  Main::Database class.
	 *
	 * For more information about the MySQL Connector/C++ used, visit its
	 *  <a href="https://dev.mysql.com/doc/connector-cpp/8.0/en/">website</a>.
	 *
	 * \note The class owns the pointer to the prepared MySQL statement.
	 */
	class PreparedSqlStatement {
	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor creating an empty statement.
		/*!
		 * \warning This constructor should only be used for reserving memory,
		 *   e.g. by using std::vector<PreparedSqlStatement>::resize().
		 */
		PreparedSqlStatement() = default;

		PreparedSqlStatement(sql::Connection * setConnection, std::string_view sqlQuery);
		virtual ~PreparedSqlStatement();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] sql::PreparedStatement& get();
		[[nodiscard]] bool valid() const noexcept;

		///@}
		///@name Lifetime
		///@{

		void prepare();
		void clear();
		void refresh(sql::Connection * newConnection);

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		PreparedSqlStatement(PreparedSqlStatement&) = delete;

		//! Deleted copy assignment operator.
		PreparedSqlStatement& operator=(PreparedSqlStatement&) = delete;

		PreparedSqlStatement(PreparedSqlStatement&& other) noexcept;
		PreparedSqlStatement& operator=(PreparedSqlStatement&& other) noexcept;

		///@}

	private:
		// pointer to the connection (not owned)
		sql::Connection * connection{nullptr};

		// internal storage of the SQL query (for recovery on connection loss)
		std::string query;

		// unique pointer to the prepared MySQL statement
		std::unique_ptr<sql::PreparedStatement> ptr;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor preparing a MySQL statement for the given connection and SQL query.
	/*!
	 * \param setConnection A pointer to the MySQL connection to use.
	 *
	 * \param sqlQuery A string view containing the SQL query to prepare.
	 */
	inline PreparedSqlStatement::PreparedSqlStatement(
			sql::Connection * setConnection,
			std::string_view sqlQuery
	) 		: connection(setConnection), query(sqlQuery) {
		this->prepare();
	}

	//! Destructor clearing the prepared MySQL statement if necessary.
	inline PreparedSqlStatement::~PreparedSqlStatement() {
		this->clear();
	}

	//! Gets a reference to the prepared MySQL statement.
	/*!
	 * \returns A reference to the prepared MySQL statement.
	 *
	 * \throws std::runtime_error if the MySQL statement is invalid.
	 */
	inline sql::PreparedStatement& PreparedSqlStatement::get() {
		if(this->ptr == nullptr) {
			throw std::runtime_error("get(): No SQL statement prepared");
		}

		return *(this->ptr);
	}

	//! Checks whether the prepared MySQL statement is valid.
	/*!
	 * \returns True, if the prepared MySQL statement is valid.
	 *   False otherwise.
	 */
	inline bool PreparedSqlStatement::valid() const noexcept {
		return this->ptr.operator bool();
	}

	//! Prepares the MySQL statement.
	/*!
	 * If a statement has been prepared before, it will be cleared
	 *  if necessary.
	 *
	 * \note If no SQL query is stored inside this instance of the class,
	 *  no new MySQL statement will be prepared.
	 */
	inline void PreparedSqlStatement::prepare() {
		this->clear();

		if(!(this->query.empty())) {
			this->ptr.reset(this->connection->prepareStatement(this->query));
		}
	}

	//! Clears the prepared MySQL statement.
	/*!
	 * The prepared MySQL statement will be invalid and valid()
	 *  will return false afterwards.
	 *
	 * \note Does nothing if the underlying MySQL statement
	 *   is not valid.
	 */
	inline void PreparedSqlStatement::clear() {
		if(this->ptr != nullptr) {
			this->ptr->close();

			this->ptr.reset();
		}
	}

	//! Refreshes the prepared MySQL statement using the new connection given.
	/*!
	 * The old prepared MySQL statement will be cleared before
	 *  setting the new connection and preparing the same SQL query
	 *  using the new connection.
	 *
	 * This function is used to handle connection losses, which will
	 *  invalidate the previous connection pointer.
	 *
	 * \param newConnection A pointer to the new connection, which will
	 *   be used from now on.
	 */
	inline void PreparedSqlStatement::refresh(sql::Connection * newConnection) {
		this->clear();

		this->connection = newConnection;

		this->prepare();
	}

	//! Move constructor.
	/*!
	 * Moves the prepared MySQL statement from the specified
	 *  location into this instance of the class.
	 *
	 * \note The other statement will be invalidated by this move.
	 *
	 * \param other The prepared MySQL statement to move from.
	 *
	 * \sa valid
	 */
	inline PreparedSqlStatement::PreparedSqlStatement(PreparedSqlStatement&& other) noexcept
		: connection(other.connection), query(std::move(other.query)), ptr(std::move(other.ptr)) {}

	//! Move assignment operator.
	/*!
	 * Moves the prepared MySQL statement from the
	 *  specified location into this instance of the class.
	 *
	 * If there already is a prepared MySQL statement
	 *  in this instance of the class, it will be cleared.
	 *
	 * \note The other statement will be invalidated by this move.
	 *
	 * \note Nothing will be done if used on itself.
	 *
	 * \param other The prepared MySQL statement to move from.
	 *
	 * \returns A reference to the instance containing the
	 *   prepared MySQL statement after moving (i.e. *this).
	 *
	 * \sa valid
	 */
	inline PreparedSqlStatement& PreparedSqlStatement::operator=(PreparedSqlStatement&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->connection = other.connection;

			this->query = std::move(other.query);

			this->ptr = std::move(other.ptr);

			other.connection = nullptr;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_PREPAREDSQLSTATEMENT_HPP_ */
