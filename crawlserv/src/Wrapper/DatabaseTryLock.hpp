/*
 *
 * ---
 *
 *  Copyright (C) 2020 Anselm Schmidt (ans[ät]ohai.su)
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
 * DatabaseTryLock.hpp
 *
 * Template class for safe in-scope database try locks.
 *
 *  Created on: Mar 7, 2019
 *      Author: ans
 */

#ifndef WRAPPER_DATABASETRYLOCK_HPP_
#define WRAPPER_DATABASETRYLOCK_HPP_

#include <string>		// std::string

namespace crawlservpp::Wrapper {

	template<class DB> // DB needs to be a Database connection class w/ .tryDatabaseLock(...) and .removeDatabaseLock()
	class DatabaseTryLock {
	public:
		// constructor: lock the database
		DatabaseTryLock(DB& db, const std::string& lockName)
				: ref(db), name(lockName), locked(false) {
			this->locked = this->ref.tryDatabaseLock(this->name);
		}

		// destructor: unlock the database
		virtual ~DatabaseTryLock() {
			if(this->locked)
				this->ref.removeDatabaseLock(this->name);
		}

		// check whether locking was successful
		bool isActive() const {
			return this->locked;
		}

		// default moveable, not copyable
		DatabaseTryLock(DatabaseTryLock&) = delete;
		DatabaseTryLock(DatabaseTryLock&&) = default;
		DatabaseTryLock& operator=(DatabaseTryLock&) = delete;
		DatabaseTryLock& operator=(DatabaseTryLock&&) = default;

	private:
		// internal reference to a database connection
		DB& ref;

		// internal lock state
		std::string name;
		bool locked;
	};

} /* crawlservpp::Wrapper */

#endif /* WRAPPER_DATABASETRYLOCK_HPP_ */
