/*
 * TableLock.cpp
 *
 * Safe in-scope table lock for the module threads.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#include "TableLock.h"

namespace crawlservpp::Wrapper {

// constructor A: lock one table
TableLock::TableLock(Database& db, const std::string& tableName) : ref(db) {
	this->ref.lockTable(tableName);
}

// constructor B: lock two tables (and the aliases 'a' and 'b' for reading access to those tables)
TableLock::TableLock(Database& db, const std::string& tableName1, const std::string& tableName2) : ref(db) {
	this->ref.lockTables(tableName1, tableName2);
}

// destructor: try to unlock the table(s)
TableLock::~TableLock() {
	try { this->ref.unlockTables(); }
	catch(...) {}
}

}
