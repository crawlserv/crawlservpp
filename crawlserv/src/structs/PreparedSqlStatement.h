/*
 * PreparedSqlStatement.h
 *
 * Content of and pointer to a prepared SQL statement.
 *
 *  Created on: Oct 12, 2018
 *      Author: ans
 */

#ifndef STRUCTS_PREPAREDSQLSTATEMENT_H_
#define STRUCTS_PREPAREDSQLSTATEMENT_H_

#include <cppconn/prepared_statement.h>

#include <string>

struct PreparedSqlStatement {
	std::string string;
	sql::PreparedStatement * statement;
};

#endif /* STRUCTS_PREPAREDSQLSTATEMENT_H_ */
