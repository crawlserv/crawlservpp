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

#include "../Helper/DateTime.hpp"
#include "../Helper/Utf8.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/TextMap.hpp"

#include <algorithm>	// std::find_if
#include <cstdint>		// std::uint8_t
#include <iterator>		// std::distance
#include <string>		// std::string, std::to_string
#include <vector>		// std::vector

//! Namespace for different types of data.
namespace crawlservpp::Data {

	/*
	 * CONSTANTS
	 */

	///@name Constants
	///@{

	//! The length of a date string in the format YYYY-MM-DD.
	constexpr auto dateLength{10};

	//! Maximum number of bytes used by one UTF-8-encoded multibyte character.
	constexpr std::uint8_t utf8MaxBytes{4};

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
	 * \note For the filtering by date to work,
	 *   all input data needs to be sorted by date,
	 *   while texts without date need to be added
	 *   first.
	 *
	 * \sa Struct::TextMap
	 */
	class Corpus {
	public:
		///@name Construction
		///@{

		explicit Corpus(bool consistencyChecks);

		///@}
		///@name Getters
		///@{

		[[nodiscard]] std::string& getCorpus();
		[[nodiscard]] const std::string& getcCorpus() const;
		[[nodiscard]] Struct::TextMap& getArticleMap();
		[[nodiscard]] const Struct::TextMap& getcArticleMap() const;
		[[nodiscard]] Struct::TextMap& getDateMap();
		[[nodiscard]] const Struct::TextMap& getcDateMap() const;
		[[nodiscard]] std::string get(std::size_t index) const;
		[[nodiscard]] std::string get(const std::string& id) const;
		[[nodiscard]] std::string getDate(const std::string& date) const;
		[[nodiscard]] std::size_t size() const;
		[[nodiscard]] bool empty() const;
		std::string substr(std::size_t from, std::size_t len);

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
		void combine(
				std::vector<std::string>& chunks,
				std::vector<Struct::TextMap>& articleMaps,
				std::vector<Struct::TextMap>& dateMaps,
				bool deleteInputData
		);

		///@}
		///@name Copying
		///@{

		void copy(std::string& to) const;
		void copy(std::string& to, Struct::TextMap& articleMapTo, Struct::TextMap& dateMapTo) const;
		void copyChunks(
				std::size_t chunkSize,
				std::vector<std::string>& to,
				std::vector<Struct::TextMap>& articleMapsTo,
				std::vector<Struct::TextMap>& dateMapsTo
		) const;

		///@}
		///@name Filtering
		///@{

		bool filterByDate(const std::string& from, const std::string& to);

		///@}
		///@name Cleanup
		///@{

		void clear();

		///@}

		//! Class for corpus exceptions.
		/*!
		 * This exception is being thrown when
		 * - an article has been requested by its index,
		 *    but the article map of the corpus is empty.
		 * - an article has been requested by its index,
		 *    but the article map of the corpus is too small
		 * - an article has been requested by its ID,
		 *    but the given ID references an empty string
		 * - articles have been requested for a specific date,
		 *    but the string containing this date has an
		 *    invalid length
		 * - consistency checks are enabled and an article
		 *    and a date map contradict each other
		 * - consistency checks are enabled and one of the
		 *    corpus chunks created is larger than the
		 *    maximum chunk size given
		 * - consistency checks are enabled and an article
		 *    map does not describe the whole corpus
		 * - consistency checks are enabled and the last
		 *    of the corpus chunks created is empty
		 * - consistency checks are enabled and the date
		 *    map does not start at the beginning of the
		 *    corpus
		 * - consistency checks are enabled and article map
		 *    or date map contain other inconsistencies
		 *
		 */
		MAIN_EXCEPTION_CLASS();

	protected:
		///@name Data
		///@{

		std::string corpus;
		Struct::TextMap articleMap;
		Struct::TextMap dateMap;

		///@}

	private:
		// private helper function
		[[nodiscard]] std::size_t getValidLengthOfSlice(
				std::size_t pos,
				std::size_t maxLength,
				std::size_t maxChunkSize
		) const;

		// private static helper function
		static void checkMap(
				const Struct::TextMap& map,
				std::size_t corpusSize
		);

		// internal property
		bool checkConsistency;
	};

