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
#include <utility>		// std::move

namespace crawlservpp::Wrapper {

	//! Template class for safe in-scope database locks.
	/*!
	 * Locks the database for specific actions on
	 *  construction if not locked already, unlocks it on
	 *  destruction if necessary.
	 *
	 * @tparam DB Database connection to be used for the
	 *   lock. Must implement the member functions
	 *   tryDatabaseLock() and removeDatabaseLock(), i.e.
	 *   inherit from Main::Database.
	 */
	template<class DB>
	class DatabaseTryLock {
	public:

		///@name Construction and Destruction
		///@{

		//! Constructor locking the database if it is not already locked.
		/*!
		 * If no other lock with the same name is active,
		 *  the database will be locked by calling an
		 *  implementation of
		 *  Main::Database::tryDatabaseLock.
		 *
		 * Otherwise, the constructor will not lock the
		 *  database. Use isActive() to check whether it
		 *  was locked or not.
		 *
		 * \param db The database connection to use.
		 *
		 * \param lockName The name of the lock. If there
		 *   is another DatabaseLock active with the same
		 *    name, the constructor will wait until it is
		 *    destructed.
		 */
		DatabaseTryLock(DB& db, const std::string& lockName)
				: ref(db), name(lockName), locked(false) {
			this->locked = this->ref.tryDatabaseLock(this->name);
		}

		//! Destructor unlocking the database if necessary.
		/*!
		 * If the constructor was successful in locking
		 *  the database, the lock will be removed by
		 *  calling an implementation of
		 *  Main::Database::removeDatabaseLock.
		 */
		virtual ~DatabaseTryLock() {
			if(this->locked) {
				this->ref.removeDatabaseLock(this->name);

				this->locked = false;
			}
		}

		///@}
		///@name Getter
		///@{

		//! Checks the status of the database lock.
		/*!
		 * \returns True if the lock is active. False if
		 *   not.
		 */
		[[nodiscard]] bool isActive() const noexcept {
			return this->locked;
		}

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		DatabaseTryLock(DatabaseTryLock&) = delete;

		//! Deleted copy assignment operator.
		DatabaseTryLock& operator=(DatabaseTryLock&) = delete;

		//! Move constructor.
		/*!
		 * Moves the database lock from the specified
		 *  location into this instance of the class.
		 *
		 * \note The other lock will be invalidated by
		 *   this move.
		 *
		 * \param other The database lock to move from.
		 */
		DatabaseTryLock(DatabaseTryLock&& other) noexcept
				: ref(other.ref), name(other.name), locked(other.locked) {
			other.locked = false;
		}

		//! Move assignment operator.
		/*!
		 * Moves the database lock from the specified
		 *  location into this instance of the class.
		 *
		 * If the current instance already holds a
		 *  database lock, it will be released before
		 *  moving in the other lock.
		 *
		 * \note The other lock will be invalidated by
		 *   this move.
		 *
		 * \note Nothing will be done if used on itself.
		 *
		 * \param other The database lock to move from.
		 *
		 * \returns A reference to the instance containing
		 *   the database lock after moving (i.e. *this).
		 */
		DatabaseTryLock& operator=(DatabaseTryLock&& other) noexcept {
			if(&other != this) {
				if(this->locked) {
					this->ref.removeDatabaseLock(this->name);

					this->locked = false;
				}

				this->ref = std::move(other.ref);
				this->name = std::move(other.name);
				this->locked = other.locked;
			}

			return *this;
		}

		///@}

	private:
		// internal reference to a database connection
		DB& ref;

		// internal lock state
		std::string name;
		bool locked;
	};

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_DATABASETRYLOCK_HPP_ */
