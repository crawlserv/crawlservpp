/*
 * TableLock.h
 *
 * Template class for safe in-scope table locks.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_TABLELOCK_H_
#define WRAPPER_TABLELOCK_H_

#include <string>

namespace crawlservpp::Wrapper {

template<class DB> // DB needs to be a Database connection class w/ .lockTable(...), .lockTables(...) and .unlockTables()
class TableLock {
public:
	// constructor A: lock one table (and its alias 'a' for reading)
	TableLock(DB& db, const std::string& tableName) : ref(db) {
		this->ref.lockTable(tableName);
	}

	// constructor B: lock two tables (and their aliases 'a' and 'b' for reading)
	TableLock(DB& db, const std::string& tableName1, const std::string& tableName2) : ref(db) {
		this->ref.lockTables(tableName1, tableName2);
	}

	// destructor: try to unlock the table(s)
	virtual ~TableLock() {
		try { this->ref.unlockTables(); }
		catch(...) {}
	}

	// not moveable, not copyable
	TableLock(TableLock&) = delete;
	TableLock(TableLock&&) = delete;
	TableLock& operator=(TableLock&) = delete;
	TableLock& operator=(TableLock&&) = delete;

private:
	// internal reference to a database connection
	DB& ref;
};

}

#endif /* WRAPPER_TABLELOCK_H_ */
