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
 * 	NOTE: All input data needs to be sorted by its date.
 *
 *  Created on: Mar 4, 2020
 *      Author: ans
 */

#ifndef DATA_CORPUS_HPP_
#define DATA_CORPUS_HPP_

#define DATA_CORPUS_CONSISTENCY_CHECKS

#include "../Helper/DateTime.hpp"
#include "../Main/Exception.hpp"
#include "../Struct/TextMap.hpp"

#include <iterator>	// std::distance
#include <string>	// std::string, std::to_string
#include <vector>	// std::vector

namespace crawlservpp::Data {

	/* DECLARATION */

	class Corpus {
	public:
		// getters
		std::string& getCorpus();
		const std::string& getCorpus() const;
		Struct::TextMap& getArticleMap();
		const Struct::TextMap& getArticleMap() const;
		Struct::TextMap& getDateMap();
		const Struct::TextMap& getDateMap() const;
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
		void clear();

		// class for corpus exceptions
		MAIN_EXCEPTION_CLASS();

	protected:
		std::string corpus;
		Struct::TextMap articleMap;
		Struct::TextMap dateMap;

	private:
		// private helper functions
		void updateChunkDate(
				size_t pos,
				size_t& n,
				Struct::TextMap& chunkDateMap,
				size_t articlePos,
				size_t articleLen
		) const;

#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
		static void checkMap(
				const Struct::TextMap& map,
				size_t corpusSize
		);
#endif /* DATA_CONSISTENCY_CHECKS */
	};

	/* IMPLEMENTATION */

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
		if(!chunkSize & !(this->corpus.empty()))
			throw Exception("Corpus::copyChunks(): Invalid chunk size for non-empty corpus");

		// check whether slicing is necessary
		if(this->corpus.size() <= chunkSize) {
			to.emplace_back(this->corpus);
			articleMapsTo.emplace_back(this->articleMap);
			dateMapsTo.emplace_back(this->dateMap);

			return;
		}

		// reserve approx. memory for chunks
		const size_t chunks = this->corpus.size() / chunkSize + 2; /* plus buffer */

		to.reserve(chunks);
		articleMapsTo.reserve(chunks);
		dateMapsTo.reserve(chunks);

		// slice corpus into chunks
		size_t pos = 0;			// position in corpus
		size_t articleN = 0;	// number of articles added so far
		size_t dateN = 0;		// number of dates added so far
		size_t size = 0;		// size of current chunk so far

