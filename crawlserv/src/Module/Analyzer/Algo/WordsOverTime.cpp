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
 * Empty.cpp
 *
 * Empty template class for adding new algorithms to the application.
 *  Duplicate this (and the header) file to implement a new algorithm,
 *  then add it to 'All.cpp' in the same directory.
 *
 * TODO: change name, date and description of the file
 *
 *  Created on: Jul 23, 2020
 *      Author: ans
 */

/*
 * TODO: change name of the header file
 */
#include "WordsOverTime.hpp"

namespace crawlservpp::Module::Analyzer::Algo {

	/*
	 * CONSTRUCTION
	 */

	/*
	 * TODO: change names of the constructors
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
		fields.emplace_back("words", "BIGINT UNSIGNED");

		this->database.setTargetFields(fields);

		// initialize target table
		this->database.setTargetFields(fields);

		this->database.initTargetTable(true, true);
	}

	//! Generates the corpus.
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

		for(std::size_t index{}; index < this->config.generalInputSources.size(); ++index) {
			this->addCorpus(index, statusSetter);
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
	 * \sa onAlgoInit
	 */
	void WordsOverTime::onAlgoTick() {
		if(this->currentCorpus < this->corpora.size()) {
			this->addCurrent();

			++(this->currentCorpus);
		}
		else {
			this->saveCounts();

			// sleep forever (i.e. until the thread is terminated)
			this->finished();

			if(this->isRunning()) {
				this->sleep(std::numeric_limits<std::uint64_t>::max());
			}
		}
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

	//! Does nothing.
	void WordsOverTime::resetAlgo() {}

	/*
	 * ALGORITHM FUNCTIONS (private)
	 */

	// add counts for current corpus
	void WordsOverTime::addCurrent() {
		// set status message and reset progress
		std::string status{"occurrences"};

		if(this->corpora.size() > 1) {
			status += " in corpus #";
			status += std::to_string(this->currentCorpus + 1);
			status += "/";
			status += std::to_string(this->corpora.size());
		}

		status += "...";

		this->setStatusMessage("Counting " + status);
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "counts " + status);

		// set current corpus
		const auto& corpus = this->corpora[this->currentCorpus];
		const auto& dateMap = corpus.getcDateMap();
		const auto& sentenceMap = corpus.getcSentenceMap();
		const auto& articleMap = corpus.getcArticleMap();
		const auto& tokens = corpus.getcTokens();

		// check date and sentence map
		if(dateMap.empty()) {
			this->log(
					generalLoggingDefault,
					"WARNING: Corpus #"
					+ std::to_string(this->currentCorpus + 1)
					+ " does not have a date map and has been skipped."
			);

			return;
		}

		const auto firstDatePos{dateMap[0].pos};
		ResultMap::iterator dateIt;
		std::string currentDateGroup;
		std::size_t articleIndex{};
		std::size_t sentenceIndex{};
		std::size_t statusCounter{};
		std::size_t resultCounter{};

		// skip articles and sentences without (i.e. before first) date
		while(
				articleIndex < articleMap.size()
				&& articleMap[articleIndex].pos < firstDatePos
		) {
			++articleIndex;
		}

		while(
				sentenceIndex < sentenceMap.size()
				&& sentenceMap[sentenceIndex].first < firstDatePos
		) {
			++sentenceIndex;
		}

		for(const auto& date : dateMap) {
			// switch date group if necessary
			std::string dateGroup{date.value};

			Helper::DateTime::reduceDate(
					dateGroup,
					this->config.groupDateResolution
			);

			if(dateGroup != currentDateGroup) {
				dateIt = this->addDateGroup(dateGroup);

				currentDateGroup = dateGroup;
			}

			// do not count empty tokens (and articles/sentences consisting of empty tokens)!
			const auto end{TextMapEntry::end(date)};
			std::size_t articleEnd{false};
			std::size_t sentenceEnd{false};
			bool articleContent{false};
			bool sentenceContent{false};

			for(std::size_t tokenIndex{date.pos}; tokenIndex < end; ++tokenIndex) {
				if(articleIndex < articleMap.size() && articleMap[articleIndex].pos == tokenIndex) {
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

				if(sentenceIndex < sentenceMap.size() && sentenceMap[sentenceIndex].first == tokenIndex) {
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

				if(!(tokens[tokenIndex].empty())) {
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
				dateIt->second.articles.emplace(articleMap[articleIndex - 1].value);
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
	void WordsOverTime::saveCounts() {
		this->setStatusMessage("Saving results...");
		this->setProgress(0.F);

		this->log(generalLoggingDefault, "saves results...");

		const auto resultTable{
			this->getTargetTableName()
		};

		std::size_t statusCounter{};
		std::size_t resultCounter{};

		for(const auto& date : this->dateResults) {
			Data::InsertFieldsMixed data;

			data.columns_types_values.reserve(wordsNumberOfColumns);

			data.table = resultTable;

			data.columns_types_values.emplace_back(
					"analyzed__date",
					Data::Type::_string,
					Data::Value(date.first)
			);

			data.columns_types_values.emplace_back(
					"analyzed__articles",
					Data::Type::_uint64,
					Data::Value(date.second.articles.size())
			);

			data.columns_types_values.emplace_back(
					"analyzed__sentences",
					Data::Type::_uint64,
					Data::Value(date.second.sentences)
			);

			data.columns_types_values.emplace_back(
					"analyzed__words",
					Data::Type::_uint64,
					Data::Value(date.second.words)
			);

			this->database.insertCustomData(data);

			// target table updated
			this->database.updateTargetTable();

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

} /* namespace crawlservpp::Module::Analyzer::Algo */
