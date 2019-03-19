/*
 * DatabaseLock.hpp
 *
 * Template class for safe in-scope database locks.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_DATABASELOCK_HPP_
#define WRAPPER_DATABASELOCK_HPP_

namespace crawlservpp::Wrapper {

	template<class DB> // DB needs to be a Database connection class w/ .lockDatabase(...) and .unlockDatabase()
	class DatabaseLock {

	public:
		// constructor: lock the database
		DatabaseLock(DB& db) : ref(db), locked(false) {
			this->ref.lockDatabase();

			this->locked = true;
		}

		// destructor: unlock the database
		virtual ~DatabaseLock() {
			if(this->locked)
				this->ref.unlockDatabase();
		}

		// not moveable, not copyable
		DatabaseLock(DatabaseLock&) = delete;
		DatabaseLock(DatabaseLock&&) = delete;
		DatabaseLock& operator=(DatabaseLock&) = delete;
		DatabaseLock& operator=(DatabaseLock&&) = delete;

	private:
		// internal reference to a database connection
		DB& ref;

		// internal lock state
		bool locked;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_DATABASELOCK_HPP_ */
