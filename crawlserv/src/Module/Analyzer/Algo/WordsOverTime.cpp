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
 * WordsOverTime.cpp
 *
 * Counts the occurrence of articles, sentences, and tokens in a corpus
 *  over time.
 *
 *  Created on: Jan 03, 2021
 *      Author: ans
 */

#include "WordsOverTime.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	//! Continues a previously interrupted algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&, const ThreadStatus&)
	 */
	WordsOverTime::WordsOverTime(
			Main::Database& dbBase,
			const ThreadOptions& threadOptions,
			const ThreadStatus& threadStatus
	) : Module::Analyzer::Thread(
				dbBase,
				threadOptions,
				threadStatus
			) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	//! Starts a new algorithm run.
	/*!
	 * \copydetails Module::Thread::Thread(Main::Database&, const ThreadOptions&)
	 */
	WordsOverTime::WordsOverTime(Main::Database& dbBase,
							const ThreadOptions& threadOptions)
								:	Module::Analyzer::Thread(dbBase, threadOptions) {
		this->disallowPausing(); // disallow pausing while initializing
	}

	/*
	 * IMPLEMENTED GETTER
	 */

	//! Returns the name of the algorithm.
	/*!
	 * \returns A string view containing the
	 *   name of the implemented algorithm.
	 */
	std::string_view WordsOverTime::getName() const {
		return "WordsOverTime";
	}

	/*
	 * IMPLEMENTED ALGORITHM FUNCTIONS
	 */

	//! Initializes the target table for the algorithm.
	/*!
	 * \note When this function is called, neither the
	 *   prepared SQL statements, nor the queries have
	 *   been initialized yet.
	 */
	void WordsOverTime::onAlgoInitTarget() {
		// set target fields
		std::vector<StringString> fields;

		fields.reserve(wordsNumberOfColumns);

		fields.emplace_back("date", "VARCHAR(10)");
		fields.emplace_back("articles", "BIGINT UNSIGNED");
		fields.emplace_back("sentences", "BIGINT UNSIGNED");
		fields.emplace_back("tokens", "BIGINT UNSIGNED");

		this->database.setTargetFields(fields);

		// initialize target table
		this->database.setTargetFields(fields);

		this->database.initTargetTable(true, true);
	}

	//! Generates the corpus.
	/*!
	 * \throws WordsOverTime::Exception if the corpus
	 *   is empty.
	 */
	void WordsOverTime::onAlgoInit() {
		StatusSetter statusSetter(
				"Initializing algorithm...",
				1.F,
				[this](const auto& status) {
					this->setStatusMessage(status);
				},
				[this](const auto progress) {
					this->setProgress(progress);
				},
				[this]() {
					return this->isRunning();
				}
		);

		// check your sources
		this->log(generalLoggingVerbose, "checks sources...");

		this->checkCorpusSources(statusSetter);

		// request text corpus
		this->log(generalLoggingDefault, "gets text corpus...");

		if(!(this->addCorpora(true, statusSetter))) {
			if(this->isRunning()) {
				throw Exception("WordsOverTime::onAlgoInit(): Corpus is empty");
			}

			return;
		}

		// algorithm is ready
		this->log(generalLoggingExtended, "is ready.");

		/*
		 * NOTE: Do not set any thread status here, as the parent class
		 *        will revert to the original thread status after initialization.
		 */
	}

	//! Counts articles, sentences, and words.
	/*!
	 * One corpus will be processed in each tick.
	 *
	 * \note The corpus has already been genererated
	 *   on initialization.
	 *
	 * \throws WordsOverTime::Exception if the
	 *   corpus has no date map.
	 *
	 * \sa onAlgoInit
	 */
	void WordsOverTime::onAlgoTick() {
		if(this->firstTick) {
			this->count();

			this->firstTick = false;

			return;
		}

		// done: save results
		this->save();

		// sleep forever (i.e. until the thread is terminated)
		this->finished();
		this->sleep(std::numeric_limits<std::uint64_t>::max());
	}

	//! Does nothing.
	void WordsOverTime::onAlgoPause() {}

	//! Does nothing.
	void WordsOverTime::onAlgoUnpause() {}

	//! Does nothing.
	void WordsOverTime::onAlgoClear() {}

	/*
	 * IMPLEMENTED CONFIGURATION FUNCTIONS
	 */

	//! Does nothing.
	void WordsOverTime::parseAlgoOption() {}

	//! Does nothing.
	void WordsOverTime::checkAlgoOptions() {
		/*
		 * WARNING: The existence of sources cannot be checked here, because
		 * 	the database has not been prepared yet. Check them in onAlgoInit() instead.
		 */
	}

	//! Resets the algorithm.
	void WordsOverTime::resetAlgo() {
		this->firstTick = true;

		Helper::Memory::free(this->dateResults);
	}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// count tokens
	void WordsOverTime::count() {
		// check for corpora
		if(this->corpora.empty()) {
			throw Exception(
					"WordsOverTime::count():"
					" No corpus set"
			);
		}

		// set status message and reset progress
		this->setStatusMessage("Counting occurrences...");
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "counts occurrences...");

		// set current corpus
		const auto& corpus = this->corpora.back();
		const auto& dateMap = corpus.getcDateMap();
		const auto& sentenceMap = corpus.getcSentenceMap();
		const auto& articleMap = corpus.getcArticleMap();
		const auto& tokens = corpus.getcTokens();

		// check date and sentence map
		if(dateMap.empty()) {
			throw Exception(
					"WordsOverTime::count():"
					" Corpus has no date map"
			);
		}

		const auto firstDatePos{TextMapEntry::pos(dateMap[0])};
		ResultMap::iterator dateIt;
		std::string currentDateGroup;
		std::size_t articleIndex{};
		std::size_t sentenceIndex{};
		std::size_t statusCounter{};
		std::size_t resultCounter{};

		// skip articles and sentences without (i.e. before first) date
		this->log(generalLoggingVerbose, "skips articles...");

		while(
				articleIndex < articleMap.size()
				&& TextMapEntry::pos(articleMap[articleIndex]) < firstDatePos
		) {
			++articleIndex;
		}

		this->log(generalLoggingVerbose, "skips sentences...");

		while(
				sentenceIndex < sentenceMap.size()
				&& TextMapEntry::pos(sentenceMap[sentenceIndex]) < firstDatePos
		) {
			++sentenceIndex;
		}

		this->log(generalLoggingVerbose, "loops through dates and articles...");

		bool firstDate{true};

		for(const auto& date : dateMap) {
			// switch date group if necessary
			std::string dateGroup{date.value};

			Helper::DateTime::reduceDate(
					dateGroup,
					this->config.groupDateResolution
			);

			if(firstDate || dateGroup != currentDateGroup) {
				dateIt = this->addDateGroup(dateGroup);

				currentDateGroup = dateGroup;

				firstDate = false;
			}

			// do not count empty tokens (and articles/sentences consisting of empty tokens)!
			const auto end{TextMapEntry::end(date)};
			std::size_t articleEnd{};
			std::size_t sentenceEnd{};
			bool articleContent{false};
			bool sentenceContent{false};

			for(std::size_t tokenIndex{TextMapEntry::pos(date)}; tokenIndex < end; ++tokenIndex) {
				if(
						articleIndex < articleMap.size()
						&& TextMapEntry::pos(articleMap[articleIndex]) == tokenIndex
				) {
					// new article
					if(articleContent) {
						dateIt->second.articles.emplace(articleMap[articleIndex - 1].value);

						articleContent = false;
					}

					articleEnd = TextMapEntry::end(articleMap[articleIndex]);

					++articleIndex;
				}

				if(tokenIndex == articleEnd) {
					// end of article
					if(articleContent) {
						dateIt->second.articles.emplace(articleMap[articleIndex - 1].value);

						articleContent = false;
					}

					articleEnd = 0;
				}

				if(
						sentenceIndex < sentenceMap.size()
						&& TextMapEntry::pos(sentenceMap[sentenceIndex]) == tokenIndex
				) {
					// new sentence
					if(sentenceContent) {
						++(dateIt->second.sentences);

						sentenceContent = false;
					}

					sentenceEnd = TextMapEntry::end(sentenceMap[sentenceIndex]);

					++sentenceIndex;
				}

				if(tokenIndex == sentenceEnd) {
					// end of sentence
					if(sentenceContent) {
						++(dateIt->second.sentences);

						sentenceContent = false;
					}

					sentenceEnd = 0;
				}

				if(tokenIndex < tokens.size() && !(tokens[tokenIndex].empty())) {
					++(dateIt->second.words);

					if(articleEnd > 0) {
						articleContent = true;
					}

					if(sentenceEnd > 0) {
						sentenceContent = true;
					}
				}
			}

			if(articleContent) {
				dateIt->second.articles.emplace(articleMap.at(articleIndex - 1).value);
			}

			if(sentenceContent) {
				++(dateIt->second.sentences);
			}

			// update status if necessary
			++statusCounter;
			++resultCounter;

			if(statusCounter == wordsUpdateProgressEvery) {
				this->setProgress(
						static_cast<float>(resultCounter)
						/ dateMap.size()
				);

				statusCounter = 0;
			}

			if(!(this->isRunning())) {
				return;
			}
		}
	}

	// save counts to database
	void WordsOverTime::save() {
		// update status and write to log
		this->setStatusMessage("Saving results...");
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "saves results...");

		// get target table to store results in
		const auto targetTable{
			this->getTargetTableName()
		};

		// go through all results
		std::size_t statusCounter{};
		std::size_t resultCounter{};

		for(const auto& result : this->dateResults) {
			// fill gap between previous and current date, if necessary
			this->fillGap(targetTable, result.first);

			// insert actual data set
			this->insertDataSet(targetTable, result.first, result.second);

			// update status if necessary
			++statusCounter;
			++resultCounter;

			if(statusCounter == wordsUpdateProgressEvery) {
				this->setProgress(
						static_cast<float>(resultCounter)
						/ this->dateResults.size()
				);

				statusCounter = 0;
			}

			if(!(this->isRunning())) {
				return;
			}
		}
	}

	// add or expand a date group, return its iterator
	WordsOverTime::ResultMap::iterator WordsOverTime::addDateGroup(
			const std::string& group
	) {
		return this->dateResults.insert({group, {}}).first;
	}

	// fill gap between previous and current date, if necessary
	void WordsOverTime::fillGap(const std::string& table, const std::string& date) {
		if(!(this->config.groupDateFillGaps)) {
			return; /* filling gaps is disabled */
		}

		if(this->previousDate.empty()) {
			// first date: store date and return
			this->previousDate = date;

			return;
		}

		// retrieve and fill gap between previous and current date
		const auto missingDates{
			Helper::DateTime::getDateGap(
					this->previousDate,
					date,
					this->config.groupDateResolution
			)
		};

		for(const auto& missingDate : missingDates) {
			this->insertDataSet(table, missingDate, {});
		}

		this->previousDate = date;
	}

	// insert dataset into the target table
	void WordsOverTime::insertDataSet(const std::string& table, const std::string& date, const DateResults& results) {
		Data::InsertFieldsMixed data;

		data.columns_types_values.reserve(wordsNumberOfColumns);

		data.table = table;

		data.columns_types_values.emplace_back(
				"analyzed__date",
				Data::Type::_string,
				Data::Value(date)
		);

		data.columns_types_values.emplace_back(
				"analyzed__articles",
				Data::Type::_uint64,
				Data::Value(results.articles.size())
		);

		data.columns_types_values.emplace_back(
				"analyzed__sentences",
				Data::Type::_uint64,
				Data::Value(results.sentences)
		);

		data.columns_types_values.emplace_back(
				"analyzed__tokens",
				Data::Type::_uint64,
				Data::Value(results.words)
		);

		this->database.insertCustomData(data);

		// target table updated
		this->database.updateTargetTable();
	}

} /* namespace crawlservpp::Module::Analyzer::Algo */
