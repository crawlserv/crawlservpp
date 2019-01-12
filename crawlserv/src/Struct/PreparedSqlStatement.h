/*
 * PreparedSqlStatement.h
 *
 * Content of and pointer to a prepared SQL statement.
 *
 *  Created on: Oct 12, 2018
 *      Author: ans
 */

#ifndef STRUCT_PREPAREDSQLSTATEMENT_H_
#define STRUCT_PREPAREDSQLSTATEMENT_H_

#include <cppconn/prepared_statement.h>

#include <string>

namespace crawlservpp::Struct {
	struct PreparedSqlStatement {
		std::string string;
		sql::PreparedStatement * statement;
	};
}

#endif /* STRUCT_PREPAREDSQLSTATEMENT_H_ */
