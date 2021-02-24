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
 * TokenRemover.hpp
 *
 * Removes tokens found in a (pre-loaded) dictionary.
 *
 *  Created on: Sep 1, 2020
 *      Author: ans
 */

#ifndef DATA_TOKENREMOVER_HPP_
#define DATA_TOKENREMOVER_HPP_

#include "Dictionary.hpp"

#include "../Helper/FileSystem.hpp"
#include "../Helper/Memory.hpp"

#include <cstddef>			// std::size_t
#include <cstdint>			// std::uint64_t
#include <fstream>			// std::ifstream
#include <string>			// std::getline, std::stoul, std::string
#include <unordered_map>	// std::unordered_map
#include <unordered_set>	// std::unordered_set
#include <utility>			// std::move

namespace crawlservpp::Data {

	/*
	 * DECLARATION
	 */

	//! Token remover.
	class TokenRemover {

		// for convenience
		using DictionaryIterator = std::unordered_map<std::string, std::unordered_set<std::string>>::const_iterator;

	public:
		///@name Token removal
		///@{

		void remove(std::string& token, const std::string& dictionary);

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}

	private:
		// dictionaries
		std::unordered_map<std::string, std::unordered_set<std::string>> dictionaries;

		// internal helper functions
		DictionaryIterator build(const std::string& dictionary);
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * TOKEN REMOVAL
	 */

	//! Removes a token if found in the dictionary.
	/*!
	 * \param token Reference to a string
	 *   containing the token to be removed, if
	 *   necessary.
	 * \param dictionary View of a string containing
	 *   the name of the dictionary to be used
	 *   for checking whether to remove the token.
	 */
	inline void TokenRemover::remove(std::string& token, const std::string& dictionary) {
		// get dictionary or build it if necessary
		DictionaryIterator dict{
			this->dictionaries.find(dictionary)
		};

		if(dict == this->dictionaries.end()) {
			dict = build(dictionary);
		}

		// get length of token and look it up in dictionary
		auto tokenLength{token.find(' ')};

		if(tokenLength > token.length()) {
			tokenLength = token.length();
		}

		const auto entry{
			dict->second.find(token.substr(0, tokenLength))
		};

		if(entry == dict->second.end()) {
			/* token not in dictionary */
			return;
		}

		// remove token
		Helper::Memory::free(token);
	}

	/*
	 * CLEANUP
	 */

	//! Clears the lemmatizer, freeing the memory used by all dictionaries.
	inline void TokenRemover::clear() {
		std::unordered_map<std::string, std::unordered_set<std::string>>().swap(this->dictionaries);
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// build the dictionary
	inline TokenRemover::DictionaryIterator TokenRemover::build(const std::string& dictionary) {
		std::unordered_set<std::string> newDictionary;

		// read dictionary file line by line
		std::string dictFileName{dictDir};

		dictFileName.push_back(Helper::FileSystem::getPathSeparator());

		dictFileName += dictionary;

		std::ifstream in(dictFileName.c_str());
		std::string line;

		while(std::getline(in, line)) {
			if(line.empty()) {
				continue;
			}

			const auto end{
				line.find('\t')
			};

			if(end == 0) {
				continue;
			}

			newDictionary.emplace(line.substr(0, end));
		}

		// move dictionary to the set and return (constant) iterator
		return this->dictionaries.emplace(dictionary, std::move(newDictionary)).first;
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_TOKENREMOVER_HPP_ */
