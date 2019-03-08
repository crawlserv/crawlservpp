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

template<class DB>
class TableLock {
public:
	// constructor A: lock one table
	TableLock(DB& db, const std::string& tableName) : ref(db) {
		this->ref.lockTable(tableName);
	}

	// constructor B: lock two tables (and the aliases 'a' and 'b' for reading access to those tables)
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
	// internal reference to the database connection of the thread
	DB& ref;
};

}

#endif /* WRAPPER_TABLELOCK_H_ */
