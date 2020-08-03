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
 * English.hpp
 *
 * Simple English stemmer based on porter2_stemmer by Sean Massung.
 *
 *  Original: https://github.com/smassung/porter2_stemmer
 *
 *  Implementation of
 *   http://snowball.tartarus.org/algorithms/english/stemmer.html
 *
 *  Created on: Aug 3, 2020
 *      Author: ans
 */

#ifndef DATA_STEMMER_ENGLISH_HPP_
#define DATA_STEMMER_ENGLISH_HPP_

#include "../../../_extern/porter2_stemmer/porter2_stemmer.h"

#include <algorithm>	// std::remove_if, std::transform
#include <cctype>		// ::tolower
#include <string>		// std::string

//! Namespace for linguistic stemmers.
namespace crawlservpp::Data::Stemmer {

	/*
	 * CONSTANTS
	 */

	///@{
	///@}

	/*
	 * DECLARATION
	 */

	void stemEnglish(std::string& token);

	/*
	 * IMPLEMENTATION
	 */

	//! Stems a token in English
	/*!
	 * \param token The token to be stemmed
	 *   in situ.
	 */
	inline void stemEnglish(std::string& token) {
		Porter2Stemmer::trim(token);
		Porter2Stemmer::stem(token);
	}

} /* namespace crawlservpp::Data::Stemmer */

#endif /* DATA_STEMMER_ENGLISH_HPP_ */
