/*
 *
 * ---
 *
 *  Copyright (C) 2021 Anselm Schmidt (ans[ät]ohai.su)
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
 * AspellChecker.hpp
 *
 * RAII wrapper for pointer to aspell spell checkers.
 *  Does NOT have ownership of the pointer, but takes care of its deletion!
 *
 *  Created on: Feb 28, 2021
 *      Author: ans
 */

#ifndef WRAPPER_ASPELLCCHECKER_HPP_
#define WRAPPER_ASPELLCHECKER_HPP_

#include "AspellConfig.hpp"
#include "AspellList.hpp"

#include "../Main/Exception.hpp"

#include <aspell.h>

#include <string>	// std::string, std::to_string
#include <vector>	// std::vector

namespace crawlservpp::Wrapper {

	/*
	 * DECLARATION
	 */

	//! RAII wrapper for @c aspell spell checkers.
	/*!
	 * Creates the spell checker on
	 *  construction and deletes it on
	 *  destruction, if still necessary,
	 *  avoiding memory leaks.
	 *
	 * \note The class does not own the
	 *   underlying pointer, but takes care
	 *   of its deletion via API call.
	 */
	class AspellChecker {
	public:
		///@name Construction and Destruction
		///@{

		//! Default constructor.
		AspellChecker() = default;

		virtual ~AspellChecker();

		///@}
		///@name Getters
		///@{

		[[nodiscard]] AspellSpeller * get();
		[[nodiscard]] const AspellSpeller * getc() const;
		[[nodiscard]] bool valid() const;

		///@}
		///@name Creation
		///@{

		void create(AspellConfig& configuration);

		///@}
		///@name Spell Check
		///@{

		[[nodiscard]] bool check(
				const std::string& token,
				std::vector<std::string>& suggestionsTo
		);

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		AspellChecker(AspellChecker&) = delete;

		//! Deleted copy assignment operator.
		AspellChecker& operator=(AspellChecker&) = delete;

		AspellChecker(AspellChecker&& other) noexcept;
		AspellChecker& operator=(AspellChecker&& other) noexcept;

		///@}

		//! Class for @c aspell spell checker-specific exceptions.
		MAIN_EXCEPTION_CLASS();

	private:
		// underlying pointer to the spell checker
		AspellSpeller * ptr{nullptr};
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Destructor deleting the spell checker, if necessary.
	/*!
	 * \sa clear
	 */
	inline AspellChecker::~AspellChecker() {
		this->clear();
	}

	/*
	 * GETTERS
	 */

	//! Gets a pointer to the underlying spell checker.
	/*!
	 * \returns A pointer to the underlying
	 *   spell checker or @c nullptr if the
	 *   spell checker is not valid.
	 *
	 * \sa getc, valid
	 */
	inline AspellSpeller * AspellChecker::get() {
		return this->ptr;
	}

	//! Gets a constant pointer to the underlying spell checker.
	/*!
	 * \returns A constant pointer to the
	 *   underlying spell checker or
	 *   @c nullptr if the spell checker is
	 *   not valid.
	 *
	 * \sa get, valid
	 */
	inline const AspellSpeller * AspellChecker::getc() const {
		return this->ptr;
	}

	//! Gets whether the spell checker is valid.
	/*!
	 * \returns True, if the spell checker
	 *   is valid. False otherwise.
	 */
	inline bool AspellChecker::valid() const {
		return this->ptr != nullptr;
	}

	/*
	 * CREATION
	 */

