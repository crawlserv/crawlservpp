/*
 * PreparedSqlStatement.h
 *
 * RAII wrapper for prepared SQL statement pointer.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_PREPAREDSQLSTATEMENT_H_
#define WRAPPER_PREPAREDSQLSTATEMENT_H_

#include <cppconn/prepared_statement.h>
#include <mysql_connection.h>

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace crawlservpp::Wrapper {

class PreparedSqlStatement {
public:
	// constructors
	PreparedSqlStatement() noexcept;
	PreparedSqlStatement(sql::Connection * setConnection, const std::string& sqlQuery);
	PreparedSqlStatement(PreparedSqlStatement&& other) noexcept;

	// destructor
	virtual ~PreparedSqlStatement();

	// getters
	sql::PreparedStatement& get();
	const sql::PreparedStatement& get() const;

	// control functions
	void prepare();
	void reset();
	void refresh(sql::Connection * newConnection);

	// operators
	operator bool() const noexcept;
	bool operator!() const noexcept;
	PreparedSqlStatement& operator=(PreparedSqlStatement&& other) noexcept;

	// not copyable
	PreparedSqlStatement(PreparedSqlStatement&) = delete;
	PreparedSqlStatement& operator=(PreparedSqlStatement&) = delete;

private:
	sql::Connection * connection;
	std::string query;
	std::unique_ptr<sql::PreparedStatement> ptr;
};

}

#endif /* WRAPPER_PREPAREDSQLSTATEMENT_H_ */
