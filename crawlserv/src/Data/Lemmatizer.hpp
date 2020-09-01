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
 * Lemmatizer.hpp
 *
 * Namespace for the multilingual lemmatizer.
 *
 *  Created on: Sep 1, 2020
 *      Author: ans
 */

#ifndef DATA_LEMMATIZER_HPP_
#define DATA_LEMMATIZER_HPP_

#include "../Helper/FileSystem.hpp"
#include "../Helper/Strings.hpp"

#include <cstdint>			// std::uint64_t
#include <fstream>			// std::ifstream
#include <string>			// std::getline, std::stoul, std::string
#include <string_view>		// std::string_view, std::string_view_literals
#include <unordered_map>	// std::unordered_map
#include <utility>			// std::move
#include <vector>			// std::vector

//! Namespace for multilingual lemmatization.
namespace crawlservpp::Data::Lemmatizer {

	using std::string_view_literals::operator""sv;

	///@name Constants
	///@{

	//! Directory for dictionaries.
	inline constexpr auto dictDir{"dict"sv};

	//! Column containing the tag in a dictionary file.
	/*!
	 * Column numbers start at zero.
	 *  Columns are separated by tabulators
	 */
	inline constexpr auto colTag{1};

	//! Column containing the lemma in a dictionary file.
	/*!
	 * Column numbers start at zero.
	 *  Columns are separated by tabulators
	 */
	inline constexpr auto colLemma{2};

	//! Column containing the number of occurences in a dictionary file.
	/*!
	 * Column numbers start at zero.
	 *  Columns are separated by tabulators
	 */
	inline constexpr auto colCount{3};

	///@}

	//! Property of a dictionary entry.
	/*!
	 * Each entry, i.e. word, may have multiple
	 *  such properties.
	 */
	struct DictionaryProperty {
		//! POS (part-of-speech) tag of the word.
		std::string tag;

		//! Lemma of the word.
		std::string lemma;

		//! Number of occurences of the word with the specified tag in the original data.
		std::uint64_t count{0};
	};

	using Dictionary = std::unordered_map<std::string, std::vector<DictionaryProperty>>;
	using DictionaryIterator = std::unordered_map<std::string, Dictionary>::const_iterator;

	//! Dictionaries.
	inline std::unordered_map<std::string, Dictionary> dictionaries;

	//! Builds the dictionary for a specific language.
	/*!
	 * \param language View of a string
	 *   containing the language of the
	 *   dictionary to be built.
	 *
	 * \returns Iterator of the newly added dictionary.
	 */
	inline DictionaryIterator build(const std::string& language) {
		Dictionary newDictionary;

		// read dictionary file line by line
		std::string dictFileName{dictDir};

		dictFileName.push_back(Helper::FileSystem::getPathSeparator());

		dictFileName += language;

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

			if(columns.size() > colTag) {
				properties.back().tag = columns[colTag];
			}

			if(columns.size() > colLemma) {
				properties.back().lemma = columns[colLemma];
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
		return dictionaries.emplace(language, std::move(newDictionary)).first;
	}

	//! Counts the number of equal characters.
	/*!
	 * \param string The first string.
	 *   Characters will be compared from the
	 *   specified position in this string.
	 * \param pos The position from which
	 *   characters will be compared in the
	 *   first string
	 * \param needle The second string.
	 *   Characters will be compared from
	 *   the beginning of this string.
	 *
	 * \returns The number of equal characters
	 */
	inline std::size_t countEqualChars(
			const std::string& string,
			std::size_t pos,
			const std::string& needle
	) {
		for(std::size_t n{0}; n < needle.length(); ++n) {
			if(
					pos + n >= string.length()
					|| string[pos + n] != needle[n]
			) {
				return n;
			}
		}

		return needle.length();
	}

	//! Lemmatizes a word.
	/*!
	 * \param word Reference to a string
	 *   containing the word to be lemmazized.
	 * \param language View of a string containing
	 *   the language of the dictionary to be used
	 *   for lemmatizing the word.
	 */
	inline void lemmatize(std::string& word, const std::string& language) {
		// get dictionary or build it if necessary
		DictionaryIterator dict{
			dictionaries.find(language)
		};

		if(dict == dictionaries.end()) {
			dict = build(language);
		}

		// get length of word
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
		std::size_t max{0};

		for(const auto property : entry->second) {
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
		for(const auto property : entry->second) {
			if(property.count == max) {
				word = property.lemma;

				return;
			}
		}
	}

}

#endif /* DATA_LEMMATIZER_HPP_ */
