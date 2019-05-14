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

#include <functional>	// std::function
#include <string>		// std::string

namespace crawlservpp::Wrapper {

	template<class DB> // DB needs to be a Database connection class w/ .lockDatabase(...) and .unlockDatabase()
	class DatabaseLock {
		// for convenience
		typedef std::function<bool()> IsRunningCallback;

	public:
		// constructor: lock the database
		DatabaseLock(DB& db, const std::string& lockName, IsRunningCallback isRunningCallback)
				: ref(db), name(lockName), locked(false) {
			this->ref.addDatabaseLock(this->name, isRunningCallback);

			this->locked = true;
		}

		// destructor: unlock the database
		virtual ~DatabaseLock() {
			if(this->locked)
				this->ref.removeDatabaseLock(this->name);
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
		std::string name;
		bool locked;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_DATABASELOCK_HPP_ */