	/*
	 * IMPLEMENTATION
	 */

	//! Constructor setting the internal property.
	/*!
	 * \param consistencyChecks If true, consistency checks
	 *   will be performed on the text corpus.
	 */
	inline Corpus::Corpus(bool consistencyChecks) : checkConsistency(consistencyChecks) {}

	//! Gets a reference to the text corpus.
	/*!
	 * \returns A reference to the text corpus
	 *   represented by the class.
	 */
	inline std::string& Corpus::getCorpus() {
		return this->corpus;
	}

	//! Gets a constant reference to the text corpus.
	/*!
	 * \returns A constant reference to the text
	 *   corpus represented by the class.
	 */
	inline const std::string& Corpus::getcCorpus() const {
		return this->corpus;
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

	//! Gets the article with the specified index.
	/*!
	 * \param index The index of the article in
	 *   the article map of the text corpus,
	 *   starting with zero.
	 *
	 * \returns A copy of the article inside the
	 *   text corpus with the given index.
	 *
	 * \throws Corpus::Exception if the article map
	 *   of the corpus is empty or the given index
	 *   is out of bounds (larger than the article map).
	 */
	inline std::string Corpus::get(std::size_t index) const {
		if(this->articleMap.empty()) {
			throw Exception(
					"Corpus::at(): requested article #"
					+ std::to_string(index)
					+ ", but article map is empty"
			);
		}

		if(index >= this->articleMap.size()) {
			throw Exception(
					"Corpus::at(): index (#"
					+ std::to_string(index)
					+ ") is out of bounds [#0;#"
					+ std::to_string(this->articleMap.size() - 1)
					+ "]"
			);
		}

		const auto& article{this->articleMap.at(index)};

		return this->corpus.substr(
				article.pos,
				article.length
		);
	}

	//! Gets the article with the specified ID.
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
	 *   an empty string.
	 */
	inline std::string Corpus::get(const std::string& id) const {
		// check argument
		if(id.empty()) {
			throw Exception("Corpus::get(): No ID specified");
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

	//! Gets all articles at the specified date.
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
	 *   has an invalid length.
	 */
	inline std::string Corpus::getDate(const std::string& date) const {
		// check argument
		if(date.length() != dateLength) {
			throw Exception(
					"Corpus::getDate(): Invalid length of date: "
					+ std::to_string(date.length())
					+ " instead of "
					+ std::to_string(dateLength)
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
				dateEntry->pos,
				dateEntry->length
		);
	}

	//! Gets the size of the text corpus in bytes.
	/*!
	 * \note The number of characters in the corpus
	 *   may differ, as it might be UTF-8-encoded.
	 *
	 * \returns The size of the corpus in bytes.
	 */
	inline std::size_t Corpus::size() const {
		return this->corpus.size();
	}

	//! Checks whether the corpus is empty.
	/*!
	 * \returns True, if the corpus is empty.
	 *   False otherwise.
	 */
	inline bool Corpus::empty() const {
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
	 * \throws std::out_of_range if the index
	 *   or length are invalid, std::bad_alloc
	 *   if the function needs to allocate storage
	 *   and fails.
	 */
	inline std::string Corpus::substr(std::size_t from, std::size_t len) {
		return this->corpus.substr(from, len);
	}

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
				std::string().swap(text);
			}

			// add space at the end of the corpus
			this->corpus.push_back(' ');
		}

		if(deleteInputData) {
			// free memory early
			std::vector<std::string>().swap(texts);
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
	 * \param dateTimes A refereence to the vector
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

		Struct::TextMapEntry dateMapEntry;

		for(std::size_t n{0}; n < texts.size(); ++n) {
			auto pos{this->corpus.size()};
			auto& text{texts.at(n)};

			// add article ID (or empty article) to article map
			if(articleIds.size() > n) {
				auto& articleId{articleIds.at(n)};

				this->articleMap.emplace_back(pos, text.length(), articleId);

				if(deleteInputData && !articleId.empty()) {
					// free memory early
					std::string().swap(articleId);
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

							dateMapEntry = Struct::TextMapEntry(this->corpus.size(), text.length(), date);
						}
					}
					else {
						// no date is set yet -> start new date
						dateMapEntry = Struct::TextMapEntry(this->corpus.size(), text.length(), date);
					}
				}
				else if(!dateMapEntry.value.empty()) {
					// no valid date found, but last date is set -> conclude last date
					this->dateMap.emplace_back(dateMapEntry);

					dateMapEntry = Struct::TextMapEntry();
				}

				if(deleteInputData && !dateTime.empty()) {
					// free memory early
					std::string().swap(dateTime);
				}
			}

			// concatenate corpus text
			this->corpus += text;

			if(deleteInputData) {
				// free memory early
				std::string().swap(text);
			}

			// add space at the end of the corpus
			this->corpus.push_back(' ');
		}

		if(deleteInputData) {
			// free memory early
			std::vector<std::string>().swap(texts);
			std::vector<std::string>().swap(articleIds);
			std::vector<std::string>().swap(dateTimes);
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

	//! Creates text corpus by combining previously separated corpus chunks, as well as their article and date maps.
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
	 * \sa copyChunks
	 */
	inline void Corpus::combine(
			std::vector<std::string>& chunks,
			std::vector<Struct::TextMap>& articleMaps,
			std::vector<Struct::TextMap>& dateMaps,
			bool deleteInputData
	) {
		// clear old corpus
		this->clear();

		// reserve memory
		const auto size{
			std::accumulate(
					chunks.cbegin(),
					chunks.cend(),
					static_cast<std::size_t>(0),
					[](const auto& a, const auto& b) {
						return a + b.size();
					}
			)
		};

		this->corpus.reserve(size);

		// add chunks
		for(std::size_t n{0}; n < chunks.size(); ++n) {
			// save current position in new corpus
			const auto pos{this->corpus.size()};

			// add text of chunk to corpus
			this->corpus += chunks.at(n);

			if(deleteInputData) {
				// free memory early
				std::string().swap(chunks.at(n));
			}

			bool beginsWithNewArticle{false};

			if(articleMaps.size() > n) {
				// add article map
				auto& map{articleMaps.at(n)};

				if(!map.empty()) {
					const auto& first{map.at(0)};

					// consistency check
					if(this->checkConsistency && first.pos > 1) {
						throw Exception(
								"Corpus::combine(): Article map in corpus chunk starts at #"
								+ std::to_string(first.pos)
								+ " instead of #0 or #1"
						);
					}

					auto it{map.cbegin()};

					// compare first new article ID with last one
					if(!(this->articleMap.empty()) && this->articleMap.back().value == first.value) {
						// append current article to last one
						this->articleMap.back().length += first.length;

						++it;
					}
					else {
						beginsWithNewArticle = true;
					}

					// add remaining articles to map
					for(; it != map.cend(); ++it) {
						this->articleMap.emplace_back(pos + it->pos, it->length, it->value);
					}

					if(deleteInputData) {
						// free memory early
						Struct::TextMap().swap(map);
					}
				}
			}

			if(dateMaps.size() > n) {
				// add date map
				auto& map{dateMaps.at(n)};

				if(!map.empty()) {
					const auto& first{map.at(0)};
					auto it{map.cbegin()};

					// compare first new date with last one
					if(!(this->dateMap.empty()) && this->dateMap.back().value == first.value) {
						// append current date to last one
						this->dateMap.back().length += first.length;

						// add missing space between articles if chunk begins with a new article and the date has been extended
						if(beginsWithNewArticle) {
							++(this->dateMap.back().length);
						}

						++it;
					}

					for(; it != map.cend(); ++it) {
						this->dateMap.emplace_back(pos + it->pos, it->length, it->value);
					}

					if(deleteInputData) {
						// free memory early
						Struct::TextMap().swap(map);
					}
				}
			}
		}

		if(deleteInputData) {
			// free memory early
			std::vector<std::string>().swap(chunks);
			std::vector<Struct::TextMap>().swap(articleMaps);
			std::vector<Struct::TextMap>().swap(dateMaps);
		}
	}

	//! Copies the underlying text corpus to the given string.
	/*!
	 * \param to Reference to a string to which the
	 *   text corpus will be copied.
	 */
	inline void Corpus::copy(std::string& to) const {
		to = this->corpus;
	}

	//! Copies the underlying text corpus, as well as its article and date map.
	/*!
	 * \param to Reference to a string to which the
	 *   text corpus will be copied.
	 * \param articleMapTo Reference to a text map
	 *   structure to which the article map will
	 *   be copied.
	 * \param dateMapTo Reference to a text map
	 *   structure to which the date map will be
	 *   copied.
	 */
	inline void Corpus::copy(std::string& to, Struct::TextMap& articleMapTo, Struct::TextMap& dateMapTo) const {
		to = this->corpus;
		articleMapTo = this->articleMap;
		dateMapTo = this->dateMap;
	}

	//! Copies underlying text corpus into chunks of the given size.
	/*!
	 * If the text corpus has an article and/or a date map,
	 *  a corresponding article and/or date map will be
	 *  created for each of the corpus slices.
	 *
	 * If the text corpus contains UTF-8-encoded
	 *  characters, they will not be split, creating the
	 *  possibility of chunks with slightly different sizes.
	 *
	 * \param chunkSize The maximum chunk size
	 *   in bytes.
	 * \param to Reference to a vector of strings
	 *   to which the texts of the corpus chunks
	 *   will be appended.
	 * \param articleMapsTo Reference to a vector
	 *   of text map structures, to which the
	 *   article maps of the chunks will be
	 *   appended. The vector will not be changed
	 *   if the text corpus does not possess an
	 *   article map.
	 * \param dateMapsTo Reference to a vector
	 *   of text map structures, to which the
	 *   date maps of the chunks will be
	 *   appended. The vector will not be changed
	 *   if the text corpus does not possess an
	 *   date map.
	 *
	 * \throws Corpus::Exception if the chunk size
	 *   is zero and the corpus is non-empty, or
	 *   if consistency checks have been enabled
	 *   on construction and
	 *   - the article and the date map contradict
	 *      each other
	 *   - one of the chunks created is larger than
	 *      the maximum chunk size given
	 *   - the article map does not describe the
	 *      whole corpus
	 *   - the last chunk created is empty
	 *
	 * \sa combine
	 */
	// copy corpus, article map and date map in chunks of specified size and return whether slicing was successful,
	//	throws Corpus::Exception
	inline void Corpus::copyChunks(
			std::size_t chunkSize,
			std::vector<std::string>& to,
			std::vector<Struct::TextMap>& articleMapsTo,
			std::vector<Struct::TextMap>& dateMapsTo
	) const {
		// check arguments
		if(chunkSize == 0) {
			if(this->corpus.empty()) {
				return;
			}

			throw Exception(
					"Corpus::copyChunks():"
					" Invalid chunk size zero for non-empty corpus"
			);
		}

		// check whether slicing is necessary
		if(this->corpus.size() <= chunkSize) {
			to.emplace_back(this->corpus);
			articleMapsTo.emplace_back(this->articleMap);
			dateMapsTo.emplace_back(this->dateMap);

			return;
		}

		// reserve probable memory for chunks
		const auto chunks{
			this->corpus.size() / chunkSize
			+ (this->corpus.size() % chunkSize > 0 ? 1 : 0)
		};


		to.reserve(chunks);

		if(!(this->articleMap.empty())) {
			articleMapsTo.reserve(chunks);
		}

		if(!(this->dateMap.empty())) {
			dateMapsTo.reserve(chunks);
		}

		// slice corpus into chunks
		bool noSpace{false};

		if(this->articleMap.empty()) {
			// no article part: simply add parts of the corpus
			std::size_t pos{0};

			while(pos < this->corpus.size()) {
				to.emplace_back(this->corpus, pos, this->getValidLengthOfSlice(pos, chunkSize, chunkSize));

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
				Struct::TextMap chunkArticleMap;
				Struct::TextMap chunkDateMap;
				std::string chunk;

				// add space if necessary
				if(noSpace) {
					chunk.push_back(' ');

					++corpusPos;

					noSpace = false;
				}

				for(; articleIt != this->articleMap.cend(); ++articleIt) { /* loop for multiple articles inside one chunk */
					if(dateIt != this->dateMap.cend()) {
						// check date of the article
						if(
								articlePos == 0
								&& articleIt->pos > dateIt->pos + dateIt->length
						) {
							++dateIt;
						}

						if(this->checkConsistency && articleIt->pos > dateIt->pos + dateIt->length) {
							throw Exception(
									"Article position (#"
									+ std::to_string(articleIt->pos)
									+ ") lies behind date at [#"
									+ std::to_string(dateIt->pos)
									+ ";#"
									+ std::to_string(dateIt->pos + dateIt->length)
									+ "]"
							);
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

								break; // chunk is full
							}
						}
						else {
							// add space to the beginning of the next chunk instead
							noSpace = true;

							// start next chunk with next article
							++articleIt;

							break; // chunk is full
						}
					}
					else {
						// fill the remainder of the chunk with part of the article
						auto fill{chunkSize - chunk.size()};

						if(fill == 0) {
							break;	/* chunk is full */
						}

						// check the end for valid UTF-8
						fill = this->getValidLengthOfSlice(corpusPos, fill, chunkSize);

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
						throw Exception(
								"Corpus::copyChunks(): Chunk is too large: "
								+ std::to_string(chunk.size())
								+ " > "
								+ std::to_string(chunkSize)
						);
					}

					if(articleIt == this->articleMap.cend() && corpusPos < this->corpus.size()) {
						throw Exception(
								"Corpus::copyChunks(): End of articles, but not of corpus: #"
								+ std::to_string(corpusPos)
								+ " < #"
								+ std::to_string(this->corpus.size())
						);
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
				throw Exception("Corpus::copyChunks(): End chunk is empty");
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
				throw Exception("Corpus::copyChunks(): End chunk is empty");
			}
		}
	}

	//! Filters the corpus by the given date(s).
	/*!
	 * Afterwards, the corpus will only contain
	 *  text marked with the given date(s), or
	 *  be empty if the given date(s) do not
	 *  correspond with any parts of the corpus.
	 *
	 * If the given strings are empty, no action
	 *  will be performed.
	 *
	 * \param from Constant reference to a string
	 *   containing the date to be filtered from.
	 * \param to Constant reference to a string
	 *   containing the date to be filtered to.
	 *
	 * \returns True, if the corpus has been
	 *   changed as result of the filtering
	 *   by the given date. False, if it remains
	 *   unchanged.
	 *
	 * \throws Corpus::Exception if consistency
	 *   checks have been enabled on construction
	 *   and the date map does not start at the
	 *   beginning of the corpus, the date map
	 *   and the article map contradict each other,
	 *   or they contain other inconsistencies.
	 *
	 */
	inline bool Corpus::filterByDate(const std::string& from, const std::string& to) {
		// check arguments
		if(from.empty() && to.empty()) {
			return false;
		}

		// check corpus
		if(this->corpus.empty()) {
			return false;
		}

		if(this->dateMap.empty()) {
			// no date map -> empty result
			this->clear();

			return true;
		}

		// consistency check
		if(this->checkConsistency && this->dateMap.front().pos > 0) {
			throw Exception("Corpus::filterByDate(): Date map does not start at index #0");
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

		if(begin == this->dateMap.cbegin() && end == this->dateMap.cend()) {
			// the whole corpus remains -> no changes necessary
			return false;
		}

		// trim date map
		if(begin != this->dateMap.cbegin()) {
			// create trimmed date map and swap it with the existing one
			Struct::TextMap(begin, end).swap(this->dateMap);
		}
		else {
			// only remove trailing dates
			this->dateMap.resize(std::distance(this->dateMap.cbegin(), end));
		}

		// save offset to be subtracted from all positions, (old) position of the last date and new total length of the corpus
		const auto offset{this->dateMap.front().pos};
		const auto last{this->dateMap.back().pos};
		const auto len{last  + this->dateMap.back().length - offset};

		// trim corpus
		if(offset > 0) {
			// replace the current corpus with the trimmed one
			this->corpus = std::string(this->corpus, offset, len);
		}
		else {
			// resize corpus to remove trailing texts
			this->corpus.resize(len);
		}

		// find first article in range
		begin = this->articleMap.cbegin();

		for(; begin != this->articleMap.cend(); ++begin) {
			if(begin->pos == offset) {
				break;
			}

			// consistency check
			if(this->checkConsistency && begin->pos > offset) {
				throw Exception(
						"Corpus::filterByDate(): mismatch between positions of article (at #"
						+ std::to_string(begin->pos)
						+ ") and of date (at #"
						+ std::to_string(offset)
						+ ") in article and date map of the corpus"
				);
			}
		}

		// consistency check
		if(this->checkConsistency && begin == this->articleMap.cend()) {
			throw Exception(
					"Corpus::filterByDate(): position of identified date (at #"
					+ std::to_string(offset)
					+ ") is behind the position of the last article (at #"
					+ std::to_string(this->articleMap.back().pos)
					+ ") in article and date map of the corpus"
			);
		}

		// find first article not in range anymore
		end = begin;

		++end; /* current article is in range as has already been checked */

		for(; end != this->articleMap.cend(); ++end) {
			if(end->pos > len) {
				break;
			}
		}

		// trim article map
		if(begin != this->articleMap.cbegin()) {
			// create trimmed article map and swap it with the the existing one
			Struct::TextMap(begin, end).swap(this->articleMap);
		}
		else {
			// only remove trailing articles
			this->articleMap.resize(std::distance(this->articleMap.cbegin(), end));
		}

		// update positions in date and article maps
		for(auto& date : this->dateMap) {
			date.pos -= offset;
		}

		for(auto& article : this->articleMap) {
			article.pos -= offset;
		}

		if(this->checkConsistency) {
			Corpus::checkMap(this->dateMap, this->corpus.size());
			Corpus::checkMap(this->articleMap, this->corpus.size());
		}

		return true;
	}

	//! Clears the corpus.
	/*!
	 * Clears the text of the corpus, as well as
	 *  article and date map, if they exist.
	 */
	inline void Corpus::clear() {
		this->corpus.clear();
		this->articleMap.clear();
		this->dateMap.clear();
	}

	// get a valid end of the current slice (without cutting off UTF-8 characters), throws Corpus::Exception
	//	NOTE: the result is between (maxLength - 3) and maxLength and at least zero
	inline std::size_t Corpus::getValidLengthOfSlice(
			std::size_t pos,
			std::size_t maxLength,
			std::size_t maxChunkSize
	) const {
		// check arguments
		if(maxLength > maxChunkSize) {
			throw Exception(
					"Corpus::getValidLengthOfSlice():"
					" Invalid maximum chunk size given ("
					+ std::to_string(maxLength)
					+ " > "
					+ std::to_string(maxChunkSize)
					+ ")"
			);
		}

		if(maxChunkSize == 0) {
			throw Exception(
					"Corpus::getValidLengthOfSlice():"
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

			if(Helper::Utf8::isLastCharValidUtf8(this->corpus.substr(checkFrom, checkLength))) {
				return maxLength - cut;
			}
		}

		if(cut == utf8MaxBytes) {
			throw Exception(
					"Corpus::getValidLengthOfSlice():"
					" Could not slice corpus because of invalid UTF-8 character"
			);
		}

		if(maxLength >= maxChunkSize) {
			throw Exception(
					"Corpus::getValidLengthOfSlice():"
					" The chunk size is too small to slice a corpus with UTF-8 character(s)"
			);
		}

		return 0;
	}

	// check text map for inconsistencies, throws Corpus::Exception
	inline void Corpus::checkMap(const Struct::TextMap& map, std::size_t corpusSize) {
		// check the argument
		if(map.empty()) {
			return;
		}

		// check the start positions of all entries in the map
		std::size_t last{0};

		for(const auto& entry : map) {
			if(entry.pos != last) {
				throw Exception(
						"Corpus::checkMap(): Invalid position #"
						+ std::to_string(entry.pos)
						+ " (expected: #"
						+ std::to_string(last - 1)
						+ ")"
				);
			}

			last = entry.pos + entry.length + 1;
		}

		// check the end position of the last entry in the map
		const auto& back{map.back()};

		if(back.pos + back.length != corpusSize) {
			throw Exception(
					"Corpus::checkMap(): Invalid end of last entry in map at #"
					+ std::to_string(back.pos + back.length)
					+ " (expected: at #"
					+ std::to_string(corpusSize)
					+ ")"
			);
		}
	}

} /* namespace crawlservpp::Data */

#endif /* DATA_CORPUS_HPP_ */