	//! Creates the spell checker.
	/*!
	 * Delets the old one, if necessary.
	 *
	 * \param configuration The @c aspell
	 *   configuration used by the
	 *   @c aspell spell checker.
	 *
	 * \throws AspellChecker::Exception if
	 *   the configuration is not valid,
	 *   or the spell checker could not be
	 *   created.
	 */
	inline void AspellChecker::create(AspellConfig& configuration) {
		this->clear();

		if(!configuration.valid()) {
			throw Exception(
					"AspellChecker::create():"
					" The configuration is not valid"
			);
		}

		AspellCanHaveError * possibleError{
			new_aspell_speller(configuration.get())
		};
		const auto errorNumber{
			aspell_error_number(possibleError)
		};

		if(errorNumber != 0) {
			throw Exception(
					"AspellChecker::create():"
					" Aspell error #"
					+ std::to_string(errorNumber)
					+ ": "
					+ aspell_error_message(possibleError)
			);
		}

		this->ptr = to_aspell_speller(possibleError);
	}

	/*
	 * SPELL CHECK
	 */

	//! Checks whether a token is correctly spelled.
	/*!
	 * \param token Constant reference
	 *   to a string containing the token
	 *   to be checked.
	 * \param suggestionsTo Reference to a
	 *   vector to which correction
	 *   suggestions for the token will be
	 *   appended, if the token is not
	 *   correctly spelled, according to
	 *   @c aspell.
	 *
	 * \returns True, if the token is
	 *   correctly spellec according to
	 *   @c aspell and the language set in
	 *   the current configuration.
	 *   Returns false otherwise.
	 *
	 * \throws AspellChecker::Exception if
	 *   the spell checker is not valid, or
	 *   an error occured while checking
	 *   the token.
	 */
	inline bool AspellChecker::check(
			const std::string& token,
			std::vector<std::string>& suggestionsTo
	) {
		if(this->ptr == nullptr) {
			throw Exception(
					"AspellChecker::check():"
					" The spell checker is not valid"
			);
		}

		const auto result{
			aspell_speller_check(this->ptr, token.c_str(), token.size())
		};

		if(result < 0) {
			throw Exception(
					"AspellChecker::check():"
					" Aspell error #"
					+ std::to_string(aspell_speller_error_number(this->ptr))
					+ ": "
					+ aspell_speller_error_message(this->ptr)
			);
		}

		if(result > 0) {
			return true;
		}

		AspellList suggestions(aspell_speller_suggest(this->ptr, token.c_str(), token.size()));
		std::string suggestion;

		while(suggestions.next(suggestion)) {
			suggestionsTo.emplace_back(suggestion);
		}

		return false;
	}

	/*
	 * CLEANUP
	 */

	//! Deletes the spell checker, if necessary.
	inline void AspellChecker::clear() {
		if(this->ptr != nullptr) {
			delete_aspell_speller(this->ptr);

			this->ptr = nullptr;
		}
	}

	/*
	 * COPY AND MOVE
	 */

	//! Move constructor.
	/*!
	 * Moves the spell checker from the
	 *  specified location into this
	 *  instance of the class.
	 *
	 * \note The other spell checker will
	 *   be invalidated by this move.
	 *
	 * \param other The spell checker to
	 *   move from.
	 *
	 * \sa valid
	 */
	inline AspellChecker::AspellChecker(AspellChecker&& other) noexcept : ptr(other.ptr) {
		other.ptr = nullptr;
	}

	//! Move assignment operator.
	/*!
	 * Moves the spell checker from the
	 *  specified location into this
	 *  instance of the class.
	 *
	 * \note The other spell checker will
	 *   be invalidated by this move.
	 *
	 * \note Nothing will be done if used on
	 *   itself.
	 *
	 * \param other The spell checker to
	 *   move from.
	 *
	 * \returns A reference to the instance
	 *   containing the spell checker after
	 *   moving (i.e. @c *this).
	 *
	 * \sa valid
	 */
	inline AspellChecker& AspellChecker::operator=(AspellChecker&& other) noexcept {
		if(&other != this) {
			this->clear();

			this->ptr = other.ptr;

			other.ptr = nullptr;
		}

		return *this;
	}

} /* namespace crawlservpp::Wrapper */

#endif /* WRAPPER_ASPELLCONFIG_HPP_ */
