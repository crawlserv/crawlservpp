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
 * Corpus.hpp
 *
 * Class representing a text corpus with optional article and date maps
 * 	that can be sliced into smaller chunks to fit into the database.
 *
 * 	NOTE:	All input data needs to be sorted by date.
 * 			Text without dates need to be added first.
 *
 *  Created on: Mar 4, 2020
 *      Author: ans
 */

#ifndef DATA_CORPUS_HPP_
#define DATA_CORPUS_HPP_

#include "Lemmatizer.hpp"
#include "Stemmer/English.hpp"
#include "Stemmer/German.hpp"
#include "Tagger.hpp"
#include "WordRemover.hpp"

#include "../Helper/DateTime.hpp"
#include "../Helper/Utf8.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/StatusSetter.hpp"
#include "../Struct/TextMap.hpp"

#include <algorithm>	// std::find_if, std::remove_if
#include <cstddef>		// std::size_t
#include <cstdint>		// std::uint8_t, std::uint16_t
#include <functional>	// std::function
#include <iterator>		// std::distance, std::make_move_iterator
#include <locale>		// std::locale
#include <map>			// std::map
#include <optional>		// std::optional
#include <sstream>		// std::ostringstream
#include <string>		// std::string, std::to_string
#include <string_view>	// std::string_view
#include <utility>		// std::pair
#include <vector>		// std::vector

//! Namespace for different types of data.
namespace crawlservpp::Data {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The length of a date string in the format YYYY-MM-DD.
	inline constexpr auto dateLength{10};

	//! Maximum number of bytes used by one UTF-8-encoded multibyte character.
	inline constexpr std::uint8_t utf8MaxBytes{4};

	//! Every after how many sentences the status is updated when tokenizing a corpus.
	inline constexpr auto tokenizeUpdateEvery{1000};

	//! Minimum length of single UTF-8 code points to remove.
	inline constexpr auto minSingleUtf8CharSize{2};

	//! Maximum length of single UTF-8 code points to remove.
	inline constexpr auto maxSingleUtf8CharSize{4};

	///@}
	///@name Sentence Manipulation
	///@{

	//! Do not manipulate sentences.
	inline constexpr std::uint16_t sentenceManipNone{0};

	//! The POS (position of speech) tagger based on @c Wapiti by Thomas Lavergne.
	inline constexpr std::uint16_t sentenceManipTagger{1};

	///@}
	///@name Word Manipulation
	///@{

	//! Do not manipulate words.
	inline constexpr std::uint16_t wordManipNone{0};

	//! Remove tokens that contain only one UTF-8 codepoint.
	inline constexpr std::uint16_t wordManipRemoveSingleUtf8Chars{1};

	//! The @c porter2_stemmer algorithm for English only, implemented by Sean Massung.
	inline constexpr std::uint16_t wordManipPorter2Stemmer{2};

	//! Simple stemmer for German only, based on @c CISTEM by Leonie Weißweiler and Alexander Fraser.
	inline constexpr std::uint16_t wordManipGermanStemmer{3};

	//! Multilingual lemmatizer.
	inline constexpr std::uint16_t wordManipLemmatizer{4};

	//! Remove single words found in a dictionary.
	inline constexpr std::uint16_t wordManipRemove{5};

	///@}

	/*
	 * DECLARATION
	 */

	//! Class representing a text corpus.
	/*!
	 * The corpus might include article and date maps
	 *  that can be sliced into smaller chunks to fit
	 *  into the database.
	 *
	 * Article and date maps are saved as text map
	 *  structures, referencing a part of the text
	 *  corpus and containing a describing label
	 *  that indicates the ID or date associated
	 *  with the referenced part of the corpus.
	 *
	 * The corpus can be manipulated using a number
	 *  of sentence and word manipulators, resulting
	 *  in a tokenized corpus. If not manipulated,
	 *  it will be stored as continuous text.
	 *
	 * \note For the filtering by date to work,
	 *   all input data needs to be sorted by date,
	 *   while texts without date need to be added
	 *   first.
	 *
	 * \sa Struct::TextMap
	 */
	class Corpus {
		// for convenience
		using StatusSetter = Struct::StatusSetter;
		using TextMap = Struct::TextMap;
		using TextMapEntry = Struct::TextMapEntry;

	public:
		using SentenceFunc = std::function<void(std::vector<std::string>&)>;
		using WordFunc = std::function<void(std::string& token)>;

		using SentenceMap = std::vector<std::pair<std::size_t, std::size_t>>;

		///@name Construction
		///@{

		explicit Corpus(bool consistencyChecks);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] std::string& getCorpus();
		[[nodiscard]] const std::string& getcCorpus() const;
		[[nodiscard]] bool isTokenized() const;
		[[nodiscard]] std::vector<std::string>& getTokens();
		[[nodiscard]] const std::vector<std::string>& getcTokens() const;
		[[nodiscard]] std::size_t getNumTokens() const;
		[[nodiscard]] TextMap& getArticleMap();
		[[nodiscard]] const TextMap& getcArticleMap() const;
		[[nodiscard]] TextMap& getDateMap();
		[[nodiscard]] const TextMap& getcDateMap() const;
		[[nodiscard]] SentenceMap& getSentenceMap();
		[[nodiscard]] const SentenceMap& getcSentenceMap() const;
		[[nodiscard]] std::string get(std::size_t index) const;
		[[nodiscard]] std::string get(const std::string& id) const;
		[[nodiscard]] std::string getDate(const std::string& date) const;
		[[nodiscard]] std::size_t size() const;
		[[nodiscard]] bool empty() const;
		[[nodiscard]] std::string substr(std::size_t from, std::size_t len);

		///@}
		///@name Creation
		///@{

		void create(
				std::vector<std::string>& texts,
				bool deleteInputData
		);
		void create(
				std::vector<std::string>& texts,
				std::vector<std::string>& articleIds,
				std::vector<std::string>& dateTimes,
				bool deleteInputData
		);
		void combineContinuous(
				std::vector<std::string>& chunks,
				std::vector<TextMap>& articleMaps,
				std::vector<TextMap>& dateMaps,
				bool deleteInputData
		);
		void combineTokenized(
				std::vector<std::string>& chunks,
				std::vector<std::size_t>& wordNums,
				std::vector<TextMap>& articleMaps,
				std::vector<TextMap>& dateMaps,
				std::vector<SentenceMap>& sentenceMaps,
				bool deleteInputData
		);

		///@}
		///@name Copying
		///@{

		void copyContinuous(std::string& to) const;
		void copyContinuous(
				std::string& to,
				TextMap& articleMapTo,
				TextMap& dateMapTo
		) const;
		void copyChunksContinuous(
				std::size_t chunkSize,
				std::vector<std::string>& to,
				std::vector<TextMap>& articleMapsTo,
				std::vector<TextMap>& dateMapsTo
		) const;
		void copyChunksTokenized(
				std::size_t chunkSize,
				std::vector<std::string>& to,
				std::vector<std::size_t>& wordNumsTo,
				std::vector<TextMap>& articleMapsTo,
				std::vector<TextMap>& dateMapsTo,
				std::vector<SentenceMap>& sentenceMapsTo
		) const;

		///@}
		///@name Filtering
		///@{

		bool filterByDate(const std::string& from, const std::string& to);

		///@}
		///@name Tokenization
		///@{

