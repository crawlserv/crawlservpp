/*
 * TargetTableLock.h
 *
 * Safe in-scope table lock for target tables.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#ifndef MODULE_TARGETTABLESLOCK_H_
#define MODULE_TARGETTABLESLOCK_H_

#include "Database.h"

namespace crawlservpp::Wrapper {

class TargetTablesLock {
public:
	TargetTablesLock(Database& db, const std::string& type, unsigned long websiteId,
			unsigned long listId, unsigned long timeOut);
	virtual ~TargetTablesLock();

	// not moveable, not copyable
	TargetTablesLock(TargetTablesLock&) = delete;
	TargetTablesLock(TargetTablesLock&&) = delete;
	TargetTablesLock& operator=(TargetTablesLock&) = delete;
	TargetTablesLock& operator=(TargetTablesLock&&) = delete;

private:
	// internal reference to the database connection of the thread
	Database& ref;

	// table type
	std::string type;
};

}

#endif /* MODULE_TARGETTABLESLOCK_H_ */
