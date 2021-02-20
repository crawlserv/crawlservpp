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
 * WordRemover.hpp
 *
 * Removes words found in a (pre-loaded) dictionary.
 *
 *  Created on: Sep 1, 2020
 *      Author: ans
 */

#ifndef DATA_WORDREMOVER_HPP_
#define DATA_WORDREMOVER_HPP_

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

	//! Word remover.
	class WordRemover {

		// for convenience
		using DictionaryIterator = std::unordered_map<std::string, std::unordered_set<std::string>>::const_iterator;

	public:
		///@name Word removal
		///@{

		void remove(std::string& word, const std::string& dictionary);

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
	 * WORD REMOVAL
	 */

	//! Removes a word if found in the dictionary.
	/*!
	 * \param word Reference to a string
	 *   containing the word to be removed, if
	 *   necessary.
	 * \param dictionary View of a string containing
	 *   the name of the dictionary to be used
	 *   for checking whether to remove the word.
	 */
	inline void WordRemover::remove(std::string& word, const std::string& dictionary) {
		// get dictionary or build it if necessary
		DictionaryIterator dict{
			this->dictionaries.find(dictionary)
		};

		if(dict == this->dictionaries.end()) {
			dict = build(dictionary);
		}

		// get length of word and look it up in dictionary
		std::size_t wordLength{word.find(' ')};

		if(wordLength > word.length()) {
			wordLength = word.length();
		}

		const auto entry{
			dict->second.find(word.substr(0, wordLength))
		};

		if(entry == dict->second.end()) {
			// word not in dictionary
			return;
		}

		// remove word
		Helper::Memory::free(word);
	}

	/*
	 * CLEANUP
	 */

	//! Clears the lemmatizer, freeing the memory used by all dictionaries.
	inline void WordRemover::clear() {
		std::unordered_map<std::string, std::unordered_set<std::string>>().swap(this->dictionaries);
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// build the dictionary
	inline WordRemover::DictionaryIterator WordRemover::build(const std::string& dictionary) {
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

#endif /* DATA_WORDREMOVER_HPP_ */