		[[nodiscard]] bool tokenize(
				const std::vector<std::uint16_t>& sentenceManipulators,
				const std::vector<std::string>& sentenceModels,
				const std::vector<std::uint16_t>& wordManipulators,
				const std::vector<std::string>& wordModels,
				std::uint64_t freeMemoryEvery,
				StatusSetter& statusSetter
		);
		[[nodiscard]] bool tokenizeCustom(
				const std::optional<SentenceFunc>& callbackSentence,
				const std::optional<WordFunc>& callbackWord,
				std::uint64_t freeMemoryEvery,
				StatusSetter& statusSetter
		);

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}

		//! Class for corpus-specific exceptions.
		MAIN_EXCEPTION_CLASS();

	protected:
		///@name Data
		///@{

		//! Continuous text corpus.
		std::string corpus;

		//! Tokenized text corpus.
		std::vector<std::string> tokens;

		//! Index of articles and their IDs.
		TextMap articleMap;

		//! Index of dates.
		TextMap dateMap;

		//! Index of sentences.
		SentenceMap sentenceMap;

		///@}

	private:
		// internal state
		bool tokenized{false};
		bool checkConsistency{false};
		std::size_t tokenBytes{0};

		// lemmatizer and word remover
		Lemmatizer lemmatizer;
		WordRemover wordRemover;

		// internal helper functions
		[[nodiscard]] bool tokenizeTokenized(
				std::optional<SentenceFunc> callbackSentence,
				std::optional<WordFunc> callbackWord,
				StatusSetter& statusSetter
		);
		[[nodiscard]] bool tokenizeContinuous(
				std::optional<SentenceFunc> callbackSentence,
				std::optional<WordFunc> callbackWord,
				std::uint64_t freeMemoryEvery,
				StatusSetter& statusSetter
		);
		void checkTokenized() const;

		// internal static helper functions
		[[nodiscard]] static std::size_t getValidLengthOfChunk(
				const std::string& source,
				std::size_t pos,
				std::size_t maxLength,
				std::size_t maxChunkSize
		);
		static void checkMap(
				std::string_view name,
				const TextMap& map,
				std::size_t corpusSize,
				bool isTokenized,
				bool isDateMap
		);
		static void checkMap(
				const SentenceMap& map,
				std::size_t corpusSize,
				bool isTokenized
		);

		template<class T> static void reserveCombined(
			const std::vector<T>& vec,
			T& combined
		) {
			combined.reserve(
					std::accumulate(
							vec.cbegin(),
							vec.cend(),
							static_cast<std::size_t>(0),
							[](const auto& a, const auto& b) {
								return a + b.size();
							}
					)
			);
		}
	};

	/*
	 * IMPLEMENTATION
	 */

	/*
	 * CONSTRUCTION
	 */

	//! Constructor setting the internal property.
	/*!
	 * \param consistencyChecks If true, consistency checks
	 *   will be performed on the text corpus.
	 */
	inline Corpus::Corpus(bool consistencyChecks) : checkConsistency(consistencyChecks) {}

	/*
	 * GETTERS
	 */

	//! Gets a reference to the continous text corpus.
	/*!
	 * \returns A reference to the text corpus
	 *   represented by the class.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has already been tokenized.
	 */
	inline std::string& Corpus::getCorpus() {
		if(this->tokenized) {
			throw Exception(
					"Corpus::getCorpus():"
					" The corpus has been tokenized."
			);
		}

		return this->corpus;
	}

	//! Gets a constant reference to the continous text corpus.
	/*!
	 * \returns A constant reference to the text
	 *   corpus represented by the class.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has already been tokenized.
	 */
	inline const std::string& Corpus::getcCorpus() const {
		if(this->tokenized) {
			throw Exception(
					"Corpus::getcCorpus():"
					" The corpus has been tokenized."
			);
		}

		return this->corpus;
	}

	//! Gets whether the corpus has been tokenized.
	/*!
	 * \returns True, if the corpus has been
	 *   tokenized. False, if it has been not
	 *   and is still continuous.
	 */
	inline bool Corpus::isTokenized() const {
		return this->tokenized;
	}

	//! Gets a reference to the tokens in a tokenized text corpus.
	/*!
	 * \returns A reference to the vector
	 *   containing the tokens of the corpus
	 *   represented by the class.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has not been tokenized.
	 */
	inline std::vector<std::string>& Corpus::getTokens() {
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::getTokens():"
					" The corpus has not been tokenized."
			);
		}

		return this->tokens;
	}

	//! Gets a constant reference to the tokens in a tokenized text corpus.
	/*!
	 * \returns A constant reference to the
	 *   vector containing the tokens of the
	 *   corpus represented by the class.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has not been tokenized.
	 */
	inline const std::vector<std::string>& Corpus::getcTokens() const {
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::getcTokens():"
					" The corpus has not been tokenized."
			);
		}

		return this->tokens;
	}

	//! Gets the number of tokens in the corpus.
	/*!
	 * \returns The number of tokens in the
	 *   corpus.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has not been tokenized
	 */
	inline std::size_t Corpus::getNumTokens() const {
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::tokens():"
					" The corpus has not been tokenized."
			);
		}

		return this->tokens.size();
	}

	//! Gets a reference to the article map of the corpus.
	/*!
	 * \returns A reference to the article map
	 *   of the corpus, if specified.
	 */
	inline Struct::TextMap& Corpus::getArticleMap() {
		return this->articleMap;
	}

	//! Gets a constant reference to the article map of the corpus.
	/*!
	 * \returns A constant reference to the
	 *   article map of the corpus, if specified.
	 */
	inline const Struct::TextMap& Corpus::getcArticleMap() const {
		return this->articleMap;
	}

	//! Gets a reference to the date map of the corpus.
	/*!
	 * \returns A reference to the date map
	 *   of the corpus, if specified.
	 */
	inline Struct::TextMap& Corpus::getDateMap() {
		return this->dateMap;
	}

	//! Gets a constant reference to the date map of the corpus.
	/*!
	 * \returns A constant reference to the
	 *   date map of the corpus, if specified.
	 */
	inline const Struct::TextMap& Corpus::getcDateMap() const {
		return this->dateMap;
	}

	//! Gets a reference to the sentence map of the corpus.
	/*!
	 * \note The corpus needs to be tokenized.
	 *
	 * \returns A reference to the sentence
	 *   map of the corpus.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has not been tokenized.
	 */
	inline Corpus::SentenceMap& Corpus::getSentenceMap() {
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::getSentenceMap():"
					" The corpus has not been tokenized"
			);
		}

		return this->sentenceMap;
	}

	//! Gets a constant reference to the sentence map of the corpus.
	/*!
	 * \note The corpus needs to be tokenized.
	 *
	 * \returns A constant reference to the
	 *   sentence map of the corpus.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has not been tokenized.
	 */
	inline const Corpus::SentenceMap& Corpus::getcSentenceMap() const {
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::getcSentenceMap():"
					" The corpus has not been tokenized"
			);
		}

		return this->sentenceMap;
	}

	//! Gets the article with the specified index from a continous text corpus.
	/*!
	 * \param index The index of the article in
	 *   the article map of the text corpus,
	 *   starting with zero.
	 *
	 * \returns A copy of the article inside the
	 *   text corpus with the given index.
	 *
	 * \throws Corpus::Exception if the article
	 *   map of the corpus is empty or the given
	 *   index is out of bounds (larger than the
	 *   article map), or if the corpus has
	 *   already been tokenized.
	 */
	inline std::string Corpus::get(std::size_t index) const {
		if(this->tokenized) {
			throw Exception(
					"Corpus::get():"
					" The corpus has been tokenized"
			);
		}

		if(this->articleMap.empty()) {
			std::ostringstream exceptionStrStr;

			exceptionStrStr.imbue(std::locale(""));

			exceptionStrStr <<	"Corpus::get():"
								" Article #";
			exceptionStrStr <<	index;
			exceptionStrStr <<	" requested,"
								" but the article map is empty";

			throw Exception(exceptionStrStr.str());
		}

		if(index >= this->articleMap.size()) {
			std::ostringstream exceptionStrStr;

			exceptionStrStr.imbue(std::locale(""));

			exceptionStrStr <<	"Corpus::get():"
								" The specified article index"
								" (#";
			exceptionStrStr <<	index;
			exceptionStrStr <<	")"
								" is out of bounds"
								" [#0;#";
			exceptionStrStr <<	this->articleMap.size() - 1;
			exceptionStrStr <<	"]";

			throw Exception(exceptionStrStr.str());
		}

		const auto& article{this->articleMap.at(index)};

		return this->corpus.substr(
				article.pos,
				article.length
		);
	}

	//! Gets the article with the specified ID from a coninuous corpus.
	/*!
	 * \param id A constant reference to a string
	 *   containing the ID of the article to be
	 *   retrieved, as specified in the article map
	 *   of the text corpus.
	 *
	 * \returns A copy of the article with the
	 *   given ID, or an empty string if an
	 *   article with this ID does not exist.
	 *
	 * \throws Corpus::Exception if no ID has
	 *   been specified, i.e. \p id references
	 *   an empty string, or if the corpus has
	 *   already been tokenized.
	 */
	inline std::string Corpus::get(const std::string& id) const {
		// check corpus
		if(this->tokenized) {
			throw Exception(
					"Corpus::get():"
					" The corpus has been tokenized"
			);
		}

		// check argument
		if(id.empty()) {
			throw Exception(
					"Corpus::get():"
					" No ID has been specified"
			);
		}

		const auto& articleEntry{
			std::find_if(
					this->articleMap.cbegin(),
					this->articleMap.cend(),
					[&id](const auto& entry) {
						return entry.value == id;
					}
			)
		};

		if(articleEntry == this->articleMap.cend()) {
			return std::string();
		}

		return this->corpus.substr(
				articleEntry->pos,
				articleEntry->length
		);
	}

	//! Gets all articles at the specified date from a continous text corpus.
	/*!
	 * \param date A constant reference to a string
	 *   containing the date of the article to be
	 *   retrieved, as specified in the date map
	 *   of the text corpus. The string should have
	 *   the format @c YYYY-MM-DD.
	 *
	 * \returns a copy of all concatenated articles at
	 *   the given date, or an empty string if no
	 *   articles have been found at this date.
	 *
	 * \throws Corpus::Exception if the date given
	 *   has an invalid length, or if the corpus has
	 *   already been tokenized.
	 */
	inline std::string Corpus::getDate(const std::string& date) const {
		// check corpus
		if(this->tokenized) {
			throw Exception(
					"Corpus::getDate():"
					" The corpus has been tokenized."
			);
		}

		// check argument
		if(date.length() != dateLength) {
			std::ostringstream exceptionStrStr;

			exceptionStrStr <<	"Corpus::getDate():"
								" Invalid length of date"
								" (";
			exceptionStrStr <<	date.length();
			exceptionStrStr <<	" instead of ";
			exceptionStrStr <<	dateLength;
			exceptionStrStr <<	")";

			throw Exception(exceptionStrStr.str());
		}

		const auto& dateEntry{
			std::find_if(
					this->dateMap.cbegin(),
					this->dateMap.cend(),
					[&date](const auto& entry) {
						return entry.value == date;
					}
			)
		};

		if(dateEntry == this->dateMap.cend()) {
			return std::string();
		}

		return this->corpus.substr(
				dateEntry->pos,
				dateEntry->length
		);
	}

	//! Gets the size of the text corpus, in bytes.
	/*!
	 * \note The number of characters in the corpus
	 *   may differ, as it might be UTF-8-encoded.
	 *
	 * \returns The size of the corpus in bytes.
	 */
	inline std::size_t Corpus::size() const {
		if(this->tokenized) {
			return this->tokenBytes;
		}

		return this->corpus.size();
	}

	//! Checks whether the corpus is empty.
	/*!
	 * \returns True, if the corpus is empty.
	 *   False otherwise.
	 */
	inline bool Corpus::empty() const {
		if(this->tokenized) {
			return this->tokens.empty();
		}

		return this->corpus.empty();
	}

	//! Gets a substring from the corpus.
	/*!
	 * \param from The zero-based index of
	 *   the first byte of the substring to
	 *   be retrieved.
	 * \param len The length of the substring
	 *   to be retreved, in bytes.
	 *
	 * \returns A copy of the specified substring.
	 *
	 * \throws Corpus::Exception if the corpus has
	 *   already been tokenized.
	 *
	 * \throws std::out_of_range if the index
	 *   or length are invalid, std::bad_alloc
	 *   if the function needs to allocate storage
	 *   and fails.
	 */
	inline std::string Corpus::substr(std::size_t from, std::size_t len) {
		// check corpus
		if(this->tokenized) {
			throw Exception(
					"Corpus::substr():"
					" The corpus has been tokenized."
			);
		}

		return this->corpus.substr(from, len);
	}

	/*
	 * CREATION
	 */

	//! Creates text corpus from a vector of strings.
	/*!
	 * Concatenates all given texts and delimits
	 *  them with spaces.
	 *
	 * \note If a corpus already exists, it will
	 *   be cleared.
	 *
	 * \param texts A reference to the vector
	 *   containing texts to create the corpus from.
	 * \param deleteInputData If true, the given texts
	 *   will be cleared, freeing the used memory early.
	 */
	inline void Corpus::create(
			std::vector<std::string>& texts,
			bool deleteInputData
	) {
		// clear old corpus
		this->clear();

		// concatenate texts
		for(auto& text : texts) {
			// add text to corpus
			this->corpus += text;

			if(deleteInputData) {
				// free memory early
				std::string{}.swap(text);
			}

			// add space at the end of the corpus
			this->corpus.push_back(' ');
		}

		if(deleteInputData) {
			// free memory early
			std::vector<std::string>{}.swap(texts);
		}

		// remove last space if necessary
		if(!(this->corpus.empty())) {
			this->corpus.pop_back();
		}
	}

	//! Creates text corpus from parsed data, including article and date maps.
	/*!
	 * Concatenates all given texts and delimits
	 *  them with spaces. Saves given article IDs
	 *  and date/times in article and date map.
	 *
	 * \note If a corpus already exists, it will
	 *   be cleared.
	 *
	 * \param texts A reference to the vector
	 *   containing texts to create the corpus from.
	 * \param articleIds A reference to the vector
	 *   containing the article IDs for the given texts.
	 * \param dateTimes A reference to the vector
	 *   containing the date/times for the given texts.
	 * \param deleteInputData If true, the given texts,
	 *   article IDs and date/times will be cleared,
	 *   freeing the used memory early.
	 */
	inline void Corpus::create(
			std::vector<std::string>& texts,
			std::vector<std::string>& articleIds,
			std::vector<std::string>& dateTimes,
			bool deleteInputData
	) {
		// check arguments
		if(articleIds.empty() && dateTimes.empty()) {
			this->create(texts, deleteInputData);

			return;
		}

		// clear old corpus
		this->clear();

		TextMapEntry dateMapEntry;

		for(std::size_t n{0}; n < texts.size(); ++n) {
			auto pos{this->corpus.size()};
			auto& text{texts.at(n)};

			// add article ID (or empty article) to article map
			if(articleIds.size() > n) {
				auto& articleId{articleIds.at(n)};

				this->articleMap.emplace_back(pos, text.length(), articleId);

				if(deleteInputData && !articleId.empty()) {
					// free memory early
					std::string{}.swap(articleId);
				}
			}
			else if(!(this->articleMap.empty()) && this->articleMap.back().value.empty()) {
				// expand empty article in the end of the article map
				this->articleMap.back().length += text.length() + 1; /* include space before current text */
			}
			else {
				// add empty article to the end of the article map
				this->articleMap.emplace_back(pos, text.length());
			}

			// add date to date map if necessary
			if(dateTimes.size() > n) {
				auto& dateTime{dateTimes.at(n)};

				// check for valid (long enough) date/time
				if(dateTime.length() >= dateLength) {
					// get only date (YYYY-MM-DD) from date/time
					const std::string date(dateTime, 0, dateLength);

					// check whether a date is already set
					if(!dateMapEntry.value.empty()) {
						// date is already set -> compare with current date
						if(dateMapEntry.value == date) {
							// last date equals current date -> append text to last date
							dateMapEntry.length += text.length() + 1;	/* include space before current text */
						}
						else {
							// last date differs from current date -> conclude last date and start new date
							this->dateMap.emplace_back(dateMapEntry);

							dateMapEntry = TextMapEntry(this->corpus.size(), text.length(), date);
						}
					}
					else {
						// no date is set yet -> start new date
						dateMapEntry = TextMapEntry(this->corpus.size(), text.length(), date);
					}
				}
				else if(!dateMapEntry.value.empty()) {
					// no valid date found, but last date is set -> conclude last date
					this->dateMap.emplace_back(dateMapEntry);

					dateMapEntry = TextMapEntry();
				}

				if(deleteInputData && !dateTime.empty()) {
					// free memory early
					std::string{}.swap(dateTime);
				}
			}

			// concatenate corpus text
			this->corpus += text;

			if(deleteInputData) {
				// free memory early
				std::string{}.swap(text);
			}

			// add space at the end of the corpus
			this->corpus.push_back(' ');
		}

		if(deleteInputData) {
			// free memory early
			std::vector<std::string>{}.swap(texts);
			std::vector<std::string>{}.swap(articleIds);
			std::vector<std::string>{}.swap(dateTimes);
		}

		// finish up corpus data
		if(!(this->corpus.empty())) {
			// remove last space
			this->corpus.pop_back();
		}

		// check for unfinished date
		if(!dateMapEntry.value.empty()) {
			// conclude last date
			this->dateMap.emplace_back(dateMapEntry);
		}
	}

	//! Creates continuous text corpus by combining previously separated chunks, as well as their article and date maps.
	/*!
	 * Performs consistency checks of the provided
	 *  article maps, if consistency checks have
	 *  been enabled on construction.
	 *
	 * \note If a corpus already exists, it will
	 *   be cleared.
	 *
	 * \param chunks Reference to a vector containing
	 *   strings with the text of the chunks.
	 * \param articleMaps Reference to a vector
	 *   containing the article maps of the chunks.
	 * \param dateMaps Reference to a vector containing
	 *   the date maps of the chunks.
	 * \param deleteInputData If true, the given texts,
	 *   article and date maps will be cleared,
	 *   freeing the used memory early.
	 *
	 * \throws Corpus::Exception if consistency checks
	 *   are enabled and an article map does not start
	 *   in the beginning of its corpus chunk.
	 *
	 * \sa copyChunksContinuous
	 */
	inline void Corpus::combineContinuous(
			std::vector<std::string>& chunks,
			std::vector<TextMap>& articleMaps,
			std::vector<TextMap>& dateMaps,
			bool deleteInputData
	) {
		// clear old corpus
		this->clear();

		// reserve memory
		Corpus::reserveCombined(chunks, this->corpus);
		Corpus::reserveCombined(articleMaps, this->articleMap);
		Corpus::reserveCombined(dateMaps, this->dateMap);

		// add chunks
		for(auto chunkIt = chunks.begin(); chunkIt != chunks.end(); ++chunkIt) {
			const auto chunkIndex{
				static_cast<std::size_t>(
						chunkIt
						- chunks.begin()
				)
			};

			// save current position in new corpus
			const auto pos{this->corpus.size()};

			// add text of chunk to corpus
			this->corpus += *chunkIt;

			if(deleteInputData) {
				// free memory early
				std::string{}.swap(*chunkIt);
			}

			bool beginsWithNewArticle{false};

			if(articleMaps.size() > chunkIndex) {
				// add article map
				auto& map{articleMaps.at(chunkIndex)};

				if(!map.empty()) {
					const auto& first{map.at(0)};

					// consistency check
					if(this->checkConsistency && first.pos > 1) {
						std::ostringstream exceptionStrStr;

						exceptionStrStr <<	"Corpus::combineContinuous():"
											" Article map in corpus chunk starts"
											" at #";
						exceptionStrStr <<	first.pos;
						exceptionStrStr <<	" instead of #0 or #1";

						throw Exception(exceptionStrStr.str());
					}

					auto it{map.cbegin()};

					// compare first new article ID with last one
					if(
							!(this->articleMap.empty())
							&& this->articleMap.back().value == first.value
					) {
						// append current article to last one
						this->articleMap.back().length += first.length;

						++it;
					}
					else {
						beginsWithNewArticle = true;
					}

					// add remaining articles to map
					for(; it != map.cend(); ++it) {
						this->articleMap.emplace_back(
								pos + it->pos,
								it->length,
								it->value
						);
					}

					if(deleteInputData) {
						// free memory early
						TextMap{}.swap(map);
					}
				}
			}

			if(dateMaps.size() > chunkIndex) {
				// add date map
				auto& map{dateMaps.at(chunkIndex)};

				if(!map.empty()) {
					const auto& first{map.at(0)};
					auto it{map.cbegin()};

					// compare first new date with last one
					if(
							!(this->dateMap.empty())
							&& this->dateMap.back().value == first.value
					) {
						// append current date to last one
						this->dateMap.back().length += first.length;

						// add space between articles if chunk begins with new article and date has been extended
						if(beginsWithNewArticle) {
							++(this->dateMap.back().length);
						}

						++it;
					}

					// add remaining dates to map
					for(; it != map.cend(); ++it) {
						this->dateMap.emplace_back(
								pos + it->pos,
								it->length,
								it->value
						);
					}

					if(deleteInputData) {
						// free memory early
						TextMap{}.swap(map);
					}
				}
			}
		}

		if(deleteInputData) {
			// free memory early
			std::vector<std::string>{}.swap(chunks);
			std::vector<TextMap>{}.swap(articleMaps);
			std::vector<TextMap>{}.swap(dateMaps);
		}
	}

	//! Creates a tokenized text corpus by combining previously separated chunks, as well as their article, date and sentence maps.
	/*!
	 * Performs consistency checks of the provided
	 *  article maps, if consistency checks have
	 *  been enabled on construction.
	 *
	 * \note If a corpus already exists, it will
	 *   be cleared.
	 *
	 * \param chunks Reference to a vector containing
	 *   strings with the text of the chunks.
	 * \param wordNums Reference to a vector containing
	 *   the number of words in each chunk.
	 * \param articleMaps Reference to a vector
	 *   containing the article maps of the chunks.
	 * \param dateMaps Reference to a vector containing
	 *   the date maps of the chunks.
	 * \param sentenceMaps Reference to a vector
	 *   containing the sentence maps of the chunks.
	 * \param deleteInputData If true, the given texts,
	 *   word counts, as well as article, date and
	 *   sentence maps will be cleared, freeing the
	 *   used memory early.
	 *
	 * \throws Corpus::Exception if the corpus is not
	 *   empty and no sentence map is given or the
	 *   combined sentence map is empty, consistency
	 *   checks are enabled and an article map or a
	 *   sentence map does not start in the beginning
	 *   of its corpus chunk, if the length of the
	 *   first sentence in a chunk conflicts with the
	 *   length given in the previous chunk, if the
	 *   length of the last sentence in the combined
	 *   corpus exceeds the length of the corpus
	 *   itself, or if there are more word counts,
	 *   article maps, date maps and/or sentence maps
	 *   given than corpus chunks.
	 *
	 * \sa copyChunksTokenized
	 */
	inline void Corpus::combineTokenized(
			std::vector<std::string>& chunks,
			std::vector<std::size_t>& wordNums,
			std::vector<TextMap>& articleMaps,
			std::vector<TextMap>& dateMaps,
			std::vector<SentenceMap>& sentenceMaps,
			bool deleteInputData
	) {
		// clear old corpus
		this->clear();

		// check arguments
		if(
				this->checkConsistency
				&& (
						wordNums.size() > chunks.size()
						|| articleMaps.size() > chunks.size()
						|| dateMaps.size() > chunks.size()
						|| sentenceMaps.size() > chunks.size()
				)
		) {
			throw Exception(
					"Corpus::combineTokenized():"
					" More word counts, article maps, date maps,"
					" and/or sentence maps than corpus chunks"
			);
		}

		if(chunks.empty()) {
			return;
		}

		if(sentenceMaps.empty()) {
			throw Exception(
					"Corpus::combineTokenized():"
					" No sentence maps for non-empty corpus"
			);
		}

		// reserve memory
		const auto totalWords{
			std::accumulate(
					wordNums.cbegin(),
					wordNums.cend(),
					static_cast<std::size_t>(0)
			)
		};

		if(deleteInputData) {
			// free memory early
			std::vector<std::size_t>{}.swap(wordNums);
		}

		this->tokens.reserve(totalWords);

		Corpus::reserveCombined(articleMaps, this->articleMap);
		Corpus::reserveCombined(dateMaps, this->dateMap);
		Corpus::reserveCombined(sentenceMaps, this->sentenceMap);

		// add words from chunks
		std::string lastWord;
		bool skipNextSeparator{false};

		for(auto chunkIt = chunks.begin(); chunkIt != chunks.end(); ++chunkIt) {
			const auto chunkIndex{
				static_cast<std::size_t>(
						chunkIt
						- chunks.begin()
				)
			};

			auto chunkOffset{this->tokens.size()};
			bool skipSeparator{skipNextSeparator};
			std::size_t begin{0};

			skipNextSeparator = false;

			while(begin < chunkIt->size()) {
				std::size_t end{begin};

				while(end < chunkIt->size()) {
					if((*chunkIt)[end] == '\n') {
						break;
					}

					++end;
				}

				if(!lastWord.empty() && end == 0) {
					++chunkOffset;

					skipSeparator = true;
				}

				if(end == chunkIt->size()) {
					lastWord = chunkIt->substr(begin, end - begin);
				}
				else if(lastWord.empty()) {
					const auto len{
						end - begin
					};

					this->tokens.emplace_back(
							chunkIt->substr(begin, len)
					);

					this->tokenBytes += len;
				}
				else {
					const auto len{
						end - begin
					};

					this->tokens.emplace_back(
							lastWord
							+ chunkIt->substr(begin, len)
					);

					this->tokenBytes += lastWord.size() + len;

					lastWord.clear();
				}

				begin = end + 1;
			}

			if(deleteInputData) {
				// free memory early
				std::string{}.swap(*chunkIt);
			}

			if(sentenceMaps.size() > chunkIndex) {
				// add sentence map
				auto& map{sentenceMaps.at(chunkIndex)};

				if(!map.empty()) {
					const auto& first{map.at(0)};

					// consistency check
					if(this->checkConsistency && first.first > 0) {
						std::ostringstream exceptionStrStr;

						exceptionStrStr.imbue(std::locale(""));

						exceptionStrStr <<	"Corpus::combineTokenized():"
											" In chunk ";
						exceptionStrStr <<	chunkIndex + 1;
						exceptionStrStr <<	"/";
						exceptionStrStr <<	chunks.size();
						exceptionStrStr <<	":"
											" Sentence map in corpus chunk starts"
											" at #";
						exceptionStrStr <<	first.first;
						exceptionStrStr <<	" instead of #0";

						throw Exception(exceptionStrStr.str());
					}

					auto it{map.cbegin()};

					if(!(this->sentenceMap.empty())) {
						// check whether the last sentence map already includes the first sentence
						const auto lastSentenceEnd{
							this->sentenceMap.back().first
							+ this->sentenceMap.back().second
						};

						if(lastSentenceEnd > chunkOffset) {
							// consistency check
							if(
									this->checkConsistency
									&&
									first.second
									!= lastSentenceEnd
									- chunkOffset
							) {
								std::ostringstream exceptionStrStr;

								exceptionStrStr.imbue(std::locale(""));

								exceptionStrStr <<	"Corpus::combineTokenized():"
													" In chunk ";
								exceptionStrStr <<	chunkIndex + 1;
								exceptionStrStr <<	"/";
								exceptionStrStr <<	chunks.size();
								exceptionStrStr <<	":"
													" Length of first sentence conflicts"
													" with length given in previous chunk"
													" (";
								exceptionStrStr <<	first.second;
								exceptionStrStr <<	" != ";
								exceptionStrStr <<	lastSentenceEnd;
								exceptionStrStr << 	" - ";
								exceptionStrStr <<	chunkOffset;
								exceptionStrStr << 	" [";
								exceptionStrStr <<	lastSentenceEnd - chunkOffset;
								exceptionStrStr << 	"])";

								throw Exception(exceptionStrStr.str());
							}

							// ignore current sentence as it has already been added with the last chunk
							++it;
						}
					}

					// add remaining sentences to map
					for(; it != map.cend(); ++it) {
						this->sentenceMap.emplace_back(chunkOffset + it->first, it->second);
					}

					if(deleteInputData) {
						// free memory early
						SentenceMap{}.swap(map);
					}
				}

				// check whether last sentence (and therefore last word) is complete
				if(
						this->sentenceMap.back().first
						+ this->sentenceMap.back().second
						== this->tokens.size()
						+ 1
				) {
					this->tokens.emplace_back(lastWord);

					this->tokenBytes += lastWord.size();

					lastWord.clear();

					skipNextSeparator = true;
				}
			}

			if(articleMaps.size() > chunkIndex) {
				// add article map
				auto& map{articleMaps.at(chunkIndex)};

				if(!map.empty()) {
					const auto& first{map.at(0)};

					// consistency check
					if(this->checkConsistency && first.pos > 0) {
						std::ostringstream exceptionStrStr;

						exceptionStrStr.imbue(std::locale(""));

						exceptionStrStr <<	"Corpus::combineTokenized():"
											" In chunk ";
						exceptionStrStr <<	chunkIndex + 1;
						exceptionStrStr <<	"/";
						exceptionStrStr <<	chunks.size();
						exceptionStrStr <<	":"
											" Article map in corpus chunk starts at #";
						exceptionStrStr <<	first.pos;
						exceptionStrStr <<	" instead of #0";

						throw Exception(exceptionStrStr.str());
					}

					auto it{map.cbegin()};

					// compare first new article ID with previous one
					if(
							!(this->articleMap.empty())
							&& this->articleMap.back().value == first.value
					) {
						// append current article to previous one
						this->articleMap.back().length += first.length;

						if(!skipSeparator) {
							// remove second part of previous token
							--(this->articleMap.back().length);
						}

						++it;
					}

					// add remaining articles to map
					for(; it != map.cend(); ++it) {
						this->articleMap.emplace_back(
								chunkOffset + it->pos,
								it->length,
								it->value
						);
					}

					if(deleteInputData) {
						// free memory early
						TextMap{}.swap(map);
					}
				}
			}

			if(dateMaps.size() > chunkIndex) {
				// add date map
				auto& map{dateMaps.at(chunkIndex)};

				if(!map.empty()) {
					const auto& first{map.at(0)};
					auto it{map.cbegin()};

					// compare first new date with previous one
					if(
							!(this->dateMap.empty())
							&& this->dateMap.back().value == first.value
					) {
						// append current date to previous one
						this->dateMap.back().length += first.length;

						if(!skipSeparator) {
							// remove second part of previous token
							--(this->dateMap.back().length);
						}

						++it;
					}

					// add remaining dates to map
					for(; it != map.cend(); ++it) {
						this->dateMap.emplace_back(
								chunkOffset + it->pos,
								it->length,
								it->value
						);
					}

					if(deleteInputData) {
						// free memory early
						TextMap{}.swap(map);
					}
				}
			}
		}

		if(!lastWord.empty()) {
			this->tokens.emplace_back(lastWord);

			this->tokenBytes += lastWord.size();
		}

		if(this->sentenceMap.empty()) {
			throw Exception(
					"Corpus::combineTokenized():"
					" Empty sentence map for non-empty corpus"
			);
		}

		if(
				this->checkConsistency
				&&
				this->sentenceMap.back().first
				+ this->sentenceMap.back().second
				> this->tokens.size()
		) {
			std::ostringstream exceptionStrStr;

			exceptionStrStr.imbue(std::locale(""));

			exceptionStrStr <<	"Corpus::combineTokenized():"
								" Length of last sentence"
								" (";
			exceptionStrStr <<	this->sentenceMap.back().first;
			exceptionStrStr <<	" + ";
			exceptionStrStr <<	this->sentenceMap.back().second;
			exceptionStrStr <<	") exceeds length of corpus"
								" (";
			exceptionStrStr <<	this->tokens.size();
			exceptionStrStr <<	")";

			throw Exception(exceptionStrStr.str());
		}

		if(deleteInputData) {
			// free memory early
			std::vector<std::string>{}.swap(chunks);
			std::vector<TextMap>{}.swap(articleMaps);
			std::vector<TextMap>{}.swap(dateMaps);
			std::vector<SentenceMap>{}.swap(sentenceMaps);
		}

		this->tokenized = true;

		// if necessary, check the consistency of the newly combined corpus
		this->checkTokenized();
	}

	/*
	 * COPYING
	 */

	//! Copies the underlying continuous text corpus to the given string.
	/*!
	 * \param to Reference to a string to which the
	 *   text corpus will be copied.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has been tokenized.
	 */
	inline void Corpus::copyContinuous(std::string& to) const {
		// check corpus
		if(this->tokenized) {
			throw Exception(
					"Corpus::copyContinuous():"
					" The corpus has been tokenized"
			);
		}

		to = this->corpus;
	}

	//! Copies the underlying continous text corpus, as well as its article and date map.
	/*!
	 * \param to Reference to a string to
	 *   which the text corpus will be copied.
	 * \param articleMapTo Reference to a text
	 *   map structure to which the article
	 *   map will be copied.
	 * \param dateMapTo Reference to a text
	 *   map structure to which the date map
	 *   will be copied.
	 *
	 * \throws Corpus::Exception if the corpus
	 *   has been tokenized.
	 */
	inline void Corpus::copyContinuous(
			std::string& to,
			TextMap& articleMapTo,
			TextMap& dateMapTo
	) const {
		// check corpus
		if(this->tokenized) {
			throw Exception(
					"Corpus::copyContinuous():"
					" The corpus has been tokenized"
			);
		}

		to = this->corpus;
		articleMapTo = this->articleMap;
		dateMapTo = this->dateMap;
	}

	//! Copies the underlying continous text corpus into chunks of the given size.
	/*!
	 * If the text corpus has an article
	 *  and/or a date map, a corresponding
	 *  article and/or date map will be
	 *  created for each of the corpus chunks.
	 *
	 * If the text corpus contains
	 *  UTF8-encoded characters, they will not
	 *  be split, creating the possibility of
	 *  chunks with slightly different sizes.
	 *
	 * \param chunkSize The maximum chunk size
	 *   in bytes.
	 * \param to Reference to a vector of
	 *   strings to which the texts of the
	 *   corpus chunks will be appended.
	 * \param articleMapsTo Reference to a
	 *   vector of text map structures, to
	 *   which the article maps of the chunks
	 *   will be appended. The vector will not
	 *   be changed if the text corpus does
	 *   not possess an article map.
	 * \param dateMapsTo Reference to a vector
	 *   of text map structures, to which the
	 *   date maps of the chunks will be
	 *   appended. The vector will not be
	 *   changed if the text corpus does not
	 *   possess an date map.
	 *
	 * \throws Corpus::Exception if the chunk
	 *   size is zero and the corpus is
	 *   non-empty, if the corpus has already
	 *   been tokenized, or if consistency
	 *   checks have been enabled and
	 *   - the article and the date map
	 *      contradict each other
	 *   - one of the chunks created is larger
	 *      than the maximum chunk size given
	 *   - the article map does not describe
	 *      the whole corpus
	 *   - the last chunk created is empty
	 *
	 * \sa combineContinuous
	 */
	inline void Corpus::copyChunksContinuous(
			std::size_t chunkSize,
			std::vector<std::string>& to,
			std::vector<TextMap>& articleMapsTo,
			std::vector<TextMap>& dateMapsTo
	) const {
		// check arguments
		if(chunkSize == 0) {
			if(this->corpus.empty()) {
				return;
			}

			throw Exception(
					"Corpus::copyChunksContinuous():"
					" Invalid chunk size (zero)"
					" for a non-empty corpus"
			);
		}

		if(this->tokenized) {
			throw Exception(
					"Corpus::copyChunksContinuous():"
					" The corpus has been tokenized"
			);
		}

		// check whether slicing is necessary
		if(this->corpus.size() <= chunkSize) {
			to.emplace_back(this->corpus);
			articleMapsTo.emplace_back(this->articleMap);
			dateMapsTo.emplace_back(this->dateMap);

			return;
		}

		// reserve memory for chunks
		const auto chunks{
			this->corpus.size() / chunkSize
			+ (this->corpus.size() % chunkSize > 0 ? 1 : 0)
		};

		to.reserve(to.size() + chunks);

		if(!(this->articleMap.empty())) {
			articleMapsTo.reserve(articleMapsTo.size() + chunks);
		}

		if(!(this->dateMap.empty())) {
			dateMapsTo.reserve(dateMapsTo.size() + chunks);
		}

		// slice corpus into chunks
		bool noSpace{false};

		if(this->articleMap.empty()) {
			// no article part: simply add parts of the corpus
			std::size_t pos{0};

			while(pos < this->corpus.size()) {
				to.emplace_back(
						this->corpus,
						pos,
						Corpus::getValidLengthOfChunk(
								this->corpus,
								pos,
								chunkSize,
								chunkSize
						)
				);

				pos += to.back().size();
			}

			noSpace = true;
		}
		else {
			std::size_t corpusPos{0};
			std::size_t articlePos{0};
			auto articleIt{this->articleMap.cbegin()};
			auto dateIt{this->dateMap.cbegin()};

			while(corpusPos < this->corpus.size()) { /* loop for chunks */
				// create chunk
				TextMap chunkArticleMap;
				TextMap chunkDateMap;
				std::string chunk;

				// add space if necessary
				if(noSpace) {
					chunk.push_back(' ');

					++corpusPos;

					noSpace = false;
				}

				// loop through multiple articles inside one chunk
				for(; articleIt != this->articleMap.cend(); ++articleIt) {
					if(dateIt != this->dateMap.cend()) {
						// check date of the article
						if(
								articlePos == 0
								&& articleIt->pos > dateIt->pos + dateIt->length
						) {
							++dateIt;
						}

						if(
								this->checkConsistency
								&&
								articleIt->pos
								> dateIt->pos
								+ dateIt->length
						) {
							std::ostringstream exceptionStrStr;

							exceptionStrStr.imbue(std::locale(""));

							exceptionStrStr <<	"Corpus::copyChunksContinuous():"
												" Article position"
												" (#";
							exceptionStrStr <<	articleIt->pos;
							exceptionStrStr <<	")"
												" lies behind date at"
												" [#";
							exceptionStrStr <<	dateIt->pos;
							exceptionStrStr <<	";#";
							exceptionStrStr <<	dateIt->pos + dateIt->length;
							exceptionStrStr <<	"]";

							throw Exception(exceptionStrStr.str());
						}
					}

					// get remaining article length
					const auto remaining{articleIt->length - articlePos};

					if(chunk.size() + remaining <= chunkSize) {
						if(remaining > 0) {
							// add the remainder of the article to the chunk
							chunkArticleMap.emplace_back(chunk.size(), remaining, articleIt->value);

							if(dateIt != this->dateMap.cend()) {
								if(!chunkDateMap.empty() && chunkDateMap.back().value == dateIt->value) {
									chunkDateMap.back().length += remaining + 1; /* including space before article */
								}
								else if(corpusPos >= dateIt->pos) {
									chunkDateMap.emplace_back(chunk.size(), remaining, dateIt->value);
								}
							}

							chunk.append(this->corpus, corpusPos, remaining);

							// update position in corpus
							corpusPos += remaining;
						}

						// reset position in (next) article
						articlePos = 0;

						if(chunk.size() < chunkSize) {
							// add space to the end of the article
							chunk.push_back(' ');

							++corpusPos;

							// check for end of chunk
							if(chunk.size() == chunkSize) {
								// start next chunk with next article
								++articleIt;

								break; /* chunk is full */
							}
						}
						else {
							// add space to the beginning of the next chunk instead
							noSpace = true;

							// start next chunk with next article
							++articleIt;

							break; /* chunk is full */
						}
					}
					else {
						// fill the remainder of the chunk with part of the article
						auto fill{chunkSize - chunk.size()};

						if(fill == 0) {
							break;	/* chunk is full */
						}

						// check the end for valid UTF-8
						fill = Corpus::getValidLengthOfChunk(
								this->corpus,
								corpusPos,
								fill,
								chunkSize
						);

						if(fill == 0) {
							break;	/* not enough space in chunk for last (UTF-8) character */
						}

						chunkArticleMap.emplace_back(chunk.size(), fill, articleIt->value);

						if(dateIt != this->dateMap.cend()) {
							if(!chunkDateMap.empty() && chunkDateMap.back().value == dateIt->value) {
								chunkDateMap.back().length += fill + 1; /* including space before the article */
							}
							else if(corpusPos >= dateIt->pos) {
								chunkDateMap.emplace_back(chunk.size(), fill, dateIt->value);
							}
						}

						chunk.append(this->corpus, corpusPos, fill);

						// update positions
						corpusPos += fill;
						articlePos += fill;

						break; /* chunk is full */
					}
				}

				// consistency checks
				if(this->checkConsistency) {
					if(chunk.size() > chunkSize) {
						std::ostringstream exceptionStrStr;

						exceptionStrStr.imbue(std::locale(""));

						exceptionStrStr <<	"Corpus::copyChunksContinuous():"
											" Chunk is too large:";
						exceptionStrStr <<	chunk.size();
						exceptionStrStr <<	" > ";
						exceptionStrStr <<	chunkSize;

						throw Exception(exceptionStrStr.str());
					}

					if(articleIt == this->articleMap.cend() && corpusPos < this->corpus.size()) {
						std::ostringstream exceptionStrStr;

						exceptionStrStr.imbue(std::locale(""));

						exceptionStrStr <<	"Corpus::copyChunksContinuous():"
											" End of articles, but not of corpus"
											" ( #";
						exceptionStrStr <<	corpusPos;
						exceptionStrStr <<	" < #";
						exceptionStrStr <<	this->corpus.size();
						exceptionStrStr <<	")";

						throw Exception(exceptionStrStr.str());
					}
				}

				// check for empty chunk (should not happen)
				if(chunk.empty()) {
					break;
				}

				// add current chunk
				to.emplace_back(chunk);
				articleMapsTo.emplace_back(chunkArticleMap);
				dateMapsTo.emplace_back(chunkDateMap);
			}
		}

		if(!(this->articleMap.empty()) && !to.empty()) {
			// consistency check
			if(this->checkConsistency && to.back().empty()) {
				throw Exception(
						"Corpus::copyChunksContinuous():"
						" The final chunk is empty"
				);
			}

			// remove last space
			if(!noSpace) {
				to.back().pop_back();
			}

			// remove last chunk if it is empty
			if(to.back().empty()) {
				to.pop_back();
			}

			// consistency check
			if(this->checkConsistency && to.back().empty()) {
				throw Exception(
						"Corpus::copyChunksContinuous():"
						" The final chunk is empty"
				);
			}
		}
	}

	//! Copies the underlying tokenized text corpus into chunks of the given size.
	/*!
	 * A corresponding sentence map will be
	 *  created for each of the corpus chunks,
	 *  although the length of the last
	 *  sentence in a chunk might exceed the
	 *  length of the chunk itself.
	 *
	 * If the text corpus has an article
	 *  and/or a date map, a corresponding
	 *  article and/or date map will be
	 *  created for each of the corpus chunks.
	 *
	 * If the text corpus contains
	 *  UTF8-encoded characters, they will not
	 *  be split, creating the possibility of
	 *  chunks with slightly different sizes.
	 *
	 * \param chunkSize The maximum chunk size
	 *   in bytes.
	 * \param to Reference to a vector of
	 *   strings to which the texts of the
	 *   corpus chunks will be appended.
	 * \param wordNumsTo Reference to a vector
	 *   to which the number of words for each
	 *   chunk will be written.
	 * \param articleMapsTo Reference to a
	 *   vector of text map structures, to
	 *   which the article maps of the chunks
	 *   will be appended. The vector will not
	 *   be changed if the text corpus does
	 *   not possess an article map.
	 * \param dateMapsTo Reference to a vector
	 *   of text map structures, to which the
	 *   date maps of the chunks will be
	 *   appended. The vector will not be
	 *   changed if the text corpus does not
	 *   possess an date map.
	 * \param sentenceMapsTo Reference to a
	 *   vector of @c [pos;length] pairs, to
	 *   which the sentence maps of the chunks
	 *   will be appended. The vector will not
	 *   be changed if the text corpus does
	 *   not possess an sentence map.
	 *
	 * \throws Corpus::Exception if the chunk
	 *   size is zero and the corpus is
	 *   non-empty, the corpus has not been
	 *   tokenized, the sentence map for a
	 *   non-empty corpus is empty, or if
	 *   consistency checks have been enabled
	 *   and
	 *   - the article, the date map and/or
	 *      the sentence map contradict each
	 *      other
	 *   - one of the chunks created is larger
	 *      than the maximum chunk size given
	 *   - the article map or the sentence map
	 *      does not describe the whole corpus
	 *   - the last chunk created is empty
	 *
	 * \sa combineTokenized
	 */
	inline void Corpus::copyChunksTokenized(
			std::size_t chunkSize,
			std::vector<std::string>& to,
			std::vector<std::size_t>& wordNumsTo,
			std::vector<TextMap>& articleMapsTo,
			std::vector<TextMap>& dateMapsTo,
			std::vector<SentenceMap>& sentenceMapsTo
	) const {
		// check arguments
		if(chunkSize == 0) {
			if(this->corpus.empty()) {
				return;
			}

			throw Exception(
					"Corpus::copyChunksTokenized():"
					" Invalid chunk size (zero)"
					" for a non-empty corpus"
			);
		}

		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::copyChunksTokenized():"
					" The corpus has not been tokenized"
			);
		}

		if(this->sentenceMap.empty()) {
			throw Exception(
					"Corpus::copyChunksTokenized():"
					" Empty sentence map"
					" for a non-empty corpus"
			);
		}

		// check whether slicing is necessary
		if(this->tokenBytes <= chunkSize) {
			// add whole corpus as one chunk
			const auto size{
				this->tokenBytes
				+ this->tokens.size()
			};

			to.emplace_back(std::string{});

			to.back().reserve(to.size() + size);

			for(const auto& token : this->tokens) {
				to.back() += token;

				to.back().push_back('\n');
			}

			if(!to.back().empty()) {
				to.back().pop_back(); /* remove last newline */
			}

			articleMapsTo.emplace_back(this->articleMap);
			dateMapsTo.emplace_back(this->dateMap);
			sentenceMapsTo.emplace_back(this->sentenceMap);
			wordNumsTo.emplace_back(this->tokens.size());

			return;
		}

		// reserve memory for chunks
		const auto chars{
			this->tokenBytes
			+ this->tokens.size()	/* newline for each token... */
			- 1						/* ...except the last one */
		};
		const auto chunks{
			chars / chunkSize
			+ (chars % chunkSize > 0 ? 1 : 0)
		};

		to.reserve(to.size() + chunks);

		if(!(this->articleMap.empty())) {
			articleMapsTo.reserve(articleMapsTo.size() + chunks);
		}

		if(!(this->dateMap.empty())) {
			dateMapsTo.reserve(dateMapsTo.size() + chunks);
		}

		sentenceMapsTo.reserve(sentenceMapsTo.size() + chunks);

		TextMap currentArticleMap;
		TextMap currentDateMap;
		SentenceMap currentSentenceMap;
		std::string currentChunk;
		std::size_t chunkOffset{0};
		std::size_t chunkNumCompleteTokens{0};
		std::size_t nextArticleIndex{0};
		std::size_t nextDateIndex{0};
		bool reserveMemory{true};

		for(const auto& sentence : this->sentenceMap) {
			// save previous length of chunk
			const auto oldChunkSize{
				currentChunk.size()
			};

			if(reserveMemory) {
				// reserve memory for chunk
				std::size_t bytes{0};

				for(
						auto tokenIt = this->tokens.begin() + sentence.first;
						tokenIt != this->tokens.end();
						++tokenIt
				) {
					bytes += tokenIt->size() + 1;

					if(bytes > chunkSize) {
						break;
					}
				}

				if(bytes > chunkSize) {
					currentChunk.reserve(chunkSize + 1);
				}
				else {
					currentChunk.reserve(bytes);
				}

				reserveMemory = false;
			}

			// add sentence to current chunk
			for(
					auto tokenIt = this->tokens.begin() + sentence.first;
					tokenIt != this->tokens.begin() + sentence.first + sentence.second;
					++tokenIt
			) {
				currentChunk += *tokenIt;

				currentChunk.push_back('\n');
			}

			chunkNumCompleteTokens += sentence.second;

			// check for beginning of next article
			if(nextArticleIndex < this->articleMap.size()) {
				while(this->articleMap.at(nextArticleIndex).pos == sentence.first) {
					const auto& nextArticle = this->articleMap.at(nextArticleIndex);

					if(nextArticle.length > 0) {
						currentArticleMap.emplace_back(
								nextArticle.pos - chunkOffset,
								nextArticle.length,
								nextArticle.value
						);
					}

					++nextArticleIndex;

					if(nextArticleIndex == this->articleMap.size()) {
						break;
					}
				}

				if(
						this->checkConsistency
						&& nextArticleIndex < this->articleMap.size()
						&& this->articleMap.at(nextArticleIndex).pos < sentence.first
				) {
					const auto& nextArticle = this->articleMap.at(nextArticleIndex);

					std::ostringstream exceptionStrStr;

					exceptionStrStr.imbue(std::locale(""));

					exceptionStrStr <<	"Corpus::copyChunksTokenized():"
										" Unexpected begin of article '";
					exceptionStrStr <<	nextArticle.value;
					exceptionStrStr <<	"'"
										" (@";
					exceptionStrStr <<	nextArticle.pos;
					exceptionStrStr <<	")"
							   	   	    " before the beginning"
										" of the current sentence"
										" (@";
					exceptionStrStr <<	sentence.first;
					exceptionStrStr <<	")";

					throw Exception(exceptionStrStr.str());
				}
			}

			// check for beginning of next date
			if(nextDateIndex < this->dateMap.size()) {
				while(this->dateMap.at(nextDateIndex).pos == sentence.first) {
					const auto& nextDate = this->dateMap.at(nextDateIndex);

					if(nextDate.length > 0) {
						currentDateMap.emplace_back(
								nextDate.pos - chunkOffset,
								nextDate.length,
								nextDate.value
						);
					}

					++nextDateIndex;

					if(nextDateIndex == this->dateMap.size()) {
						break;
					}
				}

				if(
						this->checkConsistency
						&& nextDateIndex < this->dateMap.size()
						&& this->dateMap.at(nextDateIndex).pos < sentence.first
				) {
					const auto& nextDate = this->dateMap.at(nextDateIndex);

					std::ostringstream exceptionStrStr;

					exceptionStrStr.imbue(std::locale(""));

					exceptionStrStr <<	"Corpus::copyChunksTokenized():"
										" Unexpected begin of date '";
					exceptionStrStr <<	nextDate.value;
					exceptionStrStr <<	"'"
										" (@";
					exceptionStrStr <<	nextDate.pos;
					exceptionStrStr <<	")"
										" before the beginning"
										" of the current sentence"
										" (@";
					exceptionStrStr <<	sentence.first;
					exceptionStrStr <<	")";

					throw Exception(exceptionStrStr.str());
				}
			}

			// add current sentence
			currentSentenceMap.emplace_back(
					sentence.first - chunkOffset,
					sentence.second
			);

			if(currentChunk.size() >= chunkSize) {
				currentChunk.pop_back(); /* remove last newline */

				std::string rest;
				std::size_t restNumTokens{0};
				TextMapEntry articleRest;
				TextMapEntry dateRest;
				bool splitToken{false};

				if(currentChunk.size() > chunkSize) {
					// split sentence (but do not shrink sentence map)
					const auto chunkLength{
						Corpus::getValidLengthOfChunk(
								currentChunk,
								0,
								chunkSize,
								chunkSize
						)
					};

					rest = currentChunk.substr(chunkLength);

					currentChunk.erase(chunkLength);

					reserveMemory = true;
					splitToken = true;

					// check which tokens to (also) add to the next chunk
					restNumTokens = sentence.second;

					const auto sentenceEnd{
						sentence.first
						+ sentence.second
					};
					auto chunkEnd{
						oldChunkSize
					};

					for(auto token{sentence.first}; token < sentenceEnd; ++token) {
						chunkEnd += this->tokens.at(token).size() + 1;

						if(chunkEnd - 1 > chunkSize) {
							break;
						}

						--restNumTokens;

						if(chunkEnd - 1 == chunkSize) {
							splitToken = false;

							break;
						}
					}

					// update number of tokens in current chunk
					chunkNumCompleteTokens -= restNumTokens;
				}

				if(!currentArticleMap.empty()) {
					// split article, if necessary
					const auto articleEnd{
						currentArticleMap.back().pos
						+ currentArticleMap.back().length
					};

					if(
							(articleEnd > chunkNumCompleteTokens)
							|| (splitToken && articleEnd == chunkNumCompleteTokens)
					) {
						articleRest.length = currentArticleMap.back().pos
								+ currentArticleMap.back().length
								- chunkNumCompleteTokens;

						articleRest.value = currentArticleMap.back().value;

						currentArticleMap.back().length = chunkNumCompleteTokens
								- currentArticleMap.back().pos;

						if(splitToken) {
							++currentArticleMap.back().length;
						}
					}
				}

				if(!currentDateMap.empty()) {
					// split date, if necessary
					const auto dateEnd{
						currentDateMap.back().pos
						+ currentDateMap.back().length
					};

					if(
							(dateEnd > chunkNumCompleteTokens)
							|| (splitToken && dateEnd == chunkNumCompleteTokens)
					) {
						dateRest.length = currentDateMap.back().pos
								+ currentDateMap.back().length
								- chunkNumCompleteTokens;

						dateRest.value = currentDateMap.back().value;

						currentDateMap.back().length = chunkNumCompleteTokens
								- currentDateMap.back().pos;

						if(splitToken) {
							++currentDateMap.back().length;
						}
					}
				}

				// update offset
				chunkOffset += chunkNumCompleteTokens;

				// move chunk to result
				to.emplace_back(std::move(currentChunk));

				currentChunk.clear();

				// set number of tokens
				wordNumsTo.emplace_back(chunkNumCompleteTokens);

				if(splitToken) {
					++(wordNumsTo.back());
				}

				sentenceMapsTo.emplace_back(std::move(currentSentenceMap));

				currentSentenceMap.clear();

				if(!(this->articleMap.empty())) {
					articleMapsTo.emplace_back(std::move(currentArticleMap));

					currentArticleMap.clear();
				}

				if(!(this->dateMap.empty())) {
					dateMapsTo.emplace_back(std::move(currentDateMap));

					currentDateMap.clear();
				}

				// add rest to new chunk
				if(!rest.empty()) {
					currentChunk += rest;

					currentChunk.push_back('\n');

					currentSentenceMap.emplace_back(0, restNumTokens);

					chunkNumCompleteTokens = restNumTokens;
				}
				else {
					chunkNumCompleteTokens = 0;
				}

				if(articleRest.length > 0) {
					currentArticleMap.emplace_back(std::move(articleRest));
				}

				if(dateRest.length > 0) {
					currentDateMap.emplace_back(std::move(dateRest));
				}
			}
		}

		if(!currentChunk.empty()) {
			// remove last newline
			currentChunk.pop_back();

			// add last chunk
			to.emplace_back(std::move(currentChunk));

			wordNumsTo.emplace_back(chunkNumCompleteTokens);

			sentenceMapsTo.emplace_back(std::move(currentSentenceMap));

			if(!(this->articleMap.empty())) {
				articleMapsTo.emplace_back(std::move(currentArticleMap));
			}

			if(!(this->dateMap.empty())) {
				dateMapsTo.emplace_back(std::move(currentDateMap));
			}
		}
	}

	/*
	 * FILTERING
	 */

	//! Filters a continous text corpus by the given date(s).
	/*!
	 * Afterwards, the corpus will only contain
	 *  text marked with the given date(s), or
	 *  be empty if the given date(s) do not
	 *  correspond with any parts of the
	 *  corpus.
	 *
	 * If the given strings are empty, no
	 *  action will be performed.
	 *
	 * \param from Constant reference to a
	 *   string containing the date to be
	 *   filtered from, in the format
	 *   @c YYY-MM-DD.
	 * \param to Constant reference to a string
	 *   containing the date to be filtered to,
	 *   in the format @c YYY-MM-DD.
	 *
	 * \returns True, if the corpus has been
	 *   changed as result of the filtering by
	 *   the given date. False, if it remains
	 *   unchanged.
	 *
	 * \throws Corpus::Exception if
	 *   consistency checks have been enabled
	 *   on construction and the date map does
	 *   not start at the beginning of the
	 *   corpus, the date map and the article
	 *   map contradict each other, or they
	 *   contain other inconsistencies – or if
	 *   the corpus has been tokenized.
	 */
	inline bool Corpus::filterByDate(const std::string& from, const std::string& to) {
		// check arguments
		if(from.empty() && to.empty()) {
			return false;
		}

		// check corpus
		if(this->tokenized) {
			if(this->tokens.empty()) {
				return false;
			}
		}
		else if(this->corpus.empty()) {
			return false;
		}

		if(this->dateMap.empty()) {
			// no date map -> empty result
			this->clear();

			return true;
		}

		// find first date in range
		auto begin{this->dateMap.cbegin()};

		for(; begin != this->dateMap.cend(); ++begin) {
			if(Helper::DateTime::isISODateInRange(begin->value, from, to)) {
				break;
			}
		}

		if(begin == this->dateMap.cend()) {
			// no date in range -> empty result
			this->clear();

			return true;
		}

		// find first date not in range anymore
		auto end{begin};

		++end; /* current date is in range as has already been checked */

		for(; end != this->dateMap.cend(); ++end) {
			if(!Helper::DateTime::isISODateInRange(end->value, from, to)) {
				break;
			}
		}

		// trim date map
		if(begin != this->dateMap.cbegin()) {
			// create trimmed date map and swap it with the existing one
			TextMap(begin, end).swap(this->dateMap);
		}
		else {
			// only remove trailing dates
			this->dateMap.resize(std::distance(this->dateMap.cbegin(), end));
		}

		// save offset to be subtracted from all positions, (old) position of the last date and new total length of the corpus
		const auto offset{this->dateMap.front().pos};
		const auto len{
			this->dateMap.back().pos
			+ this->dateMap.back().length
			- offset
		};

		// trim corpus
		if(this->tokenized) {
			// (and calculate new size, in bytes)
			std::size_t deleteBytes{0};

			const auto deleteTo{this->tokens.begin() + offset};

			if(deleteTo != this->tokens.begin()) {
				deleteBytes = std::accumulate(
						this->tokens.begin(),
						deleteTo,
						static_cast<std::size_t>(0),
						[](const auto& a, const auto& b) {
							return a + b.size();
						}
				);

				this->tokens.erase(this->tokens.begin(), deleteTo);
			}

			const auto deleteFrom{this->tokens.begin() + len};

			if(deleteFrom != this->tokens.end()) {
				deleteBytes += std::accumulate(
						deleteFrom,
						this->tokens.end(),
						static_cast<std::size_t>(0),
						[](const auto& a, const auto& b) {
							return a + b.size();
						}
				);

				this->tokens.erase(deleteFrom, this->tokens.end());
			}

			if(deleteBytes > 0) {
				this->tokens.shrink_to_fit();

				this->tokenBytes -= deleteBytes;
			}
		}
		else {
			// replace the current corpus with the trimmed one, and release memory
			if(offset > 0) {
				this->corpus.erase(0, offset);
			}

			this->corpus.resize(len);
			this->corpus.shrink_to_fit();
		}

		// find first article in range
		begin = this->articleMap.cbegin();

		for(; begin != this->articleMap.cend(); ++begin) {
			if(begin->pos == offset) {
				break;
			}

			// consistency check
			if(this->checkConsistency && begin->pos > offset) {
				std::ostringstream exceptionStrStr;

				exceptionStrStr.imbue(std::locale(""));

				exceptionStrStr <<	"Corpus::filterByDate():"
									" Mismatch between positions of article"
									" (@ #";
				exceptionStrStr <<	begin->pos;
				exceptionStrStr <<	") and of date (@ #";
				exceptionStrStr <<	offset;
				exceptionStrStr <<	") in article and date map of the corpus";

				throw Exception(exceptionStrStr.str());
			}
		}

		// consistency check
		if(this->checkConsistency && begin == this->articleMap.cend()) {
			std::ostringstream exceptionStrStr;

			exceptionStrStr.imbue(std::locale(""));

			exceptionStrStr <<	"Corpus::filterByDate():"
								" Position of identified date"
								" (@ #";
			exceptionStrStr <<	offset;
			exceptionStrStr <<	") is behind the position of the last article"
								" (@ #";
			exceptionStrStr <<	this->articleMap.back().pos;
			exceptionStrStr <<	") in article and date map of the corpus";

			throw Exception(exceptionStrStr.str());
		}

		// find first article not in range anymore
		end = begin;

		++end; /* current article is in range as has already been checked */

		for(; end != this->articleMap.cend(); ++end) {
			if(end->pos >= offset + len) {
				break;
			}
		}

		// trim article map
		if(begin != this->articleMap.cbegin()) {
			// create trimmed article map and swap it with the the existing one
			TextMap(begin, end).swap(this->articleMap);
		}
		else {
			// only remove trailing articles
			this->articleMap.resize(
					std::distance(this->articleMap.cbegin(), end)
			);
		}

		if(this->tokenized) {
			// find first sentence in range
			auto smBegin = this->sentenceMap.cbegin();

			for(; smBegin != this->sentenceMap.cend(); ++smBegin) {
				if(smBegin->first == offset) {
					break;
				}

				// consistency check
				if(this->checkConsistency && smBegin->first > offset) {
					std::ostringstream exceptionStrStr;

					exceptionStrStr.imbue(std::locale(""));

					exceptionStrStr <<	"Corpus::filterByDate():"
										" Mismatch between positions of sentence"
										" (@ #";
					exceptionStrStr <<	smBegin->first;
					exceptionStrStr <<	") and of date (@ #";
					exceptionStrStr <<	offset;
					exceptionStrStr <<	") in sentence and date map of the corpus";

					throw Exception(exceptionStrStr.str());
				}
			}

			// consistency check
			if(this->checkConsistency && smBegin == this->sentenceMap.cend()) {
				std::ostringstream exceptionStrStr;

				exceptionStrStr.imbue(std::locale(""));

				exceptionStrStr <<	"Corpus::filterByDate():"
									" Position of identified date"
									" (@ #";
				exceptionStrStr <<	offset;
				exceptionStrStr <<	") is behind the position of the last sentence"
									" (@ #";
				exceptionStrStr <<	this->articleMap.back().pos;
				exceptionStrStr <<	") in sentence and date map of the corpus";

				throw Exception(exceptionStrStr.str());
			}

			// find first sentence not in range anymore
			auto smEnd = smBegin;

			++smEnd; /* current sentence is in range as has already been checked */

			for(; smEnd != this->sentenceMap.cend(); ++smEnd) {
				if(smEnd->first > len) {
					break;
				}
			}

			// trim sentence map
			if(smBegin != this->sentenceMap.cbegin()) {
				// create trimmed sentence map and swap it with the the existing one
				SentenceMap(smBegin, smEnd).swap(this->sentenceMap);
			}
			else {
				// only remove trailing sentences
				this->sentenceMap.resize(
						std::distance(this->sentenceMap.cbegin(), smEnd)
				);
			}
		}

		// update positions in date, article and sentence maps
		for(auto& date : this->dateMap) {
			date.pos -= offset;
		}

		for(auto& article : this->articleMap) {
			article.pos -= offset;
		}

		for(auto& sentence : this->sentenceMap) {
			sentence.first -= offset;
		}

		if(this->checkConsistency) {
			const auto size{
				this->tokenized ? this->tokens.size() : this->corpus.size()
			};

			Corpus::checkMap("date map", this->dateMap, size, this->tokenized, true);
			Corpus::checkMap("article map", this->articleMap, size, this->tokenized, false);
			Corpus::checkMap(this->sentenceMap, size, this->tokenized);
		}

		return true;
	}

	/*
	 * TOKENIZATION
	 */

	//! Converts a text corpus into processed tokens.
	/*!
	 * If sentence manipulators are given,
	 *  first the sentence as a whole will
	 *  be manipulated, then the individual
	 *  words contained in this sentence.
	 *
	 * Please make sure that the corpus is
	 *  tidied beforehand, i.e. UTF-8 and
	 *  other non-space whitespaces are
	 *  replaced by simple spaces. If needed,
	 *  sentences are created by simple
	 *  punctuation analysis.
	 *
	 * \warning Once tokenized, the continous
	 *   text corpus will be lost. Create
	 *   a copy beforehand if you still need
	 *   the original corpus.
	 *
	 * \param sentenceManipulators Vector
	 *   containing the IDs of the default
	 *   manipulators that will be used on
	 *   all sentences in the corpus, where
	 *   every sentence is separated by one
	 *   of the following punctuations from
	 *   the others: @c .:!?; or by the end
	 *   of the current article, date, or the
	 *   whole corpus.
	 * \param sentenceModels Vector of
	 *   strings containing the models used
	 *   by the sentence manipulators.
	 * \param wordManipulators Vector
	 *   containing the IDs of the default
	 *   manipulators that will be used
	 *   for each word in the corpus,
	 *   where every word is separated by a
	 *   simple space from the others.
	 * \param wordModels Vector of strings
	 *   containing the models used by the
	 *   word manipulators.
	 * \param freeMemoryEvery Number of
	 *   processed bytes in a continuous
	 *   corpus after which memory will be
	 *   freed. If zero, memory will only be
	 *   freed after processing is complete.
	 * \param statusSetter Reference to a
	 *   structure containing callbacks
	 *   for updating the status and
	 *   checking whether the thread is
	 *   still supposed to be running.
	 *
	 * \returns True, if the corpus has been
	 *   successfully tokenized. False, if
	 *   tokenization has been cancelled.
	 *
	 * \throws Corpus::Exception if an invalid
	 *   sentence or word manipulator has been
	 *   specified.
	 */
	inline bool Corpus::tokenize(
			const std::vector<std::uint16_t>& sentenceManipulators,
			const std::vector<std::string>& sentenceModels,
			const std::vector<std::uint16_t>& wordManipulators,
			const std::vector<std::string>& wordModels,
			std::uint64_t freeMemoryEvery,
			StatusSetter& statusSetter
	) {
		bool sentenceManipulation{false};
		bool wordManipulation{false};

		for(const auto manipulator : sentenceManipulators) {
			if(manipulator > 0) {
				sentenceManipulation = true;

				break;
			}
		}

		for(const auto manipulator : wordManipulators) {
			if(manipulator > 0) {
				wordManipulation = true;

				break;
			}
		}

		// prepare manipulators
		std::vector<Data::Tagger> taggers(sentenceManipulators.size());

		for(
				auto it{sentenceManipulators.cbegin()};
				it != sentenceManipulators.cend();
				++it
		) {
			const auto index{
				static_cast<std::size_t>(
						it - sentenceManipulators.cbegin()
				)
			};

			switch(*it) {
			case sentenceManipTagger:
				if(sentenceModels.size() > index) {
					taggers.at(index).loadModel(sentenceModels.at(index));
				}

				break;

			default:
				break;
			}
		}

		auto sentenceLambda = [&sentenceManipulators, &taggers](std::vector<std::string>& sentence) {
			for(
					auto it{sentenceManipulators.cbegin()};
					it != sentenceManipulators.cend();
					++it
			) {
				const auto index{
					it - sentenceManipulators.cbegin()
				};

				switch(*it) {
				case sentenceManipNone:
					return;

				case sentenceManipTagger:
					taggers.at(index).label(sentence);

					break;

				default:
					throw Exception(
							"Corpus::tokenize():"
							" Invalid sentence manipulator"
					);
				}
			}
		};

		auto wordLambda = [&wordManipulators, &wordModels, this](std::string& word) {
			for(
					auto it{wordManipulators.cbegin()};
					it != wordManipulators.cend();
					++it
			) {
				const auto index{
					it - wordManipulators.cbegin()
				};

				switch(*it) {
				case wordManipNone:
					return;

				case wordManipRemoveSingleUtf8Chars:
					if(
							word.size() <= maxSingleUtf8CharSize
							&& word.size() >= minSingleUtf8CharSize
					) {
						if(Helper::Utf8::isSingleUtf8Char(word)) {
							std::string{}.swap(word);
						}
					}

					break;

				case wordManipPorter2Stemmer:
					Data::Stemmer::stemEnglish(word);

					break;

				case wordManipGermanStemmer:
					Data::Stemmer::stemGerman(word);

					break;

				case wordManipLemmatizer:
					this->lemmatizer.lemmatize(word, wordModels.at(index));

					break;

				case wordManipRemove:
					this->wordRemover.remove(word, wordModels.at(index));

					break;

				default:
					throw Exception(
							"Corpus::tokenize():"
							" Invalid word manipulator"
					);
				}
			}
		};

		// tokenize corpus
		return this->tokenizeCustom(
				sentenceManipulation ? std::optional<SentenceFunc>{sentenceLambda} : std::nullopt,
				wordManipulation ? std::optional<WordFunc>{wordLambda} : std::nullopt,
				freeMemoryEvery,
				statusSetter
		);
	}

	//! Converts a text corpus into processed tokens, using custom manipulators.
	/*!
	 * If a sentence manipulator is given,
	 *  first the sentence as a whole will
	 *  be manipulated, then the individual
	 *  words contained in this sentence.
	 *
	 * Please make sure that the corpus is
	 *  tidied beforehand, i.e. UTF-8 and
	 *  other non-space whitespaces are
	 *  replaced by simple spaces. If needed,
	 *  sentences are created by simple
	 *  punctuation analysis.
	 *
	 * \warning Once tokenized, the continous
	 *   text corpus will be lost. Create
	 *   a copy beforehand if you still need
	 *   the original corpus.
	 *
	 * \param callbackSentence Optional
	 *   callback function (or lamda)
	 *   that will be called for each
	 *   sentence in the corpus, where every
	 *   every sentence is separated by
	 *   one of the following punctuations
	 *   from the others: @c .:!? or by the
	 *   end of the current article, date,
	 *   or corpus.
	 * \param callbackWord Optional callback
	 *   function (or lambda) that will be used
	 *   on all sentences in the corpus, where
	 *   every sentence is separated by one of
	 *   the following punctuations from the
	 *   others: @c .:;!? or by the end of the
	 *   current article, date, or the whole
	 *   corpus. A word will not be added to
	 *   the tokens of the corpus, if the
	 *   callback function empties it.
	 * \param freeMemoryEvery Number of
	 *   processed bytes in a continuous
	 *   corpus after which memory will be
	 *   freed. If zero, memory will only be
	 *   freed after processing is complete.
	 * \param statusSetter Reference to a
	 *   structure containing callbacks for
	 *   updating the status and checking
	 *   whether the thread is still supposed
	 *   to be running.
	 *
	 * \returns True, if the corpus has been
	 *   successfully tokenized. False, if
	 *   tokenization has been cancelled.
	 *
	 * \throws Corpus::Exception if consistency
	 *   checks are enabled, and article and/or
	 *   date map are inconsistent with the
	 *   content of the corpus.
	 */
	inline bool Corpus::tokenizeCustom(
			const std::optional<SentenceFunc>& callbackSentence,
			const std::optional<WordFunc>& callbackWord,
			std::uint64_t freeMemoryEvery,
			StatusSetter& statusSetter
	) {
		if(this->tokenized) {
			if(!(this->tokenizeTokenized(
					callbackSentence,
					callbackWord,
					statusSetter
			))) {
				return false;
			}
		}
		else {
			if(!(this->tokenizeContinuous(
					callbackSentence,
					callbackWord,
					freeMemoryEvery,
					statusSetter
			))) {
				return false;
			}
		}

		statusSetter.finish();

		// free memory
		this->lemmatizer.clear();
		this->wordRemover.clear();

		return true;
	}

	/*
	 * CLEANUP
	 */

	//! Clears the corpus.
	/*!
	 * Clears the text of the corpus, as well as
	 *  article and date map, if they exist.
	 *
	 * Frees the corresponding memory.
	 */
	inline void Corpus::clear() {
		std::string{}.swap(this->corpus);
		std::vector<std::string>{}.swap(this->tokens);

		TextMap{}.swap(this->articleMap);
		TextMap{}.swap(this->dateMap);
		SentenceMap{}.swap(this->sentenceMap);

		this->tokenized = false;
		this->tokenBytes = 0;
	}

	/*
	 * INTERNAL HELPER FUNCTIONS (private)
	 */

	// tokenize already tokenized corpus
	inline bool Corpus::tokenizeTokenized(
			std::optional<SentenceFunc> callbackSentence,
			std::optional<WordFunc> callbackWord,
			StatusSetter& statusSetter
	) {
		// reset number of bytes
		this->tokenBytes = 0;

		// run manipulators on already tokenized corpus
		if(callbackSentence || callbackWord) {
			std::size_t numDeletedWords{0};
			std::size_t dateIndex{0};
			std::size_t articleIndex{0};
			std::size_t sentenceCounter{0};
			std::size_t statusCounter{0};
			bool inDate{false};
			bool inArticle{false};
			bool emptyDates{false};
			bool emptyArticles{false};
			bool emptySentences{false};

			for(auto& sentenceEntry : this->sentenceMap) {
				// skip dates before current sentence
				while(
						dateIndex < this->dateMap.size()
						&& (
								this->dateMap[dateIndex].pos
								+ this->dateMap[dateIndex].length
								< sentenceEntry.first
								|| this->dateMap[dateIndex].length == 0
						)
				) {
					inDate = false;

					++dateIndex;
				}

				// skip articles before current sentence
				while(
						articleIndex < this->articleMap.size()
						&& (
								this->articleMap[articleIndex].pos
								+ this->articleMap[articleIndex].length
								< sentenceEntry.first
								|| this->articleMap[articleIndex].length == 0
						)
				) {
					inArticle = false;

					++articleIndex;
				}

				// check for beginning of date
				if(
						dateIndex < this->dateMap.size()
						&& this->dateMap[dateIndex].pos
						== sentenceEntry.first
				) {
					inDate = true;

					// update beginning of date
					this->dateMap[dateIndex].pos -= numDeletedWords;
				}

				// check for beginning of article
				if(
						articleIndex < this->articleMap.size()
						&& this->articleMap[articleIndex].pos
						== sentenceEntry.first
				) {
					inArticle = true;

					// update beginning of article
					this->articleMap[articleIndex].pos -= numDeletedWords;
				}

				sentenceEntry.first -= numDeletedWords;

				const auto sentenceEnd{
					sentenceEntry.first
					+ sentenceEntry.second
				};

				std::vector<std::string> sentence(
						this->tokens.begin() + sentenceEntry.first,
						this->tokens.begin() + sentenceEnd
				);

				if(callbackSentence) {
					(*callbackSentence)(sentence);
				}

				for(auto n{sentenceEntry.first + 1}; n <= sentenceEnd; ++n) {
					auto& word{
						this->tokens.at(n - 1)
					};

					if(callbackWord) {
						(*callbackWord)(word);
					}

					if(word.empty()) {
						// delete empty word
						this->tokens.erase(
								this->tokens.begin()
								+ n
								- 1
						);

						--n;

						if(inDate && this->dateMap[dateIndex].length > 0) {
							// update length of date
							--(this->dateMap[dateIndex].length);

							// delete empty dates
							if(this->dateMap[dateIndex].length == 0) {
								emptyDates = true;
							}
						}

						if(inArticle && this->articleMap[articleIndex].length > 0) {
							// update length of article
							--(this->articleMap[articleIndex].length);

							// delete empty articles
							if(this->articleMap[articleIndex].length == 0) {
								emptyArticles = true;
							}
						}

						if(sentenceEntry.second > 0) {
							// update length of sentence
							--(sentenceEntry.second);

							// delete empty sentences
							if(sentenceEntry.second == 0) {
								emptySentences = true;
							}
						}

						++numDeletedWords;
					}
					else {
						this->tokenBytes += word.size();
					}
				}

				++sentenceCounter;
				++statusCounter;

				if(statusCounter == tokenizeUpdateEvery) {
					if(!statusSetter.update(
							static_cast<float>(sentenceCounter)
							/ static_cast<float>(this->sentenceMap.size()),
							true
					)) {
						return false;
					}

					statusCounter = 0;
				}
			}

			if(emptyDates) {
				static_cast<void>(
						std::remove_if(
								this->dateMap.begin(),
								this->dateMap.end(),
								[](const auto& date) {
									return date.length == 0;
								}
						)
				);
			}

			if(emptyArticles) {
				static_cast<void>(
						std::remove_if(
								this->articleMap.begin(),
								this->articleMap.end(),
								[](const auto& article) {
									return article.length == 0;
								}
						)
				);
			}

			if(emptySentences) {
				static_cast<void>(
						std::remove_if(
								this->sentenceMap.begin(),
								this->sentenceMap.end(),
								[](const auto& pair) {
									return pair.second == 0;
								}
						)
				);
			}
		}

		// check consistency
		if(this->checkConsistency) {
			Corpus::checkMap("date map", this->dateMap, this->tokens.size(), true, true);
			Corpus::checkMap("article map", this->articleMap, this->tokens.size(), true, false);
			Corpus::checkMap(this->sentenceMap, this->tokens.size(), true);
		}

		return statusSetter.isRunning();
	}

	// tokenize still continuous corpus
	inline bool Corpus::tokenizeContinuous(
			std::optional<SentenceFunc> callbackSentence,
			std::optional<WordFunc> callbackWord,
			std::uint64_t freeMemoryEvery,
			StatusSetter& statusSetter
	) {
		// tokenize continuous text corpus
		std::vector<std::string> sentence;

		std::size_t wordBegin{0};
		std::size_t sentenceFirstWord{0};
		std::size_t currentWord{0};
		std::size_t statusCounter{0};
		std::size_t corpusTrimmed{0};

		bool inArticle{false};
		bool inDate{false};

		std::size_t articleFirstWord{0};
		std::size_t dateFirstWord{0};
		std::size_t articleEnd{0};
		std::size_t dateEnd{0};
		std::size_t nextArticle{0};
		std::size_t nextDate{0};

		TextMap newArticleMap;
		TextMap newDateMap;

		newArticleMap.reserve(this->articleMap.size());
		newDateMap.reserve(this->dateMap.size());

		// go through all characters in the continous text corpus
		for(std::size_t pos{0}; pos < this->corpus.size() + corpusTrimmed; ++pos) {
			bool sentenceEnd{false};
			bool noSeparator{false};
			bool appendToArticle{false};
			bool appendToDate{false};

			if(!(this->articleMap.empty())) {
				// check for beginning of article
				if(
						!inArticle
						&& nextArticle < this->articleMap.size()
						&& pos == this->articleMap[nextArticle].pos
				) {
					articleFirstWord = currentWord;
					articleEnd = pos + this->articleMap[nextArticle].length;

					inArticle = true;

					++nextArticle;
				}

				// check for end of article
				if(
						inArticle
						&& pos == articleEnd
				) {
					inArticle = false;

					newArticleMap.emplace_back(
							articleFirstWord,
							currentWord - articleFirstWord,
							this->articleMap[nextArticle - 1].value
					);

					sentenceEnd = true;
					appendToArticle = true;
				}
			}

			if(!(this->dateMap.empty())) {
				// check for beginning of date
				if(
						!inDate
						&& nextDate < this->dateMap.size()
						&& pos == this->dateMap[nextDate].pos
				) {
					dateFirstWord = currentWord;
					dateEnd = pos + this->dateMap[nextDate].length;

					inDate = true;

					++nextDate;
				}

				// check for end of date
				if(
						inDate
						&& pos == dateEnd
				) {
					inDate = false;

					newDateMap.emplace_back(
							dateFirstWord,
							currentWord - dateFirstWord,
							this->dateMap[nextDate - 1].value
					);

					sentenceEnd = true;
					appendToDate = true;
				}
			}

			// check for end of sentence
			switch(this->corpus[pos - corpusTrimmed]) {
			case '.':
			case ':':
			case ';':
			case '!':
			case '?':
				sentenceEnd = true;

				break;

			case ' ':
			case ',':
			case '/':
			case '\\':
			case '|':
			case '\a':
			case '\b':
			case '\t':
			case '\n':
			case '\v':
			case '\f':
			case '\r':
			case '\0':
				break;

			default:
				if(sentenceEnd) {
					// end of word and sentence without separating character
					noSeparator = true;
				}
				else {
					// go to next character
					continue;
				}
			}

			// end word
			auto wordLength = pos - wordBegin;

			if(noSeparator) {
				++wordLength;
			}

			if(wordLength > 0) {
				sentence.emplace_back(this->corpus, wordBegin - corpusTrimmed, wordLength);

				++currentWord;

				if(appendToArticle) {
					++(newArticleMap.back().length);
				}

				if(appendToDate) {
					++(newDateMap.back().length);
				}
			}

			if(freeMemoryEvery > 0 && pos - corpusTrimmed > freeMemoryEvery) {
				// free memory, i.e. remove the already processed beginning of the corpus
				this->corpus.erase(0, pos - corpusTrimmed);
				this->corpus.shrink_to_fit();

				corpusTrimmed = pos;
			}

			wordBegin = pos + 1;

			if(sentenceEnd && !sentence.empty()) {
				if(callbackSentence) {
					// modify sentence
					(*callbackSentence)(sentence);
				}

				// modify words of the sentence, do not keep emptied words
				for(auto wordIt{sentence.begin()}; wordIt != sentence.end(); ) {
					if(callbackWord) {
						(*callbackWord)(*wordIt);
					}

					if(wordIt->empty()) {
						// remove empty word
						wordIt = sentence.erase(wordIt);

						--currentWord;

						// shrink article and date, if necessary
						if(appendToArticle) {
							--(newArticleMap.back().length);

							if(newArticleMap.back().length == 0) {
								newArticleMap.pop_back();
							}
						}

						if(appendToDate) {
							--(newDateMap.back().length);

							if(newDateMap.back().length == 0) {
								newDateMap.pop_back();
							}
						}
					}
					else {
						this->tokenBytes += wordIt->size();

						++wordIt;
					}
				}

				if(!sentence.empty()) {
					// add sentence to map
					this->sentenceMap.emplace_back(
							sentenceFirstWord,
							sentence.size()
					);

					// move the words in the finished sentence into the tokens of the corpus
					this->tokens.insert(
							this->tokens.end(),
							std::make_move_iterator(sentence.begin()),
							std::make_move_iterator(sentence.end())
					);

					sentence.clear();
				}

				sentenceFirstWord = currentWord; /* (= already next word) */

				// update status if necessary
				++statusCounter;

				if(statusCounter == tokenizeUpdateEvery) {
					if(!statusSetter.update(
							static_cast<float>(pos + 1)
							/ static_cast<float>(this->corpus.size() + corpusTrimmed),
							true
					)) {
						return false;
					}

					statusCounter = 0;
				}
			}
		}

		// check for end of last article
		bool endOfLastArticle{false};

		if(
				inArticle
				&& this->corpus.size() + corpusTrimmed == articleEnd
		) {
			inArticle = false;

			newArticleMap.emplace_back(
					articleFirstWord,
					currentWord - articleFirstWord,
					this->articleMap[nextArticle - 1].value
			);

			endOfLastArticle = true;
		}

		// check for end of last date
		bool endOfLastDate{false};

		if(
				inDate
				&& this->corpus.size() + corpusTrimmed == dateEnd
		) {
			inDate = false;

			newDateMap.emplace_back(
					dateFirstWord,
					currentWord - dateFirstWord,
					this->dateMap[nextDate - 1].value
			);

			endOfLastDate = true;
		}

		// add last word if not added yet
		if(wordBegin - corpusTrimmed < this->corpus.size()) {
			sentence.emplace_back(
					this->corpus,
					wordBegin - corpusTrimmed,
					this->corpus.size() + corpusTrimmed - wordBegin
			);

			if(endOfLastArticle) {
				++(newDateMap.back().length);
			}

			if(endOfLastDate) {
				++(newArticleMap.back().length);
			}
		}

		// add last sentence if not added yet
		if(!sentence.empty()) {
			if(callbackSentence) {
				// modify sentence
				(*callbackSentence)(sentence);
			}

			if(callbackWord) {
				// modify words of the sentence, do not keep emptied words
				for(auto wordIt{sentence.begin()}; wordIt != sentence.end(); ) {
					(*callbackWord)(*wordIt);

					if(wordIt->empty()) {
						// remove word
						wordIt = sentence.erase(wordIt);

						// shrink article and date, if necessary
						if(endOfLastArticle) {
							--(newArticleMap.back().length);

							if(newArticleMap.back().length == 0) {
								newArticleMap.pop_back();
							}
						}

						if(endOfLastDate) {
							--(newDateMap.back().length);

							if(newDateMap.back().length == 0) {
								newDateMap.pop_back();
							}
						}
					}
					else {
						++wordIt;
					}
				}
			}

			if(!sentence.empty()) {
				// add sentence to map
				this->sentenceMap.emplace_back(
						sentenceFirstWord,
						sentence.size()
				);

				// move the words in the finished sentence into the tokens of the corpus
				this->tokens.insert(
						this->tokens.end(),
						std::make_move_iterator(sentence.begin()),
						std::make_move_iterator(sentence.end())
				);
			}
		}

		std::string{}.swap(this->corpus);

		// check consistency
		if(this->checkConsistency) {
			if(inArticle) {
				throw Exception(
						"Corpus::tokenizeContinuous():"
						" Last article '"
						+ this->articleMap[nextArticle - 1].value
						+ "' has not been finished"
				);
			}

			if(inDate) {
				throw Exception(
						"Corpus::tokenizeContinuous():"
						" Last date '"
						+ this->dateMap[nextDate - 1].value
						+ "' has not been finished"
				);
			}

			if(nextArticle < this->articleMap.size()) {
				throw Exception(
						"Corpus::tokenizeContinuous():"
						" Unexpected article '"
						+ this->articleMap[nextArticle].value
						+ "' after end of corpus"
				);
			}

			if(nextDate < this->dateMap.size()) {
				throw Exception(
						"Corpus::tokenizeContinuous():"
						" Unexpected date '"
						+ this->dateMap[nextDate].value
						+ "' after end of corpus"
				);
			}
		}

		newArticleMap.swap(this->articleMap);
		newDateMap.swap(this->dateMap);

		this->tokenized = true;

		// check consistency
		if(this->checkConsistency) {
			Corpus::checkMap("date map", this->dateMap, this->tokens.size(), true, true);
			Corpus::checkMap("article map", this->articleMap, this->tokens.size(), true, false);
			Corpus::checkMap(this->sentenceMap, this->tokens.size(), true);
		}

		return statusSetter.isRunning();
	}

	// check consistency of tokenized corpus, if necessary, throws Corpus::Exception
	inline void Corpus::checkTokenized() const {
		if(
				this->checkConsistency
				&& !(this->articleMap.empty())
				&& !(this->dateMap.empty())
				&& !(this->sentenceMap.empty())
		) {
			auto article = this->articleMap.cbegin();
			auto sentence = this->sentenceMap.cbegin();

			// go through all dates
			for(auto date{this->dateMap.cbegin()}; date != this->dateMap.cend(); ++date) {
				// jump to first article of date
				while(article != this->articleMap.cend() && article->pos < date->pos) {
					++article;
				}

				// jump to first sentence of date
				while(sentence != this->sentenceMap.cend() && sentence->first < date->pos) {
					++sentence;
				}

				// go through all articles in current date
				const auto dateEnd{date->pos + date->length};

				while(article != this->articleMap.cend() && article->pos < dateEnd) {
					// jump to first sentence of article
					while(sentence != this->sentenceMap.cend() && sentence->first < date->pos) {
						++sentence;
					}

					if(sentence == this->sentenceMap.cend()) {
						break;
					}

					// go through all sentences in current article
					const auto articleEnd{article->pos + article->length};

					while(sentence != this->sentenceMap.cend() && sentence->first < articleEnd) {
						// check whether sentence is out of bounds
						const auto sentenceEnd{sentence->first + sentence->second};

						if(sentenceEnd > dateEnd) {
							std::ostringstream exceptionStrStr;

							exceptionStrStr.imbue(std::locale(""));

							exceptionStrStr <<	"Corpus::checkTokenized():"
												" End of sentence (l=";
							exceptionStrStr <<	sentence->second;
							exceptionStrStr <<	") is behind end of date '";
							exceptionStrStr <<	date->value;
							exceptionStrStr <<	"' (l=";
							exceptionStrStr << date->length;
							exceptionStrStr <<	"): ";
							exceptionStrStr <<	sentenceEnd;

							if(sentenceEnd > 0 && sentenceEnd <= this->tokens.size()) {
								exceptionStrStr <<	" ['";
								exceptionStrStr <<	this->tokens.at(sentenceEnd - 1);
								exceptionStrStr <<	"']";
							}
							else if(sentenceEnd == 0) {
								exceptionStrStr << " [BEGIN]";
							}
							else {
								exceptionStrStr << " [BEHIND]";
							}

							exceptionStrStr <<	" > ";
							exceptionStrStr <<	dateEnd;

							if(dateEnd > 0 && dateEnd <= this->tokens.size()) {
								exceptionStrStr <<	" ['";
								exceptionStrStr <<	this->tokens.at(dateEnd - 1);
								exceptionStrStr <<	"']";
							}
							else if(dateEnd == 0) {
								exceptionStrStr << " [BEGIN]";
							}
							else {
								exceptionStrStr << " [BEHIND]";
							}

							exceptionStrStr <<	" (";
							exceptionStrStr <<	"sentence: '";

							bool addSpace{false};

							for(std::size_t word{sentence->first}; word < sentenceEnd; ++word) {
								if(word < this->tokens.size()) {
									if(addSpace) {
										exceptionStrStr << ' ';
									}
									else {
										addSpace = true;
									}

									exceptionStrStr << this->tokens[word];
								}
							}

							exceptionStrStr <<	"'";

							++date;

							if(date != this->dateMap.cend()) {
								exceptionStrStr << " (next date: '";
								exceptionStrStr << date->value;
								exceptionStrStr << "')";
							}

							throw Exception(exceptionStrStr.str());
						}

						if(sentence->first + sentence->second > articleEnd) {
							std::ostringstream exceptionStrStr;

							exceptionStrStr.imbue(std::locale(""));

							exceptionStrStr <<	"Corpus::checkTokenized():"
												" End of sentence (l=";
							exceptionStrStr <<	sentence->second;
							exceptionStrStr <<	") is behind end of article '";
							exceptionStrStr <<	article->value;
							exceptionStrStr <<	"' (l=";
							exceptionStrStr << article->length;
							exceptionStrStr <<	"): ";
							exceptionStrStr <<	sentenceEnd;

							if(sentenceEnd > 0 && sentenceEnd <= this->tokens.size()) {
								exceptionStrStr <<	" ['";
								exceptionStrStr <<	this->tokens.at(sentenceEnd - 1);
								exceptionStrStr <<	"']";
							}
							else if(sentenceEnd == 0) {
								exceptionStrStr << " [BEGIN]";
							}
							else {
								exceptionStrStr << " [BEHIND]";
							}

							exceptionStrStr <<	" > ";
							exceptionStrStr <<	articleEnd;

							if(articleEnd > 0 && articleEnd <= this->tokens.size()) {
								exceptionStrStr <<	" ['";
								exceptionStrStr <<	this->tokens.at(articleEnd - 1);
								exceptionStrStr <<	"']";
							}
							else if(articleEnd == 0) {
								exceptionStrStr << " [BEGIN]";
							}
							else {
								exceptionStrStr << " [BEHIND]";
							}

							exceptionStrStr <<	" (";
							exceptionStrStr <<	"sentence: '";

							bool addSpace{false};

							for(std::size_t word{sentence->first}; word < sentenceEnd; ++word) {
								if(word < this->tokens.size()) {
									if(addSpace) {
										exceptionStrStr << ' ';
									}
									else {
										addSpace = true;
									}

									exceptionStrStr << this->tokens[word];
								}
							}

							exceptionStrStr <<	"'";

							++article;

							if(article != this->articleMap.cend()) {
								exceptionStrStr << ", next article: '";
								exceptionStrStr << article->value;
								exceptionStrStr << "'";
							}

							exceptionStrStr << ")";

							throw Exception(exceptionStrStr.str());
						}

						// go to next sentence
						++sentence;
					}

					// go to next article
					++article;
				}
			}
		}
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTIONS (private)
	 */

	// get a valid end of the current chunk (without cutting off UTF-8 characters), throws Corpus::Exception
	//	NOTE: the result is between (maxLength - 3) and maxLength and at least zero
	inline std::size_t Corpus::getValidLengthOfChunk(
			const std::string& source,
			std::size_t pos,
			std::size_t maxLength,
			std::size_t maxChunkSize
	) {
		// check arguments
		if(maxLength > maxChunkSize) {
			std::ostringstream exceptionStrStr;

			exceptionStrStr.imbue(std::locale(""));

			exceptionStrStr <<	"Corpus::getValidLengthOfChunk():"
								"Invalid maximum chunk size given"
								" (";
			exceptionStrStr <<	maxLength;
			exceptionStrStr <<	" > ";
			exceptionStrStr <<	maxChunkSize;
			exceptionStrStr <<	")";

			throw Exception(exceptionStrStr.str());
		}

		if(maxChunkSize == 0) {
			throw Exception(
					"Corpus::getValidLengthOfChunk():"
					" Invalid maximum chunk size of zero"
			);
		}

		if(maxLength == 0) {
			return 0;
		}

		// cut a maximum of three bytes
		std::uint8_t cut{0};

		for(; cut < utf8MaxBytes; ++cut) {
			if(cut > maxLength) {
				break;
			}

			// check last four of the remaining characters (if available)
			const auto maxBack{static_cast<std::uint8_t>(cut + utf8MaxBytes)};
			const auto checkFrom{maxLength > maxBack ? pos + maxLength - maxBack : pos};
			const auto checkLength{maxLength > maxBack ? utf8MaxBytes : maxLength - cut};

			if(Helper::Utf8::isLastCharValidUtf8(source.substr(checkFrom, checkLength))) {
				return maxLength - cut;
			}
		}

		if(cut == utf8MaxBytes) {
			throw Exception(
					"Corpus::getValidLengthOfChunk():"
					" Could not slice corpus"
					" because of invalid UTF-8 character"
			);
		}

		if(maxLength >= maxChunkSize) {
			throw Exception(
					"Corpus::getValidLengthOfChunk():"
					" The chunk size is too small"
					" to slice a corpus with UTF-8 character(s)"
			);
		}

		return 0;
	}

	// check article or date map for inconsistencies, throws Corpus::Exception
	inline void Corpus::checkMap(
			std::string_view name,
			const TextMap& map,
			std::size_t corpusSize,
			bool isTokenized,
			bool isDateMap
	) {
		// check the argument
		if(map.empty()) {
			return;
		}

		// check the start positions of all entries in the map
		std::size_t last{0};

		for(const auto& entry : map) {
			if(last > 0 && entry.pos != last) {
				std::ostringstream exceptionStrStr;

				exceptionStrStr.imbue(std::locale(""));

				exceptionStrStr <<	"Corpus::checkMap():"
									" Invalid position #";
				exceptionStrStr << entry.pos;
				exceptionStrStr << " (expected: #";
				exceptionStrStr << last;
				exceptionStrStr << ") in ";
				exceptionStrStr << name;

				throw Exception(exceptionStrStr.str());
			}

			last = entry.pos + entry.length;

			if(!isTokenized) {
				++last;
			}

			if(isDateMap && entry.value.length() != dateLength) {
				std::string exceptionString{
					"Invalid date in date map: '"
				};

				exceptionString +=	entry.value;
				exceptionString +=	"'"
									" (expected string of length ";
				exceptionString +=	std::to_string(dateLength);
				exceptionString +=	") in ";
				exceptionString += name;

				throw Exception(exceptionString);
			}
		}

		// check the end position of the last entry in the map
		const auto& back{map.back()};

		if(back.pos + back.length != corpusSize) {
			std::ostringstream exceptionStrStr;

			exceptionStrStr.imbue(std::locale(""));

			exceptionStrStr <<	"Corpus::checkMap():"
								" Invalid end of last entry in map at #";
			exceptionStrStr <<	back.pos + back.length;
			exceptionStrStr <<	" (expected: at #";
			exceptionStrStr <<	corpusSize;
			exceptionStrStr <<	") in ";
			exceptionStrStr << name;
		}
	}

	// check sentence map for inconsistencies, throws Corpus::Exception
	inline void Corpus::checkMap(
			const SentenceMap& map,
			std::size_t corpusSize,
			bool isTokenized
	) {
		// check the argument
		if(map.empty()) {
			return;
		}

		// check the start positions of all entries in the map
		std::size_t last{0};

		for(const auto& entry : map) {
			if(entry.first != last) {
				std::ostringstream exceptionStrStr;

				exceptionStrStr.imbue(std::locale(""));

				exceptionStrStr <<	"Corpus::checkMap():"
									" Invalid position #";
				exceptionStrStr << entry.first;
				exceptionStrStr << " (expected: #";
				exceptionStrStr << last - 1;
				exceptionStrStr << ") in sentence map";

				throw Exception(exceptionStrStr.str());
			}

			last = entry.first + entry.second;

			if(!isTokenized) {
				++last;
			}
		}

		// check the end position of the last entry in the map
		const auto& back{map.back()};

		if(back.first + back.second != corpusSize) {
			std::ostringstream exceptionStrStr;

			exceptionStrStr.imbue(std::locale(""));

			exceptionStrStr <<	"Corpus::checkMap():"
								" Invalid end of last entry in map at #";
			exceptionStrStr <<	back.first + back.second;
			exceptionStrStr <<	" (expected: at #";
			exceptionStrStr <<	corpusSize;
			exceptionStrStr <<	") in sentence map";
		}
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_CORPUS_HPP_ */
