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
#include "TokenRemover.hpp"

#include "../Helper/Container.hpp"
#include "../Helper/DateTime.hpp"
#include "../Helper/Memory.hpp"
#include "../Helper/Utf8.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/StatusSetter.hpp"
#include "../Struct/TextMap.hpp"

#include <algorithm>	// std::all_of, std::any_of, std::count_if, std::find_if, std::remove_if
#include <cctype>		// std::toupper
#include <cstddef>		// std::size_t
#include <cstdint>		// std::int64_t, std::uint8_t, std::uint16_t
#include <functional>	// std::function, std::reference_wrapper
#include <iterator>		// std::distance
#include <locale>		// std::locale
#include <map>			// std::map
#include <numeric>		// std::accumulate
#include <optional>		// std::optional, std::nullopt
#include <ostream>		// std::ostream
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
	///@name Sentence and Token Manipulation
	///@{

	//! Do not manipulate anything.
	inline constexpr std::uint16_t corpusManipNone{0};

	//! The POS (position of speech) tagger based on @c Wapiti by Thomas Lavergne.
	inline constexpr std::uint16_t corpusManipTagger{1};

	//! The posterior POS tagger based on @c Wapiti by Thomas Lavergne (slow, but more accurate).
	inline constexpr std::uint16_t corpusManipTaggerPosterior{2};

	//! The @c porter2_stemmer algorithm for English only, implemented by Sean Massung.
	inline constexpr std::uint16_t corpusManipEnglishStemmer{3};

	//! Simple stemmer for German only, based on @c CISTEM by Leonie Weißweiler and Alexander Fraser.
	inline constexpr std::uint16_t corpusManipGermanStemmer{4};

	//! Multilingual lemmatizer.
	inline constexpr std::uint16_t corpusManipLemmatizer{5};

	//! Remove single tokens found in a dictionary.
	inline constexpr std::uint16_t corpusManipRemove{6};

	//! Correct single tokens using a @c aspell dictionary.
	inline constexpr std::uint16_t corpusManipCorrect{7};

	//TODO(ans): add corpusManipReplace (by dictionary)

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
	 * The corpus can be preprocessed using a number
	 *  of manipulators, resulting in a tokenized
	 *  corpus. If not preprocessed, it will be
	 *  stored as continuous text.
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
		using Sizes = std::vector<std::size_t>;
		using Tokens = std::vector<std::string>;

		using ArticleFunc = std::function<bool(const Tokens&, std::size_t, std::size_t)>;
		using SentenceFunc = std::function<void(Tokens::iterator, Tokens::iterator)>;

		using DateArticleSentenceMap
				= std::map<std::string, std::map<std::string, std::vector<Tokens>>>;
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
		[[nodiscard]] Tokens& getTokens();
		[[nodiscard]] const Tokens& getcTokens() const;
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
		[[nodiscard]] Tokens getTokenized(std::size_t index) const;
		[[nodiscard]] Tokens getTokenized(const std::string& id) const;
		[[nodiscard]] Tokens getDateTokenized(const std::string& date) const;
		[[nodiscard]] std::vector<Tokens> getArticles() const;

		[[nodiscard]] std::size_t size() const;
		[[nodiscard]] bool empty() const;

		[[nodiscard]] std::string substr(std::size_t from, std::size_t len);

		///@}
		///@name Creation
		///@{

		void create(
				Tokens& texts,
				bool deleteInputData
		);
		void create(
				Tokens& texts,
				std::vector<std::string>& articleIds,
				std::vector<std::string>& dateTimes,
				bool deleteInputData
		);
		void combineContinuous(
				Tokens& chunks,
				std::vector<TextMap>& articleMaps,
				std::vector<TextMap>& dateMaps,
				bool deleteInputData
		);
		void combineTokenized(
				Tokens& chunks,
				Sizes& tokenNums,
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
				Tokens& to,
				std::vector<TextMap>& articleMapsTo,
				std::vector<TextMap>& dateMapsTo
		) const;
		void copyChunksTokenized(
				std::size_t chunkSize,
				Tokens& to,
				Sizes& tokenNumsTo,
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
				const std::vector<std::uint16_t>& manipulators,
				const std::vector<std::string>& models,
				const std::vector<std::string>& dictionaries,
				const std::vector<std::string>& languages,
				std::uint64_t freeMemoryEvery,
				StatusSetter& statusSetter
		);
		[[nodiscard]] bool tokenizeCustom(
				const std::optional<SentenceFunc>& callback,
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
		Tokens tokens;

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

		// internal helper functions
		void moveCombinedIn(DateArticleSentenceMap& from);

		void checkThatNotTokenized(std::string_view function) const;
		void checkThatTokenized(std::string_view function) const;

		void addArticle(
				std::string& text,
				std::string& id,
				std::string& dateTime,
				TextMapEntry& dateMapEntry,
				bool deleteInputData
		);
		void addChunk(
				const std::string& content,
				const std::optional<std::reference_wrapper<const TextMap>>& articles,
				const std::optional<std::reference_wrapper<const TextMap>>& dates,
				const SentenceMap& sentences,
				bool& continueToken
		);

		void check(std::string_view function) const;
		void checkTokenized(std::string_view function) const;

		void addAsOneChunk(
				std::size_t size,
				Tokens& to,
				Sizes& tokenNumsTo,
				std::vector<TextMap>& articleMapsTo,
				std::vector<TextMap>& dateMapsTo,
				std::vector<SentenceMap>& sentenceMapsTo
		) const;

		void reTokenize();

		[[nodiscard]] bool tokenizeTokenized(
				std::optional<SentenceFunc> callback,
				StatusSetter& statusSetter
		);
		[[nodiscard]] bool tokenizeContinuous(
				std::optional<SentenceFunc> callback,
				std::uint64_t freeMemoryEvery,
				StatusSetter& statusSetter
		);

		// internal static helper functions
		static bool combineCorpora(
				std::vector<Corpus>& from,
				DateArticleSentenceMap& to,
				StatusSetter& statusSetter
		);

		[[nodiscard]] static Tokens getTokensForEntry(
				const TextMap& map,
				const std::string& id,
				const Tokens& tokens
		);

		[[nodiscard]] static std::size_t getValidLengthOfChunk(
				const std::string& source,
				std::size_t pos,
				std::size_t maxLength,
				std::size_t maxChunkSize
		);
		[[nodiscard]] static std::size_t getValidLengthOfChunk(
				const std::string& chunkContent,
				std::size_t maxChunkSize
		);

		static void checkTokensForChunking(const Tokens& tokens);

		static void reserveChunks(
				std::size_t chunks,
				Tokens& to,
				Sizes& tokenNumsTo,
				std::vector<TextMap>& articleMapsTo,
				std::vector<TextMap>& dateMapsTo,
				std::vector<SentenceMap>& sentenceMapsTo,
				bool hasArticleMap,
				bool hasDateMap
		);

		static void checkForEntry(
				std::string_view type,
				const SentenceMapEntry& sentence,
				std::size_t& nextIndex,
				const TextMap& map,
				std::size_t chunkOffset,
				TextMap& chunkMap,
				bool checkConsistency
		);

		static void finishChunk(
				std::string& contentFrom,
				SentenceMap& sentencesFrom,
				Tokens& contentTo,
				Sizes& tokenNumTo,
				std::vector<SentenceMap>& sentencesTo,
				std::size_t chunkTokens,
				std::size_t& chunkOffset,
				bool splitToken,
				std::size_t nextChunkSize
		);
		static void splitEntry(
				TextMap& entry,
				std::size_t token,
				bool splitToken,
				TextMapEntry& remainingTo
		);
		static void finishMap(
				TextMap& from,
				std::vector<TextMap>& to,
				TextMapEntry& remaining
		);

		static void notUsed(
				std::string_view type,
				const std::vector<std::string>& values,
				std::size_t index
		);

		static std::size_t bytes(const Tokens& tokens);

		static void addChunkMap(
				const std::optional<std::reference_wrapper<const TextMap>>& from,
				TextMap& to,
				std::size_t offset,
				bool splitToken
		);

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

		static void skipEntriesBefore(
				const TextMap& map,
				std::size_t& entryIndex,
				std::size_t& entryEnd,
				std::size_t pos,
				bool& inEntryTo
		);

		static void removeEmpty(Tokens& from);
		static void removeToken(TextMap& map, std::size_t entryIndex, bool& emptiedTo);
		static void removeToken(SentenceMapEntry& entry, bool& emptiedTo);

		static std::size_t getFirstEnd(const TextMap& map);
		static std::size_t getEntryEnd(const TextMap& map, std::size_t entryIndex);

		static void processSentence(
				Tokens& sentence,
				const std::optional<SentenceFunc>& callback,
				bool inArticle,
				bool inDate,
				std::size_t& currentToken,
				std::size_t& sentenceFirstToken,
				TextMap& articleMap,
				TextMap& dateMap,
				SentenceMap& sentenceMap,
				Tokens& tokens,
				std::size_t& tokenBytes
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
				std::vector<Tokens>& from,
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

		static bool pushSentence(
				const SentenceMapEntry& sentence,
				std::size_t chunkSize,
				std::size_t chunkOffset,
				std::size_t& chunkTokens,
				std::string& chunkContent,
				SentenceMap& chunkSentences,
				const Tokens& tokens,
				std::size_t& tokensComplete,
				std::size_t& additionalBytes
		);

		static std::string mergingStatus(std::size_t number, std::size_t total);

		static void locale(std::ostream& os);

		// internal static helper functions for exception throwing
		static void exceptionGetNoArticleMap(
				std::string_view function,
				std::size_t article
		);
		static void exceptionArticleOutOfBounds(
				std::string_view function,
				std::size_t article,
				std::size_t size
		);
		static void exceptionDateLength(
				std::string_view function,
				std::size_t length
		);
		static void exceptionArticleMapStart(
				std::string_view function,
				std::string_view expected,
				std::size_t chunkIndex,
				std::size_t numberOfChunks,
				std::size_t start
		);
		static void exceptionLastSentenceLength(
				std::size_t pos,
				std::size_t length,
				std::size_t corpusSize
		);
		static void exceptionArticleBehindDate(
				std::size_t articlePos,
				std::size_t datePos,
				std::size_t dateEnd
		);
		static void exceptionChunkSize(std::size_t size, std::size_t chunkSize);
		static void exceptionArticleMapEnd(std::size_t pos, std::size_t size);
		static void exceptionUnexpectedBeforeSentence(
				std::string_view type,
				std::string_view name,
				std::size_t pos,
				std::size_t sentencePos
		);
		static void exceptionMismatchWithDate(
				std::string_view type,
				std::size_t pos,
				std::size_t datePos
		);
		static void exceptionDateBehindLast(
				std::string_view type,
				std::size_t datePos,
				std::size_t lastPos
		);
		static void exceptionSentenceBehind(
				std::string_view function,
				std::string_view type,
				const std::pair<std::size_t, std::size_t>& sentence,
				const TextMapEntry& entry,
				const TextMap& map,
				const TextMap::const_iterator& next,
				const Tokens& tokens
		);
		static void exceptionTokenBytes(
				std::string_view function,
				std::size_t size,
				std::size_t actualSize
		);
		static void exceptionInvalidMaxChunkSize(std::size_t size, std::size_t max);
		static void exceptionPositionTooSmall(
				std::size_t pos,
				std::size_t expectedMin,
				std::string_view name
		);
		static void exceptionInvalidPosition(
				std::string_view function,
				std::size_t pos,
				std::size_t expected,
				std::string_view name
		);
		static void exceptionInvalidDate(
				std::string_view function,
				std::string_view value,
				std::string_view name
		);
		static void exceptionInvalidEnd(
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
							std::size_t{},
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

		// remove empty entries from map (checking all of their tokens)
		template<typename T> static void removeEmptyEntries(
				T& map,
				const Tokens& tokens
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

		// remove empty entries from map (checking just their length)
		template<typename T> static void removeEmptyEntries(T& map) {
			map.erase(
					std::remove_if(
							map.begin(),
							map.end(),
							[](const auto& entry) {
								return TextMapEntry::length(entry) == 0;
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
			if(pos == 0) {
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
				Corpus::exceptionPositionTooSmall(
						TextMapEntry::pos(map[entryIndex]),
						removed,
						type
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
		this->checkThatNotTokenized("getCorpus");

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
		this->checkThatNotTokenized("getcCorpus");

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
	inline Corpus::Tokens& Corpus::getTokens() {
		this->checkThatTokenized("getTokens");

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
	inline const Corpus::Tokens& Corpus::getcTokens() const {
		this->checkThatTokenized("getcTokens");

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
		this->checkThatTokenized("getNumTokens");

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
		this->checkThatTokenized("getSentenceMap");

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
		this->checkThatTokenized("getcSentenceMap");

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
		this->checkThatNotTokenized("get");

		if(this->articleMap.empty()) {
			Corpus::exceptionGetNoArticleMap(
					"get",
					index
			);
		}

		if(index >= this->articleMap.size()) {
			Corpus::exceptionArticleOutOfBounds(
					"get",
					index,
					this->articleMap.size()
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
		this->checkThatNotTokenized("get");

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
		this->checkThatNotTokenized("getDate");

		// check argument
		if(date.length() != dateLength) {
			Corpus::exceptionDateLength(
					"getDate",
					date.length()
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
	inline Corpus::Tokens Corpus::getTokenized(std::size_t index) const {
		this->checkThatTokenized("getTokenized");

		if(this->articleMap.empty()) {
			Corpus::exceptionGetNoArticleMap(
					"getTokenized",
					index
			);
		}

		if(index >= this->articleMap.size()) {
			Corpus::exceptionArticleOutOfBounds(
					"getTokenized",
					index,
					this->articleMap.size()
			);
		}

		const auto& articleEntry{this->articleMap.at(index)};
		const auto articleEnd{TextMapEntry::end(articleEntry)};

		Tokens copy;

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
	inline Corpus::Tokens Corpus::getTokenized(const std::string& id) const {
		this->checkThatTokenized("getTokenized");

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
	inline Corpus::Tokens Corpus::getDateTokenized(const std::string& date) const {
		this->checkThatTokenized("getDateTokenized");

		// check argument
		if(date.length() != dateLength) {
			Corpus::exceptionDateLength(
					"getDateTokenized",
					date.length()
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
	inline std::vector<Corpus::Tokens> Corpus::getArticles() const {
		this->checkThatTokenized("getArticles");

		std::vector<Tokens> copy;

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
		this->checkThatNotTokenized("substr");

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
			Tokens& texts,
			bool deleteInputData
	) {
		// clear old corpus
		this->clear();

		// concatenate texts
		for(auto& text : texts) {
			// add text to corpus
			this->corpus += text;

			Helper::Memory::freeIf(deleteInputData, text);

			// add space at the end of the corpus
			this->corpus.push_back(' ');
		}

		Helper::Memory::freeIf(deleteInputData, texts);

		// remove last space, if necessary
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
			Tokens& texts,
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

		Helper::Memory::freeIf(deleteInputData, texts);
		Helper::Memory::freeIf(deleteInputData, articleIds);
		Helper::Memory::freeIf(deleteInputData, dateTimes);

		// remove last space, if necessary
		if(!(this->corpus.empty())) {
			this->corpus.pop_back();
		}

		// conclude last date, if unfinished
		if(!dateMapEntry.value.empty()) {
			this->dateMap.emplace_back(dateMapEntry);
		}
	}
	//! Creates continuous text corpus by combining previously separated chunks as well as their article and date maps.
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
			Tokens& chunks,
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

			Helper::Memory::freeIf(deleteInputData, *chunkIt);

			bool beginsWithNewArticle{false};

			if(articleMaps.size() > chunkIndex) {
				// add article map
				auto& map{articleMaps.at(chunkIndex)};

				if(!map.empty()) {
					const auto& first{map.at(0)};

					// perform consistency check, if necessary
					if(this->checkConsistency && TextMapEntry::pos(first) > 1) {
						Corpus::exceptionArticleMapStart(
								"combineContinuous",
								"#0 or #1",
								chunkIndex,
								chunks.size(),
								TextMapEntry::pos(first)
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

					Helper::Memory::freeIf(deleteInputData, map);
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

					Helper::Memory::freeIf(deleteInputData, map);
				}
			}
		}

		Helper::Memory::freeIf(deleteInputData, chunks);
		Helper::Memory::freeIf(deleteInputData, articleMaps);
		Helper::Memory::freeIf(deleteInputData, dateMaps);
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
	 * \param tokenNums Reference to a vector containing
	 *   the number of tokens in each chunk.
	 * \param articleMaps Reference to a vector
	 *   containing the article maps of the chunks.
	 * \param dateMaps Reference to a vector containing
	 *   the date maps of the chunks.
	 * \param sentenceMaps Reference to a vector
	 *   containing the sentence maps of the chunks.
	 * \param deleteInputData If true, the given texts,
	 *   token counts, as well as article, date and
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
	 *   itself, or if there are more token counts,
	 *   article maps, date maps and/or sentence maps
	 *   given than corpus chunks.
	 *
	 * \sa copyChunksTokenized
	 */
	inline void Corpus::combineTokenized(
			Tokens& chunks,
			Sizes& tokenNums,
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
						tokenNums.size() > chunks.size()
						|| articleMaps.size() > chunks.size()
						|| dateMaps.size() > chunks.size()
						|| sentenceMaps.size() > chunks.size()
				)
		) {
			throw Exception(
					"Corpus::combineTokenized():"
					" More token counts, article maps, date maps,"
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
		const auto totalTokens{
			std::accumulate(
					tokenNums.cbegin(),
					tokenNums.cend(),
					std::size_t{}
			)
		};

		Helper::Memory::freeIf(deleteInputData, tokenNums);

		this->tokens.reserve(totalTokens);

		Corpus::reserveCombined(articleMaps, this->articleMap);
		Corpus::reserveCombined(dateMaps, this->dateMap);
		Corpus::reserveCombined(sentenceMaps, this->sentenceMap);

		// add tokens from chunks
		std::size_t chunkIndex{};
		bool splitToken{false};

		for(auto& chunk : chunks) {
			this->addChunk(
					chunk,
					(chunkIndex < articleMaps.size()) ?
							std::optional<std::reference_wrapper<const TextMap>>{articleMaps[chunkIndex]}
							: std::nullopt,
					(chunkIndex < dateMaps.size()) ?
							std::optional<std::reference_wrapper<const TextMap>>{dateMaps[chunkIndex]}
							: std::nullopt,
					sentenceMaps.at(chunkIndex),
					splitToken
			);

			Helper::Memory::freeIf(deleteInputData, chunk);
			Helper::Memory::freeIf(deleteInputData, sentenceMaps.at(chunkIndex));

			if(chunkIndex < articleMaps.size()) {
				Helper::Memory::freeIf(deleteInputData, articleMaps.at(chunkIndex));
			}

			if(chunkIndex < dateMaps.size()) {
				Helper::Memory::freeIf(deleteInputData, dateMaps.at(chunkIndex));
			}

			++chunkIndex;
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
			Corpus::exceptionLastSentenceLength(
					TextMapEntry::pos(this->sentenceMap.back()),
					TextMapEntry::length(this->sentenceMap.back()),
					this->tokens.size()
			);
		}

		Helper::Memory::freeIf(deleteInputData, chunks);
		Helper::Memory::freeIf(deleteInputData, articleMaps);
		Helper::Memory::freeIf(deleteInputData, dateMaps);
		Helper::Memory::freeIf(deleteInputData, sentenceMaps);

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
		this->checkThatNotTokenized("copyContinuous");

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
		this->checkThatNotTokenized("copyContinuous");

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
			Tokens& to,
			std::vector<TextMap>& articleMapsTo,
			std::vector<TextMap>& dateMapsTo
	) const {
		// check corpus and argument
		if(this->corpus.empty()) {
			return;
		}

		if(chunkSize == 0) {
			throw Exception(
					"Corpus::copyChunksContinuous():"
					" Invalid chunk size (zero)"
					" for a non-empty corpus"
			);
		}

		this->checkThatNotTokenized("copyChunksContinuous");

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
							Corpus::exceptionArticleBehindDate(
									articleIt->p,
									dateIt->p,
									TextMapEntry::end(*dateIt)
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
						Corpus::exceptionChunkSize(chunk.size(), chunkSize);
					}

					if(articleIt == this->articleMap.cend() && corpusPos < this->corpus.size()) {
						Corpus::exceptionArticleMapEnd(corpusPos, this->corpus.size());
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
	 * \param tokenNumsTo Reference to a vector
	 *   to which the number of tokens for each
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
			Tokens& to,
			Sizes& tokenNumsTo,
			std::vector<TextMap>& articleMapsTo,
			std::vector<TextMap>& dateMapsTo,
			std::vector<SentenceMap>& sentenceMapsTo
	) const {
		// check corpus and argument
		if(this->tokens.empty()) {
			return;
		}

		if(chunkSize == 0) {
			throw Exception(
					"Corpus::copyChunksTokenized():"
					" Invalid chunk size (zero)"
					" for a non-empty corpus"
			);
		}

		if(this->sentenceMap.empty()) {
			throw Exception(
					"Corpus::copyChunksTokenized():"
					" Empty sentence map"
					" for a non-empty corpus"
			);
		}

		this->checkThatTokenized("copyChunksTokenized");

		Corpus::checkTokensForChunking(this->tokens);

		// check whether slicing is necessary
		const auto size{
			this->tokenBytes
			+ this->tokens.size() /* include newline after each token */
			- 1 /* (except the last one) */
		};

		if(size < chunkSize) {
			this->addAsOneChunk(
					size,
					to,
					tokenNumsTo,
					articleMapsTo,
					dateMapsTo,
					sentenceMapsTo
			);

			/* added whole corpus as one chunk */
			return;
		}

		// reserve memory for chunks
		const auto sizeOfLastChunk{size % chunkSize};
		const auto numberOfChunks{size / chunkSize + (sizeOfLastChunk > 0 ? 1 : 0)};

		Corpus::reserveChunks(
				numberOfChunks,
				to,
				tokenNumsTo,
				articleMapsTo,
				dateMapsTo,
				sentenceMapsTo,
				!(this->articleMap.empty()),
				!(this->dateMap.empty())
		);

		// fill chunk with content, sentences, dates, and articles
		std::size_t chunkOffset{};
		std::size_t chunkTokens{};
		std::string chunkContent;
		SentenceMap chunkSentences;
		TextMap chunkDates;
		TextMap chunkArticles;
		std::size_t nextDate{};
		std::size_t nextArticle{};
		std::size_t tokensComplete{};
		std::size_t additionalBytes{};
		TextMapEntry remainingDate;
		TextMapEntry remainingArticle;

		chunkContent.reserve(chunkSize);

		for(const auto& sentence : this->sentenceMap) {
			Corpus::checkForEntry(
					"date",
					sentence,
					nextDate,
					this->dateMap,
					chunkOffset,
					chunkDates,
					this->checkConsistency
			);

			Corpus::checkForEntry(
					"article",
					sentence,
					nextArticle,
					this->articleMap,
					chunkOffset,
					chunkArticles,
					this->checkConsistency
			);

			while(
					Corpus::pushSentence(
							sentence,
							chunkSize,
							chunkOffset,
							chunkTokens,
							chunkContent,
							chunkSentences,
							this->tokens,
							tokensComplete,
							additionalBytes
					)
			) {
				const bool splitToken{additionalBytes > 0};

				Corpus::finishChunk(
						chunkContent,
						chunkSentences,
						to,
						tokenNumsTo,
						sentenceMapsTo,
						chunkTokens,
						chunkOffset,
						splitToken,
						(sizeOfLastChunk == 0 || to.size() < (numberOfChunks - 1)) ?
								chunkSize : (sizeOfLastChunk + 1)
				);

				Corpus::splitEntry(chunkDates, chunkTokens, splitToken, remainingDate);
				Corpus::splitEntry(chunkArticles, chunkTokens, splitToken, remainingArticle);

				Corpus::finishMap(chunkDates, dateMapsTo, remainingDate);
				Corpus::finishMap(chunkArticles, articleMapsTo, remainingArticle);

				// reset number of tokens in chunk
				chunkTokens = 0;
			}
		}

		// finish last chunk
		Corpus::finishChunk(
				chunkContent,
				chunkSentences,
				to,
				tokenNumsTo,
				sentenceMapsTo,
				chunkTokens,
				chunkOffset,
				false,
				0
		);

		Corpus::finishMap(chunkDates, dateMapsTo, remainingDate);
		Corpus::finishMap(chunkArticles, articleMapsTo, remainingArticle);

		// remove last newline
		if(!to.empty()) {
			to.back().pop_back();

			if(to.back().empty()) {
				to.pop_back();
				tokenNumsTo.pop_back();
				sentenceMapsTo.pop_back();

				if(!articleMapsTo.empty()) {
					articleMapsTo.pop_back();
				}

				if(!dateMapsTo.empty()) {
					dateMapsTo.pop_back();
				}
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
						std::size_t{},
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
						std::size_t{},
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
				Corpus::exceptionMismatchWithDate(
						"article",
						begin->p,
						offset
				);
			}
		}

		// consistency check
		if(this->checkConsistency && begin == this->articleMap.cend()) {
			Corpus::exceptionDateBehindLast(
					"article",
					offset,
					TextMapEntry::pos(this->articleMap.back())
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
					Corpus::exceptionMismatchWithDate(
							"sentence",
							smBegin->first,
							offset
					);
				}
			}

			// consistency check
			if(this->checkConsistency && smBegin == this->sentenceMap.cend()) {
				Corpus::exceptionDateBehindLast(
						"sentence",
						offset,
						TextMapEntry::pos(this->sentenceMap.back())
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
	inline std::size_t Corpus::filterArticles(
			const ArticleFunc& callbackArticle,
			StatusSetter& statusSetter
	) {
		this->checkThatTokenized("filterArticle");

		if(this->tokens.empty()) {
			return 0;
		}

		statusSetter.change("Filtering corpus...");

		std::size_t articleCounter{};
		std::size_t statusCounter{};
		std::size_t removed{};

		for(const auto& article : this->articleMap) {
			const auto articleEnd{TextMapEntry::end(article)};

			if(
					!callbackArticle(
							this->tokens,
							TextMapEntry::pos(article),
							articleEnd
					)
			) {
				// empty all tokens belonging to the article that has been filtered out
				for(
						std::size_t tokenIndex{TextMapEntry::pos(article)};
						tokenIndex < articleEnd;
						++tokenIndex
				) {
					this->tokenBytes -= this->tokens[tokenIndex].size();

					Helper::Memory::free(this->tokens[tokenIndex]);
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
	 * It will use the given manipulators
	 *  to manipulate each sentence (or
	 *  their respective tokens) first.
	 *
	 * Please make sure that the corpus is
	 *  tidied beforehand, i.e. UTF-8 and
	 *  other non-space whitespaces are
	 *  replaced by simple spaces. If needed,
	 *  sentences are created by simple
	 *  punctuation analysis.
	 *
	 * \note Once tokenized, the continous text
	 *   corpus will be lost. Create a copy
	 *   beforehand if you still need the
	 *   original corpus.
	 *
	 * \warning The vectors containing the
	 *   manipulators, models, dictionaries,
	 *   and languages must have the same
	 *   number of elements.
	 *
	 * \param manipulators A vector
	 *   containing the IDs of the
	 *   manipulators that will be used on all
	 *   sentences (or all of their tokens) in
	 *   the corpus, where every sentence is
	 *   separated by one of the following
	 *   punctuations from the others: @c .:!?;
	 *   or by the end of the current article,
	 *   date, or the whole corpus.
	 * \param models A vector of strings
	 *   containing the model to be used by the
	 *   manipulator with the same array index,
	 *   or an empty string if the respective
	 *   manipulator does not require a model.
	 * \param dictionaries A vector of strings
	 *   containing the dictionary to be used
	 *   by the manipulator with the same array
	 *   index, or an empty string if the
	 *   respective manipulator does not
	 *   require a dictionary.
	 * \param languages A vector of strings
	 *   containing the language to be used by
	 *   the manipulator with the same array
	 *   index, or an empty string if the
	 *   respective manipulator does not
	 *   require a dictionary or its default
	 *   language should be used.
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
	 *   manipulator has been specified, a
	 *   model or dictionary is missing for
	 *   a manipulator requiring one, or a
	 *   model, dictionary or language is set
	 *   for a manipulator that does not use
	 *   one.
	 */
	inline bool Corpus::tokenize(
			const std::vector<std::uint16_t>& manipulators,
			const std::vector<std::string>& models,
			const std::vector<std::string>& dictionaries,
			const std::vector<std::string>& languages,
			std::uint64_t freeMemoryEvery,
			StatusSetter& statusSetter
	) {
		bool isManipulation{
			std::any_of(
					manipulators.begin(),
					manipulators.end(),
					[](const auto manipulator) {
						return manipulator > corpusManipNone;
					}
			)
		};

		// prepare manipulators and check their configurations
		std::vector<Data::Tagger> taggers;
		Lemmatizer lemmatizer;
		TokenRemover tokenRemover;
		std::size_t manipulatorIndex{};
		std::size_t taggerIndex{};

		for(const auto& manipulator : manipulators) {
			switch(manipulator) {
			case corpusManipNone:
				break;

			case corpusManipEnglishStemmer:
			case corpusManipGermanStemmer:
				Corpus::notUsed("model", models, manipulatorIndex);
				Corpus::notUsed("dictionary", dictionaries, manipulatorIndex);
				Corpus::notUsed("language", languages, manipulatorIndex);

				break;

			case corpusManipTagger:
			case corpusManipTaggerPosterior:
				if(models.at(manipulatorIndex).empty()) {
					throw Exception(
						"Corpus::tokenize():"
						" No model set for part-of-speech tagger (manipulator #"
						+ std::to_string(manipulatorIndex + 1)
						+ ")"
					);
				}

				Corpus::notUsed("dictionary", dictionaries, manipulatorIndex);
				Corpus::notUsed("language", languages, manipulatorIndex);

				taggers.emplace_back();

				taggers.back().loadModel(models.at(manipulatorIndex));
				taggers.back().setPosteriorDecoding(manipulator == corpusManipTaggerPosterior);

				++taggerIndex;

				break;

			case corpusManipLemmatizer:
				if(dictionaries.at(manipulatorIndex).empty()) {
					throw Exception(
						"Corpus::tokenize():"
						" No dictionary set for lemmatizer (manipulator #"
						+ std::to_string(manipulatorIndex + 1)
						+ ")"
					);
				}

				Corpus::notUsed("model", models, manipulatorIndex);
				Corpus::notUsed("language", languages, manipulatorIndex);

				break;

			case corpusManipRemove:
				if(dictionaries.at(manipulatorIndex).empty()) {
					throw Exception(
						"Corpus::tokenize():"
						" No dictionary set for token remover (manipulator #"
						+ std::to_string(manipulatorIndex + 1)
						+ ")"
					);
				}

				Corpus::notUsed("model", models, manipulatorIndex);
				Corpus::notUsed("language", languages, manipulatorIndex);

				break;

			case corpusManipCorrect:
				Corpus::notUsed("model", models, manipulatorIndex);
				Corpus::notUsed("dictionary", dictionaries, manipulatorIndex);

				break;

			default:
				throw Exception(
						"Corpus::tokenize():"
						" Invalid manipulator (#"
						+ std::to_string(manipulator)
						+ ")"
				);
			}

			++manipulatorIndex;
		}

		// set manipulation callback
		auto callbackLambda = [
							   &manipulators,
							   &taggers,
							   &dictionaries,
							   &languages,
							   &lemmatizer,
							   &tokenRemover
		](
				Tokens::iterator sentenceBegin,
				Tokens::iterator sentenceEnd
		) {
			std::size_t manipulatorIndex{};
			std::size_t taggerIndex{};

			for(const auto& manipulator : manipulators) {
				switch(manipulator) {
				case corpusManipNone:
					break;

				case corpusManipTagger:
				case corpusManipTaggerPosterior:
					taggers.at(taggerIndex).label(sentenceBegin, sentenceEnd);

					++taggerIndex;

					break;

				case corpusManipEnglishStemmer:
					for(auto tokenIt{sentenceBegin}; tokenIt != sentenceEnd; ++tokenIt) {
						Data::Stemmer::stemEnglish(*tokenIt);
					}

					break;

				case  corpusManipGermanStemmer:
					for(auto tokenIt{sentenceBegin}; tokenIt != sentenceEnd; ++tokenIt) {
						Data::Stemmer::stemGerman(*tokenIt);
					}

					break;

				case corpusManipLemmatizer:
					for(auto& tokenIt{sentenceBegin}; tokenIt != sentenceEnd; ++tokenIt) {
						lemmatizer.lemmatize(*tokenIt, dictionaries.at(manipulatorIndex));
					}

					break;

				case corpusManipRemove:
					for(auto& tokenIt{sentenceBegin}; tokenIt != sentenceEnd; ++tokenIt) {
						tokenRemover.remove(*tokenIt, dictionaries.at(manipulatorIndex));
					}

					break;

				case corpusManipCorrect:
					//TODO

					break;

				default:
					throw Exception(
							"Corpus::tokenize():"
							" Invalid manipulator (#"
							+ std::to_string(manipulator)
							+ ")"
					);
				}

				++manipulatorIndex;
			}
		};

		// tokenize corpus
		return this->tokenizeCustom(
				isManipulation ? std::optional<SentenceFunc>{callbackLambda} : std::nullopt,
				freeMemoryEvery,
				statusSetter
		);
	}

	//! Converts a text corpus into processed tokens, using custom manipulators.
	/*!
	 * If a sentence manipulator is given,
	 *  first the sentence as a whole will
	 *  be manipulated, then the individual
	 *  tokens contained in this sentence.
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
	 * \param callback Optional callback
	 *   function (or lambda) that will be used
	 *   on all sentences in the corpus, where
	 *   every sentence is separated by one of
	 *   the following punctuations from the
	 *   others: @c .:;!? or by the end of the
	 *   current article, date, or the whole
	 *   corpus. A token will not be added to
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
			const std::optional<SentenceFunc>& callback,
			std::uint64_t freeMemoryEvery,
			StatusSetter& statusSetter
	) {
		if(this->tokenized) {
			if(
					!(
							this->tokenizeTokenized(
									callback,
									statusSetter
							)
					)
			) {
				return false;
			}
		}
		else {
			if(
					!(
							this->tokenizeContinuous(
									callback,
									freeMemoryEvery,
									statusSetter
							)
					)
			) {
				return false;
			}
		}

		statusSetter.finish();

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
		Helper::Memory::free(this->corpus);
		Helper::Memory::free(this->tokens);
		Helper::Memory::free(this->articleMap);
		Helper::Memory::free(this->dateMap);
		Helper::Memory::free(this->articleMap);
		Helper::Memory::free(this->sentenceMap);

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

					this->tokenBytes += Helper::Container::bytes(sentence);

					Helper::Container::moveInto(this->tokens, sentence);
				}

				TextMapEntry::length(this->articleMap.back()) = articleLength;
			}

			TextMapEntry::length(this->dateMap.back()) = dateLength;
		}

		this->tokenized = true;
	}

	// check that the corpus has not been tokenized, throw an exception otherwise
	inline void Corpus::checkThatNotTokenized(std::string_view function) const {
		if(this->tokenized) {
			throw Exception(
					"Corpus::"
					+ std::string(function)
					+ "(): The corpus has been tokenized"
			);
		}
	}

	// check that the corpus has been tokenized, throw an exception otherwise
	inline void Corpus::checkThatTokenized(std::string_view function) const {
		if(!(this->tokenized)) {
			throw Exception(
					"Corpus::"
					+ std::string(function)
					+ "(): The corpus has not been tokenized"
			);
		}
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

			Helper::Memory::freeIf(deleteInputData, id);
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

				Helper::Memory::free(dateMapEntry);
			}

			Helper::Memory::freeIf(deleteInputData, dateTime);
		}

		// concatenate corpus text
		this->corpus += text;

		Helper::Memory::freeIf(deleteInputData, text);

		// add space at the end of the corpus
		this->corpus.push_back(' ');
	}

	// add chunk to (tokenized) corpus
	inline void Corpus::addChunk(
			const std::string& content,
			const std::optional<std::reference_wrapper<const TextMap>>& articles,
			const std::optional<std::reference_wrapper<const TextMap>>& dates,
			const SentenceMap& sentences,
			bool& continueToken
	) {
		if(content.empty()) {
			return;
		}

		const auto chunkOffset{
			this->tokens.empty() ? 0 : this->tokens.size() - 1
		};

		// add sentences
		bool skip{
			/* does first sentence continue previous one (that has already been added)? */
			!(this->sentenceMap.empty())
			&& TextMapEntry::end(this->sentenceMap.back()) > chunkOffset
		};

		for(const auto& sentence : sentences) {
			if(skip) {
				/* skip first sentence */
				skip = false;

				continue;
			}

			this->sentenceMap.emplace_back(sentence);

			TextMapEntry::pos(this->sentenceMap.back()) += chunkOffset;
		}

		// prepare first token
		if(this->tokens.empty()) {
			this->tokens.emplace_back();
		}

		// add tokens
		for(const auto c : content) {
			if(c == '\n') {
				this->tokens.emplace_back();

				continue;
			}

			this->tokens.back().push_back(c);

			++(this->tokenBytes);
		}

		// add articles and dates, if necessary
		Corpus::addChunkMap(articles, this->articleMap, chunkOffset, continueToken);
		Corpus::addChunkMap(dates, this->dateMap, chunkOffset, continueToken);

		// save whether token will be continued in next chunk (if there is one)
		continueToken = content.back() != '\n';
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
							Corpus::exceptionSentenceBehind(
									function,
									"date",
									*sentence,
									*date,
									this->dateMap,
									std::next(date),
									this->tokens
							);
						}

						if(sentenceEnd > articleEnd) {
							Corpus::exceptionSentenceBehind(
									function,
									"article",
									*sentence,
									*article,
									this->articleMap,
									std::next(article),
									this->tokens
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

		// check number of size of tokenized corpus
		const auto bytes{
			std::accumulate(
					this->tokens.begin(),
					this->tokens.end(),
					std::size_t{},
					[](const auto& size, const auto& token) {
						return size + token.size();
					}
			)
		};

		if(bytes != this->tokenBytes) {
			Corpus::exceptionTokenBytes(function, this->tokenBytes, bytes);
		}
	}

	// add whole corpus as one chunk
	inline void Corpus::addAsOneChunk(
			std::size_t size,
			Tokens& to,
			Sizes& tokenNumsTo,
			std::vector<TextMap>& articleMapsTo,
			std::vector<TextMap>& dateMapsTo,
			std::vector<SentenceMap>& sentenceMapsTo
	) const {
		to.emplace_back(std::string{});

		to.back().reserve(to.size() + size);

		for(const auto& token : this->tokens) {
			to.back() += token;

			to.back().push_back('\n');
		}

		// remove last newline, if necessary
		if(!to.back().empty()) {
			to.back().pop_back();
		}

		articleMapsTo.emplace_back(this->articleMap);
		dateMapsTo.emplace_back(this->dateMap);
		sentenceMapsTo.emplace_back(this->sentenceMap);
		tokenNumsTo.emplace_back(this->tokens.size());
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
		Corpus::removeEmpty(this->tokens);
	}

	// tokenize already tokenized corpus, return whether thread is still running
	inline bool Corpus::tokenizeTokenized(
			std::optional<SentenceFunc> callback,
			StatusSetter& statusSetter
	) {
		// run manipulators on already tokenized corpus
		if(!callback) {
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

			// modify sentence (or its tokens), if necessary
			if(callback) {
				(*callback)(
						this->tokens.begin() + sentenceBegin,
						this->tokens.begin() + sentenceEnd
				);
			}

			for(auto tokenIndex{sentenceBegin}; tokenIndex < sentenceEnd; ++tokenIndex) {
				const auto& token{this->tokens.at(tokenIndex)};

				// remove empty token from date, article, and sentence map
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
			Corpus::removeEmptyEntries(this->dateMap);
		}

		// delete empty articles
		if(emptyArticles) {
			Corpus::removeEmptyEntries(this->articleMap);
		}

		// delete empty sentences
		if(emptySentences) {
			Corpus::removeEmptyEntries(this->sentenceMap);
		}

		// delete empty tokens
		if(numDeletedTokens > 0) {
			Corpus::removeEmpty(this->tokens);
		}

		// check consistency
		if(this->checkConsistency) {
			this->check("tokenizeTokenized");
		}

		return statusSetter.isRunning();
	}

	// tokenize still continuous corpus
	inline bool Corpus::tokenizeContinuous(
			std::optional<SentenceFunc> callback,
			std::uint64_t freeMemoryEvery,
			StatusSetter& statusSetter
	) {
		// tokenize continuous text corpus
		Tokens sentence;

		std::size_t tokenBegin{};
		std::size_t sentenceFirstToken{};
		std::size_t currentToken{};
		std::size_t statusCounter{};
		std::size_t corpusTrimmed{};

		bool inArticle{false};
		bool inDate{false};

		std::size_t articleFirstToken{};
		std::size_t dateFirstToken{};
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
					articleFirstToken = currentToken;
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
							articleFirstToken,
							currentToken - articleFirstToken,
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
					dateFirstToken = currentToken;
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
							dateFirstToken,
							currentToken - dateFirstToken,
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
					// end of token and sentence without separating character
					noSeparator = true;
				}
				else {
					// go to next character
					continue;
				}
			}

			// end token
			auto tokenLength{pos - tokenBegin};

			if(noSeparator) {
				++tokenLength;
			}

			if(tokenLength > 0) {
				sentence.emplace_back(this->corpus, tokenBegin - corpusTrimmed, tokenLength);

				++currentToken;

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

			tokenBegin = pos + 1;

			if(sentenceEnd && !sentence.empty()) {
				Corpus::processSentence(
						sentence,
						callback,
						appendToArticle,
						appendToDate,
						currentToken,
						sentenceFirstToken,
						newArticleMap,
						newDateMap,
						this->sentenceMap,
						this->tokens,
						this->tokenBytes
				);

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
					articleFirstToken,
					currentToken - articleFirstToken,
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
					dateFirstToken,
					currentToken - dateFirstToken,
					this->dateMap[nextDate - 1].value
			);

			endOfLastDate = true;
		}

		// add last token if not added yet
		if(tokenBegin - corpusTrimmed < this->corpus.size()) {
			sentence.emplace_back(
					this->corpus,
					tokenBegin - corpusTrimmed,
					this->corpus.size() + corpusTrimmed - tokenBegin
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
			Corpus::processSentence(
					sentence,
					callback,
					endOfLastArticle,
					endOfLastDate,
					currentToken,
					sentenceFirstToken,
					newArticleMap,
					newDateMap,
					this->sentenceMap,
					this->tokens,
					this->tokenBytes
			);
		}

		Helper::Memory::free(this->corpus);

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

		Helper::Memory::free(from);

		return statusSetter.isRunning();
	}

	// get all tokens that belong to a specific date or article
	inline Corpus::Tokens Corpus::getTokensForEntry(
			const TextMap& map,
			const std::string& id,
			const Tokens& tokens
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
			return Tokens{};
		}

		const auto entryEnd{TextMapEntry::end(*found)};

		Tokens copy;

		copy.reserve(found->l);

		for(auto tokenIndex{found->p}; tokenIndex < entryEnd; ++tokenIndex) {
			copy.emplace_back(tokens[tokenIndex]);
		}

		return copy;
	}

	// remove empty tokens
	inline void Corpus::removeEmpty(Tokens& from) {
		from.erase(
				std::remove_if(
						from.begin(),
						from.end(),
						[](const auto& str) {
							return str.empty();
						}
				),
				from.end()
		);
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
	//	NOTE: the result is between (maxLength - 3) and maxLength, although at least zero
	inline std::size_t Corpus::getValidLengthOfChunk(
			const std::string& source,
			std::size_t pos,
			std::size_t maxLength,
			std::size_t maxChunkSize
	) {
		// check arguments
		if(maxLength > maxChunkSize) {
			Corpus::exceptionInvalidMaxChunkSize(maxLength, maxChunkSize);
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

	// get a valid end of the current chunk (without cutting off UTF-8 characters), throws Corpus::Exception
	//	NOTE: the result is between (maxLength - 3) and maxLength, although at least zero
	inline std::size_t Corpus::getValidLengthOfChunk(
			const std::string& chunkContent,
			std::size_t maxChunkSize
	) {
		return Corpus::getValidLengthOfChunk(chunkContent, 0, maxChunkSize, maxChunkSize);
	}

	// check whether any token contains a newline
	inline void Corpus::checkTokensForChunking(const Tokens& tokens) {
		if(
				std::any_of(tokens.begin(), tokens.end(), [](const auto& token) {
					return std::any_of(token.begin(), token.end(), [](const auto c) {
						return c == '\n';
					});
				})
		) {
			throw Exception(
					"Corpus::copyChunksTokenized():"
					" Cannot split corpus into chunks"
					" as one of its tokens contains a newline"
			);
		}
	}

	// reserve memory for chunks
	inline void Corpus::reserveChunks(
			std::size_t chunks,
			Tokens& to,
			Sizes& tokenNumsTo,
			std::vector<TextMap>& articleMapsTo,
			std::vector<TextMap>& dateMapsTo,
			std::vector<SentenceMap>& sentenceMapsTo,
			bool hasArticleMap,
			bool hasDateMap
	) {
		to.reserve(to.size() + chunks);

		if(hasArticleMap) {
			articleMapsTo.reserve(articleMapsTo.size() + chunks);
		}

		if(hasDateMap) {
			dateMapsTo.reserve(dateMapsTo.size() + chunks);
		}

		sentenceMapsTo.reserve(sentenceMapsTo.size() + chunks);
		tokenNumsTo.reserve(tokenNumsTo.size() + chunks);
	}

	// check current sentence for map entry while filling tokenized chunk
	inline void Corpus::checkForEntry(
			std::string_view type,
			const SentenceMapEntry& sentence,
			std::size_t& nextIndex,
			const TextMap& map,
			std::size_t chunkOffset,
			TextMap& chunkMap,
			bool checkConsistency
	) {
		if(nextIndex > map.size()) {
			throw Exception(
					"Corpus::copyChunksTokenized():"
					" Skipped beyond end of last "
					+ std::string(type)
			);
		}

		if(nextIndex == map.size()) {
			return;
		}

		while(
				TextMapEntry::pos(map.at(nextIndex))
				== TextMapEntry::pos(sentence)
		) {
			const auto& next{map.at(nextIndex)};

			if(TextMapEntry::length(next) > 0) {
				chunkMap.emplace_back(
						TextMapEntry::pos(next) - chunkOffset,
						TextMapEntry::length(next),
						next.value
				);
			}

			++nextIndex;

			if(nextIndex == map.size()) {
				break;
			}
		}

		if(
				checkConsistency
				&& nextIndex < map.size()
				&& TextMapEntry::pos(map.at(nextIndex))
				< TextMapEntry::pos(sentence)
		) {
			const auto& next{map.at(nextIndex)};

			Corpus::exceptionUnexpectedBeforeSentence(
					type,
					next.value,
					TextMapEntry::pos(next),
					TextMapEntry::pos(sentence)
			);
		}
	}

	// finish chunk
	inline void Corpus::finishChunk(
			std::string& contentFrom,
			SentenceMap& sentencesFrom,
			Tokens& contentTo,
			Sizes& tokenNumTo,
			std::vector<SentenceMap>& sentencesTo,
			std::size_t chunkTokens,
			std::size_t& chunkOffset,
			bool splitToken,
			std::size_t nextChunkSize
	) {
		// move content
		contentTo.emplace_back(std::move(contentFrom));

		contentFrom.clear();

		if(nextChunkSize > 0) {
			contentFrom.reserve(nextChunkSize);
		}

		// copy sentences
		sentencesTo.emplace_back(sentencesFrom);

		sentencesFrom.clear();

		// add token count
		tokenNumTo.push_back(chunkTokens + (splitToken ? 1 : 0));

		// update chunk offset
		chunkOffset += chunkTokens;
	}

	// check whether to split current text map entry for chunking
	inline void Corpus::splitEntry(
			TextMap& map,
			std::size_t token,
			bool splitToken,
			TextMapEntry& remainingTo
	) {
		if(map.empty()) {
			return;
		}

		const auto end{TextMapEntry::end(map.back())};

		if(end > token || (end == token && splitToken)) {
			TextMapEntry::length(remainingTo) = end - token;
			TextMapEntry::length(map.back()) -= TextMapEntry::length(remainingTo);

			if(splitToken) {
				++TextMapEntry::length(map.back());
			}

			remainingTo.value = map.back().value;
		}
	}

	// finish text map for current chunk
	inline void Corpus::finishMap(TextMap& from, std::vector<TextMap>& to, TextMapEntry& remaining) {
		while(!from.empty() && TextMapEntry::length(from.back()) == 0) {
			from.pop_back();
		}

		to.emplace_back(std::move(from));

		from.clear();

		if(TextMapEntry::length(remaining) > 0) {
			from.emplace_back(std::move(remaining));

			Helper::Memory::free(remaining);
		}
	}

	// check that the specified value is not set, throw an exception otherwise
	inline void Corpus::notUsed(
			std::string_view type,
			const Tokens& values,
			std::size_t index
	) {
		if(!values.at(index).empty()) {
			std::string typeCapitalized(type);

			if(!type.empty()) {
				typeCapitalized[0] = std::toupper(typeCapitalized[0]);
			}

			throw Exception(
					"Corpus::tokenize():"
					" "
					+ typeCapitalized
					+ " ('"
					+ values.at(index)
					+ "') set but not used by manipulator #"
					+ std::to_string(index + 1)
			);
		}
	}

	// add map from chunk to (tokenized) corpus
	inline void Corpus::addChunkMap(
			const std::optional<std::reference_wrapper<const TextMap>>& from,
			TextMap& to,
			std::size_t offset,
			bool splitToken
	) {
		if(!from) {
			return;
		}

		if(from.value().get().empty()) {
			return;
		}

		bool skip{false};

		if(!to.empty() && to.back().value == from.value().get()[0].value) {
			/* combine last with current map */
			TextMapEntry::length(to.back()) += TextMapEntry::length(from.value().get()[0]);

			if(splitToken) {
				/* remove second part of splitted token from length */
				--TextMapEntry::length(to.back());
			}

			skip = true;
		}

		for(const auto& entry : from.value().get()) {
			if(skip) {
				/* skip first map entry */
				skip = false;

				continue;
			}

			to.emplace_back(entry);

			TextMapEntry::pos(to.back()) += offset;
		}
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
				Corpus::exceptionInvalidPosition(
						function,
						TextMapEntry::pos(entry),
						last,
						name
				);
			}

			last = TextMapEntry::end(entry);

			if(!isTokenized) {
				++last;
			}

			if(isDateMap && entry.value.length() != dateLength) {
				Corpus::exceptionInvalidDate(
						function,
						entry.value,
						name
				);
			}
		}

		// check the end position of the last entry in the map
		const auto& back{map.back()};

		if(TextMapEntry::end(back) != end) {
			Corpus::exceptionInvalidEnd(
					function,
					TextMapEntry::end(back),
					end,
					name
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
				Corpus::exceptionInvalidPosition(
						function,
						TextMapEntry::pos(entry),
						last,
						"sentence map"
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

	// process sentence for tokenization of the corpus
	inline void Corpus::processSentence(
			Tokens& sentence,
			const std::optional<SentenceFunc>& callback,
			bool inArticle,
			bool inDate,
			std::size_t& currentToken,
			std::size_t& sentenceFirstToken,
			TextMap& articleMap,
			TextMap& dateMap,
			SentenceMap& sentenceMap,
			Tokens& tokens,
			std::size_t& tokenBytes
	) {
		if(callback) {
			// modify sentence (or its tokens), if necessary
			(*callback)(sentence.begin(), sentence.end());
		}

		// modify tokens of the sentence, do not keep emptied tokens
		for(auto tokenIt{sentence.begin()}; tokenIt != sentence.end(); ) {
			if(tokenIt->empty()) {
				// remove empty token
				tokenIt = sentence.erase(tokenIt);

				--currentToken;

				// shrink article and date, if necessary
				if(inArticle) {
					--TextMapEntry::length(articleMap.back());

					if(TextMapEntry::length(articleMap.back()) == 0) {
						articleMap.pop_back();
					}
				}

				if(inDate) {
					--TextMapEntry::length(dateMap.back());

					if(TextMapEntry::length(dateMap.back()) == 0) {
						dateMap.pop_back();
					}
				}
			}
			else {
				tokenBytes += tokenIt->size();

				++tokenIt;
			}
		}

		if(!sentence.empty()) {
			// add sentence to map
			sentenceMap.emplace_back(
					sentenceFirstToken,
					sentence.size()
			);

			// move the tokens in the finished sentence into the tokens of the corpus
			Helper::Container::moveInto(tokens, sentence);
		}

		sentence.clear();

		sentenceFirstToken = currentToken; /* (= already next token) */
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
		std::vector<Tokens> content;

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
			std::vector<Tokens>& from,
			DateArticleSentenceMap& to,
			const std::string& date,
			const std::string& article
	) {
		if(from.empty()) {
			return;
		}

		auto& articleSentences{to[date][article]};

		Helper::Container::moveInto(articleSentences, from);
		Helper::Memory::free(from);
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

	// push as much of a (remaining) sentence into a chunk as possible,
	//  return whether the chunk is full, throws Corpus::Exception
	inline bool Corpus::pushSentence(
			const SentenceMapEntry& sentence,
			std::size_t chunkSize,
			std::size_t chunkOffset,
			std::size_t& chunkTokens,
			std::string& chunkContent,
			SentenceMap& chunkSentences,
			const Tokens& tokens,
			std::size_t& tokensComplete,
			std::size_t& additionalBytes
	) {
		auto bytesBefore{additionalBytes};

		// add sentence to chunk
		const auto sentenceOffset{
			tokensComplete - TextMapEntry::pos(sentence)
		};

		chunkSentences.emplace_back(
				TextMapEntry::pos(sentence) + sentenceOffset - chunkOffset,
				TextMapEntry::length(sentence) - sentenceOffset
		);

		// add tokens to chunk
		for(std::size_t token{tokensComplete}; token < TextMapEntry::end(sentence); ++token) {
			// get (remaining) token
			const auto oldSize{chunkContent.size()};

			chunkContent += (
					additionalBytes > 0 ? tokens.at(token).substr(additionalBytes)
					: tokens.at(token)
			);

			chunkContent.push_back('\n');

			if(chunkContent.size() > chunkSize) {
				/* (remaining) token does not fit into chunk completely */
				const auto size{Corpus::getValidLengthOfChunk(chunkContent, chunkSize)};

				chunkContent.erase(size);

				additionalBytes += chunkContent.size() - oldSize;

				if(token == TextMapEntry::pos(sentence) + sentenceOffset && additionalBytes == bytesBefore) {
					/* no content from current sentence has been added */
					chunkSentences.pop_back();

					if(tokensComplete == chunkOffset) {
						throw Exception(
								"Corpus::copyChunksTokenized():"
								" Separating tokens into chunks failed - chunk size too small?"
						);
					}
				}

				return true;
			}

			/* (remaining) token fits into chunk completely */
			additionalBytes = 0;
			bytesBefore = 0;

			++tokensComplete;
			++chunkTokens;
		}

		return false;
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

	// set locale for output streams
	inline void Corpus::locale(std::ostream& os) {
		os.imbue(std::locale(""));
	}

	/*
	 * INTERNAL STATIC HELPER FUNCTIONS FOR EXCEPTION HANDLING (private)
	 */

	// exception when trying to get an article: article map is empty
	inline void Corpus::exceptionGetNoArticleMap(
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

		throw Exception(exception.str());
	}

	// exception when trying to get an article: article is out of the article map's bounds
	inline void Corpus::exceptionArticleOutOfBounds(
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

		throw Exception(exception.str());
	}

	// exception when trying to get a date: invalid date length
	inline void Corpus::exceptionDateLength(
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

		throw Exception(exception.str());
	}

	// exception when combining chunks: article map of chunk does not start at its beginning
	inline void Corpus::exceptionArticleMapStart(
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

		throw Exception(exception.str());
	}

	// exception when combining tokenized chunks: length of last sentence exceeeds length of corpus
	inline void Corpus::exceptionLastSentenceLength(
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
		exception << "]) exceeds length of corpus (";
		exception << corpusSize;
		exception << ")";

		throw Exception(exception.str());
	}

	// exception when copying chunks: article lies behind its date
	inline void Corpus::exceptionArticleBehindDate(
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

		throw Exception(exception.str());
	}

	// exception when copying chunks: chunk size is too large
	inline void Corpus::exceptionChunkSize(std::size_t size, std::size_t chunkSize)	{
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::copyChunksContinuous(): Chunk is too large:";
		exception << size;
		exception << " > ";
		exception << chunkSize;

		throw Exception(exception.str());
	}

	// exception when copying chunks: end of articles reached before corpus ends
	inline void Corpus::exceptionArticleMapEnd(std::size_t pos, std::size_t size) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::copyChunksContinuous(): End of articles, but not of corpus ( #";
		exception << pos;
		exception << " < #";
		exception << size;
		exception << ")";

		throw Exception(exception.str());
	}

	// exception when copying tokenized chunks: article or date begins before current sentence
	inline void Corpus::exceptionUnexpectedBeforeSentence(
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

		throw Exception(exception.str());
	}

	// exception when filtering corpus by date: mismatch between article or sentence and date position
	inline void Corpus::exceptionMismatchWithDate(
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

		throw Exception(exception.str());
	}

	// exception when filtering corpus by date: date lies behind last article or sentence
	inline void Corpus::exceptionDateBehindLast(
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

		throw Exception(exception.str());
	}

	// exception when checking tokenized corpus: end of sentence behind date or article
	inline void Corpus::exceptionSentenceBehind(
			std::string_view function,
			std::string_view type,
			const std::pair<std::size_t, std::size_t>& sentence,
			const TextMapEntry& entry,
			const TextMap& map,
			const TextMap::const_iterator& next,
			const Tokens& tokens
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

		if(sentenceEnd > 0 && sentenceEnd <= tokens.size()) {
			exception << " ['";
			exception << tokens.at(sentenceEnd - 1);
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

		if(entryEnd > 0 && entryEnd <= tokens.size()) {
			exception << " ['";
			exception << tokens.at(entryEnd - 1);
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

		for(std::size_t token{TextMapEntry::pos(sentence)}; token < sentenceEnd; ++token) {
			if(token < tokens.size()) {
				if(addSpace) {
					exception << ' ';
				}
				else {
					addSpace = true;
				}

				exception << tokens.at(token);
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

		throw Exception(exception.str());
	}

	// exception when the stored size of a tokenized corpus is wrong
	inline void Corpus::exceptionTokenBytes(
			std::string_view function,
			std::size_t size,
			std::size_t actualSize
	) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::";
		exception << function;
		exception << "(): Corpus size is set to ";
		exception << size;
		exception << "B, but actual corpus size is ";
		exception << actualSize;
		exception << "B";

		throw Exception(exception.str());
	}

	// exception when setting maximum chunk size: invalid maximum chunk size given
	inline void Corpus::exceptionInvalidMaxChunkSize(std::size_t size, std::size_t max) {
		std::ostringstream exception;

		Corpus::locale(exception);

		exception << "Corpus::getValidLengthOfChunk(): Invalid maximum chunk size (";
		exception << size;
		exception << " > ";
		exception << max;
		exception << ")";

		throw Exception(exception.str());
	}

	// exception when filtering map: invalid position
	inline void Corpus::exceptionPositionTooSmall(
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

		throw Exception(exception.str());
	}

	// exception when checking map: invalid position
	inline void Corpus::exceptionInvalidPosition(
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

		throw Exception(exception.str());
	}

	// exception when checking date map: invalid date length
	inline void Corpus::exceptionInvalidDate(
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

		throw Exception(exception.str());
	}

	// exception when checking map: invalid end of last entry
	inline void Corpus::exceptionInvalidEnd(
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

		throw Exception(exception.str());
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_CORPUS_HPP_ */
