/*
 * TargetTableLock.cpp
 *
 * Safe in-scope table lock for target tables.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#include "TargetTablesLock.h"

namespace crawlservpp::Wrapper {

// constructor: lock the custom table
TargetTablesLock::TargetTablesLock(Database& db, const std::string &type, unsigned long websiteId,
		unsigned long listId, unsigned long timeOut) : ref(db), type(type) {
	this->ref.lockTargetTables(type, websiteId, listId, timeOut);
}

// destructor: try to unlock the custom table
TargetTablesLock::~TargetTablesLock() {
	try { this->ref.unlockTargetTables(this->type); }
	catch(...) {}
}

}
