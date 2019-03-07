/*
 * TableLock.h
 *
 * Safe in-scope table lock for the module threads.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#ifndef MODULE_TABLELOCK_H_
#define MODULE_TABLELOCK_H_

#include "Database.h"

namespace crawlservpp::Wrapper {

class TableLock {
public:
	TableLock(Database& db, const std::string& tableName);
	TableLock(Database& db, const std::string& tableName1, const std::string& tableName2);
	virtual ~TableLock();

	// not moveable, not copyable
	TableLock(TableLock&) = delete;
	TableLock(TableLock&&) = delete;
	TableLock& operator=(TableLock&) = delete;
	TableLock& operator=(TableLock&&) = delete;

private:
	// internal reference to the database connection of the thread
	Database& ref;
};

}

#endif /* MODULE_TABLELOCK_H_ */
