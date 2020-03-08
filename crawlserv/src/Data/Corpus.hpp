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
#include "../Main/Exception.hpp"
#include "../Struct/TextMap.hpp"

#include <algorithm>	// std::find_if
#include <iterator>		// std::distance
#include <string>		// std::string, std::to_string
#include <vector>		// std::vector

namespace crawlservpp::Data {

	/* DECLARATION */

	class Corpus {
	public:
		// constructor and destructor
		Corpus(bool consistencyChecks);
		virtual ~Corpus();

		// getters
		std::string& getCorpus();
		const std::string& getCorpus() const;
		Struct::TextMap& getArticleMap();
		const Struct::TextMap& getArticleMap() const;
		Struct::TextMap& getDateMap();
		const Struct::TextMap& getDateMap() const;
		std::string get(size_t index) const;
		std::string get(const std::string& id) const;
		std::string getDate(const std::string& date) const;
		size_t size() const;
		bool empty() const;

		// copyers
		void copy(std::string& to) const;
		void copy(std::string& to, Struct::TextMap& articleMapTo, Struct::TextMap& dateMapTo) const;
		void copyChunks(
				size_t chunkSize,
				std::vector<std::string>& to,
				std::vector<Struct::TextMap>& articleMapsTo,
				std::vector<Struct::TextMap>& dateMapsTo
		) const;

		// creaters (with optional data deletion)
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

		// functionality
		bool filterByDate(const std::string& from, const std::string& to);
		std::string substr(size_t from, size_t len);
		void clear();

		// class for corpus exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		std::string corpus;
		Struct::TextMap articleMap;
		Struct::TextMap dateMap;
		bool checkConsistency;

	private:
		static void checkMap(
				const Struct::TextMap& map,
				size_t corpusSize
		);
	};

	/* IMPLEMENTATION */

	// constructor
	inline Corpus::Corpus(bool consistencyChecks) : checkConsistency(consistencyChecks) {}

	// destructor
	inline Corpus::~Corpus() {}

	// get reference to corpus
	inline std::string& Corpus::getCorpus() {
		return this->corpus;
	}

	// get const reference to corpus
	inline const std::string& Corpus::getCorpus() const {
		return this->corpus;
	}

	// get reference to article map
	inline Struct::TextMap& Corpus::getArticleMap() {
		return this->articleMap;
	}

	// get const reference to article map
	inline const Struct::TextMap& Corpus::getArticleMap() const {
		return this->articleMap;
	}

	// get reference to date map
	inline Struct::TextMap& Corpus::getDateMap() {
		return this->dateMap;
	}

	// get const reference to date map
	inline const Struct::TextMap& Corpus::getDateMap() const {
		return this->dateMap;
	}

	// get the article with the specified index, throws Corpus::Exception
	inline std::string Corpus::get(size_t index) const {
		if(this->articleMap.empty())
			throw Exception(
					"Corpus::at(): requested article #"
					+ std::to_string(index)
					+ ", but article map is empty"
			);

		if(index >= this->articleMap.size())
			throw Exception(
					"Corpus::at(): index (#"
					+ std::to_string(index)
					+ ") is out of bounds [#0;#"
					+ std::to_string(this->articleMap.size() - 1)
					+ "]"
			);

		const auto& article = this->articleMap.at(index);

		return this->corpus.substr(
				article.pos,
				article.length
		);
	}

	// get the article with the specified ID, return empty string if the ID does not exist, throws Corpus::Exception
	inline std::string Corpus::get(const std::string& id) const {
		// check argument
		if(id.empty())
			throw Exception("Corpus::get(): No ID specified");

		const auto& articleEntry = std::find_if(
				this->articleMap.begin(),
				this->articleMap.end(),
				[&id](const auto& entry) {
					return entry.value == id;
				}
		);

		if(articleEntry == this->articleMap.end())
			return std::string();

		return this->corpus.substr(
				articleEntry->pos,
				articleEntry->length
		);
	}

