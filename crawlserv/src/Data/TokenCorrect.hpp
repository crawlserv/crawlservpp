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
 * TokenCorrect.hpp
 *
 * Corrects tokens using an aspell dictionary.
 *
 *  Created on: Feb 28, 2021
 *      Author: ans
 */

#ifndef DATA_TOKENCORRECT_HPP_
#define DATA_TOKENCORRECT_HPP_

#include "../Wrapper/AspellChecker.hpp"
#include "../Wrapper/AspellConfig.hpp"

#include <string>	// std::string
#include <vector>	// std::vector

namespace crawlservpp::Data {

	/*
	 * DECLARATION
	 */

	//! Corrects tokens using an @c aspell dictionary.
	class TokenCorrect {

		// for convenience
		using AspellChecker = Wrapper::AspellChecker;
		using AspellConfig = Wrapper::AspellConfig;

	public:
		///@name Construction and Destruction
		///@{

		explicit TokenCorrect(const std::string& language);

		//! Default destructor.
		virtual ~TokenCorrect() = default;

		///@}
		///@name Token correction
		///@{

		void correct(std::string& token);

		///@}
		/**@name Copy and Move
		 * The class is not copyable, only moveable.
		 */
		///@{

		//! Deleted copy constructor.
		TokenCorrect(TokenCorrect&) = delete;

		//! Default move constructor.
		TokenCorrect(TokenCorrect&&) = default;

		//! Deleted copy assignment operator.
		TokenCorrect& operator=(TokenCorrect&) = delete;

		//! Default move assignment operator.
		TokenCorrect& operator=(TokenCorrect&&) = default;

		///@}

	private:
		// configuration and spell checker
		AspellConfig config;
		AspellChecker checker;
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION AND DESTRUCTION
	 */

	//! Constructor setting options for the token correction.
	/*!
	 * \param language The language (i.e.
	 *   @c aspell dictionary) to be used
	 *   for token correction.
	 */
	inline TokenCorrect::TokenCorrect(const std::string& language) {
		this->config.setOption("encoding", "utf-8"); /* UTF-8 encoding */
		this->config.setOption("size", "10"); /* small list size (only first word needed) */

		if(!language.empty()) {
			this->config.setOption("lang", language); /* language, if not default */
		}

		this->checker.create(this->config);
	}

	/*
	 * TOKEN CORRECTION
	 */

	//! Corrects a token, if @c aspell offers at least one correction proposal.
	/*!
	 * \param token Reference to a string
	 *   containing the token to be corrected, if
	 *   necessary.
	 */
	inline void TokenCorrect::correct(std::string& token) {
		std::vector<std::string> suggested;

		if(this->checker.check(token, suggested) || suggested.empty()) {
			return;
		}

		token = suggested.front();
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_TOKENCORRECT_HPP_ */
