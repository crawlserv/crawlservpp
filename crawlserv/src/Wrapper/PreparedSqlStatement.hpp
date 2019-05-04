/*
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

#include "../Main/Exception.hpp"

#include <cppconn/prepared_statement.h>
#include <mysql_connection.h>

#include <memory>
#include <string>
#include <utility>

namespace crawlservpp::Wrapper {

	class PreparedSqlStatement {
	public:
		// constructor: create empty statement for std::vector::resize()
		PreparedSqlStatement() noexcept : connection(nullptr) {}

		// constructor: create prepared SQL statement for specified connection and SQL query
		PreparedSqlStatement(sql::Connection * setConnection, const std::string& sqlQuery)
				: connection(setConnection), query(sqlQuery) {
			this->prepare();
		}

		// move constructor
		PreparedSqlStatement(PreparedSqlStatement&& other) noexcept
				: connection(other.connection), query(other.query), ptr(std::move(other.ptr)) {}

		// destructor: reset prepared SQL statement
		~PreparedSqlStatement() {
			this->reset();
		}

		// get reference to prepared SQL statement, throws Main::Exception
		sql::PreparedStatement& get() {
			if(!(this->ptr))
				throw Main::Exception("get(): No SQL statement prepared");
			return *(this->ptr);
		}

		// get const reference to prepared SQL statement, throws Main::Exception
		const sql::PreparedStatement& get() const {
			if(!(this->ptr))
				throw Main::Exception("get(): No SQL statement prepared");
			return *(this->ptr);
		}

		// prepare SQL statement
		void prepare() {
			this->reset();
			if(!(this->query.empty()))
				this->ptr.reset(this->connection->prepareStatement(this->query));
		}

		// reset SQL statement
		void reset() {
			if(this->ptr) {
				this->ptr->close();
				this->ptr.reset();
			}
		}

		// refresh prepared SQL statement
		void refresh(sql::Connection * newConnection) {
			this->reset();
			this->connection = newConnection;
			this->prepare();
		}

		// bool operator
		operator bool() const noexcept {
			return this->ptr.operator bool();
		}

		// not operator
		bool operator!() const noexcept {
			return !(this->ptr);
		}

		// move assignment operator
		PreparedSqlStatement& operator=(PreparedSqlStatement&& other) noexcept {
			if(&other != this) {
				this->connection = other.connection;
				this->ptr = std::move(other.ptr);
				this->query = other.query;
			}
			return *this;
		}

		// not copyable
		PreparedSqlStatement(PreparedSqlStatement&) = delete;
		PreparedSqlStatement& operator=(PreparedSqlStatement&) = delete;

	private:
		// pointer to connection
		sql::Connection * connection;

		// internal storage of SQL query string (for recovery on connection loss)
		std::string query;

		// unique pointer to prepared SQL statement
		std::unique_ptr<sql::PreparedStatement> ptr;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_PREPAREDSQLSTATEMENT_HPP_ */
