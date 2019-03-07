/*
 * CustomTableLock.cpp
 *
 * Safe in-scope table lock for custom module tables.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#include "CustomTableLock.h"

namespace crawlservpp::Wrapper {

// constructor: lock the custom table
CustomTableLock::CustomTableLock(Database& db, const std::string &type, unsigned long websiteId,
		unsigned long listId, unsigned long timeOut) : ref(db), type(type) {
	this->ref.lockCustomTables(type, websiteId, listId, timeOut);
}

// destructor: try to unlock the custom table
CustomTableLock::~CustomTableLock() {
	try { this->ref.unlockCustomTables(this->type); }
	catch(...) {}
}

}
