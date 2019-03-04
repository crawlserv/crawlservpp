/*
 * PreparedSqlStatement.cpp
 *
 * RAII wrapper for prepared SQL statement pointer.
 *
 *  Created on: Feb 7, 2019
 *      Author: ans
 */

#include "PreparedSqlStatement.h"

namespace crawlservpp::Wrapper {

// constructor: create empty statement for std::vector::resize()
PreparedSqlStatement::PreparedSqlStatement() noexcept : connection(NULL) {}

// constructor: create prepared SQL statement for specified connection and SQL query
PreparedSqlStatement::PreparedSqlStatement(sql::Connection * setConnection, const std::string& sqlQuery)
		: connection(setConnection), query(sqlQuery) {
	this->prepare();
}

// move constructor
PreparedSqlStatement::PreparedSqlStatement(PreparedSqlStatement&& other) noexcept
		: connection(other.connection), query(other.query), ptr(std::move(other.ptr)) {}

// destructor: reset prepared SQL statement
PreparedSqlStatement::~PreparedSqlStatement() {
	this->reset();
}

// get reference to prepared SQL statement
sql::PreparedStatement& PreparedSqlStatement::get() {
	if(!(this->ptr))
		throw std::runtime_error("PreparedSqlStatement::get(): No SQL statement prepared");
	return *(this->ptr);
}

// get const reference to prepared SQL statement
const sql::PreparedStatement& PreparedSqlStatement::get() const{
	if(!(this->ptr))
		throw std::runtime_error("PreparedSqlStatement::get(): No SQL statement prepared");
	return *(this->ptr);
}

// prepare SQL statement
void PreparedSqlStatement::prepare() {
	this->reset();
	if(!(this->query.empty()))
		this->ptr.reset(this->connection->prepareStatement(this->query));
}

// reset SQL statement
void PreparedSqlStatement::reset() {
	if(this->ptr) {
		this->ptr->close();
		this->ptr.reset();
	}
}

// refresh prepared SQL statement
void PreparedSqlStatement::refresh(sql::Connection * newConnection) {
	this->reset();
	this->connection = newConnection;
	this->prepare();
}

// bool operator
PreparedSqlStatement::operator bool() const noexcept {
	return this->ptr.operator bool();
}

// not operator
bool PreparedSqlStatement::operator!() const noexcept {
	return !(this->ptr);
}

// move assignment operator
PreparedSqlStatement& PreparedSqlStatement::operator=(PreparedSqlStatement&& other) noexcept {
	if(&other != this) {
		this->connection = other.connection;
		this->ptr = std::move(other.ptr);
		this->query = other.query;
	}
	return *this;
}

}
