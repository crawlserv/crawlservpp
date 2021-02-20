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

#include <algorithm>	// std::accumulate, std::all_of, std::count_if, std::find_if, std::remove_if
#include <cstddef>		// std::size_t
#include <cstdint>		// std::int64_t, std::uint8_t, std::uint16_t
#include <functional>	// std::function
#include <iterator>		// std::distance, std::make_move_iterator
#include <locale>		// std::locale
#include <map>			// std::map
#include <optional>		// std::optional
#include <ostream>		// std::ostream
#include <sstream>		// std::ostringstream
#include <string>		// std::string
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

	//! After how many sentences the status is updated when merging corpora.
	inline constexpr auto mergeUpdateEvery{10000};

	//! After how many sentences the status is updated when tokenizing a corpus.
	inline constexpr auto tokenizeUpdateEvery{10000};

	//! After how many articles the status is updated when filtering a corpus (by queries).
	inline constexpr auto filterUpdateEvery{10000};

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
		using ArticleFunc = std::function<bool(const std::vector<std::string>&, std::size_t, std::size_t)>;
		using SentenceFunc = std::function<void(std::vector<std::string>&)>;
		using WordFunc = std::function<void(std::string& token)>;

		using DateArticleSentenceMap = std::map<std::string, std::map<std::string, std::vector<std::vector<std::string>>>>;
		using SentenceMap = std::vector<std::pair<std::size_t, std::size_t>>;
		using PositionLength = std::pair<std::size_t, std::size_t>;
		using SentenceMapEntry = PositionLength;

		///@name Construction
		///@{

		explicit Corpus(bool consistencyChecks);
		Corpus(std::vector<Corpus>& others, bool consistencyChecks, StatusSetter& statusSetter);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] std::string& getCorpus();
		[[nodiscard]] const std::string& getcCorpus() const;

		[[nodiscard]] bool isTokenized() const;
		[[nodiscard]] std::vector<std::string>& getTokens();
		[[nodiscard]] const std::vector<std::string>& getcTokens() const;
		[[nodiscard]] std::size_t getNumTokens() const;

		[[nodiscard]] bool hasArticleMap() const;
		[[nodiscard]] TextMap& getArticleMap();
		[[nodiscard]] const TextMap& getcArticleMap() const;

		[[nodiscard]] bool hasDateMap() const;
		[[nodiscard]] TextMap& getDateMap();
		[[nodiscard]] const TextMap& getcDateMap() const;

		[[nodiscard]] bool hasSentenceMap() const;
		[[nodiscard]] SentenceMap& getSentenceMap();
		[[nodiscard]] const SentenceMap& getcSentenceMap() const;

		[[nodiscard]] std::string get(std::size_t index) const;
		[[nodiscard]] std::string get(const std::string& id) const;
		[[nodiscard]] std::string getDate(const std::string& date) const;
		[[nodiscard]] std::vector<std::string> getTokenized(std::size_t index) const;
		[[nodiscard]] std::vector<std::string> getTokenized(const std::string& id) const;
		[[nodiscard]] std::vector<std::string> getDateTokenized(const std::string& date) const;
		[[nodiscard]] std::vector<std::vector<std::string>> getArticles() const;

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
		std::size_t filterArticles(const ArticleFunc& callbackArticle, StatusSetter& statusSetter);

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
		std::size_t tokenBytes{};

		// lemmatizer and word remover
		Lemmatizer lemmatizer;
		WordRemover wordRemover;

		// internal helper functions
		void moveCombinedIn(DateArticleSentenceMap& from);

		void addArticle(
				std::string& text,
				std::string& id,
				std::string& dateTime,
				TextMapEntry& dateMapEntry,
				bool deleteInputData
		);

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
		void reTokenize();

		void check(std::string_view function) const;
		void checkTokenized(std::string_view function) const;

		// internal static helper functions
		static bool combineCorpora(
				std::vector<Corpus>& from,
				DateArticleSentenceMap& to,
				StatusSetter& statusSetter
		);

		[[nodiscard]] static std::vector<std::string> getTokensForEntry(
				const TextMap& map,
				const std::string& id,
				const std::vector<std::string>& tokens
		);

		static bool addCorpus(
				Corpus& from,
				DateArticleSentenceMap& to,
				std::size_t number,
				std::size_t total,
				StatusSetter& statusSetter
		);
		static bool addSentences(
				Corpus& from,
				DateArticleSentenceMap& to,
				StatusSetter& statusSetter
		);

		static void finishArticle(
				std::vector<std::vector<std::string>>& from,
				DateArticleSentenceMap& to,
				const std::string& date,
				const std::string& article
		);
		static void nextEntry(
				const TextMap& map,
				std::size_t index,
				std::string& nameTo,
				std::size_t& endTo,
				std::size_t corpusEnd
		);

		static std::size_t getFirstEnd(const TextMap& map);
		static std::size_t getEntryEnd(const TextMap& map, std::size_t entryIndex);

		static void skipEntriesBefore(
				const TextMap& map,
				std::size_t& entryIndex,
				std::size_t& entryEnd,
				std::size_t pos,
				bool& inEntryTo
		);

		static void removeToken(TextMap& map, std::size_t entryIndex, bool& emptiedTo);
		static void removeToken(SentenceMapEntry& entry, bool& emptiedTo);

		[[nodiscard]] static std::size_t getValidLengthOfChunk(
				const std::string& source,
				std::size_t pos,
				std::size_t maxLength,
				std::size_t maxChunkSize
		);

		static std::size_t bytes(const std::vector<std::string>& words);

		static void checkMap(
				std::string_view function,
				std::string_view name,
				const TextMap& map,
				std::size_t end,
				bool isTokenized,
				bool isDateMap
		);
		static void checkMap(
				std::string_view function,
				const SentenceMap& map,
				std::size_t end,
				bool isTokenized
		);

		static void locale(std::ostream& os);

		static std::string mergingStatus(std::size_t number, std::size_t total);

		// internal static helper functions for exception throwing
		static std::string exceptionGetNoArticleMap(
				std::string_view function,
				std::size_t article
		);
		static std::string exceptionArticleOutOfBounds(
				std::string_view function,
				std::size_t article,
				std::size_t size
		);
		static std::string exceptionDateLength(
				std::string_view function,
				std::size_t length
		);
		static std::string exceptionArticleMapStart(
				std::string_view function,
				std::string_view expected,
				std::size_t chunkIndex,
				std::size_t numberOfChunks,
				std::size_t start
		);
		static std::string exceptionSentenceMapStart(
				std::size_t chunkIndex,
				std::size_t numberOfChunks,
				std::size_t start
		);
		static std::string exceptionFirstSentenceLength(
				std::size_t chunkIndex,
				std::size_t numberOfChunks,
				std::size_t length,
				std::size_t lastEnd,
				std::size_t offset
		);
		static std::string exceptionLastSentenceLength(
				std::size_t pos,
				std::size_t length,
				std::size_t corpusSize
		);
		static std::string exceptionArticleBehindDate(
				std::size_t articlePos,
				std::size_t datePos,
				std::size_t dateEnd
		);
		static std::string exceptionChunkSize(std::size_t size, std::size_t chunkSize);
		static std::string exceptionArticleMapEnd(std::size_t pos, std::size_t size);
		static std::string exceptionUnexpectedBeforeSentence(
				std::string_view type,
				std::string_view name,
				std::size_t pos,
				std::size_t sentencePos
		);
		static std::string exceptionMismatchWithDate(
				std::string_view type,
				std::size_t pos,
				std::size_t datePos
		);
		static std::string exceptionDateBehindLast(
				std::string_view type,
				std::size_t datePos,
				std::size_t lastPos
		);
		static std::string exceptionSentenceBehind(
				std::string_view function,
				std::string_view type,
				const std::pair<std::size_t, std::size_t>& sentence,
				const TextMapEntry& entry,
				const TextMap& map,
				const TextMap::const_iterator& next,
				const std::vector<std::string>& words
		);
		static std::string exceptionInvalidMaxChunkSize(std::size_t size, std::size_t max);
		static std::string exceptionPositionTooSmall(
				std::size_t pos,
				std::size_t expectedMin,
				std::string_view name
		);
		static std::string exceptionInvalidPosition(
				std::string_view function,
				std::size_t pos,
				std::size_t expected,
				std::string_view name
		);
		static std::string exceptionInvalidDate(
				std::string_view function,
				std::string_view value,
				std::string_view name
		);
		static std::string exceptionInvalidEnd(
				std::string_view function,
				std::size_t pos,
				std::size_t expected,
				std::string_view name
		);

		/*
		 * INTERNAL HELPER FUNCTION TEMPLATES (private)
		 */

		// reserve memory for combined maps
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

		// check whether current map entry begins at current position
		template<typename T> [[nodiscard]] static bool entryBeginsAt(
				const T& map,
				size_t entryIndex,
				size_t pos
		) {
			return entryIndex < map.size() && TextMapEntry::pos(map[entryIndex]) == pos;
		}

		// remove empty entries from map
		template<typename T> static void removeEmptyEntries(
				T& map,
				const std::vector<std::string>& tokens
		) {
			map.erase(
					std::remove_if(
							map.begin(),
							map.end(),
							[&tokens](const auto& entry) {
								const auto end{TextMapEntry::end(entry)};

								for(
										auto tokenIndex{TextMapEntry::pos(entry)};
										tokenIndex < end;
										++tokenIndex
								) {
									if(!(tokens[tokenIndex].empty())) {
										return false;
									}
								}

								return true;
							}
					),
					map.end()
			);
		}

		// skip map entries before current token
		template<typename T> static void skipEntriesBefore(
				const T& map,
				std::size_t& entryIndex,
				PositionLength& origin,
				std::size_t pos
		) {
			if(entryIndex == 0 && !map.empty()) {
				/* first token: set origin to first map entry */
				origin.first = TextMapEntry::pos(map[0]);
				origin.second = TextMapEntry::end(map[0]);
			}

			while(
					entryIndex < map.size()
					&& (
							origin.second <= pos
							|| TextMapEntry::length(map[entryIndex]) == 0
					)
			) {
				++entryIndex;

				if(entryIndex < map.size()) {
					origin.first = TextMapEntry::pos(map[entryIndex]);
					origin.second = TextMapEntry::end(map[entryIndex]);
				}
			}
		}

		// update the position of the current map entry
		template<typename T> static void updatePosition(
				std::string_view type,
				T& map,
				std::size_t entryIndex,
				std::size_t entryPos,
				std::size_t pos,
				std::size_t removed
		) {
			if(
					entryIndex >= map.size() /* end of map reached */
					|| pos != entryPos /* entry not yet or already reached */
			) {
				return;
			}

			if(removed > TextMapEntry::pos(map[entryIndex])) {
				throw Exception(
						Corpus::exceptionPositionTooSmall(
								TextMapEntry::pos(map[entryIndex]),
								removed,
								type
						)
				);
			}

			TextMapEntry::pos(map[entryIndex]) -= removed;
		}

		// decrease the length of the current entry
		template<typename T> static void removeTokenFromLength(
				T& map,
				std::size_t entryIndex,
				const PositionLength& origin,
				std::size_t tokenIndex
		) {
			if(
					entryIndex < map.size()
					&& tokenIndex >= origin.first
					&& tokenIndex < origin.second
			) {
				--(TextMapEntry::length(map[entryIndex]));
			}
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

	//! Constructor creating a tokenized corpus from multiple other tokenized corpora.
	/*!
	 * Moves the data out of the previous corpora
	 *  and frees their memory.
	 *
	 * \param sources Reference to a vector containing
	 *  the tokenized corpora.
	 * \param statusSetter Reference to a
	 *   Struct::StatusSetter to be used for updating
	 *   the status while combining the sources.
	 *
	 * \throws Corpus::Exception if one of the corpora
	 *   is not tokenized and non-empty, or if one of
	 *   their date, article, and/or sentence maps are
	 *   incoherent.
	 */
	inline Corpus::Corpus(
			std::vector<Corpus>& others,
			bool consistencyChecks,
			StatusSetter& statusSetter
	) : checkConsistency(consistencyChecks) {
		// check arguments
		if(others.empty()) {
			return;
		}

		if(others.size() == 1) {
			std::swap(*this, others[0]);

			return;
		}

		// combine corpora
		DateArticleSentenceMap combined;

		if(!Corpus::combineCorpora(others, combined, statusSetter)) {
			return;
		}

		// move resulting corpus into class
		statusSetter.change("Preparing combined corpus...");

		this->moveCombinedIn(combined);
	}

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

	//! Checks whether the corpus has an article map.
	/*!
	 * \returns True, if the corpus has an
	 *   article map, i.e. consists of
	 *   articles and is not empty.
	 */
	inline bool Corpus::hasArticleMap() const {
		return !(this->articleMap.empty());
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

	//! Checks whether the corpus has a date map.
	/*!
	 * \returns True, if the corpus has a
	 *   date map, i.e. consists of dates
	 *   and is not empty.
	 */
	inline bool Corpus::hasDateMap() const {
		return !(this->dateMap.empty());
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

	//! Checks whether the corpus has sentence map.
	/*!
	 * \returns True, if the corpus has a
	 *   sentence map, i.e. consists of
	 *   sentences and is not empty.
	 */
	inline bool Corpus::hasSentenceMap() const {
		return !(this->sentenceMap.empty());
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
	 *   map of the corpus is empty, the given
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
			throw Exception(
					Corpus::exceptionGetNoArticleMap(
							"get",
							index
					)
			);
		}

		if(index >= this->articleMap.size()) {
			throw Exception(
					Corpus::exceptionArticleOutOfBounds(
							"get",
							index,
							this->articleMap.size()
					)
			);
		}

		const auto& articleEntry{this->articleMap.at(index)};

		return this->corpus.substr(
				TextMapEntry::pos(articleEntry),
				TextMapEntry::length(articleEntry)
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
				articleEntry->p,
				articleEntry->l
		);
	}

	//! Gets all articles at the specified date from a continous text corpus.
	/*!
	 * \param date A constant reference to a string
	 *   containing the date of the articles to be
	 *   retrieved, as specified in the date map
	 *   of the text corpus. The string should have
	 *   the format @c YYYY-MM-DD.
	 *
	 * \returns A copy of all concatenated articles at
	 *   the given date, or an empty string if no
	 *   articles have been found at this date.
	 *
	 * \throws Corpus::Exception if the given date
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
			throw Exception(
					Corpus::exceptionDateLength(
							"getDate",
							date.length()
					)
			);
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
				dateEntry->p,
				dateEntry->l
		);
	}

	//! Gets the article with the specified index from a tokenized text corpus.
	/*!
	 * \param index The index of the article in
	 *   the article map of the text corpus,
	 *   starting with zero.
	 *
	 * \returns A vector containing copies of the
	 *   tokens of the article inside the text
	 *   corpus with the given index.
	 *
	 * \throws Corpus::Exception if the article
	 *   map of the corpus is empty, the given
	 *   index is out of bounds (larger than the
	 *   article map), or if the corpus has
	 *   not been tokenized.
	 */
	inline std::vector<std::string> Corpus::getTokenized(std::size_t index) const {
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::getTokenized():"
					" The corpus has not been tokenized"
			);
		}

		if(this->articleMap.empty()) {
			throw Exception(
					Corpus::exceptionGetNoArticleMap(
							"getTokenized",
							index
					)
			);
		}

		if(index >= this->articleMap.size()) {
			throw Exception(
					Corpus::exceptionArticleOutOfBounds(
							"getTokenized",
							index,
							this->articleMap.size()
					)
			);
		}

		const auto& articleEntry{this->articleMap.at(index)};
		const auto articleEnd{TextMapEntry::end(articleEntry)};

		std::vector<std::string> copy;

		copy.reserve(TextMapEntry::length(articleEntry));

		for(auto tokenIndex{TextMapEntry::pos(articleEntry)}; tokenIndex < articleEnd; ++tokenIndex) {
			copy.emplace_back(this->tokens[tokenIndex]);
		}

		return copy;
	}

	//! Gets the article with the specified ID from a tokenized corpus.
	/*!
	 * \param id A constant reference to a string
	 *   containing the ID of the article to be
	 *   retrieved, as specified in the article map
	 *   of the text corpus.
	 *
	 * \returns A vector containing copies of the
	 *   tokens of the article with the given ID,
	 *   or an empty vector if an article with this
	 *   ID does not exist.
	 *
	 * \throws Corpus::Exception if no ID has
	 *   been specified, i.e. \p id references
	 *   an empty string, or if the corpus has
	 *   not been tokenized.
	 */
	inline std::vector<std::string> Corpus::getTokenized(const std::string& id) const {
		// check corpus
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::getTokenized():"
					" The corpus has not been tokenized"
			);
		}

		// check argument
		if(id.empty()) {
			throw Exception(
					"Corpus::getTokenized():"
					" No ID has been specified"
			);
		}

		return Corpus::getTokensForEntry(this->articleMap, id, this->tokens);
	}

	//! Gets the tokens of all articles at the specified date from a tokenized text corpus.
	/*!
	 * \param date A constant reference to a string
	 *   containing the date of the articles to be
	 *   retrieved, as specified in the date map
	 *   of the text corpus. The string should have
	 *   the format @c YYYY-MM-DD.
	 *
	 * \returns A vector containing copies of all
	 *   tokens of the articles at the given date,
	 *   or an empty vector if no articles have been
	 *   found at this date.
	 *
	 * \throws Corpus::Exception if the given date
	 *   has an invalid length, or if the corpus has
	 *   not been tokenized.
	 */
	inline std::vector<std::string> Corpus::getDateTokenized(const std::string& date) const {
		// check corpus
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::getDateTokenized():"
					" The corpus has not been tokenized."
			);
		}

		// check argument
		if(date.length() != dateLength) {
			throw Exception(
					Corpus::exceptionDateLength(
							"getDateTokenized",
							date.length()
					)
			);
		}

		return Corpus::getTokensForEntry(this->dateMap, date, this->tokens);
	}

	//! Gets the tokens of all articles from a tokenized corpus.
	/*!
	 * \returns A vector containing all articles, each
	 *   represented by a vector containing copies of
	 *   their tokens.
	 *
	 * \throws Corpus::Exception if the corpus has not
	 *   been tokenized.
	 */
	inline std::vector<std::vector<std::string>> Corpus::getArticles() const {
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::getArticles():"
					" The corpus has not been tokenized"
			);
		}

		std::vector<std::vector<std::string>> copy;

		copy.reserve(this->articleMap.size());

		for(const auto& article : this->articleMap) {
			copy.emplace_back();

			copy.back().reserve(TextMapEntry::length(article));

			const auto articleEnd{TextMapEntry::end(article)};

			for(auto tokenIndex{TextMapEntry::pos(article)}; tokenIndex < articleEnd; ++tokenIndex) {
				copy.back().emplace_back(this->tokens[tokenIndex]);
			}
		}

		return copy;
	}

	//! Gets the size of the text corpus, in bytes.
	/*!
	 * \note The number of characters in the corpus
	 *   may differ, as it might be UTF-8-encoded.
	 *
	 * \returns The size of the corpus, in bytes.
	 */
	inline std::size_t Corpus::size() const {
		return this->tokenized ? this->tokenBytes : this->corpus.size();
	}

	//! Checks whether the corpus is empty.
	/*!
	 * \note If the corpus is tokenized,
	 *   the tokens will be checked whether
	 *   all of them are empty.
	 *
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

		std::string emptyString;
		TextMapEntry dateMapEntry;

		for(std::size_t n{}; n < texts.size(); ++n) {
			this->addArticle(
					texts.at(n),
					articleIds.size() > n ? articleIds.at(n) : emptyString,
					dateTimes.size() > n ? dateTimes.at(n) : emptyString,
					dateMapEntry,
					deleteInputData
			);
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
					if(this->checkConsistency && TextMapEntry::pos(first) > 1) {
						throw Exception(
								Corpus::exceptionArticleMapStart(
										"combineContinuous",
										"#0 or #1",
										chunkIndex,
										chunks.size(),
										TextMapEntry::pos(first)
								)
						);
					}

					auto it{map.cbegin()};

					// compare first new article ID with last one
					if(
							!(this->articleMap.empty())
							&& this->articleMap.back().value == first.value
					) {
						// append current article to last one
						TextMapEntry::length(this->articleMap.back()) += TextMapEntry::length(first);

						++it;
					}
					else {
						beginsWithNewArticle = true;
					}

					// add remaining articles to map
					for(; it != map.cend(); ++it) {
						this->articleMap.emplace_back(
								pos + it->p,
								it->l,
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
						TextMapEntry::length(this->dateMap.back()) += TextMapEntry::length(first);

						// add space between articles if chunk begins with new article and date has been extended
						if(beginsWithNewArticle) {
							++TextMapEntry::length(this->dateMap.back());
						}

						++it;
					}

					// add remaining dates to map
					for(; it != map.cend(); ++it) {
						this->dateMap.emplace_back(
								pos + it->p,
								it->l,
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
			std::size_t begin{};

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
					if(this->checkConsistency && TextMapEntry::pos(first) > 0) {
						throw Exception(
								Corpus::exceptionSentenceMapStart(
										chunkIndex,
										chunks.size(),
										TextMapEntry::pos(first)
								)
						);
					}

					auto it{map.cbegin()};

					if(!(this->sentenceMap.empty())) {
						// check whether the last sentence map already includes the first sentence
						const auto lastSentenceEnd{TextMapEntry::end(this->sentenceMap.back())};

						if(lastSentenceEnd > chunkOffset) {
							// consistency check
							if(
									this->checkConsistency
									&&
									first.second
									!= lastSentenceEnd
									- chunkOffset
							) {
								throw Exception(
										Corpus::exceptionFirstSentenceLength(
												chunkIndex,
												chunks.size(),
												first.second,
												lastSentenceEnd,
												chunkOffset
										)
								);
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
				if(TextMapEntry::end(this->sentenceMap.back()) == this->tokens.size() + 1) {
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
					if(this->checkConsistency && TextMapEntry::pos(first) > 0) {
						throw Exception(
								Corpus::exceptionArticleMapStart(
										"combineTokenized",
										"#0",
										chunkIndex,
										chunks.size(),
										TextMapEntry::pos(first)
								)
						);
					}

					auto it{map.cbegin()};

					// compare first new article ID with previous one
					if(
							!(this->articleMap.empty())
							&& this->articleMap.back().value == first.value
					) {
						// append current article to previous one
						TextMapEntry::length(this->articleMap.back()) += TextMapEntry::length(first);

						if(!skipSeparator) {
							// remove second part of previous token
							--TextMapEntry::length(this->articleMap.back());
						}

						++it;
					}

					// add remaining articles to map
					for(; it != map.cend(); ++it) {
						this->articleMap.emplace_back(
								chunkOffset + it->p,
								it->l,
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
						TextMapEntry::length(this->dateMap.back()) += TextMapEntry::length(first);

						if(!skipSeparator) {
							// remove second part of previous token
							--TextMapEntry::length(this->dateMap.back());
						}

						++it;
					}

					// add remaining dates to map
					for(; it != map.cend(); ++it) {
						this->dateMap.emplace_back(
								chunkOffset + it->p,
								it->l,
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
				&& TextMapEntry::end(this->sentenceMap.back()) > this->tokens.size()
		) {
			throw Exception(
					Corpus::exceptionLastSentenceLength(
							TextMapEntry::pos(this->sentenceMap.back()),
							TextMapEntry::length(this->sentenceMap.back()),
							this->tokens.size()
					)
			);
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
		if(this->checkConsistency) {
			this->check("combineTokenized");
		}
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
			std::size_t pos{};

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
			std::size_t corpusPos{};
			std::size_t articlePos{};
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
								&& articleIt->p > TextMapEntry::end(*dateIt)
						) {
							++dateIt;
						}

						if(
								this->checkConsistency
								&&
								articleIt->p
								> dateIt->p
								+ dateIt->l
						) {
							throw Exception(
									Corpus::exceptionArticleBehindDate(
											articleIt->p,
											dateIt->p,
											TextMapEntry::end(*dateIt)
									)
							);
						}
					}

					// get remaining article length
					const auto remaining{articleIt->l - articlePos};

					if(chunk.size() + remaining <= chunkSize) {
						if(remaining > 0) {
							// add the remainder of the article to the chunk
							chunkArticleMap.emplace_back(chunk.size(), remaining, articleIt->value);

							if(dateIt != this->dateMap.cend()) {
								if(!chunkDateMap.empty() && chunkDateMap.back().value == dateIt->value) {
									/* including space before article */
									TextMapEntry::length(chunkDateMap.back()) += remaining + 1;
								}
								else if(corpusPos >= dateIt->p) {
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
								/* including space before the article */
								TextMapEntry::length(chunkDateMap.back()) += fill + 1;
							}
							else if(corpusPos >= dateIt->p) {
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
						throw Exception(Corpus::exceptionChunkSize(chunk.size(), chunkSize));
					}

					if(articleIt == this->articleMap.cend() && corpusPos < this->corpus.size()) {
						throw Exception(Corpus::exceptionArticleMapEnd(corpusPos, this->corpus.size()));
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
		std::size_t chunkOffset{};
		std::size_t chunkNumCompleteTokens{};
		std::size_t nextArticleIndex{};
		std::size_t nextDateIndex{};
		bool reserveMemory{true};

		for(const auto& sentence : this->sentenceMap) {
			// save previous length of chunk
			const auto oldChunkSize{
				currentChunk.size()
			};

			if(reserveMemory) {
				// reserve memory for chunk
				std::size_t bytes{};

				for(
						auto tokenIt = this->tokens.begin() + TextMapEntry::pos(sentence);
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
					auto tokenIt = this->tokens.begin() + TextMapEntry::pos(sentence);
					tokenIt != this->tokens.begin() + TextMapEntry::end(sentence);
					++tokenIt
			) {
				currentChunk += *tokenIt;

				currentChunk.push_back('\n');
			}

			chunkNumCompleteTokens += sentence.second;

			// check for beginning of next article
			if(nextArticleIndex < this->articleMap.size()) {
				while(
						TextMapEntry::pos(this->articleMap.at(nextArticleIndex))
						== TextMapEntry::pos(sentence)
				) {
					const auto& nextArticle = this->articleMap.at(nextArticleIndex);

					if(TextMapEntry::length(nextArticle) > 0) {
						currentArticleMap.emplace_back(
								TextMapEntry::pos(nextArticle) - chunkOffset,
								TextMapEntry::length(nextArticle),
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
						&& TextMapEntry::pos(this->articleMap.at(nextArticleIndex))
						< TextMapEntry::pos(sentence)
				) {
					const auto& nextArticle = this->articleMap.at(nextArticleIndex);

					throw Exception(
							Corpus::exceptionUnexpectedBeforeSentence(
									"article",
									nextArticle.value,
									TextMapEntry::pos(nextArticle),
									TextMapEntry::pos(sentence)
							)
					);
				}
			}

			// check for beginning of next date
			if(nextDateIndex < this->dateMap.size()) {
				while(
						TextMapEntry::pos(this->dateMap.at(nextDateIndex))
						== TextMapEntry::pos(sentence)
				) {
					const auto& nextDate = this->dateMap.at(nextDateIndex);

					if(TextMapEntry::length(nextDate) > 0) {
						currentDateMap.emplace_back(
								TextMapEntry::pos(nextDate) - chunkOffset,
								TextMapEntry::length(nextDate),
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
						&& TextMapEntry::pos(this->dateMap.at(nextDateIndex))
						< TextMapEntry::pos(sentence)
				) {
					const auto& nextDate = this->dateMap.at(nextDateIndex);

					throw Exception(
							Corpus::exceptionUnexpectedBeforeSentence(
									"date",
									nextDate.value,
									TextMapEntry::pos(nextDate),
									TextMapEntry::pos(sentence)
							)
					);
				}
			}

			// add current sentence
			currentSentenceMap.emplace_back(
					TextMapEntry::pos(sentence) - chunkOffset,
					TextMapEntry::length(sentence)
			);

			if(currentChunk.size() >= chunkSize) {
				currentChunk.pop_back(); /* remove last newline */

				std::string rest;
				std::size_t restNumTokens{};
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

					const auto sentenceEnd{TextMapEntry::end(sentence)};
					auto chunkEnd{oldChunkSize};

					for(auto token{TextMapEntry::pos(sentence)}; token < sentenceEnd; ++token) {
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
					const auto articleEnd{TextMapEntry::end(currentArticleMap.back())};

					if(
							(articleEnd > chunkNumCompleteTokens)
							|| (splitToken && articleEnd == chunkNumCompleteTokens)
					) {
						TextMapEntry::length(articleRest) = articleEnd - chunkNumCompleteTokens;
						articleRest.value = currentArticleMap.back().value;

						TextMapEntry::length(currentArticleMap.back()) = chunkNumCompleteTokens
								- TextMapEntry::pos(currentArticleMap.back());

						if(splitToken) {
							++TextMapEntry::length(currentArticleMap.back());
						}
					}
				}

				if(!currentDateMap.empty()) {
					// split date, if necessary
					const auto dateEnd{TextMapEntry::end(currentDateMap.back())};

					if(
							(dateEnd > chunkNumCompleteTokens)
							|| (splitToken && dateEnd == chunkNumCompleteTokens)
					) {
						TextMapEntry::length(dateRest) = dateEnd - chunkNumCompleteTokens;
						dateRest.value = currentDateMap.back().value;

						TextMapEntry::length(currentDateMap.back()) = chunkNumCompleteTokens
								- TextMapEntry::pos(currentDateMap.back());

						if(splitToken) {
							++TextMapEntry::length(currentDateMap.back());
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

				if(TextMapEntry::length(articleRest) > 0) {
					currentArticleMap.emplace_back(std::move(articleRest));
				}

				if(TextMapEntry::length(dateRest) > 0) {
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

	//! Filters a text corpus by the given date(s).
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
	 * \throws Corpus::Exception if consistency
	 *   checks have been enabled on
	 *   construction and the date map does not
	 *   start at the beginning of the corpus,
	 *   the date map and the article map
	 *   contradict each other, or they contain
	 *   other inconsistencies.
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
		const auto offset{TextMapEntry::pos(this->dateMap.front())};
		const auto len{TextMapEntry::end(this->dateMap.back()) - offset};

		// trim corpus
		if(this->tokenized) {
			// (and calculate new size, in bytes)
			std::size_t deleteBytes{};

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
			if(begin->p == offset) {
				break;
			}

			// consistency check
			if(this->checkConsistency && begin->p > offset) {
				throw Exception(
						Corpus::exceptionMismatchWithDate(
								"article",
								begin->p,
								offset
						)
				);
			}
		}

		// consistency check
		if(this->checkConsistency && begin == this->articleMap.cend()) {
			throw Exception(
					Corpus::exceptionDateBehindLast(
							"article",
							offset,
							TextMapEntry::pos(this->articleMap.back())
					)
			);
		}

		// find first article not in range anymore
		end = begin;

		++end; /* current article is in range as has already been checked */

		for(; end != this->articleMap.cend(); ++end) {
			if(end->p >= offset + len) {
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
					throw Exception(
							Corpus::exceptionMismatchWithDate(
									"sentence",
									smBegin->first,
									offset
							)
					);
				}
			}

			// consistency check
			if(this->checkConsistency && smBegin == this->sentenceMap.cend()) {
				throw Exception(
						Corpus::exceptionDateBehindLast(
								"sentence",
								offset,
								TextMapEntry::pos(this->sentenceMap.back())
						)
				);
			}

			// find first sentence not in range anymore
			auto smEnd = smBegin;

			++smEnd; /* current sentence is in range as has already been checked */

			for(; smEnd != this->sentenceMap.cend(); ++smEnd) {
				if(smEnd->first >= offset + len) {
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
			TextMapEntry::pos(date) -= offset;
		}

		for(auto& article : this->articleMap) {
			TextMapEntry::pos(article) -= offset;
		}

		for(auto& sentence : this->sentenceMap) {
			TextMapEntry::pos(sentence) -= offset;
		}

		if(this->checkConsistency) {
			this->check("filterByDate");
		}

		return true;
	}

	//! Filters a tokenized corpus by removing articles.
	/*!
	 * \param callbackArticle Callback
	 *   function that returns whether to
	 *   keep the given article. It will
	 *   receive a constant reference to
	 *   all tokens in the corpus, as well
	 *   as the start and the length of
	 *   the article. If it the callback
	 *   function returns true, the given
	 *   article will be kept. If not, its
	 *   tokens will be removed.
	 * \param statusSetter Reference to a
	 *   structure containing callbacks
	 *   for updating the status and
	 *   checking whether the thread is
	 *   still supposed to be running.
	 *
	 * \returns The number of articles
	 *   that have been removed, or zero
	 *   if no changes have been made to
	 *   the corpus.
	 *
	 * \throws Corpus::Exception if the
	 *   corpus has not been tokenized.
	 */
	inline std::size_t Corpus::filterArticles(const ArticleFunc& callbackArticle, StatusSetter& statusSetter) {
		if(!(this->tokenized)) {
			throw Exception("Corpus::filterArticle(): Corpus has not been tokenized");
		}

		if(this->tokens.empty()) {
			return 0;
		}

		statusSetter.change("Filtering corpus...");

		std::size_t articleCounter{};
		std::size_t statusCounter{};
		std::size_t removed{};

		for(const auto& article : this->articleMap) {
			if(
					!callbackArticle(
							this->tokens,
							TextMapEntry::pos(article),
							TextMapEntry::length(article)
					)
			) {
				// empty all tokens belonging to the article that has been filtered out
				const auto articleEnd{TextMapEntry::end(article)};

				for(
						std::size_t tokenIndex{TextMapEntry::pos(article)};
						tokenIndex < articleEnd;
						++tokenIndex
				) {
					this->tokenBytes -= this->tokens[tokenIndex].size();

					std::string{}.swap(this->tokens[tokenIndex]);
				}

				++removed;
			}

			++articleCounter;
			++statusCounter;

			if(statusCounter == filterUpdateEvery) {
				if(!statusSetter.update(articleCounter, this->articleMap.size(), true)) {
					return 0;
				}

				statusCounter = 0;
			}
		}

		statusSetter.finish();

		if(removed == 0) {
			return 0;
		}

		// remove emptied dates, articles, and tokens
		this->reTokenize();

		// check consistency, if necessary
		if(this->checkConsistency) {
			this->check("filterArticles");
		}

		return removed;
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

	// move data of combined corpus into class
	inline void Corpus::moveCombinedIn(DateArticleSentenceMap& from) {
		for(auto& [date, articles] : from) {
			std::size_t dateLength{};

			this->dateMap.emplace_back();

			TextMapEntry::pos(this->dateMap.back()) = this->tokens.size();
			this->dateMap.back().value = date;

			for(auto& [article, sentences] : articles) {
				std::size_t articleLength{};

				this->articleMap.emplace_back();

				TextMapEntry::pos(this->articleMap.back()) = this->tokens.size();
				this->articleMap.back().value = article;

				for(auto& sentence : sentences) {
					this->sentenceMap.emplace_back(
							this->tokens.size(),
							sentence.size()
					);

					dateLength += sentence.size();
					articleLength += sentence.size();

					this->tokenBytes += Corpus::bytes(sentence);

					this->tokens.insert(
							this->tokens.end(),
							std::make_move_iterator(sentence.begin()),
							std::make_move_iterator(sentence.end())
					);
				}

				TextMapEntry::length(this->articleMap.back()) = articleLength;
			}

			TextMapEntry::length(this->dateMap.back()) = dateLength;
		}

		this->tokenized = true;
	}

	// add an article to the (continuous) corpus
	inline void Corpus::addArticle(
			std::string& text,
			std::string& id,
			std::string& dateTime,
			TextMapEntry& dateMapEntry,
			bool deleteInputData
	) {
		auto pos{this->corpus.size()};

		// add article ID (or empty article) to article map
		if(!id.empty()) {
			this->articleMap.emplace_back(pos, text.length(), id);

			if(deleteInputData) {
				// free memory early
				std::string{}.swap(id);
			}
		}
		else if(!(this->articleMap.empty()) && this->articleMap.back().value.empty()) {
			// expand empty article in the end of the article map
			//  (including space before current text)
			TextMapEntry::length(this->articleMap.back()) += text.length() + 1;
		}
		else {
			// add empty article to the end of the article map
			this->articleMap.emplace_back(pos, text.length());
		}

		// add date to date map if necessary
		if(!dateTime.empty()) {
			// check for valid (long enough) date/time
			if(dateTime.length() >= dateLength) {
				// get only date (YYYY-MM-DD) from date/time
				const std::string date(dateTime, 0, dateLength);

				// check whether a date is already set
				if(!dateMapEntry.value.empty()) {
					// date is already set -> compare with current date
					if(dateMapEntry.value == date) {
						// last date equals current date -> append text to last date
						//  (including space before current text)
						TextMapEntry::length(dateMapEntry) += text.length() + 1;
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

				TextMapEntry{}.swap(dateMapEntry);
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

	// tokenize already tokenized corpus, return whether thread is still running
	inline bool Corpus::tokenizeTokenized(
			std::optional<SentenceFunc> callbackSentence,
			std::optional<WordFunc> callbackWord,
			StatusSetter& statusSetter
	) {
		// run manipulators on already tokenized corpus
		if(!callbackSentence && !callbackWord) {
			return statusSetter.isRunning();
		}

		std::size_t numDeletedTokens{};
		std::size_t dateIndex{};
		std::size_t articleIndex{};
		std::size_t dateEnd{};
		std::size_t articleEnd{};
		std::size_t sentenceCounter{};
		std::size_t statusCounter{};
		bool inDate{false};
		bool inArticle{false};
		bool emptyDates{false};
		bool emptyArticles{false};
		bool emptySentences{false};

		// reset number of bytes
		this->tokenBytes = 0;

		// go through all sentences
		for(auto& sentenceEntry : this->sentenceMap) {
			// skip dates and articles before current sentence
			//  (including last date and article, if finished)
			Corpus::skipEntriesBefore(
					this->dateMap,
					dateIndex,
					dateEnd,
					TextMapEntry::pos(sentenceEntry),
					inDate
			);
			Corpus::skipEntriesBefore(
					this->articleMap,
					articleIndex,
					articleEnd,
					TextMapEntry::pos(sentenceEntry),
					inArticle
			);

			// check for beginning of date and/or article
			if(
					Corpus::entryBeginsAt(
							this->dateMap,
							dateIndex,
							TextMapEntry::pos(sentenceEntry)
					)
			) {
				inDate = true;

				// update beginning of date
				TextMapEntry::pos(this->dateMap[dateIndex]) -= numDeletedTokens;
			}

			if(
					Corpus::entryBeginsAt(
							this->articleMap,
							articleIndex,
							TextMapEntry::pos(sentenceEntry)
					)
			) {
				inArticle = true;

				// update beginning of article
				TextMapEntry::pos(this->articleMap[articleIndex]) -= numDeletedTokens;
			}

			// store unchanged sentence data
			const auto sentenceBegin{TextMapEntry::pos(sentenceEntry)};
			const auto sentenceEnd{TextMapEntry::end(sentenceEntry)};

			// update beginning of sentence
			TextMapEntry::pos(sentenceEntry) -= numDeletedTokens;

			std::vector<std::string> sentence(
					this->tokens.begin() + sentenceBegin,
					this->tokens.begin() + sentenceEnd
			);

			// process sentence if necessary
			if(callbackSentence) {
				(*callbackSentence)(sentence);
			}

			for(auto n{sentenceBegin}; n < sentenceEnd; ++n) {
				auto& token{
					this->tokens.at(n)
				};

				// process word if necessary
				if(callbackWord) {
					(*callbackWord)(token);
				}

				// remove empty word from date, article, and sentence map
				if(token.empty()) {
					if(inDate) {
						Corpus::removeToken(this->dateMap, dateIndex, emptyDates);
					}

					if(inArticle) {
						Corpus::removeToken(this->articleMap, articleIndex, emptyArticles);
					}

					Corpus::removeToken(sentenceEntry, emptySentences);

					// delete token later
					++numDeletedTokens;
				}
				else {
					this->tokenBytes += token.size();
				}
			}

			++sentenceCounter;
			++statusCounter;

			if(statusCounter == tokenizeUpdateEvery) {
				if(!statusSetter.update(sentenceCounter, this->sentenceMap.size(), true)) {
					return false;
				}

				statusCounter = 0;
			}
		}

		statusSetter.change("Cleaning corpus...");

		// delete empty dates
		if(emptyDates) {
			this->dateMap.erase(
					std::remove_if(
							this->dateMap.begin(),
							this->dateMap.end(),
							[](const auto& date) {
								return TextMapEntry::length(date) == 0;
							}
					),
					this->dateMap.end()
			);
		}

		// delete empty articles
		if(emptyArticles) {
			this->articleMap.erase(
					std::remove_if(
							this->articleMap.begin(),
							this->articleMap.end(),
							[](const auto& article) {
								return TextMapEntry::length(article) == 0;
							}
					),
					this->articleMap.end()
			);
		}

		// delete empty sentences
		if(emptySentences) {
			this->sentenceMap.erase(
					std::remove_if(
							this->sentenceMap.begin(),
							this->sentenceMap.end(),
							[](const auto& sentence) {
								return TextMapEntry::length(sentence) == 0;
							}
					),
					this->sentenceMap.end()
			);
		}

		// delete empty tokens
		if(numDeletedTokens > 0) {
			this->tokens.erase(
					std::remove_if(
							this->tokens.begin(),
							this->tokens.end(),
							[](const auto& token) {
								return token.empty();
							}
					),
					this->tokens.end()
			);
		}

		// check consistency
		if(this->checkConsistency) {
			this->check("tokenizeTokenized");
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

		std::size_t wordBegin{};
		std::size_t sentenceFirstWord{};
		std::size_t currentWord{};
		std::size_t statusCounter{};
		std::size_t corpusTrimmed{};

		bool inArticle{false};
		bool inDate{false};

		std::size_t articleFirstWord{};
		std::size_t dateFirstWord{};
		std::size_t articleEnd{Corpus::getFirstEnd(this->articleMap)};
		std::size_t dateEnd{Corpus::getFirstEnd(this->dateMap)};
		std::size_t nextArticle{};
		std::size_t nextDate{};

		TextMap newArticleMap;
		TextMap newDateMap;

		newArticleMap.reserve(this->articleMap.size());
		newDateMap.reserve(this->dateMap.size());

		// go through all characters in the continous text corpus
		for(std::size_t pos{}; pos < this->corpus.size() + corpusTrimmed; ++pos) {
			bool sentenceEnd{false};
			bool noSeparator{false};
			bool appendToArticle{false};
			bool appendToDate{false};

			if(!(this->articleMap.empty())) {
				// check for beginning of article
				if(
						!inArticle
						&& nextArticle < this->articleMap.size()
						&& pos == TextMapEntry::pos(this->articleMap[nextArticle])
				) {
					articleFirstWord = currentWord;
					articleEnd = TextMapEntry::end(this->articleMap[nextArticle]);

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
						&& pos == TextMapEntry::pos(this->dateMap[nextDate])
				) {
					dateFirstWord = currentWord;
					dateEnd = TextMapEntry::end(this->dateMap[nextDate]);

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
			case '&':
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
					++TextMapEntry::length(newArticleMap.back());
				}

				if(appendToDate) {
					++TextMapEntry::length(newDateMap.back());
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
							--TextMapEntry::length(newArticleMap.back());

							if(TextMapEntry::length(newArticleMap.back()) == 0) {
								newArticleMap.pop_back();
							}
						}

						if(appendToDate) {
							--TextMapEntry::length(newDateMap.back());

							if(TextMapEntry::length(newDateMap.back()) == 0) {
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
					if(
							!statusSetter.update(
									pos + 1,
									this->corpus.size() + corpusTrimmed,
									true
							)
					) {
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
				++TextMapEntry::length(newDateMap.back());
			}

			if(endOfLastDate) {
				++TextMapEntry::length(newArticleMap.back());
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
							--TextMapEntry::length(newArticleMap.back());

							if(TextMapEntry::length(newArticleMap.back()) == 0) {
								newArticleMap.pop_back();
							}
						}

						if(endOfLastDate) {
							--TextMapEntry::length(newDateMap.back());

							if(TextMapEntry::length(newDateMap.back()) == 0) {
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
			this->check("tokenizeContinuous");
		}

		return statusSetter.isRunning();
	}

	// re-tokenize corpus, removing all empty tokens, articles, and dates
	inline void Corpus::reTokenize() {
		// remove empty entries from maps
		Corpus::removeEmptyEntries(this->dateMap, this->tokens);
		Corpus::removeEmptyEntries(this->articleMap, this->tokens);
		Corpus::removeEmptyEntries(this->sentenceMap, this->tokens);

		// remove empty tokens from maps
		std::size_t dateIndex{};
		std::size_t articleIndex{};
		std::size_t sentenceIndex{};
		PositionLength originDate{};
		PositionLength originArticle{};
		PositionLength originSentence{};
		std::size_t removed{};

		for(std::size_t tokenIndex{}; tokenIndex < this->tokens.size(); ++tokenIndex) {
			Corpus::skipEntriesBefore(this->dateMap, dateIndex, originDate, tokenIndex);
			Corpus::skipEntriesBefore(this->articleMap, articleIndex, originArticle, tokenIndex);
			Corpus::skipEntriesBefore(this->sentenceMap, sentenceIndex, originSentence, tokenIndex);

			Corpus::updatePosition(
					"date map",
					this->dateMap,
					dateIndex,
					originDate.first,
					tokenIndex,
					removed
			);
			Corpus::updatePosition(
					"article map",
					this->articleMap,
					articleIndex,
					originArticle.first,
					tokenIndex,
					removed
			);
			Corpus::updatePosition(
					"sentence map",
					this->sentenceMap,
					sentenceIndex,
					originSentence.first,
					tokenIndex,
					removed
			);

			if(this->tokens[tokenIndex].empty()) {
				Corpus::removeTokenFromLength(this->dateMap, dateIndex, originDate, tokenIndex);
				Corpus::removeTokenFromLength(this->articleMap, articleIndex, originArticle, tokenIndex);
				Corpus::removeTokenFromLength(this->sentenceMap, sentenceIndex, originSentence, tokenIndex);

				++removed;
			}
		}

		// remove empty tokens
		this->tokens.erase(
				std::remove_if(
						this->tokens.begin(),
						this->tokens.end(),
						[](const auto& token) {
							return token.empty();
						}
				),
				this->tokens.end()
		);
	}

	// check consistency of corpus after manipulation, throws Corpus::Exception
	inline void Corpus::check(std::string_view function) const {
		if(this->tokenized) {
			this->checkTokenized(function);
		}

		const auto end{
			this->tokenized ? this->tokens.size() : this->corpus.size()
		};

		Corpus::checkMap(function, "date map", this->dateMap, end, this->tokenized, true);
		Corpus::checkMap(function, "article map", this->articleMap, end, this->tokenized, false);
		Corpus::checkMap(function, this->sentenceMap, end, this->tokenized);
	}

	// check consistency of tokenized corpus, throws Corpus::Exception
	inline void Corpus::checkTokenized(std::string_view function) const {
		if(
				!(this->articleMap.empty())
				&& !(this->dateMap.empty())
				&& !(this->sentenceMap.empty())
		) {
			auto article = this->articleMap.cbegin();
			auto sentence = this->sentenceMap.cbegin();

			// go through all dates
			for(auto date{this->dateMap.cbegin()}; date != this->dateMap.cend(); ++date) {
				// jump to first article of date
				while(article != this->articleMap.cend() && article->p < date->p) {
					++article;
				}

				// jump to first sentence of date
				while(sentence != this->sentenceMap.cend() && sentence->first < date->p) {
					++sentence;
				}

				// go through all articles in current date
				const auto dateEnd{TextMapEntry::end(*date)};

				while(article != this->articleMap.cend() && article->p < dateEnd) {
					// jump to first sentence of article
					while(sentence != this->sentenceMap.cend() && sentence->first < date->p) {
						++sentence;
					}

					if(sentence == this->sentenceMap.cend()) {
						break;
					}

					// go through all sentences in current article
					const auto articleEnd{TextMapEntry::end(*article)};

					while(sentence != this->sentenceMap.cend() && sentence->first < articleEnd) {
						// check whether sentence is out of bounds
						const auto sentenceEnd{TextMapEntry::end(*sentence)};

						if(sentenceEnd > dateEnd) {
							throw Exception(
									Corpus::exceptionSentenceBehind(
											function,
											"date",
											*sentence,
											*date,
											this->dateMap,
											std::next(date),
											this->tokens
									)
							);
						}

						if(sentenceEnd > articleEnd) {
							throw Exception(
									Corpus::exceptionSentenceBehind(
											function,
											"article",
											*sentence,
											*article,
											this->articleMap,
											std::next(article),
											this->tokens
									)
							);
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

	// combine corpora, return whether thread is still running
	inline bool Corpus::combineCorpora(
			std::vector<Corpus>& from,
			DateArticleSentenceMap& to,
			StatusSetter& statusSetter
	) {
		std::size_t corpusCounter{};

		for(auto& corpus : from) {
			++corpusCounter;

			if(!Corpus::addCorpus(corpus, to, corpusCounter, from.size(), statusSetter)) {
				return false;
			}
		}

		// free memory of input data
		std::vector<Corpus>{}.swap(from);

		return statusSetter.isRunning();
	}

	// get all tokens that belong to a specific date or article
	inline std::vector<std::string> Corpus::getTokensForEntry(
			const TextMap& map,
			const std::string& id,
			const std::vector<std::string>& tokens
	) {
		const auto& found{
			std::find_if(
					map.cbegin(),
					map.cend(),
					[&id](const auto& entry) {
						return entry.value == id;
					}
			)
		};

		if(found == map.cend()) {
			return std::vector<std::string>();
		}

		const auto entryEnd{TextMapEntry::end(*found)};

		std::vector<std::string> copy;

		copy.reserve(found->l);

		for(auto tokenIndex{found->p}; tokenIndex < entryEnd; ++tokenIndex) {
			copy.emplace_back(tokens[tokenIndex]);
		}

		return copy;
	}

	// add corpus to combined corpus, return whether thread is still running
	inline bool Corpus::addCorpus(
			Corpus& from,
			DateArticleSentenceMap& to,
			std::size_t number,
			std::size_t total,
			StatusSetter& statusSetter
	) {
		if(from.empty()) {
			return true;
		}

		if(!from.tokenized) {
			throw Exception(
					"Corpus::Corpus():"
					" All sources need to be tokenized."
			);
		}

		// set status and add sentences to combined corpus
		const bool isRunning{
				statusSetter.change(Corpus::mergingStatus(number, total))
				&& Corpus::addSentences(from, to, statusSetter)
		};

		statusSetter.finish();

		// free memory of finished corpus
		from.clear();

		return isRunning;
	}

	// add sentences from corpus to combined corpus, return whether thread is still running
	inline bool Corpus::addSentences(
			Corpus& from,
			DateArticleSentenceMap& to,
			StatusSetter& statusSetter
	) {
		std::size_t articleIndex{};
		std::size_t dateIndex{};
		std::size_t articleEnd{Corpus::getFirstEnd(from.articleMap)};
		std::size_t dateEnd{Corpus::getFirstEnd(from.dateMap)};
		bool inArticle{false};
		bool inDate{false};
		std::size_t sentenceCounter{};
		std::size_t statusCounter{};
		std::string article;
		std::string date;
		std::vector<std::vector<std::string>> content;

		// go through all sentences
		for(auto& sentence : from.sentenceMap) {
			// skip articles and dates before current sentence
			//  (including last article and date, if finished)
			Corpus::skipEntriesBefore(
					from.articleMap,
					articleIndex,
					articleEnd,
					TextMapEntry::pos(sentence),
					inArticle
			);
			Corpus::skipEntriesBefore(
					from.dateMap,
					dateIndex,
					dateEnd,
					TextMapEntry::pos(sentence),
					inDate
			);

			// check for beginning of article and/or date
			if(Corpus::entryBeginsAt(from.articleMap, articleIndex, TextMapEntry::pos(sentence))) {
				// finish last article
				Corpus::finishArticle(
						content,
						to,
						date,
						article
				);

				// get next article
				Corpus::nextEntry(
						from.articleMap,
						articleIndex,
						article,
						articleEnd,
						from.tokens.size()
				);

				inArticle = true;
			}
			else if(!inArticle) {
				article = "";
			}

			if(Corpus::entryBeginsAt(from.dateMap, dateIndex, TextMapEntry::pos(sentence))) {
				// get next date
				Corpus::nextEntry(
						from.dateMap,
						dateIndex,
						date,
						dateEnd,
						from.tokens.size()
				);

				inDate = true;
			}
			else if(!inDate) {
				date = "";
			}

			// add sentence to content
			content.emplace_back(
					from.tokens.begin() + TextMapEntry::pos(sentence),
					from.tokens.begin() + TextMapEntry::end(sentence)
			);

			// update status
			++sentenceCounter;
			++statusCounter;

			if(statusCounter == mergeUpdateEvery) {
				if(!statusSetter.update(sentenceCounter, from.sentenceMap.size(), true)) {
					return false;
				}

				statusCounter = 0;
			}
		}

		// finish last article
		Corpus::finishArticle(
				content,
				to,
				date,
				article
		);

		return true;
	}

	// append  or add article to combined corpus
	inline void Corpus::finishArticle(
			std::vector<std::vector<std::string>>& from,
			DateArticleSentenceMap& to,
			const std::string& date,
			const std::string& article
	) {
		if(from.empty()) {
			return;
		}

		auto& articleSentences{to[date][article]};

		articleSentences.insert(
				articleSentences.end(),
				std::make_move_iterator(from.begin()),
				std::make_move_iterator(from.end())
		);

		std::vector<std::vector<std::string>>{}.swap(from);
	}

	// go to next article or date to be added to the combined corpus
	inline void Corpus::nextEntry(
			const TextMap& map,
			std::size_t index,
			std::string& nameTo,
			std::size_t& endTo,
			std::size_t corpusEnd
	) {
		if(index < map.size()) {
			nameTo = map[index].value;
			endTo = TextMapEntry::end(map[index]);
		}
		else {
			nameTo = "";
			endTo = corpusEnd;
		}
	}

	// get the end of the first article/date, regardless of whether it is in the map or not
	inline std::size_t Corpus::getFirstEnd(const TextMap& map) {
		if(!map.empty()) {
			if(TextMapEntry::pos(map[0]) > 0) {
				return TextMapEntry::pos(map[0]);
			}

			return TextMapEntry::length(map[0]);
		}

		return 0;
	}

	// get the end of a text map entry with the given index (or the end of the map)
	inline std::size_t Corpus::getEntryEnd(const TextMap& map, std::size_t entryIndex) {
		if(map.empty()) {
			return 0;
		}

		if(entryIndex < map.size()) {
			return TextMapEntry::end(map[entryIndex]);
		}

		return TextMapEntry::end(map.back());
	}

	// skip map entries before current position
	inline void Corpus::skipEntriesBefore(
			const TextMap& map,
			std::size_t& entryIndex,
			std::size_t& entryEnd,
			std::size_t pos,
			bool& inEntryTo
	) {
		bool increaseIndex{inEntryTo};
		bool skipped{false};

		while(
				entryIndex < map.size()
				&& (entryEnd <= pos || TextMapEntry::length(map[entryIndex]) == 0)
		) {
			if(increaseIndex) {
				++entryIndex;
			}
			else {
				increaseIndex = true;
			}

			entryEnd = Corpus::getEntryEnd(map, entryIndex);

			skipped = true;
		}

		if(skipped) {
			inEntryTo = false;
		}
	}

	// remove token from an article or date map
	inline void Corpus::removeToken(TextMap& map, std::size_t entryIndex, bool& emptiedTo) {
		if(TextMapEntry::length(map[entryIndex]) == 0) {
			throw Exception(
					"Corpus::removeToken():"
					" Could not remove token from map:"
					"  Map entry is already empty."
			);
		}

		// update length of map entry
		--TextMapEntry::length(map[entryIndex]);

		// check whether map entry is empty
		if(TextMapEntry::length(map[entryIndex]) == 0) {
			emptiedTo = true;
		}
	}

	// remove token from a sentence map entry
	inline void Corpus::removeToken(SentenceMapEntry& entry, bool& emptiedTo) {
		if(entry.second == 0) {
			throw Exception(
					"Corpus::removeToken():"
					" Could not remove token from sentence:"
					"  Sentence is already empty."
			);
		}

		// update length of sentence
		--(entry.second);

		// check whether sentence is empty
		if(entry.second == 0) {
			emptiedTo = true;
		}
	}

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
			throw Exception(Corpus::exceptionInvalidMaxChunkSize(maxLength, maxChunkSize));
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
		std::uint8_t cut{};

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

	// return the number of bytes in a vector of strings
	inline std::size_t Corpus::bytes(const std::vector<std::string>& words) {
		return std::accumulate(
				words.begin(),
				words.end(),
				std::size_t{},
				[](std::size_t bytes, const auto& word) {
					return bytes + word.size();
				}
		);
	}

	// check article or date map for inconsistencies, throws Corpus::Exception
	inline void Corpus::checkMap(
			std::string_view function,
			std::string_view name,
			const TextMap& map,
			std::size_t end,
			bool isTokenized,
			bool isDateMap
	) {
		// check the argument
		if(map.empty()) {
			return;
		}

		// check the start positions of all entries in the map
		std::size_t last{};

		for(const auto& entry : map) {
			if(last > 0 && TextMapEntry::pos(entry) != last) {
				throw Exception(
						Corpus::exceptionInvalidPosition(
								function,
								TextMapEntry::pos(entry),
								last,
								name
						)
				);
			}

			last = TextMapEntry::end(entry);

			if(!isTokenized) {
				++last;
			}

			if(isDateMap && entry.value.length() != dateLength) {
				throw Exception(
						Corpus::exceptionInvalidDate(
								function,
								entry.value,
								name
						)
				);
			}
		}

		// check the end position of the last entry in the map
		const auto& back{map.back()};

		if(TextMapEntry::end(back) != end) {
			throw Exception(
					Corpus::exceptionInvalidEnd(
							function,
							TextMapEntry::end(back),
							end,
							name
					)
			);
		}
	}

	// check sentence map for inconsistencies, throws Corpus::Exception
	inline void Corpus::checkMap(
			std::string_view function,
			const SentenceMap& map,
			std::size_t end,
			bool isTokenized
	) {
		// check the argument
		if(map.empty()) {
			return;
		}

		// check the start positions of all entries in the map
		std::size_t last{};

		for(const auto& entry : map) {
			if(TextMapEntry::pos(entry) != last) {
				throw Exception(
						Corpus::exceptionInvalidPosition(
								function,
								TextMapEntry::pos(entry),
								last - 1,
								"sentence map"
						)
				);
			}

			last = TextMapEntry::end(entry);

			if(!isTokenized) {
				++last;
			}
		}

		// check the end position of the last entry in the map
		const auto& back{map.back()};

		if(TextMapEntry::end(back) != end) {
			Corpus::exceptionInvalidEnd(
					function,
					TextMapEntry::end(back),
					end,
					"sentence map"
			);
		}
	}

	// set locale for output streams
	inline void Corpus::locale(std::ostream& os) {
		os.imbue(std::locale(""));
	}

	// get string for thread status
	inline std::string Corpus::mergingStatus(std::size_t number, std::size_t total) {
		std::ostringstream status;

		Corpus::locale(status);

		status << "Merging corpora (";
		status << number;
		status << "/";
		status << total;
		status << ")...";

		return status.str();
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTIONS FOR EXCEPTION HANDLING (private)
	 */

	// exception when trying to get an article: article map is empty
	inline std::string Corpus::exceptionGetNoArticleMap(
			std::string_view function,
			std::size_t article
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): Article #";
		exception << article;
		exception << " requested, but the article map is empty";

		return exception.str();
	}

	// exception when trying to get an article: article is out of the article map's bounds
	inline std::string Corpus::exceptionArticleOutOfBounds(
			std::string_view function,
			std::size_t article,
			std::size_t size
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): The specified article index (#";
		exception << article;
		exception << ") is out of bounds [#0;#";
		exception << size - 1;
		exception << "]";

		return exception.str();
	}

	// exception when trying to get a date: invalid date length
	inline std::string Corpus::exceptionDateLength(
			std::string_view function,
			std::size_t length
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): Invalid length of date (";
		exception << length;
		exception << " instead of ";
		exception << dateLength;
		exception << ")";

		return exception.str();
	}

	// exception when combining chunks: article map of chunk does not start at its beginning
	inline std::string Corpus::exceptionArticleMapStart(
			std::string_view function,
			std::string_view expected,
			std::size_t chunkIndex,
			std::size_t numberOfChunks,
			std::size_t start
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): Article map in corpus chunk ";
		exception << chunkIndex + 1;
		exception << "/";
		exception << numberOfChunks;
		exception << " starts at #";
		exception << start;
		exception << " instead of ";
		exception << expected;

		return exception.str();
	}

	// exception when combining tokenized chunks: sentence map of chunk does not start at its beginning
	inline std::string Corpus::exceptionSentenceMapStart(
			std::size_t chunkIndex,
			std::size_t numberOfChunks,
			std::size_t start
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::combineTokenized(): Sentence map in corpus chunk ";
		exception << chunkIndex + 1;
		exception << "/";
		exception << numberOfChunks;
		exception << " starts at #";
		exception << start;
		exception << " instead of #0";

		return exception.str();
	}

	// exception when combining tokenized chunks: length of first sentence does not match last one in previous chunk
	inline std::string Corpus::exceptionFirstSentenceLength(
			std::size_t chunkIndex,
			std::size_t numberOfChunks,
			std::size_t length,
			std::size_t lastEnd,
			std::size_t offset
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::combineTokenized(): Length of first sentence in chunk ";
		exception << chunkIndex + 1;
		exception << "/";
		exception << numberOfChunks;
		exception << "conflicts with length given in previous chunk (";
		exception << length;
		exception << " != ";
		exception << lastEnd;
		exception << " - ";
		exception << offset;
		exception << " [";
		exception << lastEnd - offset;
		exception << "])";

		return exception.str();
	}

	// exception when combining tokenized chunks: length of last sentence exceeeds length of corpus
	inline std::string Corpus::exceptionLastSentenceLength(
			std::size_t pos,
			std::size_t length,
			std::size_t corpusSize
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::combineTokenized(): Length of last sentence (";
		exception << pos;
		exception << " + ";
		exception << length;
		exception << " [";
		exception << pos + length;
		exception << ") exceeds length of corpus (";
		exception << corpusSize;
		exception << ")";

		return exception.str();
	}

	// exception when copying chunks: article lies behind its date
	inline std::string Corpus::exceptionArticleBehindDate(
			std::size_t articlePos,
			std::size_t datePos,
			std::size_t dateEnd
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::copyChunksContinuous(): Article position (#";
		exception << articlePos;
		exception << ") lies behind date at [#";
		exception << datePos;
		exception << ";#";
		exception << dateEnd;
		exception << "]";

		return exception.str();
	}

	// exception when copying chunks: chunk size is too large
	inline std::string Corpus::exceptionChunkSize(std::size_t size, std::size_t chunkSize)	{
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::copyChunksContinuous(): Chunk is too large:";
		exception << size;
		exception << " > ";
		exception << chunkSize;

		return exception.str();
	}

	// exception when copying chunks: end of articles reached before corpus ends
	inline std::string Corpus::exceptionArticleMapEnd(std::size_t pos, std::size_t size) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::copyChunksContinuous(): End of articles, but not of corpus ( #";
		exception << pos;
		exception << " < #";
		exception << size;
		exception << ")";

		return exception.str();
	}

	// exception when copying tokenized chunks: article or date begins before current sentence
	inline std::string Corpus::exceptionUnexpectedBeforeSentence(
			std::string_view type,
			std::string_view name,
			std::size_t pos,
			std::size_t sentencePos
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::copyChunksTokenized(): Unexpected begin of ";
		exception << type;
		exception << " '";
		exception << name;
		exception << "' (@";
		exception << pos;
		exception << ") before the beginning of the current sentence (@";
		exception << sentencePos;
		exception << ")";

		return exception.str();
	}

	// exception when filtering corpus by date: mismatch between article or sentence and date position
	inline std::string Corpus::exceptionMismatchWithDate(
			std::string_view type,
			std::size_t pos,
			std::size_t datePos
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::filterByDate(): Mismatch between positions of ";
		exception << type;
		exception << " (@ #";
		exception << pos;
		exception << ") and date (@ #";
		exception << datePos;
		exception << ") in ";
		exception << type;
		exception << " and date map of the corpus";

		return exception.str();
	}

	// exception when filtering corpus by date: date lies behind last article or sentence
	inline std::string Corpus::exceptionDateBehindLast(
			std::string_view type,
			std::size_t datePos,
			std::size_t lastPos
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::filterByDate(): Position of identified date (@ #";
		exception << datePos;
		exception << ") is behind the position of the last ";
		exception << type;
		exception << " (@ #";
		exception << lastPos;
		exception << ") in ";
		exception << type;
		exception << " and date map of the corpus";

		return exception.str();
	}

	// exception when checking tokenized corpus: end of sentence behind date or article
	inline std::string Corpus::exceptionSentenceBehind(
			std::string_view function,
			std::string_view type,
			const std::pair<std::size_t, std::size_t>& sentence,
			const TextMapEntry& entry,
			const TextMap& map,
			const TextMap::const_iterator& next,
			const std::vector<std::string>& words
	) {
		std::ostringstream exception;

		const auto sentenceEnd{TextMapEntry::end(sentence)};
		const auto entryEnd{TextMapEntry::end(entry)};

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): End of sentence (l=";
		exception << sentence.second;
		exception << ") is behind end of ";
		exception << type;
		exception << " '";
		exception << entry.value;
		exception << "' (l=";
		exception << TextMapEntry::length(entry);
		exception << "): ";
		exception << sentenceEnd;

		if(sentenceEnd > 0 && sentenceEnd <= words.size()) {
			exception << " ['";
			exception << words.at(sentenceEnd - 1);
			exception << "']";
		}
		else if(sentenceEnd == 0) {
			exception << " [BEGIN]";
		}
		else {
			exception << " [BEHIND]";
		}

		exception << " > ";
		exception << entryEnd;

		if(entryEnd > 0 && entryEnd <= words.size()) {
			exception << " ['";
			exception << words.at(entryEnd - 1);
			exception << "']";
		}
		else if(entryEnd == 0) {
			exception << " [BEGIN]";
		}
		else {
			exception << " [BEHIND]";
		}

		exception << " (";
		exception << "sentence: '";

		bool addSpace{false};

		for(std::size_t word{TextMapEntry::pos(sentence)}; word < sentenceEnd; ++word) {
			if(word < words.size()) {
				if(addSpace) {
					exception << ' ';
				}
				else {
					addSpace = true;
				}

				exception << words[word];
			}
		}

		exception << "'";

		if(next != map.cend()) {
			exception << " (next ";
			exception << type;
			exception << ": '";
			exception << next->value;
			exception << "')";
		}

		return exception.str();
	}

	// exception when setting maximum chunk size: invalid maximum chunk size given
	inline std::string Corpus::exceptionInvalidMaxChunkSize(std::size_t size, std::size_t max) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::getValidLengthOfChunk(): Invalid maximum chunk size (";
		exception << size;
		exception << " > ";
		exception << max;
		exception << ")";

		return exception.str();
	}

	// exception when filtering map: invalid position
	inline std::string Corpus::exceptionPositionTooSmall(
			std::size_t pos,
			std::size_t expectedMin,
			std::string_view name
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::reTokenize(): Invalid position #";
		exception << pos;
		exception << " (expected: >= #";
		exception << expectedMin;
		exception << ") in ";
		exception << name;

		return exception.str();
	}

	// exception when checking map: invalid position
	inline std::string Corpus::exceptionInvalidPosition(
			std::string_view function,
			std::size_t pos,
			std::size_t expected,
			std::string_view name
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): Invalid position #";
		exception << pos;
		exception << " (expected: #";
		exception << expected;
		exception << ") in ";
		exception << name;

		return exception.str();
	}

	// exception when checking date map: invalid date length
	inline std::string Corpus::exceptionInvalidDate(
			std::string_view function,
			std::string_view value,
			std::string_view name
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): Invalid date in date map: '";
		exception << value;
		exception << "' (expected string of length ";
		exception << dateLength;
		exception << ") in '";
		exception << name;
		exception << "'";

		return exception.str();
	}

	// exception when checking map: invalid end of last entry
	inline std::string Corpus::exceptionInvalidEnd(
			std::string_view function,
			std::size_t pos,
			std::size_t expected,
			std::string_view name
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): Invalid end of last entry in map at #";
		exception << pos;
		exception << " (expected: at #";
		exception << expected;
		exception << ") in ";
		exception << name;

		return exception.str();
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_CORPUS_HPP_ */
