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
 * Lemmatizer.hpp
 *
 * Multilingual lemmatizer.
 *
 *  Created on: Sep 1, 2020
 *      Author: ans
 */

#ifndef DATA_LEMMATIZER_HPP_
#define DATA_LEMMATIZER_HPP_

#include "Dictionary.hpp"

#include "../Helper/FileSystem.hpp"
#include "../Helper/Memory.hpp"
#include "../Helper/Strings.hpp"

#include <cstddef>			// std::size_t
#include <cstdint>			// std::uint64_t
#include <fstream>			// std::ifstream
#include <string>			// std::getline, std::stoul, std::string
#include <unordered_map>	// std::unordered_map
#include <utility>			// std::move
#include <vector>			// std::vector

namespace crawlservpp::Data {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! Column containing the lemma in a dictionary file.
	/*!
	 * Column numbers start at zero.
	 *  Columns are separated by tabulators
	 */
	inline constexpr auto colLemma{1};

	//! Column containing the tag in a dictionary file.
	/*!
	 * Column numbers start at zero.
	 *  Columns are separated by tabulators
	 */
	inline constexpr auto colTag{2};

	//! Column containing the number of occurences in a dictionary file.
	/*!
	 * Column numbers start at zero.
	 *  Columns are separated by tabulators
	 */
	inline constexpr auto colCount{3};

	///@}

	/*
	 * DECLARATION
	 */

	//! Lemmatizer.
	class Lemmatizer {
		// property of a dictionary entry (each entry, i.e. word, may have multiple such properties)
		struct DictionaryProperty {
			std::string tag;
			std::string lemma;
			std::uint64_t count{};
		};

		// for convenience
		using Dictionary = std::unordered_map<std::string, std::vector<DictionaryProperty>>;
		using DictionaryIterator = std::unordered_map<std::string, Dictionary>::const_iterator;

	public:
		///@name Lemmatization
		///@{

		void lemmatize(std::string& word, const std::string& dictionary);

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}

	private:
		// dictionaries
		std::unordered_map<std::string, Dictionary> dictionaries;

		// internal helper functions
		DictionaryIterator build(const std::string& dictionary);
		static std::size_t countEqualChars(
				const std::string& string,
				std::size_t pos,
				const std::string& needle
		);
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * LEMMATIZATION
	 */

	//! Lemmatizes a word.
	/*!
	 * \param word Reference to a string
	 *   containing the word to be lemmazized.
	 * \param dictionary View of a string containing
	 *   the name of the dictionary to be used
	 *   for lemmatizing the word.
	 */
	inline void Lemmatizer::lemmatize(std::string& word, const std::string& dictionary) {
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

		if(entry == dict->second.end() || entry->second.empty()) {
			// word not in dictionary
			return;
		}

		if(entry->second.size() == 1) {
			// exactly one entry
			word = entry->second[0].lemma;

			return;
		}

		// compare tags
		std::vector<std::size_t> equalChars;
		std::size_t max{};

		for(const auto& property : entry->second) {
			const std::size_t count{
				countEqualChars(word, wordLength + 1, property.tag)
			};

			equalChars.push_back(count);

			if(count > max) {
				max = count;
			}
		}

		// compare occurences
		max = 0;

		for(std::size_t i = 0; i < entry->second.size(); ++i) {
			if(
					equalChars[i] == max
					&& entry->second[i].count > max
			) {
				max = entry->second[i].count;
			}
		}

		// return lemma with most equal characters in tag and most occurences
		for(const auto& property : entry->second) {
			if(property.count == max) {
				word = property.lemma;

				return;
			}
		}
	}

	/*
	 * CLEANUP
	 */

	//! Clears the lemmatizer, freeing the memory used by all dictionaries.
	inline void Lemmatizer::clear() {
		Helper::Memory::free(this->dictionaries);
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// build the dictionary for a specific language
	inline Lemmatizer::DictionaryIterator Lemmatizer::build(const std::string& dictionary) {
		Dictionary newDictionary;

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

			const auto columns{
				Helper::Strings::split(line, '\t')
			};

			if(columns.empty()) {
				continue;
			}

			std::vector<DictionaryProperty> properties(1);

			if(columns.size() > colLemma) {
				properties.back().lemma = columns[colLemma];
			}

			if(columns.size() > colTag) {
				properties.back().tag = columns[colTag];
			}

			if(columns.size() > colCount) {
				properties.back().count = std::stoul(columns[colCount]);
			}

			const auto added{
				newDictionary.emplace(columns[0], properties)
			};

			if(!added.second) {
				added.first->second.emplace_back(std::move(properties.back()));
			}
		}

		// move dictionary to the set and return (constant) iterator
		return this->dictionaries.emplace(dictionary, std::move(newDictionary)).first;
	}

	// count number of equal characters (from specific position of string and from beginning of needle)
	inline std::size_t Lemmatizer::countEqualChars(
			const std::string& string,
			std::size_t pos,
			const std::string& needle
	) {
		for(std::size_t n{}; n < needle.length(); ++n) {
			if(
					pos + n >= string.length()
					|| string[pos + n] != needle[n]
			) {
				return n;
			}
		}

		return needle.length();
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_LEMMATIZER_HPP_ */