		while(pos < this->corpus.size()) {
			if(this->articleMap.empty()) {
				// simply add next part of the corpus
				to.emplace_back(this->corpus.substr(pos, chunkSize));

				pos += chunkSize;
			}
			else {
				// create chunk from remaining articles
				std::string chunk;
				Struct::TextMap chunkArticleMap, chunkDateMap;

				for(; articleN < this->articleMap.size(); ++articleN) {
					// get current article
					const auto& article = this->articleMap.at(articleN);

					// concistency check
					if(article.pos != pos)
						throw Exception(
								"Corpus::copyChunks(): Error in article map - expected begin of article at #"
								+ std::to_string(pos)
								+ " instead of #"
								+ std::to_string(article.pos)
						);

					// check whether article is too large to fit into any chunk
					if(article.length > chunkSize) {
						// fill up current chunk with the beginning of the article
						size_t posInArticle = chunkSize - chunk.size();

						chunk.append(this->corpus.substr(article.pos, posInArticle));

						chunkArticleMap.emplace_back(pos, posInArticle, article.value);

						// update date map of the chunk
						this->updateChunkDate(pos, dateN, chunkDateMap, article.pos, posInArticle);

						to.emplace_back(chunk);
						articleMapsTo.emplace_back(chunkArticleMap);
						dateMapsTo.emplace_back(chunkDateMap);

						// add article-only chunks
						while(article.length - posInArticle > chunkSize) {
							Struct::TextMap oneArticleMap, oneDateMap;

							oneArticleMap.emplace_back(0, chunkSize, article.value);

							if(!(this->dateMap.empty()))
								oneDateMap.emplace_back(0, chunkSize, this->dateMap.at(dateN).value);

							to.emplace_back(this->corpus.substr(article.pos + posInArticle, chunkSize));

							articleMapsTo.emplace_back(oneArticleMap);
							dateMapsTo.emplace_back(oneDateMap);

							posInArticle += chunkSize;
						}

						// begin next chunk
						Struct::TextMap oneArticleMap, oneDateMap;

						oneArticleMap.emplace_back(0, article.length - posInArticle, article.value);

						if(!(this->dateMap.empty()))
							oneDateMap.emplace_back(0, article.length - posInArticle, this->dateMap.at(dateN).value);

						chunk = this->corpus.substr(article.pos + posInArticle, article.length - posInArticle);

						chunkArticleMap.swap(oneArticleMap);
						chunkDateMap.swap(oneDateMap);
					}
					// check whether article is too large to fit into current chunk
					else if(size + article.length > chunkSize)
						break;
					else {
						// save current length of chunk
						const auto len = chunk.length();

						// add article to current chunk
						chunk.append(this->corpus.substr(article.pos, article.length));

						// add article to current article map
						chunkArticleMap.emplace_back(len, article.length, article.value);

						// update date map of the chunk
						this->updateChunkDate(pos, dateN, chunkDateMap, article.pos, article.length);
					}

					// update position in corpus
					pos = article.pos + article.length;
				}

				// add current chunk
				to.emplace_back(chunk);
				articleMapsTo.emplace_back(chunkArticleMap);
				dateMapsTo.emplace_back(chunkDateMap);
			}
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
			auto& text = texts.at(n);

			// add article ID (or empty string) to article map
			if(articleIds.size() > n) {
				auto& articleId = articleIds.at(n);

				this->articleMap.emplace_back(this->corpus.size(), text.length(), articleId);

				if(deleteInputData && !articleId.empty())
					// free memory early
					std::string().swap(articleId);
			}
			else
				this->articleMap.emplace_back(this->corpus.size(), text.length());

			// add date to date map if necessary
			if(dateTimes.size() > n) {
				auto& dateTime = dateTimes.at(n);

				// check for valid (long enough) date/time
				if(dateTime.length() > 9) {
					// get only date (YYYY-MM-DD) from date/time
					const std::string date(dateTime, 10);

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

			if(articleMaps.size() > n) {
				// add article map
				auto& map = articleMaps.at(n);

				if(!map.empty()) {
					const auto& first = map.at(0);

#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
					// consistency check
					if(first.pos)
						throw Exception("Corpus::combine(): Article map in corpus chunk does not start at #0");
#endif /* DATA_CONSISTENCY_CHECKS */

					auto it = map.begin();

					// compare first new article ID with last one
					if(!(this->articleMap.empty()) && this->articleMap.back().value == first.value) {
						// append current article to last one
						this->articleMap.back().length += first.length;

						++it;
					}

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

#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
					// consistency check
					if(first.pos)
						throw("Corpus::combine(): Date map in corpus chunk does not start at #0");
#endif /* DATA_CONSISTENCY_CHECKS */

					auto it = map.begin();

					// compare first new date with last one
					if(!(this->dateMap.empty()) && this->dateMap.back().value == first.value) {
						// append current date to last one
						this->dateMap.back().length += first.length;

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

#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
		// consistency check
		if(this->dateMap.front().pos)
			throw Exception("Corpus::filterByDate(): Date map does not start at index #0");
#endif /* DATA_CONSISTENCY_CHECKS */

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

#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
			// consistency check
			else if(begin->pos > offset)
				throw Exception(
						"Corpus::filterByDate(): mismatch between positions of article (at #"
						+ std::to_string(begin->pos)
						+ ") and of date (at #"
						+ std::to_string(offset)
						+ ") in article and date maps of the corpus"
				);

		// consistency check
		if(begin == this->articleMap.end())
			throw Exception(
					"Corpus::filterByDate(): position of identified date (at #"
					+ std::to_string(offset)
					+ ") is behind the position of the last article (at #"
					+ std::to_string(this->articleMap.back().pos)
					+ ") in article and date maps of the corpus"
			);
#endif /* DATA_CONSISTENCY_CHECKS */

		// find first article not in range anymore
		end = begin;

		++end; /* current article is in range as has already been checked */

		for(; end != this->articleMap.end(); ++end)
			if(end->pos > last)
				break;

		// trim article map
		if(begin != this->articleMap.begin())
			// create trimmed article map and swap it with the the existing one
			Struct::TextMap(begin, end).swap(this->articleMap);
		else
			// only remove trailing articles
			this->dateMap.resize(std::distance(this->articleMap.begin(), end));

		// update positions in date and article maps
		for(auto& date : this->dateMap)
			date.pos -= offset;

		for(auto& article : this->articleMap)
			article.pos -= offset;

#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
		Corpus::checkMap(this->dateMap, this->corpus.size());
		Corpus::checkMap(this->articleMap, this->corpus.size());
#endif /* DATA_CONSISTENCY_CHECKS */

		return true;
	}

	// clear corpus
	inline void Corpus::clear() {
		this->corpus.clear();
		this->articleMap.clear();
		this->dateMap.clear();
	}

	// private helper function: check whether date has changed and update the date map accordingly
	inline void Corpus::updateChunkDate(
			size_t pos,
			size_t& n,
			Struct::TextMap& chunkDateMap,
			size_t articlePos,
			size_t articleLen
	) const {
		if(this->dateMap.empty())
			return;

		const auto& date = this->dateMap.at(n);

		if(chunkDateMap.empty()) {
#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
			// consistency check
			if(articlePos < date.pos || articlePos > date.pos + date.length)
				throw Exception("Corpus::updateChunkDate(): Error in article map or date map");
#endif /* DATA_CONSISTENCY_CHECKS */

			// use first date
			chunkDateMap.emplace_back(pos, articleLen, date.value);
		}
		else if(articlePos > date.pos + date.length) {
			// go to next date
			++n;

			const auto& nextDate = this->dateMap.at(n);

#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
			// consistency check
			if(articlePos < nextDate.pos || articlePos > nextDate.pos + nextDate.length)
				throw Exception("Corpus::copyChunks(): Error in article map or date map");
#endif /* DATA_CONSISTENCY_CHECKS */

			// use next date
			chunkDateMap.emplace_back(pos, articleLen, nextDate.value);
		}
		else
			// extend current date
			chunkDateMap.back().length += articleLen;
	}

#ifdef DATA_CORPUS_CONSISTENCY_CHECKS
	// check text map for inconsistencies, throws Corpus::Exception
	inline void Corpus::checkMap(const Struct::TextMap& map, size_t corpusSize) {
		if(map.empty())
			return;

		size_t last = 0;

		for(const auto& entry : map) {
			if(entry.pos != last)
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
#endif /* DATA_CONSISTENCY_CHECKS */

} /* crawlservpp::Data */


#endif /* DATA_CORPUS_HPP_ */
