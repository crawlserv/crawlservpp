/*
 * CustomTableLock.h
 *
 * Safe in-scope table lock for custom module tables.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#ifndef MODULE_CUSTOMTABLELOCK_H_
#define MODULE_CUSTOMTABLELOCK_H_

#include "Database.h"

namespace crawlservpp::Wrapper {

class CustomTableLock {
public:
	CustomTableLock(Database& db, const std::string& type, unsigned long websiteId,
			unsigned long listId, unsigned long timeOut);
	virtual ~CustomTableLock();

	// not moveable, not copyable
	CustomTableLock(CustomTableLock&) = delete;
	CustomTableLock(CustomTableLock&&) = delete;
	CustomTableLock& operator=(CustomTableLock&) = delete;
	CustomTableLock& operator=(CustomTableLock&&) = delete;

private:
	// internal reference to the database connection of the thread
	Database& ref;

	// table type
	std::string type;
};

}

#endif /* MODULE_CUSTOMTABLELOCK_H_ */
