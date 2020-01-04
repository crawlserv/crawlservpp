/*
 *
 * ---
 *
 *  Copyright (C) 2019 Anselm Schmidt (ans[ät]ohai.su)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version in addition to the terms of any
 *  licences already herein identified.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * ---
 *
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
		using IsRunningCallback = std::function<bool()>;

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
