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
#include <utility>		// std::move

namespace crawlservpp::Wrapper {

	//! Template class for safe in-scope database locks.
	/*!
	 * Locks the database for specific actions on construction,
	 *  unlocks it on destruction.
	 *
	 * @tparam DB Database connection to be used for the lock.
	 *   Must implement the member functions addDatabaseLock(...)
	 *   and removeDatabaseLock(), i.e. inherit from Main::Database.
	 */
	template<class DB>
	class DatabaseLock {
		// for convenience
		using IsRunningCallback = std::function<bool()>;

	public:

		///@name Construction and Destruction
		///@{

		//! Constructor locking the database after waiting for another lock if necessary.
		/*!
		 * If no other lock with the same name is active, the database will be locked
		 *  by calling an implementation of Main::Database::addDatabaseLock.
		 *
		 * Otherwise, the constructor will wait for the other lock to be released.
		 *
		 * @param db The database connection to use.
		 *
		 * @param lockName The name of the lock. If there is another DatabaseLock active
		 *   with the same name, the constructor will wait until it is destructed.
		 *
		 * @param isRunningCallback A function that returns whether both the current
		 *   thread and program are still running.
		 */
		DatabaseLock(DB& db, const std::string& lockName, IsRunningCallback isRunningCallback)
				: ref(db), name(lockName), locked(false) {
			this->ref.addDatabaseLock(this->name, isRunningCallback);

			this->locked = true;
		}

		//! Destructor unlocking the database.
		/*!
		 * If the constructor was successful in locking the database, the lock
		 *  will be removed by calling an implementation of
		 *  Main::Database::removeDatabaseLock.
		 */
		virtual ~DatabaseLock() {
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
		 * \returns True if the lock is active. False if not.
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
		DatabaseLock(DatabaseLock&) = delete;

		//! Deleted copy assignment operator.
		DatabaseLock& operator=(DatabaseLock&) = delete;

		//! Move constructor.
		/*!
		 * Moves the database lock from the specified location
		 *  into this instance of the class.
		 *
		 * \note The other lock will be invalidated by this move.
		 *
		 * \param other The database lock to move from.
		 */
		DatabaseLock(DatabaseLock&& other) noexcept
				: ref(other.ref), name(other.name), locked(other.locked) {
			other.locked = false;
		}

		//! Move assignment operator.
		/*!
		 * Moves the database lock from the specified location
		 *  into this instance of the class.
		 *
		 * If the current instance already holds a database lock,
		 *  it will be released before moving in the other lock.
		 *
		 * \note The other lock will be invalidated by this move.
		 *
		 * \note Nothing will be done if used on itself.
		 *
		 * \param other The database lock to move from.
		 *
		 * \returns A reference to the instance containing
		 *   the database lock after moving (i.e. *this).
		 */
		DatabaseLock& operator=(DatabaseLock&& other) noexcept {
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

#endif /* WRAPPER_DATABASELOCK_HPP_ */
