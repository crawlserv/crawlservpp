/*
 * TableLock.hpp
 *
 * Template class for safe in-scope table locks.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_TABLELOCK_HPP_
#define WRAPPER_TABLELOCK_HPP_

#include "../Struct/TableLockProperties.hpp"

#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace crawlservpp::Wrapper {

	template<class DB> // DB needs to be a Database connection class w/ .lockTable(...), .lockTables(...) and .unlockTables()
	class TableLock {
		// for convenience
		typedef Struct::TableLockProperties TableLockProperties;

	public:
		// constructor A: lock one table (and its aliases for reading)
		TableLock(DB& db, const TableLockProperties& lockProperties) : ref(db), locked(false) {
			this->ref.lockTable(lockProperties);

			this->locked = true;
		}

		// constructor B: lock two tables (and their aliases for reading)
		TableLock(DB& db, const TableLockProperties& lockProperties1, const TableLockProperties& lockProperties2)
				: ref(db), locked(false) {
			this->ref.lockTables(lockProperties1, lockProperties2);

			this->locked = true;
		}

		// constructor C: lock multiple tables (and their aliases for reading)
		TableLock(DB& db, const std::vector<TableLockProperties>& lockProperties) : ref(db), locked(false) {
			this->ref.lockTables(lockProperties);

			this->locked = true;
		}

		// destructor: try to unlock the table(s)
		virtual ~TableLock() {
			if(this->locked) {
				try {
					this->ref.unlockTables();
				}
				catch(const std::exception& e) {
					std::cout << std::endl << "WARNING: Could not unlock table(s) - " << e.what() << std::flush;
				}
				catch(...) {
					std::cout << std::endl << "WARNING: Could not unlock table(s)!" << std::flush;
				}
			}
		}

		// not moveable, not copyable
		TableLock(TableLock&) = delete;
		TableLock(TableLock&&) = delete;
		TableLock& operator=(TableLock&) = delete;
		TableLock& operator=(TableLock&&) = delete;

	private:
		// internal reference to a database connection
		DB& ref;

		// internal lock state
		bool locked;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_TABLELOCK_HPP_ */