	// get all articles at the specified date, return empty string if none exist, throws Corpus::Exception
	inline std::string Corpus::getDate(const std::string& date) const {
		// check argument
		if(date.length() != 10)
			throw Exception(
					"Corpus::getDate(): Invalid length of date: "
					+ std::to_string(date.length())
					+ " instead of 10"
			);

		const auto& dateEntry = std::find_if(
				this->dateMap.begin(),
				this->dateMap.end(),
				[&date](const auto& entry) {
					return entry.value == date;
				}
		);

		if(dateEntry == this->dateMap.end())
			return std::string();

		return this->corpus.substr(
				dateEntry->pos,
				dateEntry->length
		);
	}

	// get size of corpus
	inline size_t Corpus::size() const {
		return this->corpus.size();
	}

	// check whether corpus is empty
	inline bool Corpus::empty() const {
		return this->corpus.empty();
	}

	// copy corpus
	inline void Corpus::copy(std::string& to) const {
		to = this->corpus;
	}

	// copy corpus, article map and date map
	inline void Corpus::copy(std::string& to, Struct::TextMap& articleMapTo, Struct::TextMap& dateMapTo) const {
		to = this->corpus;
		articleMapTo = this->articleMap;
		dateMapTo = this->dateMap;
	}

	// copy corpus, article map and date map in chunks of specified size and return whether slicing was successful,
	//	throws Corpus::Exception
	inline void Corpus::copyChunks(
			size_t chunkSize,
			std::vector<std::string>& to,
			std::vector<Struct::TextMap>& articleMapsTo,
			std::vector<Struct::TextMap>& dateMapsTo
	) const {
		// check arguments
		if(!chunkSize) {
			if(this->corpus.empty())
				return;
			else
				throw Exception("Corpus::copyChunks(): Invalid chunk size zero for non-empty corpus");
		}

		// check whether slicing is necessary
		if(this->corpus.size() <= chunkSize) {
			to.emplace_back(this->corpus);
			articleMapsTo.emplace_back(this->articleMap);
			dateMapsTo.emplace_back(this->dateMap);

			return;
		}

		// reserve approx. memory for chunks
		const size_t chunks = this->corpus.size() / chunkSize
				+ (this->corpus.size() % chunkSize > 0 ? 1 : 0);

		to.reserve(chunks);

		if(!(this->articleMap.empty()))
			articleMapsTo.reserve(chunks);

		if(!(this->dateMap.empty()))
			dateMapsTo.reserve(chunks);

		// slice corpus into chunks
		bool noSpace = false;

		if(this->articleMap.empty()) {
			// no article part: simply add parts of the corpus
			size_t pos = 0;

			while(pos < this->corpus.size()) {
				to.emplace_back(this->corpus.substr(pos, chunkSize));

				pos += chunkSize;
			}

			noSpace = true;
		}
		else {
			size_t corpusPos = 0, articlePos = 0;
			auto articleIt = this->articleMap.begin(), dateIt = this->dateMap.begin();

			while(corpusPos < this->corpus.size()) { /* loop for chunks */
				// create chunk
				std::string chunk;
				Struct::TextMap chunkArticleMap, chunkDateMap;

				// add space if necessary
				if(noSpace) {
					chunk.push_back(' ');

					++corpusPos;

					noSpace = false;
				}

				for(; articleIt != this->articleMap.end(); ++articleIt) { /* loop for multiple articles inside one chunk */
					if(!(this->dateMap.empty())) {
						// check date of the article
						if(!articlePos && articleIt->pos > dateIt->pos + dateIt->length)
							++dateIt;

						if(this->checkConsistency && articleIt->pos > dateIt->pos + dateIt->length)
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

					// get remaining article length
					const size_t remaining = articleIt->length - articlePos;

					if(chunk.size() + remaining <= chunkSize) {
						if(remaining) {
							// add the remainder of the article to the chunk
							chunkArticleMap.emplace_back(chunk.size(), remaining, articleIt->value);

							if(!(this->dateMap.empty())) {
								if(!chunkDateMap.empty() && chunkDateMap.back().value == dateIt->value)
									chunkDateMap.back().length += remaining + 1; /* including space before article */
								else if(corpusPos >= dateIt->pos)
									chunkDateMap.emplace_back(chunk.size(), remaining, dateIt->value);
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
						const size_t fill = chunkSize - chunk.size();

						if(fill) {
							chunkArticleMap.emplace_back(chunk.size(), fill, articleIt->value);

							if(!(this->dateMap.empty())) {
								if(!chunkDateMap.empty() && chunkDateMap.back().value == dateIt->value)
									chunkDateMap.back().length += fill + 1; /* including space before the article */
								else if(corpusPos >= dateIt->pos)
									chunkDateMap.emplace_back(chunk.size(), fill, dateIt->value);
							}

							chunk.append(this->corpus, corpusPos, fill);

							// update positions
							corpusPos += fill;
							articlePos += fill;
						}

						break; // chunk is full
					}
				}

				// consistency checks
				if(this->checkConsistency) {
					if(chunk.size() > chunkSize)
						throw Exception(
								"Corpus::copyChunks(): Chunk is too large: "
								+ std::to_string(chunk.size())
								+ " > "
								+ std::to_string(chunkSize)
						);

					if(articleIt == this->articleMap.end() && corpusPos < this->corpus.size())
						throw Exception(
								"Corpus::copyChunks(): End of articles, but not of corpus: #"
								+ std::to_string(corpusPos)
								+ " < #"
								+ std::to_string(this->corpus.size())
						);
				}

				// check for empty chunk (should not happen)
				if(chunk.empty())
					break;

				// add current chunk
				to.emplace_back(chunk);
				articleMapsTo.emplace_back(chunkArticleMap);
				dateMapsTo.emplace_back(chunkDateMap);
			}
		}

		if(!(this->articleMap.empty()) && !to.empty()) {
			// consistency check
			if(this->checkConsistency && to.back().empty())
				throw Exception("Corpus::copyChunks(): End chunk is empty");

			// remove last space
			if(!noSpace)
				to.back().pop_back();

			// remove last chunk if it is empty
			if(to.back().empty())
				to.pop_back();

			// consistency check
			if(this->checkConsistency && to.back().empty())
				throw Exception("Corpus::copyChunks(): End chunk is empty");
		}
	}

	// create corpus from texts
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

			if(deleteInputData)
				// free memory early
				std::string().swap(text);

			// add space at the end of the corpus
			this->corpus.push_back(' ');
		}

		if(deleteInputData)
			// free memory early
			std::vector<std::string>().swap(texts);

		// remove last space if necessary
		if(!(this->corpus.empty()))
			this->corpus.pop_back();
	}

	// create corpus from parsed data
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

		for(size_t n = 0; n < texts.size(); ++n) {
			size_t pos = this->corpus.size();

			auto& text = texts.at(n);

			// add article ID (or empty string) to article map
			if(articleIds.size() > n) {
				auto& articleId = articleIds.at(n);

				this->articleMap.emplace_back(pos, text.length(), articleId
				);

				if(deleteInputData && !articleId.empty())
					// free memory early
					std::string().swap(articleId);
			}
			else
				this->articleMap.emplace_back(pos, text.length());

			// add date to date map if necessary
			if(dateTimes.size() > n) {
				auto& dateTime = dateTimes.at(n);

				// check for valid (long enough) date/time
				if(dateTime.length() > 9) {
					// get only date (YYYY-MM-DD) from date/time
					const std::string date(dateTime, 0, 10);

					// check whether a date is already set
					if(!dateMapEntry.value.empty()) {
						// date is already set -> compare with current date
						if(dateMapEntry.value == date)
							// last date equals current date -> append text to last date
							dateMapEntry.length += text.length() + 1;	/* include space before current text */
						else {
							// last date differs from current date -> conclude last date and start new date
							this->dateMap.emplace_back(dateMapEntry);

							dateMapEntry = Struct::TextMapEntry(this->corpus.size(), text.length(), date);
						}
					}
					else
						// no date is set yet -> start new date
						dateMapEntry = Struct::TextMapEntry(this->corpus.size(), text.length(), date);
				}
				else if(!dateMapEntry.value.empty()) {
					// no valid date found, but last date is set -> conclude last date
					this->dateMap.emplace_back(dateMapEntry);

					dateMapEntry = Struct::TextMapEntry();
				}

				if(deleteInputData && !dateTime.empty())
					// free memory early
					std::string().swap(dateTime);
			}

			// concatenate corpus text
			this->corpus += text;

			if(deleteInputData)
				// free memory early
				std::string().swap(text);

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
		if(!(this->corpus.empty()))
			// remove last space
			this->corpus.pop_back();

		// check for unfinished date
		if(!dateMapEntry.value.empty())
			// conclude last date
			this->dateMap.emplace_back(dateMapEntry);
	}

	// create corpus by combining corpus chunks and their article and date maps, throws Corpus::Exception
	inline void Corpus::combine(
			std::vector<std::string>& chunks,
			std::vector<Struct::TextMap>& articleMaps,
			std::vector<Struct::TextMap>& dateMaps,
			bool deleteInputData
	) {
		// clear old corpus
		this->clear();

		// add chunks
		for(size_t n = 0; n < chunks.size(); ++n) {
			// save current position in new corpus
			const size_t pos = this->corpus.size();

			// add text of chunk to corpus
			this->corpus += chunks.at(n);

			if(deleteInputData)
				// free memory early
				std::string().swap(chunks.at(n));

			bool beginsWithNewArticle = false;

			if(articleMaps.size() > n) {
				// add article map
				auto& map = articleMaps.at(n);

				if(!map.empty()) {
					const auto& first = map.at(0);

					// consistency check
					if(this->checkConsistency && first.pos > 1)
						throw Exception(
								"Corpus::combine(): Article map in corpus chunk starts at #"
								+ std::to_string(first.pos)
								+ " instead of #0 or #1"
					);

					auto it = map.begin();

					// compare first new article ID with last one
					if(!(this->articleMap.empty()) && this->articleMap.back().value == first.value) {
						// append current article to last one
						this->articleMap.back().length += first.length;

						++it;
					}
					else
						beginsWithNewArticle = true;

					// add remaining articles to map
					for(; it != map.end(); ++it)
						this->articleMap.emplace_back(pos + it->pos, it->length, it->value);

					if(deleteInputData)
						// free memory early
						Struct::TextMap().swap(map);
				}
			}

			if(dateMaps.size() > n) {
				// add date map
				auto& map = dateMaps.at(n);

				if(!map.empty()) {
					const auto& first = map.at(0);

					auto it = map.begin();

					// compare first new date with last one
					if(!(this->dateMap.empty()) && this->dateMap.back().value == first.value) {
						// append current date to last one
						this->dateMap.back().length += first.length;

						// add missing space between articles if chunk begins with a new article and the date has been extended
						if(beginsWithNewArticle)
							++(this->dateMap.back().length);

						++it;
					}

					for(; it != map.end(); ++it)
						this->dateMap.emplace_back(pos + it->pos, it->length, it->value);

					if(deleteInputData)
						// free memory early
						Struct::TextMap().swap(map);
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

	// filter corpus by date(s), return whether the corpus has changed, throws Corpus::Exception
	inline bool Corpus::filterByDate(const std::string& from, const std::string& to) {
		// check arguments
		if(from.empty() && to.empty())
			return false;

		// check corpus
		if(this->corpus.empty())
			return false;

		if(this->dateMap.empty()) {
			// no date map -> empty result
			this->clear();

			return true;
		}

		// consistency check
		if(this->checkConsistency && this->dateMap.front().pos)
			throw Exception("Corpus::filterByDate(): Date map does not start at index #0");

		// find first date in range
		auto begin = this->dateMap.begin();

		for(; begin != this->dateMap.end(); ++begin)
			if(Helper::DateTime::isISODateInRange(begin->value, from, to))
				break;

		if(begin == this->dateMap.end()) {
			// no date in range -> empty result
			this->clear();

			return true;
		}

		// find first date not in range anymore
		auto end = begin;

		++end; /* current date is in range as has already been checked */

		for(; end != this->dateMap.end(); ++end)
			if(!Helper::DateTime::isISODateInRange(end->value, from, to))
				break;

		if(begin == this->dateMap.begin() && end == this->dateMap.end())
			// the whole corpus remains -> no changes necessary
			return false;

		// trim date map
		if(begin != this->dateMap.begin())
			// create trimmed date map and swap it with the existing one
			Struct::TextMap(begin, end).swap(this->dateMap);
		else
			// only remove trailing dates
			this->dateMap.resize(std::distance(this->dateMap.begin(), end));

		// save offset to be subtracted from all positions, (old) position of the last date and new total length of the corpus
		const size_t offset = this->dateMap.front().pos;
		const size_t last = this->dateMap.back().pos;
		const size_t len = last  + this->dateMap.back().length - offset;

		// trim corpus
		if(offset)
			// create trimmed corpus and swap it with the existing one
			std::string(this->corpus, offset, len).swap(this->corpus);
		else
			// resize corpus to remove trailing texts
			this->corpus.resize(len);

		// find first article in range
		begin = this->articleMap.begin();

		for(; begin != this->articleMap.end(); ++begin)
			if(begin->pos == offset)
				break;
			// consistency check
			else if(this->checkConsistency && begin->pos > offset)
				throw Exception(
						"Corpus::filterByDate(): mismatch between positions of article (at #"
						+ std::to_string(begin->pos)
						+ ") and of date (at #"
						+ std::to_string(offset)
						+ ") in article and date maps of the corpus"
				);

		// consistency check
		if(this->checkConsistency && begin == this->articleMap.end())
			throw Exception(
					"Corpus::filterByDate(): position of identified date (at #"
					+ std::to_string(offset)
					+ ") is behind the position of the last article (at #"
					+ std::to_string(this->articleMap.back().pos)
					+ ") in article and date maps of the corpus"
			);

		// find first article not in range anymore
		end = begin;

		++end; /* current article is in range as has already been checked */

		for(; end != this->articleMap.end(); ++end)
			if(end->pos > len)
				break;

		// trim article map
		if(begin != this->articleMap.begin())
			// create trimmed article map and swap it with the the existing one
			Struct::TextMap(begin, end).swap(this->articleMap);
		else
			// only remove trailing articles
			this->articleMap.resize(std::distance(this->articleMap.begin(), end));

		// update positions in date and article maps
		for(auto& date : this->dateMap)
			date.pos -= offset;

		for(auto& article : this->articleMap)
			article.pos -= offset;

		if(this->checkConsistency) {
			Corpus::checkMap(this->dateMap, this->corpus.size());
			Corpus::checkMap(this->articleMap, this->corpus.size());
		}

		return true;
	}

	// get a substring from the corpus
	inline std::string Corpus::substr(size_t from, size_t len) {
		return this->corpus.substr(from, len);
	}

	// clear corpus
	inline void Corpus::clear() {
		this->corpus.clear();
		this->articleMap.clear();
		this->dateMap.clear();
	}

	// check text map for inconsistencies, throws Corpus::Exception
	inline void Corpus::checkMap(const Struct::TextMap& map, size_t corpusSize) {
		if(map.empty())
			return;

		size_t last = -1;

		for(const auto& entry : map) {
			if(entry.pos != last + 1)
				throw Exception(
						"Corpus::checkMap(): Invalid position #"
						+ std::to_string(entry.pos)
						+ " (expected: #"
						+ std::to_string(last)
						+ ")"
				);

			last = entry.pos + entry.length;
		}

		const auto& back = map.back();

		if(back.pos + back.length != corpusSize)
			throw Exception(
					"Corpus::checkMap(): Invalid end of last entry in map at #"
					+ std::to_string(back.pos + back.length)
					+ " (expected: at #"
					+ std::to_string(corpusSize)
					+ ")"
			);
	}

} /* crawlservpp::Data */


#endif /* DATA_CORPUS_HPP_ */
